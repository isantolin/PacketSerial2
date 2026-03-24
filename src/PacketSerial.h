#pragma once

#include <stdint.h>
#include <stddef.h>
#include <etl/delegate.h>
#include <etl/circular_buffer.h>
#include <etl/bitset.h>
#include <etl/span.h>
#include <etl/expected.h>
#include <etl/algorithm.h>

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
 * @brief Industrial-grade PacketSerial implementation (v2.2).
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
     */
    template <typename StreamType>
    void update(StreamType& stream) {
        while (stream.available() > 0) {
            uint8_t data = static_cast<uint8_t>(stream.read());

            if (data == Codec::Marker) {
                size_t bytes_to_process = 0;
                
                _lock.lock();
                bytes_to_process = _rx_buffer.size();
                if (bytes_to_process > 0) {
                    // [SIL-2] Atomic linearization to work buffer
                    for (size_t i = 0; i < bytes_to_process; ++i) {
                        _work_buffer[i] = _rx_buffer[i];
                    }
                    _rx_buffer.clear();
                }
                _lock.unlock();

                if (bytes_to_process > 0) {
                    processFrame(bytes_to_process);
                }
            } else {
                _lock.lock();
                if (_rx_buffer.full()) {
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
     */
    template <typename StreamType>
    etl::expected<size_t, ErrorCode> send(StreamType& stream, etl::span<const uint8_t> packet) {
        if (packet.empty()) return 0;

        const size_t payloadSize = packet.size();
        const size_t totalUnencodedSize = payloadSize + CRCType::Size;
        
        if (Codec::getEncodedBufferSize(totalUnencodedSize) > _work_buffer.size()) {
            return etl::unexpected(ErrorCode::BufferFull);
        }

        // [SIL-2] Use transient buffer to prepare frame.
        // We use the end of _work_buffer as a scratchpad if the packet is not in _work_buffer.
        // To be safe and simple, we copy to the beginning of _work_buffer.
        
        _crc_engine.reset();
        for (size_t i = 0; i < payloadSize; ++i) {
            _work_buffer[i] = packet[i];
            _crc_engine.add(packet[i]);
        }

        if constexpr (CRCType::Size > 0) {
            uint32_t cv = _crc_engine.value();
            for (size_t i = 0; i < CRCType::Size; ++i) {
                _work_buffer[payloadSize + i] = static_cast<uint8_t>((cv >> (i * 8)) & 0xFF);
            }
        }

        Codec codec;
        // In-place encoding is NOT guaranteed by all codecs. 
        // We assume Codec::encode is safe or we use a double buffer approach.
        // For COBS, it is safe if we encode from a separate input. 
        // Here we use a temp copy or just encode from _work_buffer to _work_buffer
        // IF the codec supports it. COBS encode is NOT in-place safe in our impl.
        
        // FIX: Use a temporary offset for encoding to avoid overlap if needed, 
        // but COBS encode always expands, so in-place is impossible.
        // Let's use the second half of _work_buffer.
        const size_t mid = _work_buffer.size() / 2;
        if (Codec::getEncodedBufferSize(totalUnencodedSize) > mid || totalUnencodedSize > mid) {
             return etl::unexpected(ErrorCode::BufferFull);
        }

        auto result = codec.encode(etl::span<const uint8_t>(_work_buffer.data(), totalUnencodedSize), 
                                   _work_buffer.subspan(mid));

        if (result.has_value()) {
            size_t encodedSize = result.value();
            for (size_t i = 0; i < encodedSize; ++i) {
                stream.write(_work_buffer[mid + i]);
                if ((i % 32) == 0) _watchdog.feed();
            }
            stream.write(Codec::Marker);
            return encodedSize + 1;
        } else {
            return etl::unexpected(result.error());
        }
    }

private:
    void processFrame(size_t size) {
        Codec codec;
        // COBS decode is in-place safe in our implementation.
        auto result = codec.decode(etl::span<const uint8_t>(_work_buffer.data(), size), 
                                   _work_buffer);

        if (!result.has_value()) {
            handleError(result.error());
            return;
        }

        size_t decodedSize = result.value();
        _watchdog.feed();

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
    }

    void handleError(ErrorCode error) {
        if (_onError) _onError(error);
    }

    etl::circular_buffer_ext<uint8_t> _rx_buffer;
    etl::span<uint8_t> _work_buffer;

    LockPolicy _lock;
    WatchdogPolicy _watchdog;

    PacketHandler _onPacket;
    ErrorHandler _onError;
    CRCType _crc_engine;
};

} // namespace PacketSerial2
