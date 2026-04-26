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

template <typename Codec, 
          typename CRCType = PacketSerial2::NoCRC,
          typename LockPolicy = PacketSerial2::NoLock,
          typename WatchdogPolicy = PacketSerial2::NoWatchdog>
class PacketSerial {
public:
    using PacketHandler = etl::delegate<void(etl::span<const uint8_t>)>;
    using ErrorHandler = etl::delegate<void(ErrorCode)>;

    static constexpr size_t CRCSize = CRCType::ByteSize;

    PacketSerial(etl::span<uint8_t> rx_storage, etl::span<uint8_t> work_buffer,
                 LockPolicy lock = LockPolicy(), WatchdogPolicy watchdog = WatchdogPolicy())
        : _rx_buffer(rx_storage.data(), rx_storage.size()), 
          _work_buffer(work_buffer),
          _lock(lock),
          _watchdog(watchdog) {}

    void setPacketHandler(PacketHandler handler) { _onPacket = handler; }
    void setErrorHandler(ErrorHandler handler) { _onError = handler; }

    template <typename StreamType>
    void update(StreamType& stream) {
        int available = stream.available();
        if (available <= 0) return;

        // Limit read to available buffer space
        size_t free_space = _rx_buffer.capacity() - _rx_buffer.size();
        int to_read = (available < (int)free_space) ? available : (int)free_space;

        for (int i = 0; i < to_read; ++i) {
            int c = stream.read();
            if (c < 0) break;
            uint8_t data = static_cast<uint8_t>(c);

            if (data == Codec::Marker) {
                this->_ps_internal_process_marker();
                free_space = _rx_buffer.capacity() - _rx_buffer.size();
            } else {
                _lock.lock();
                _rx_buffer.push(data);
                _lock.unlock();
            }
        }
    }

    template <typename StreamType>
    etl::expected<size_t, ErrorCode> send(StreamType& stream, etl::span<const uint8_t> packet) {
        if (packet.empty()) return 0;
        const size_t payloadSize = packet.size();
        const size_t totalSize = payloadSize + CRCSize;
        const size_t encodedSizeLimit = Codec::getEncodedBufferSize(totalSize);

        if (encodedSizeLimit > _work_buffer.size()) return etl::unexpected(ErrorCode::BufferFull);

        etl::copy(packet.begin(), packet.end(), _work_buffer.begin());
        if constexpr (CRCSize > 0) {
            _crc_engine.reset();
            for (uint8_t b : packet) _crc_engine.add(b);
            uint32_t crc = _crc_engine.value();
            for (size_t i = 0; i < CRCSize; ++i) _work_buffer[payloadSize + i] = (crc >> (i * 8)) & 0xFF;
        }

        Codec codec;
        auto res = codec.encode(etl::span<const uint8_t>(_work_buffer.data(), totalSize), 
                                etl::span<uint8_t>(_work_buffer.data() + totalSize, _work_buffer.size() - totalSize));
        
        if (!res) return etl::unexpected(res.error());
        
        size_t encodedSize = res.value();
        uint8_t* encodedData = _work_buffer.data() + totalSize;
        
        for (size_t i = 0; i < encodedSize; ++i) stream.write(encodedData[i]);
        stream.write(Codec::Marker);
        return encodedSize + 1;
    }

private:
    void _ps_internal_process_marker() {
        size_t frame_size = _rx_buffer.size();
        if (frame_size == 0) return;

        etl::copy(_rx_buffer.begin(), _rx_buffer.end(), _work_buffer.begin());
        _rx_buffer.clear();

        Codec codec;
        auto res = codec.decode(etl::span<const uint8_t>(_work_buffer.data(), frame_size), _work_buffer);
        if (!res) {
            if (_onError) _onError(res.error());
            return;
        }

        size_t decodedSize = res.value();
        if constexpr (CRCSize > 0) {
            if (decodedSize < CRCSize) {
                if (_onError) _onError(ErrorCode::MalformedFrame);
                return;
            }
            size_t payloadSize = decodedSize - CRCSize;
            _crc_engine.reset();
            for (size_t i = 0; i < payloadSize; ++i) _crc_engine.add(_work_buffer[i]);
            uint32_t calc = _crc_engine.value();
            uint32_t recv = 0;
            for (size_t i = 0; i < CRCSize; ++i) recv |= (static_cast<uint32_t>(_work_buffer[payloadSize + i]) << (i * 8));
            if (calc != recv) {
                if (_onError) _onError(ErrorCode::InvalidChecksum);
                return;
            }
            if (_onPacket) _onPacket(etl::span<const uint8_t>(_work_buffer.data(), payloadSize));
        } else {
            if (_onPacket) _onPacket(etl::span<const uint8_t>(_work_buffer.data(), decodedSize));
        }
    }

    etl::circular_buffer_ext<uint8_t> _rx_buffer;
    etl::span<uint8_t> _work_buffer;
    PacketHandler _onPacket;
    ErrorHandler _onError;
    LockPolicy _lock;
    WatchdogPolicy _watchdog;
    CRCType _crc_engine;
};

} // namespace PacketSerial2
