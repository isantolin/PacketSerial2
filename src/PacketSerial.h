#pragma once

#include <stdint.h>
#include <stddef.h>
#include <etl/delegate.h>
#include <etl/delegate_observable.h>
#include <etl/circular_buffer.h>
#include <etl/span.h>
#include <etl/expected.h>
#include <etl/algorithm.h>
#include <etl/endianness.h>
#include "ErrorCode.h"
#include "ICodec.h"
#include "Safety.h"

namespace PacketSerial2 {

template <typename...>
using void_t = void;

template <typename T, typename = void>
struct CRCSizeHelper {
    static constexpr size_t value = sizeof(typename T::value_type);
};

template <typename T>
struct CRCSizeHelper<T, void_t<decltype(T::ByteSize)>> {
    static constexpr size_t value = T::ByteSize;
};

template <typename Codec, 
          typename CRCType = PacketSerial2::NoCRC,
          typename LockPolicy = PacketSerial2::NoLock,
          typename WatchdogPolicy = PacketSerial2::NoWatchdog,
          size_t MaxSubscribers = 4>
class PacketSerial {
    static_assert(MaxSubscribers > 0, "PacketSerial: MaxSubscribers must be greater than 0");
    static_assert(sizeof(Codec::Marker) == 1, "PacketSerial: Codec must define an 8-bit Marker");
public:
    using PacketHandler = etl::delegate<void(etl::span<const uint8_t>)>;
    using ErrorHandler = etl::delegate<void(ErrorCode)>;

    static constexpr size_t CRCSize = CRCSizeHelper<CRCType>::value;

    PacketSerial(etl::span<uint8_t> rx_storage, etl::span<uint8_t> work_buffer,
                 LockPolicy lock = LockPolicy(), WatchdogPolicy watchdog = WatchdogPolicy())
        : _rx_buffer(rx_storage.data(), rx_storage.size()), 
          _work_buffer(work_buffer),
          _lock(lock),
          _watchdog(watchdog) {}

    void addPacketHandler(PacketHandler handler) { _onPacketObservers.add_observer(handler); }
    void setPacketHandler(PacketHandler handler) {
        _onPacketObservers.clear_observers();
        _onPacketObservers.add_observer(handler);
    }
    void setErrorHandler(ErrorHandler handler) { _onError = handler; }

    template <typename StreamType>
    void update(StreamType& stream) {
        _watchdog.feed();
        int available = stream.available();
        if (available <= 0) return;

        // Limit read to available buffer space
        size_t free_space = _rx_buffer.capacity() - _rx_buffer.size();
        int to_read = (available < (int)free_space) ? available : (int)free_space;

        update_recursive(stream, 0, to_read);
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
            etl::for_each(packet.begin(), packet.end(), [this](uint8_t b) { _crc_engine.add(b); });
            uint32_t crc = _crc_engine.value();
            uint8_t* dest = _work_buffer.data() + payloadSize;
            etl::copy_n(reinterpret_cast<const uint8_t*>(&crc), CRCSize, dest);
            if constexpr (etl::endianness::value() == etl::endian::big) {
                etl::reverse(dest, dest + CRCSize);
            }
        }

        Codec codec;
        auto res = codec.encode(etl::span<const uint8_t>(_work_buffer.data(), totalSize), 
                                etl::span<uint8_t>(_work_buffer.data() + totalSize, _work_buffer.size() - totalSize));
        
        if (!res) return etl::unexpected(res.error());
        
        size_t encodedSize = res.value();
        uint8_t* encodedData = _work_buffer.data() + totalSize;
        
        stream.write(encodedData, encodedSize);
        stream.write(Codec::Marker);
        return encodedSize + 1;
    }

private:
    template <typename StreamType>
    void update_recursive(StreamType& stream, int bytes_read, int to_read) {
        if (bytes_read >= to_read) return;

        int c = stream.read();
        if (c < 0) return;
        uint8_t data = static_cast<uint8_t>(c);

        if (data == Codec::Marker) {
            this->_ps_internal_process_marker();
        } else {
            _lock.lock();
            _rx_buffer.push(data);
            _lock.unlock();
        }

        update_recursive(stream, bytes_read + 1, to_read);
    }

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
            etl::for_each(_work_buffer.begin(), _work_buffer.begin() + payloadSize, [this](uint8_t b) {
                _crc_engine.add(b);
            });
            uint32_t calc = _crc_engine.value();
            uint32_t recv = 0;
            uint8_t* src = _work_buffer.data() + payloadSize;
            etl::copy_n(src, CRCSize, reinterpret_cast<uint8_t*>(&recv));
            if constexpr (etl::endianness::value() == etl::endian::big) {
                etl::reverse(reinterpret_cast<uint8_t*>(&recv), reinterpret_cast<uint8_t*>(&recv) + CRCSize);
            }
            if (calc != recv) {
                if (_onError) _onError(ErrorCode::InvalidChecksum);
                return;
            }
            _onPacketObservers.notify_observers(etl::span<const uint8_t>(_work_buffer.data(), payloadSize));
        } else {
            _onPacketObservers.notify_observers(etl::span<const uint8_t>(_work_buffer.data(), decodedSize));
        }
    }

    etl::circular_buffer_ext<uint8_t> _rx_buffer;
    etl::span<uint8_t> _work_buffer;
    etl::delegate_observable<etl::span<const uint8_t>, MaxSubscribers> _onPacketObservers;
    ErrorHandler _onError;
    LockPolicy _lock;
    WatchdogPolicy _watchdog;
    CRCType _crc_engine;
};

} // namespace PacketSerial2
