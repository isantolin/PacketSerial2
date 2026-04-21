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
    using value_type = uint8_t;
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

    static constexpr size_t CRCSize = etl::is_same<CRCType, NoCRC>::value ? 0 : sizeof(typename CRCType::value_type);

    PacketSerial(etl::span<uint8_t> rx_storage, 
                 etl::span<uint8_t> work_buffer,
                 LockPolicy lock = LockPolicy(),
                 WatchdogPolicy watchdog = WatchdogPolicy()) 
        : _rx_buffer(rx_storage.data(), rx_storage.size()), 
          _work_buffer(work_buffer),
          _lock(lock),
          _watchdog(watchdog) {}

    void setPacketHandler(PacketHandler handler) { this->_onPacket = handler; }
    void setErrorHandler(ErrorHandler handler) { this->_onError = handler; }

    /**
     * @brief Process incoming data from a stream.
     */
    template <typename StreamType>
    void update(StreamType& stream) {
        size_t available = static_cast<size_t>(stream.available());
        if (available == 0) return;

        // Functional-style loop using a range-based approach or local loop proxy.
        // We simulate the iteration using for_each on a count.
        etl::for_each(etl::make_index_iterator(size_t(0)), etl::make_index_iterator(available), [this, &stream](size_t) {
            uint8_t data = static_cast<uint8_t>(stream.read());

            if (data == Codec::Marker) {
                this->_ps_internal_process_marker();
            } else {
                this->_ps_internal_push_rx(data);
            }
        });
    }

    /**
     * @brief Encode and send a packet of data.
     */
    template <typename StreamType>
    etl::expected<size_t, ErrorCode> send(StreamType& stream, etl::span<const uint8_t> packet) {
        if (packet.empty()) return 0;

        const size_t payloadSize = packet.size();
        const size_t totalUnencodedSize = payloadSize + CRCSize;
        const size_t requiredEncodedSize = Codec::getEncodedBufferSize(totalUnencodedSize);
        
        if (requiredEncodedSize > _work_buffer.size()) {
            return etl::unexpected(ErrorCode::BufferFull);
        }

        // Check if the packet points inside our work buffer to avoid collisions
        // Using etl::span properties instead of raw pointer math
        bool overlaps = (packet.data() >= _work_buffer.data() && 
                         packet.data() < _work_buffer.data() + _work_buffer.size());

        if (overlaps || CRCSize > 0) {
            if (totalUnencodedSize + requiredEncodedSize > _work_buffer.size()) {
                return etl::unexpected(ErrorCode::BufferFull);
            }

            // 1. Copy payload (using etl::copy which is optimized)
            if (packet.data() != _work_buffer.data()) {
                etl::copy(packet.begin(), packet.end(), _work_buffer.begin());
            }

            // 2. Add CRC if needed
            if constexpr (CRCSize > 0) {
                _crc_engine.reset();
                etl::for_each(_work_buffer.begin(), _work_buffer.begin() + payloadSize, [this](uint8_t b) {
                    _crc_engine.add(b);
                });
                
                typename CRCType::value_type crcValue = this->_ps_internal_get_crc();
                
                auto crc_span = _work_buffer.subspan(payloadSize, CRCSize);
                uint8_t shift = 0;
                etl::for_each(crc_span.begin(), crc_span.end(), [&crcValue, &shift](uint8_t& b) {
                    b = static_cast<uint8_t>((crcValue >> (shift * 8)) & 0xFF);
                    shift++;
                });
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

            etl::for_each(etl::make_index_iterator(size_t(0)), etl::make_index_iterator(num_chunks), [this, &stream, total_encoded, encode_offset](size_t chunk_idx) {
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

            etl::for_each(etl::make_index_iterator(size_t(0)), etl::make_index_iterator(num_chunks), [this, &stream, total_encoded](size_t chunk_idx) {
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
            this->processFrame(bytes_to_process);
        }
    }

    void _ps_internal_push_rx(uint8_t data) {
        _lock.lock();
        if (_rx_buffer.full()) {
            _rx_buffer.clear(); 
            _lock.unlock();
            this->handleError(ErrorCode::BufferOverflow);
        } else {
            _rx_buffer.push(data);
            _lock.unlock();
        }
    }
    typename CRCType::value_type _ps_internal_get_crc() const {
        return _crc_engine.value();
    }

    void processFrame(size_t size) {
        Codec codec;
        // COBS decode is in-place safe in our implementation.
        auto result = codec.decode(etl::span<const uint8_t>(this->_work_buffer.data(), size), 
                                   this->_work_buffer);

        if (!result.has_value()) {
            this->handleError(result.error());
            return;
        }

        size_t decodedSize = result.value();
        this->_watchdog.feed();

        if constexpr (CRCSize > 0) {
            if (decodedSize < CRCSize) {
                this->handleError(ErrorCode::IncompletePacket);
                return;
            }
            
            size_t payloadSize = decodedSize - CRCSize;
            this->_crc_engine.reset();
            
            etl::for_each(this->_work_buffer.begin(), this->_work_buffer.begin() + payloadSize, [this](uint8_t byte) {
                this->_crc_engine.add(byte);
            });
            
            uint32_t receivedCrc = 0;
            uint8_t shift_rx = 0;
            etl::for_each(this->_work_buffer.begin() + payloadSize, this->_work_buffer.begin() + payloadSize + CRCSize, [&receivedCrc, &shift_rx](uint8_t b) {
                receivedCrc |= (static_cast<uint32_t>(b) << (shift_rx * 8));
                shift_rx++;
            });
            
            if (this->_crc_engine.value() != receivedCrc) {
                this->handleError(ErrorCode::InvalidChecksum);
                return;
            }
            
            if (this->_onPacket) this->_onPacket(etl::span<const uint8_t>(this->_work_buffer.data(), payloadSize));
        } else {
            if (this->_onPacket) this->_onPacket(etl::span<const uint8_t>(this->_work_buffer.data(), decodedSize));
        }
    }

    void handleError(ErrorCode error) {
        if (this->_onError) this->_onError(error);
    }

    PacketHandler _onPacket;
    ErrorHandler _onError;

    etl::circular_buffer_ext<uint8_t> _rx_buffer;
    etl::span<uint8_t> _work_buffer;

    LockPolicy _lock;
    WatchdogPolicy _watchdog;

    CRCType _crc_engine;
};

} // namespace PacketSerial2
