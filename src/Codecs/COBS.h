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

        auto out_it = output.begin();
        auto code_it = out_it; 
        out_it++;
        uint8_t code = 1;

        etl::for_each(input.begin(), input.end(), [this, &out_it, &code, &code_it](uint8_t byte) {
            if (byte == Marker) {
                *code_it = code;
                code = 1;
                code_it = out_it;
                out_it++;
            } else {
                *out_it = byte;
                out_it++;
                code++;
                if (code == 0xFF) {
                    *code_it = code;
                    code = 1;
                    code_it = out_it;
                    out_it++;
                }
            }
        });
        
        *code_it = code;
        return etl::distance(output.begin(), out_it);
    }

    /**
     * @brief Decode a COBS-encoded buffer.
     * 
     * [SIL-2] This implementation is safe for in-place decoding (input == output).
     */
    etl::expected<size_t, ErrorCode> decode_impl(etl::span<const uint8_t> input, etl::span<uint8_t> output) {
        if (input.empty()) return 0;

        auto out_it = output.begin();
        auto in_it = input.begin();
        ErrorCode error = ErrorCode::None;
        
        etl::for_each(input.begin(), input.end(), [this, &in_it, &out_it, &input, &output, &error](const uint8_t&) {
            if (in_it == input.end() || error != ErrorCode::None) return;

            uint8_t code = *in_it;
            in_it++;
            const size_t num_literals = code - 1;

            if (in_it + num_literals > input.end()) {
                error = ErrorCode::MalformedFrame;
                in_it = input.end();
                return;
            }

            if (num_literals > 0) {
                if (etl::distance(out_it, output.end()) < static_cast<ptrdiff_t>(num_literals)) {
                    error = ErrorCode::BufferFull;
                    in_it = input.end();
                    return;
                }
                etl::copy_n(in_it, num_literals, out_it);
                out_it += num_literals;
                in_it += num_literals;
            }

            if (code < 0xFF && in_it != input.end()) {
                if (out_it == output.end()) {
                    error = ErrorCode::BufferFull;
                    in_it = input.end();
                    return;
                }
                *out_it = Marker;
                out_it++;
            }
        });

        if (error != ErrorCode::None) {
            return etl::unexpected(error);
        }

        return etl::distance(output.begin(), out_it);
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
