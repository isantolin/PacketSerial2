#pragma once

#include <stdint.h>
#include <stddef.h>
#include <etl/delegate.h>
#include <etl/circular_buffer.h>
#include <etl/span.h>
#include <etl/expected.h>
#include <etl/algorithm.h>
#include <etl/type_traits.h>
#include <etl/bitset.h>
#include <etl/vector.h>

#include "ErrorCode.h"
#include "ICodec.h"
#include "Safety.h"

namespace PacketSerial2 {

/**
 * @brief Internal state flags for PacketSerial.
 */
enum class StateFlag : uint8_t {
    BufferOverflow = 0,
    InvalidChecksum = 1,
    InSync = 2,
    StateError = 3
};

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
 * @brief Common base class to reduce template bloat.
 */
class PacketSerialBase {
public:
    using PacketHandler = etl::delegate<void(etl::span<const uint8_t>)>;
    using ErrorHandler = etl::delegate<void(ErrorCode)>;

    /**
     * @brief Set a single packet handler. This will clear any previously registered handlers.
     * @param handler The delegate to be called when a packet is received.
     */
    void setPacketHandler(PacketHandler handler) { 
        _onPacket.clear();
        if (!_onPacket.full()) {
            _onPacket.push_back(handler); 
        }
    }

    /**
     * @brief Add a packet handler to the list of subscribers.
     * @param handler The delegate to be called when a packet is received.
     */
    void addPacketHandler(PacketHandler handler) { 
        if (!_onPacket.full()) {
            _onPacket.push_back(handler); 
        }
    }

    /**
     * @brief Set the error handler.
     * @param handler The delegate to be called when an error occurs.
     */
    void setErrorHandler(ErrorHandler handler) { _onError = handler; }

protected:
    void handleError(ErrorCode error) {
        if (_onError) _onError(error);
    }

    void dispatchPacket(etl::span<const uint8_t> packet) {
        etl::for_each(_onPacket.begin(), _onPacket.end(), [&packet](PacketHandler& handler) {
            handler(packet);
        });
    }

    etl::vector<PacketHandler, 2> _onPacket;
    ErrorHandler _onError;
};

/**
 * @brief Industrial-grade PacketSerial implementation (v2.2).
 */
template <
    typename Codec, 
    typename CRCType = NoCRC, 
    typename LockPolicy = NoLock,
    typename WatchdogPolicy = NoWatchdog
>
class PacketSerial : public PacketSerialBase {
    static_assert(etl::is_base_of<ICodec<Codec>, Codec>::value, "Codec must inherit from ICodec<Codec>");

public:
    static constexpr size_t CRCSize = etl::is_same<CRCType, NoCRC>::value ? 0 : sizeof(typename CRCType::value_type);

    PacketSerial(etl::span<uint8_t> rx_storage, 
                 etl::span<uint8_t> work_buffer,
                 LockPolicy lock = LockPolicy(),
                 WatchdogPolicy watchdog = WatchdogPolicy()) 
        : _rx_buffer(rx_storage.data(), rx_storage.size()), 
          _work_buffer(work_buffer),
          _lock(lock),
          _watchdog(watchdog) {
              _state.reset();
          }

    /**
     * @brief Process incoming data from a stream.
     */
    template <typename StreamType>
    void update(StreamType& stream) {
        size_t available = static_cast<size_t>(stream.available());
        if (available == 0) return;

        // Limit read to available buffer space
        size_t free_space = _rx_buffer.capacity() - _rx_buffer.size();
        size_t to_read = (available < free_space) ? available : free_space;

        for(size_t i = 0; i < to_read; ++i) {
            int c = stream.read();
            if (c < 0) break;
            uint8_t data = static_cast<uint8_t>(c);
            
            _lock.lock();
            _rx_buffer.push(data);
            _lock.unlock();
            
            if (data == Codec::Marker) {
                this->_ps_internal_process_buffer();
                // After processing a frame, more space might be available
                free_space = _rx_buffer.capacity() - _rx_buffer.size();
                // We don't update to_read here, we'll just read up to the original limit
                // and the next update() call will handle the rest.
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
        const size_t totalUnencodedSize = payloadSize + CRCSize;
        const size_t requiredEncodedSize = Codec::getEncodedBufferSize(totalUnencodedSize);
        
        if (requiredEncodedSize > _work_buffer.size()) {
            return etl::unexpected(ErrorCode::BufferFull);
        }

        bool overlaps = (packet.data() >= _work_buffer.data() && 
                         packet.data() < _work_buffer.data() + _work_buffer.size());

        if (overlaps || CRCSize > 0) {
            if (totalUnencodedSize + requiredEncodedSize > _work_buffer.size()) {
                return etl::unexpected(ErrorCode::BufferFull);
            }

            if (packet.data() != _work_buffer.data()) {
                etl::copy(packet.begin(), packet.end(), _work_buffer.begin());
            }

            if constexpr (CRCSize > 0) {
                _crc_engine.reset();
                etl::for_each(_work_buffer.begin(), _work_buffer.begin() + payloadSize, [this](uint8_t b) {
                    this->_crc_engine.add(b);
                });
                
                typename CRCType::value_type crcValue = this->_crc_engine.value();
                auto crc_span = _work_buffer.subspan(payloadSize, CRCSize);
                uint8_t shift = 0;
                etl::for_each(crc_span.begin(), crc_span.end(), [crcValue, &shift](uint8_t& b) {
                    b = static_cast<uint8_t>((crcValue >> (shift * 8)) & 0xFF);
                    shift++;
                });
            }

            const size_t encode_offset = _work_buffer.size() - requiredEncodedSize;
            Codec codec;
            auto result = codec.encode(etl::span<const uint8_t>(_work_buffer.data(), totalUnencodedSize), 
                                       _work_buffer.subspan(encode_offset));
            
            if (!result.has_value()) return result;

            const size_t total_encoded = result.value();
            this->_ps_internal_block_send(stream, _work_buffer.subspan(encode_offset, total_encoded));
            
            stream.write(Codec::Marker);
            return total_encoded + 1;
        } else {
            Codec codec;
            auto result = codec.encode(packet, _work_buffer);
            if (!result.has_value()) return result;

            const size_t total_encoded = result.value();
            this->_ps_internal_block_send(stream, _work_buffer.subspan(0, total_encoded));

            stream.write(Codec::Marker);
            return total_encoded + 1;
        }
    }

private:
    void _ps_internal_process_buffer() {
        while (true) {
            auto it = etl::find(_rx_buffer.begin(), _rx_buffer.end(), Codec::Marker);
            if (it == _rx_buffer.end()) break;

            size_t frame_size = etl::distance(_rx_buffer.begin(), it);
            
            if (frame_size > 0) {
                if (frame_size <= _work_buffer.size()) {
                    etl::copy_n(_rx_buffer.begin(), frame_size, _work_buffer.begin());
                    this->processFrame(frame_size);
                } else {
                    this->handleError(ErrorCode::BufferFull);
                }
            }
            
            _lock.lock();
            for(size_t i = 0; i <= frame_size; ++i) _rx_buffer.pop();
            _lock.unlock();
        }
    }

    template <typename StreamType>
    void _ps_internal_block_send(StreamType& stream, etl::span<const uint8_t> data) {
        size_t total = data.size();
        size_t offset = 0;
        
        etl::for_each(data.begin(), data.end(), [this, &stream, &offset, total, &data](const uint8_t& /*byte*/) {
            if ((offset % 32) == 0) {
                size_t chunk = (total - offset > 32) ? 32 : (total - offset);
                stream.write(data.data() + offset, chunk);
                this->_watchdog.feed();
            }
            offset++;
        });
    }

    etl::expected<void, ErrorCode> processFrame(size_t size) {
        Codec codec;
        auto result = codec.decode(etl::span<const uint8_t>(this->_work_buffer.data(), size), 
                                   this->_work_buffer);

        if (!result.has_value()) {
            this->handleError(result.error());
            return etl::unexpected(result.error());
        }

        size_t decodedSize = result.value();
        this->_watchdog.feed();
        size_t payloadSize = decodedSize;

        if constexpr (CRCSize > 0) {
            if (decodedSize < CRCSize) {
                this->handleError(ErrorCode::IncompletePacket);
                return etl::unexpected(ErrorCode::IncompletePacket);
            }
            
            payloadSize = decodedSize - CRCSize;
            this->_crc_engine.reset();
            etl::for_each(this->_work_buffer.begin(), this->_work_buffer.begin() + payloadSize, [this](uint8_t byte) {
                this->_crc_engine.add(byte);
            });
            
            uint32_t receivedCrc = 0;
            uint8_t shift_rx = 0;
            auto crc_span = _work_buffer.subspan(payloadSize, CRCSize);
            etl::for_each(crc_span.begin(), crc_span.end(), [&receivedCrc, &shift_rx](uint8_t b) {
                receivedCrc |= (static_cast<uint32_t>(b) << (shift_rx * 8));
                shift_rx++;
            });
            
            if (this->_crc_engine.value() != receivedCrc) {
                _state.set(static_cast<size_t>(StateFlag::InvalidChecksum));
                this->handleError(ErrorCode::InvalidChecksum);
                return etl::unexpected(ErrorCode::InvalidChecksum);
            }
        }
        
        this->dispatchPacket(etl::span<const uint8_t>(this->_work_buffer.data(), payloadSize));
        return {};
    }

    etl::circular_buffer_ext<uint8_t> _rx_buffer;
    etl::span<uint8_t> _work_buffer;

    LockPolicy _lock;
    WatchdogPolicy _watchdog;

    CRCType _crc_engine;
    etl::bitset<8> _state;
};

} // namespace PacketSerial2
