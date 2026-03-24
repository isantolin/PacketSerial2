#pragma once

#include "../ICodec.h"
#include <etl/expected.h>
#include <etl/span.h>
#include <cstdint>
#include <cstddef>

namespace PacketSerial2 {

/**
 * @brief Consistent Overhead Byte Stuffing (COBS) Codec.
 * 
 * COBS is an encoding that removes all 0 bytes from arbitrary binary data.
 * This implementation is designed for in-place decoding safety and SIL-2 robustness.
 */
class COBS : public ICodec<COBS> {
public:
    static constexpr uint8_t Marker = 0x00;

    /**
     * @brief Encode a byte buffer with COBS.
     * 
     * @param input The unencoded input buffer.
     * @param output The buffer for the encoded bytes.
     * @return etl::expected<size_t, ErrorCode> Number of bytes written or error.
     */
    etl::expected<size_t, ErrorCode> encode_impl(etl::span<const uint8_t> input, etl::span<uint8_t> output) {
        const size_t encoded_size = getEncodedBufferSize_impl(input.size());
        if (output.size() < encoded_size) {
            return etl::unexpected(ErrorCode::BufferFull);
        }

        size_t write_index = 1;
        size_t code_index = 0;
        uint8_t code = 1;

        for (size_t i = 0; i < input.size(); ++i) {
            uint8_t byte = input[i];
            if (byte == Marker) {
                output[code_index] = code;
                code = 1;
                code_index = write_index++;
            } else {
                output[write_index++] = byte;
                code++;
                if (code == 0xFF) {
                    output[code_index] = code;
                    code = 1;
                    code_index = write_index++;
                }
            }
        }
        output[code_index] = code;
        return write_index;
    }

    /**
     * @brief Decode a COBS-encoded buffer.
     * 
     * [SIL-2] This implementation is safe for in-place decoding (input == output).
     * 
     * @param input The encoded input buffer.
     * @param output The target buffer for the decoded bytes.
     * @return etl::expected<size_t, ErrorCode> Number of bytes written or error.
     */
    etl::expected<size_t, ErrorCode> decode_impl(etl::span<const uint8_t> input, etl::span<uint8_t> output) {
        if (input.empty()) return 0;

        size_t read_index = 0;
        size_t write_index = 0;

        while (read_index < input.size()) {
            uint8_t code = input[read_index++];
            
            // Check for malformed frame: code points beyond buffer
            if (read_index + code - 1 > input.size()) {
                return etl::unexpected(ErrorCode::MalformedFrame);
            }

            // Copy code - 1 bytes of literal data
            for (uint8_t i = 1; i < code; ++i) {
                if (write_index >= output.size()) return etl::unexpected(ErrorCode::BufferOverflow);
                output[write_index++] = input[read_index++];
            }

            // If code is < 0xFF, it represents a block ending in 0.
            // But if it's the very last block in the frame, we don't add the trailing 0
            // because COBS framing (using 0x00 as delimiter) doesn't encode the final 0.
            // Wait, standard COBS: if the block is not 0xFF, we append a 0 unless it's the end of input.
            if (code < 0xFF && read_index < input.size()) {
                if (write_index >= output.size()) return etl::unexpected(ErrorCode::BufferOverflow);
                output[write_index++] = Marker;
            }
        }

        return write_index;
    }

    /**
     * @brief Get the maximum encoded buffer size for an unencoded buffer size.
     */
    static constexpr size_t getEncodedBufferSize_impl(size_t unencodedBufferSize) {
        // COBS adds 1 byte for every 254 bytes, plus 1 for the first block.
        return unencodedBufferSize + (unencodedBufferSize / 254) + 1;
    }
};

} // namespace PacketSerial2
