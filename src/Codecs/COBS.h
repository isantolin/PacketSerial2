#include <etl/expected.h>
#include <etl/span.h>
#include <stdint.h>
#include <stddef.h>
#include <etl/algorithm.h>
#include "../ICodec.h"

namespace PacketSerial2 {

class COBS : public ICodec<COBS> {
public:
    static constexpr uint8_t Marker = 0x00;

    etl::expected<size_t, ErrorCode> encode_impl(etl::span<const uint8_t> input, etl::span<uint8_t> output) {
        const size_t encoded_size = getEncodedBufferSize_impl(input.size());
        if (output.size() < encoded_size) return etl::unexpected(ErrorCode::BufferFull);

        auto out_it = output.begin();
        auto code_it = out_it++;
        uint8_t code = 1;

        for (uint8_t byte : input) {
            if (byte == Marker) {
                *code_it = code;
                code = 1;
                code_it = out_it++;
            } else {
                *out_it++ = byte;
                if (++code == 0xFF) {
                    *code_it = code;
                    code = 1;
                    code_it = out_it++;
                }
            }
        }
        *code_it = code;
        return etl::distance(output.begin(), out_it);
    }

    etl::expected<size_t, ErrorCode> decode_impl(etl::span<const uint8_t> input, etl::span<uint8_t> output) {
        if (input.empty()) return 0;
        
        auto in_it = input.begin();
        auto out_it = output.begin();
        
        while (in_it < input.end()) {
            uint8_t code = *in_it++;
            size_t num_literals = code - 1;
            
            if (in_it + num_literals > input.end()) return etl::unexpected(ErrorCode::MalformedFrame);
            
            if (num_literals > 0) {
                if (etl::distance(out_it, output.end()) < static_cast<ptrdiff_t>(num_literals)) 
                    return etl::unexpected(ErrorCode::BufferFull);
                
                // Use memmove if they overlap, but copy_n is safe if out <= in
                etl::copy_n(in_it, num_literals, out_it);
                in_it += num_literals;
                out_it += num_literals;
            }
            
            if (code < 0xFF && in_it < input.end()) {
                if (out_it == output.end()) return etl::unexpected(ErrorCode::BufferFull);
                *out_it++ = Marker;
            }
        }
        return etl::distance(output.begin(), out_it);
    }

    static constexpr size_t getEncodedBufferSize_impl(size_t unencodedBufferSize) {
        return unencodedBufferSize + (unencodedBufferSize / 254) + 1;
    }
};

} // namespace PacketSerial2
