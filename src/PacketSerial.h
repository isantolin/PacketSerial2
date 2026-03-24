#pragma once

#include <stdint.h>
#include <stddef.h>
#include <etl/delegate.h>
#include <etl/circular_buffer.h>
#include <etl/bitset.h>
#include <etl/span.h>
#include <etl/expected.h>

#include "ErrorCode.h"
#include "ICodec.h"
#include "Safety.h"

namespace PacketSerial2 {

/**
 * @brief Null CRC implementation (Default).
 */
struct NoCRC {
    using ValueType = uint32_t;
    static constexpr size_t Size = 0;
    void reset() {}
    void add(uint8_t) {}
    uint32_t value() const { return 0; }
};

/**
 * @brief Industrial-grade PacketSerial implementation (v2.1).
 * 
 * Features:
 * - ISR-Safe: Customizable Locking Policy (Atomic sections).
 * - SIL-2 Ready: Watchdog Heartbeat hooks.
 * - Zero-Heap / Zero-STL.
 * - Deterministic Memory: User-provided static buffers.
 * 
 * @tparam Codec The codec class (e.g., COBS, SLIP).
 * @tparam CRCType The CRC implementation to use (default is NoCRC).
 * @tparam LockPolicy Locking policy for ISR safety (default is NoLock).
 * @tparam WatchdogPolicy Heartbeat policy for WDT (default is NoWatchdog).
 */
template <
    typename Codec, 
    typename CRCType = NoCRC, 
    typename LockPolicy = NoLock,
    typename WatchdogPolicy = NoWatchdog
>
class PacketSerial {
public:
    using PacketHandler = etl::delegate<void(etl::span<const uint8_t>)>;
    using ErrorHandler = etl::delegate<void(ErrorCode)>;

    /**
     * @brief Construct PacketSerial with external memory and safety policies.
     * 
     * @param rx_storage Buffer for raw incoming bytes.
     * @param work_buffer Buffer for decoding/encoding (must be large enough).
     * @param lock User-provided lock instance (optional).
     * @param watchdog User-provided watchdog instance (optional).
     */
    PacketSerial(etl::span<uint8_t> rx_storage, 
                 etl::span<uint8_t> work_buffer,
                 LockPolicy lock = LockPolicy(),
                 WatchdogPolicy watchdog = WatchdogPolicy()) 
        : _rx_buffer(rx_storage.data(), rx_storage.size()), 
          _work_buffer(work_buffer),
          _lock(lock),
          _watchdog(watchdog) {}

    void setPacketHandler(PacketHandler handler) { _onPacket = handler; }
    void setErrorHandler(ErrorHandler handler) { _onError = handler; }

    /**
     * @brief Process incoming data from a stream.
     * 
     * Uses the LockPolicy to protect the internal circular buffer.
     */
    template <typename StreamType>
    void update(StreamType& stream) {
        while (stream.available() > 0) {
            uint8_t data = static_cast<uint8_t>(stream.read());

            if (data == Codec::Marker) {
                // Critical section starts for buffer access
                _lock.lock();
                bool has_data = !_rx_buffer.empty();
                _lock.unlock();

                if (has_data) {
                    processPacket();
                }
            } else {
                _lock.lock();
                if (_rx_buffer.full()) {
                    _status.set(STATUS_OVERFLOW);
                    _rx_buffer.clear(); 
                    _lock.unlock();
                    handleError(ErrorCode::BufferOverflow);
                } else {
                    _rx_buffer.push_back(data);
                    _lock.unlock();
                }
            }
        }
    }

    /**
     * @brief Encode and send a packet of data.
     * 
     * Optimized version: Calculates CRC on-the-fly and uses work_buffer to 
     * avoid any stack allocation of frame buffers.
     */
    template <typename StreamType>
    etl::expected<size_t, ErrorCode> send(StreamType& stream, etl::span<const uint8_t> packet) {
        if (packet.empty()) return 0;

        size_t payloadSize = packet.size();
        size_t totalUnencodedSize = payloadSize + CRCType::Size;
        
        // 1. Check if work_buffer can hold the encoded result
        if (Codec::getEncodedBufferSize(totalUnencodedSize) > _work_buffer.size()) {
            handleError(ErrorCode::BufferFull);
            return etl::unexpected(ErrorCode::BufferFull);
        }

        // 2. Prepare frame in work_buffer (Zero-Copy CRC)
        _lock.lock();
        _crc_engine.reset();
        
        // Copy payload and calculate CRC at the same time
        for (size_t i = 0; i < payloadSize; ++i) {
            _work_buffer[i] = packet[i];
            _crc_engine.add(packet[i]);
            if ((i % 64) == 0) _watchdog.feed(); // Feed WDT for long packets
        }

        if constexpr (CRCType::Size > 0) {
            uint32_t cv = _crc_engine.value();
            for (size_t i = 0; i < CRCType::Size; ++i) {
                _work_buffer[payloadSize + i] = static_cast<uint8_t>((cv >> (i * 8)) & 0xFF);
            }
        }

        // 3. Encode Frame
        Codec codec;
        // The encode implementation should handle potential overlaps or 
        // we use a safe offset for the work_buffer if needed. 
        // For simplicity, we encode the frame prepared at [0...totalUnencodedSize].
        auto result = codec.encode(etl::span<const uint8_t>(_work_buffer.data(), totalUnencodedSize), 
                                   _work_buffer);

        if (result.has_value()) {
            size_t encodedSize = result.value();
            for (size_t i = 0; i < encodedSize; ++i) {
                stream.write(_work_buffer[i]);
            }
            stream.write(Codec::Marker);
            _lock.unlock();
            return encodedSize + 1;
        } else {
            _lock.unlock();
            handleError(result.error());
            return etl::unexpected(result.error());
        }
    }

    static constexpr size_t STATUS_OVERFLOW = 0;

private:
    /**
     * @brief Internal processing of a complete frame.
     */
    void processPacket() {
        _lock.lock();
        size_t size = _rx_buffer.size();
        if (size > _work_buffer.size()) {
            _rx_buffer.clear();
            _lock.unlock();
            handleError(ErrorCode::BufferOverflow);
            return;
        }

        // Linearize circular to work buffer
        for (size_t i = 0; i < size; ++i) {
            _work_buffer[i] = _rx_buffer[i];
            if ((i % 64) == 0) _watchdog.feed();
        }
        _rx_buffer.clear();
        _lock.unlock();

        // 1. Decode Frame
        Codec codec;
        auto result = codec.decode(etl::span<const uint8_t>(_work_buffer.data(), size), 
                                   _work_buffer);

        if (result.has_value()) {
            size_t decodedSize = result.value();
            _watchdog.feed();

            // 2. Validate CRC
            if constexpr (CRCType::Size > 0) {
                if (decodedSize < CRCType::Size) {
                    handleError(ErrorCode::IncompletePacket);
                    return;
                }
                
                size_t payloadSize = decodedSize - CRCType::Size;
                _crc_engine.reset();
                for (size_t i = 0; i < payloadSize; ++i) {
                    _crc_engine.add(_work_buffer[i]);
                }
                
                uint32_t receivedCrc = 0;
                for (size_t i = 0; i < CRCType::Size; ++i) {
                    receivedCrc |= (static_cast<uint32_t>(_work_buffer[payloadSize + i]) << (i * 8));
                }
                
                if (_crc_engine.value() != receivedCrc) {
                    handleError(ErrorCode::InvalidChecksum);
                    return;
                }
                
                if (_onPacket) _onPacket(etl::span<const uint8_t>(_work_buffer.data(), payloadSize));
            } else {
                if (_onPacket) _onPacket(etl::span<const uint8_t>(_work_buffer.data(), decodedSize));
            }
        } else {
            handleError(result.error());
        }
    }

    void handleError(ErrorCode error) {
        if (_onError) _onError(error);
    }

    // Shared State (Protected by _lock)
    etl::circular_buffer_ext<uint8_t> _rx_buffer;
    etl::span<uint8_t> _work_buffer;
    etl::bitset<8> _status;

    // Policies
    LockPolicy _lock;
    WatchdogPolicy _watchdog;

    // Engine Components
    PacketHandler _onPacket;
    ErrorHandler _onError;
    CRCType _crc_engine;
};

} // namespace PacketSerial2
