#pragma once

#include <etl/expected.h>
#include <etl/span.h>
#include <stdint.h>
#include <stddef.h>
#include <etl/algorithm.h>
#include "../ICodec.h"

namespace PacketSerial2 {

class COBSR : public ICodec<COBSR> {
public:
    static constexpr uint8_t Marker = 0x00;

    static constexpr size_t getEncodedBufferSize_impl(size_t unencodedBufferSize) {
        return unencodedBufferSize + (unencodedBufferSize / 254) + 1;
    }

    etl::expected<size_t, ErrorCode> encode_impl(etl::span<const uint8_t> input, etl::span<uint8_t> output) {
        const size_t encoded_size_limit = getEncodedBufferSize_impl(input.size());
        if (output.size() < encoded_size_limit) return etl::unexpected(ErrorCode::BufferFull);

        auto out_ptr = output.data();
        auto code_ptr = out_ptr++;
        uint8_t code = 1;

        size_t i = 0;
        const size_t input_size = input.size();
        size_t result_size = 0;
        bool done = false;

        etl::for_each(input.begin(), input.end(), [&](uint8_t byte) {
            if (done) return;
            bool is_last = (i == input_size - 1);

            if (byte == Marker) {
                *code_ptr = code;
                code = 1;
                code_ptr = out_ptr++;
            } else {
                if (is_last) {
                    // COBS/R optimization for final block
                    if (byte >= code + 1) {
                        *code_ptr = byte;
                        result_size = static_cast<size_t>(out_ptr - output.data());
                        done = true;
                        return;
                    }
                }

                *out_ptr++ = byte;
                if (++code == 0xFF) {
                    *code_ptr = code;
                    code = 1;
                    code_ptr = out_ptr++;
                }
            }
            ++i;
        });

        if (done) return result_size;
        *code_ptr = code;
        return static_cast<size_t>(out_ptr - output.data());
    }

    etl::expected<size_t, ErrorCode> decode_impl(etl::span<const uint8_t> input, etl::span<uint8_t> output) {
        if (input.empty()) return 0;
        return decode_recursive(input.data(), input.data() + input.size(), output.data(), output.data() + output.size(), output.data());
    }

private:
    etl::expected<size_t, ErrorCode> decode_recursive(const uint8_t* in_ptr, const uint8_t* in_end, uint8_t* out_ptr, uint8_t* out_end, uint8_t* out_start) {
        if (in_ptr >= in_end) {
            return static_cast<size_t>(out_ptr - out_start);
        }

        uint8_t code = *in_ptr++;
        size_t remaining = static_cast<size_t>(in_end - in_ptr);

        if (code == 0) return etl::unexpected(ErrorCode::MalformedFrame);

        if (static_cast<size_t>(code - 1) > remaining) {
            // COBS/R optimized final block
            uint8_t num_literals = static_cast<uint8_t>(remaining);
            if (out_ptr + num_literals + 1 > out_end) return etl::unexpected(ErrorCode::BufferFull);

            out_ptr = etl::copy_n(in_ptr, num_literals, out_ptr);
            in_ptr += num_literals;
            *out_ptr++ = code;
            return static_cast<size_t>(out_ptr - out_start);
        } else {
            uint8_t num_literals = code - 1;
            if (in_ptr + num_literals > in_end) return etl::unexpected(ErrorCode::MalformedFrame);
            if (out_ptr + num_literals > out_end) return etl::unexpected(ErrorCode::BufferFull);

            out_ptr = etl::copy_n(in_ptr, num_literals, out_ptr);
            in_ptr += num_literals;

            if (code < 0xFF && in_ptr < in_end) {
                if (out_ptr == out_end) return etl::unexpected(ErrorCode::BufferFull);
                *out_ptr++ = Marker;
            }
        }

        return decode_recursive(in_ptr, in_end, out_ptr, out_end, out_start);
    }
};

} // namespace PacketSerial2
