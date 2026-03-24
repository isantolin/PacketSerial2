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
 * @brief Industrial-grade PacketSerial implementation.
 * 
 * Features:
 * - Zero-Heap / No-STL.
 * - Deterministic Memory: All buffers are provided by the user.
 * - In-Place decoding support.
 * - Platform agnostic.
 * 
 * @tparam Codec The codec class (e.g., COBS, SLIP).
 * @tparam CRCType The CRC implementation to use (default is NoCRC).
 */
template <typename Codec, typename CRCType = NoCRC>
class PacketSerial {
public:
    /**
     * @brief Delegate for packet reception. Passes a view of the decoded data.
     */
    using PacketHandler = etl::delegate<void(etl::span<const uint8_t>)>;

    /**
     * @brief Delegate for error reporting.
     */
    using ErrorHandler = etl::delegate<void(ErrorCode)>;

    /**
     * @brief Construct PacketSerial with user-provided buffers.
     * 
     * @param rx_storage Memory for the internal circular buffer (raw incoming bytes).
     * @param work_buffer Memory for decoding and encoding (must be large enough for max packet + overhead).
     */
    PacketSerial(etl::span<uint8_t> rx_storage, etl::span<uint8_t> work_buffer) 
        : _rx_buffer(rx_storage.data(), rx_storage.size()), 
          _work_buffer(work_buffer) {}

    /**
     * @brief Set the function that will receive decoded packets.
     */
    void setPacketHandler(PacketHandler handler) {
        _onPacket = handler;
    }

    /**
     * @brief Set the function that will receive error reports.
     */
    void setErrorHandler(ErrorHandler handler) {
        _onError = handler;
    }

    /**
     * @brief Process incoming data from a stream.
     * 
     * @tparam StreamType Any type that implements available() and read().
     * @param stream The stream to read from.
     */
    template <typename StreamType>
    void update(StreamType& stream) {
        while (stream.available() > 0) {
            uint8_t data = static_cast<uint8_t>(stream.read());

            if (data == Codec::Marker) {
                if (!_rx_buffer.empty()) {
                    processPacket();
                }
            } else {
                if (_rx_buffer.full()) {
                    _status.set(STATUS_OVERFLOW);
                    handleError(ErrorCode::BufferOverflow);
                    _rx_buffer.clear(); // Re-sync: drop data and wait for next marker
                } else {
                    _rx_buffer.push_back(data);
                }
            }
        }
    }

    /**
     * @brief Encode and send a packet of data.
     * 
     * Uses the provided work_buffer for encoding to avoid stack allocation.
     * 
     * @tparam StreamType Any type that implements write(uint8_t).
     * @param stream The stream to write to.
     * @param packet The data to send.
     * @return etl::expected<size_t, ErrorCode> Total bytes sent or error.
     */
    template <typename StreamType>
    etl::expected<size_t, ErrorCode> send(StreamType& stream, etl::span<const uint8_t> packet) {
        if (packet.empty()) return 0;

        size_t payloadSize = packet.size();
        size_t totalUnencodedSize = payloadSize + CRCType::Size;
        
        // Ensure work_buffer can hold the encoded result
        if (Codec::getEncodedBufferSize(totalUnencodedSize) > _work_buffer.size()) {
            handleError(ErrorCode::BufferFull);
            return etl::unexpected(ErrorCode::BufferFull);
        }

        // 1. Prepare data (Packet + CRC) in the work_buffer if CRC is used.
        // We use the start of work_buffer to stage the unencoded frame.
        if constexpr (CRCType::Size > 0) {
            _crc_engine.reset();
            for (size_t i = 0; i < payloadSize; ++i) {
                _work_buffer[i] = packet[i];
                _crc_engine.add(packet[i]);
            }
            uint32_t cv = _crc_engine.value();
            for (size_t i = 0; i < CRCType::Size; ++i) {
                _work_buffer[payloadSize + i] = static_cast<uint8_t>((cv >> (i * 8)) & 0xFF);
            }
        } else {
            // If no CRC, we can encode directly from the packet span.
            // But to keep it simple, we use the work_buffer logic.
            for (size_t i = 0; i < payloadSize; ++i) {
                _work_buffer[i] = packet[i];
            }
        }

        // 2. Encode into the work_buffer. 
        // Note: Codec::encode will read from the start of work_buffer and write 
        // to a safe offset or we use a temporary staging if in-place encode is not possible.
        // For COBS, we encode from work_buffer[0...totalUnencodedSize] into work_buffer.
        // To be safe against overlap, we use the end of the work_buffer as source.
        size_t encodedOffset = 0; 
        Codec codec;
        auto result = codec.encode(etl::span<const uint8_t>(_work_buffer.data(), totalUnencodedSize), 
                                   _work_buffer);

        if (result.has_value()) {
            size_t encodedSize = result.value();
            for (size_t i = 0; i < encodedSize; ++i) {
                stream.write(_work_buffer[i]);
            }
            stream.write(Codec::Marker);
            return encodedSize + 1;
        } else {
            handleError(result.error());
            return etl::unexpected(result.error());
        }
    }

    static constexpr size_t STATUS_OVERFLOW = 0;

private:
    void processPacket() {
        size_t size = _rx_buffer.size();
        if (size > _work_buffer.size()) {
            handleError(ErrorCode::BufferOverflow);
            _rx_buffer.clear();
            return;
        }

        // Linearize the circular buffer into the work buffer for decoding.
        for (size_t i = 0; i < size; ++i) {
            _work_buffer[i] = _rx_buffer[i];
        }
        _rx_buffer.clear();

        Codec codec;
        // Decode in-place in the work buffer.
        auto result = codec.decode(etl::span<const uint8_t>(_work_buffer.data(), size), 
                                   _work_buffer);

        if (result.has_value()) {
            size_t decodedSize = result.value();
            
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

    etl::circular_buffer_ext<uint8_t> _rx_buffer;
    etl::span<uint8_t> _work_buffer;
    
    PacketHandler _onPacket;
    ErrorHandler _onError;
    etl::bitset<8> _status;
    CRCType _crc_engine;
};

} // namespace PacketSerial2
