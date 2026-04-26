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
        
        auto in_ptr = input.data();
        auto in_end = in_ptr + input.size();
        auto out_ptr = output.data();
        auto out_end = out_ptr + output.size();
        
        while (in_ptr < in_end) {
            uint8_t code = *in_ptr++;
            uint8_t num_literals = code - 1;
            
            if (in_ptr + num_literals > in_end) return etl::unexpected(ErrorCode::MalformedFrame);
            
            if (out_ptr + num_literals > out_end) return etl::unexpected(ErrorCode::BufferFull);
            
            for (uint8_t i = 0; i < num_literals; ++i) {
                *out_ptr++ = *in_ptr++;
            }
            
            if (code < 0xFF && in_ptr < in_end) {
                if (out_ptr == out_end) return etl::unexpected(ErrorCode::BufferFull);
                *out_ptr++ = Marker;
            }
        }
        return static_cast<size_t>(out_ptr - output.data());
    }

    static constexpr size_t getEncodedBufferSize_impl(size_t unencodedBufferSize) {
        return unencodedBufferSize + (unencodedBufferSize / 254) + 1;
    }
};

} // namespace PacketSerial2
