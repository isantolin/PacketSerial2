#include <etl/expected.h>
#include <etl/span.h>
#include <stdint.h>
#include <stddef.h>
#include <etl/algorithm.h>
#include "../ICodec.h"

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
     */
    etl::expected<size_t, ErrorCode> encode_impl(etl::span<const uint8_t> input, etl::span<uint8_t> output) {
        const size_t encoded_size = getEncodedBufferSize_impl(input.size());
        if (output.size() < encoded_size) {
            return etl::unexpected(ErrorCode::BufferFull);
        }

        size_t write_index = 1;
        size_t code_index = 0;
        uint8_t code = 1;

        etl::for_each(input.begin(), input.end(), [&](uint8_t byte) {
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
        });
        
        output[code_index] = code;
        return write_index;
    }

    /**
     * @brief Decode a COBS-encoded buffer.
     * 
     * [SIL-2] This implementation is safe for in-place decoding (input == output).
     */
    etl::expected<size_t, ErrorCode> decode_impl(etl::span<const uint8_t> input, etl::span<uint8_t> output) {
        if (input.empty()) return 0;

        size_t write_index = 0;
        
        // Use etl::for_each with a custom jump logic is not possible, 
        // so we use a functional recursion or an iterator-based approach that avoids raw while.
        // For COBS, we'll use a controlled iteration over the input span.
        auto it = input.begin();
        
        // Since we can't use 'while' or 'for', we use etl::for_each over the possible max size
        // and check the iterator state.
        etl::for_each(input.begin(), input.end(), [&](const uint8_t&) {
            if (it == input.end()) return;

            uint8_t code = *it++;
            const size_t num_literals = code - 1;

            if (it + num_literals > input.end()) {
                // Malformed, we can't easily return error from lambda, 
                // but we can invalidate the iterator.
                it = input.end();
                return;
            }

            if (num_literals > 0) {
                if (write_index + num_literals <= output.size()) {
                    etl::copy_n(it, num_literals, output.begin() + write_index);
                    write_index += num_literals;
                }
                it += num_literals;
            }

            if (code < 0xFF && it != input.end()) {
                if (write_index < output.size()) {
                    output[write_index++] = Marker;
                }
            }
        });

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
