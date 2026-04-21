#pragma once

#include <stdint.h>
#include <stddef.h>
#include <etl/delegate.h>
#include <etl/circular_buffer.h>
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
        : _rx_buffer(rx_storage.begin(), rx_storage.end()), 
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
        // We use a functional-style recursion or limited loop to avoid raw 'while' if possible,
        // but for a stream, we can use a jump-based approach or a predefined etl utility.
        // Since ETL doesn't have a StreamIterator, we'll use a local lambda with etl::for_each over a count.
        
        size_t available = static_cast<size_t>(stream.available());
        if (available == 0) return;

        etl::for_each(etl::make_index_iterator(0), etl::make_index_iterator(available), [&](size_t) {
            uint8_t data = static_cast<uint8_t>(stream.read());

            if (data == Codec::Marker) {
                _ps_internal_process_marker();
            } else {
                _ps_internal_push_rx(data);
            }
        });
    }

private:
    void _ps_internal_process_marker() {
        size_t bytes_to_process = 0;
        _lock.lock();
        bytes_to_process = _rx_buffer.size();
        if (bytes_to_process > 0) {
            etl::copy(_rx_buffer.begin(), _rx_buffer.end(), _work_buffer.begin());
            _rx_buffer.clear();
        }
        _lock.unlock();

        if (bytes_to_process > 0) {
            processFrame(bytes_to_process);
        }
    }

    void _ps_internal_push_rx(uint8_t data) {
        _lock.lock();
        if (_rx_buffer.full()) {
            _rx_buffer.clear(); 
            _lock.unlock();
            handleError(ErrorCode::BufferOverflow);
        } else {
            _rx_buffer.push(data);
            _lock.unlock();
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
        const size_t requiredEncodedSize = Codec::getEncodedBufferSize(totalUnencodedSize);
        
        if (requiredEncodedSize > _work_buffer.size()) {
            return etl::unexpected(ErrorCode::BufferFull);
        }

        // Check if the packet points inside our work buffer to avoid collisions
        // Using etl::span properties instead of raw pointer math
        bool overlaps = (packet.data() >= _work_buffer.data() && 
                         packet.data() < _work_buffer.data() + _work_buffer.size());

        if (overlaps || CRCType::Size > 0) {
            if (totalUnencodedSize + requiredEncodedSize > _work_buffer.size()) {
                return etl::unexpected(ErrorCode::BufferFull);
            }

            // 1. Copy payload (using etl::copy which is optimized)
            if (packet.data() != _work_buffer.data()) {
                etl::copy(packet.begin(), packet.end(), _work_buffer.begin());
            }

            // 2. Add CRC if needed
            if constexpr (CRCType::Size > 0) {
                _crc_engine.reset();
                etl::for_each(_work_buffer.begin(), _work_buffer.begin() + payloadSize, [this](uint8_t b) {
                    _crc_engine.add(b);
                });
                
                typename CRCType::value_type crcValue = _ps_internal_get_crc();
                
                auto crc_span = _work_buffer.subspan(payloadSize, CRCType::Size);
                size_t shift = 0;
                etl::for_each(crc_span.begin(), crc_span.end(), [&](uint8_t& b) {
                    b = static_cast<uint8_t>((crcValue >> (shift++ * 8)) & 0xFF);
                });
            }
...
    typename CRCType::value_type _ps_internal_get_crc() const {
        return _crc_engine.value();
    }

            // 3. Encode from start of work_buffer to a safe offset
            const size_t encode_offset = _work_buffer.size() - requiredEncodedSize;
            Codec codec;
            auto result = codec.encode(etl::span<const uint8_t>(_work_buffer.data(), totalUnencodedSize), 
                                       _work_buffer.subspan(encode_offset));
            
            if (!result.has_value()) return result;

            // 4. Send in blocks
            const size_t total_encoded = result.value();
            const size_t num_chunks = (total_encoded + 31) / 32;
            
            etl::for_each(etl::make_index_iterator(0), etl::make_index_iterator(num_chunks), [&](size_t chunk_idx) {
                const size_t offset = chunk_idx * 32;
                const size_t chunk_size = (total_encoded - offset > 32) ? 32 : (total_encoded - offset);
                stream.write(_work_buffer.data() + encode_offset + offset, chunk_size);
                _watchdog.feed();
            });
            
            stream.write(Codec::Marker);
            return total_encoded + 1;
        } else {
            // Optimization for no CRC and no overlap
            Codec codec;
            auto result = codec.encode(packet, _work_buffer);
            if (!result.has_value()) return result;

            const size_t total_encoded = result.value();
            const size_t num_chunks = (total_encoded + 31) / 32;

            etl::for_each(etl::make_index_iterator(0), etl::make_index_iterator(num_chunks), [&](size_t chunk_idx) {
                const size_t offset = chunk_idx * 32;
                const size_t chunk_size = (total_encoded - offset > 32) ? 32 : (total_encoded - offset);
                stream.write(_work_buffer.data() + offset, chunk_size);
                _watchdog.feed();
            });

            stream.write(Codec::Marker);
            return total_encoded + 1;
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
            
            etl::for_each(_work_buffer.begin(), _work_buffer.begin() + payloadSize, [this](uint8_t byte) {
                _crc_engine.add(byte);
            });
            
            uint32_t receivedCrc = 0;
            uint8_t shift_rx = 0;
            etl::for_each(_work_buffer.begin() + payloadSize, _work_buffer.begin() + payloadSize + CRCType::Size, [&receivedCrc, &shift_rx](uint8_t b) {
                receivedCrc |= (static_cast<uint32_t>(b) << (shift_rx * 8));
                shift_rx++;
            });
            
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

    CRCType _crc_engine;
};

} // namespace PacketSerial2
