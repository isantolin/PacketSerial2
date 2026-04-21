#pragma once

#include <stdint.h>
#include <stddef.h>
#include <etl/expected.h>
#include <etl/span.h>
#include "../ICodec.h"

namespace PacketSerial2 {

/**
 * @brief Serial Line Internet Protocol (SLIP) Codec.
 */
class SLIP : public ICodec<SLIP> {
public:
    enum Control : uint8_t {
        END = 192,
        ESC = 219,
        ESC_END = 220,
        ESC_ESC = 221
    };

    /**
     * @brief Encode a byte buffer with SLIP.
     */
    etl::expected<size_t, ErrorCode> encode_impl(etl::span<const uint8_t> input, etl::span<uint8_t> output) {
        auto out_it = output.begin();
        
        if (out_it != output.end()) {
            *out_it = END;
            out_it++;
        } else {
            return etl::unexpected(ErrorCode::BufferFull);
        }

        etl::for_each(input.begin(), input.end(), [this, &out_it, &output](uint8_t byte) {
            if (out_it == output.end()) return;
            
            if (byte == END) {
                if (etl::distance(out_it, output.end()) >= 2) {
                    *out_it = ESC;
                    out_it++;
                    *out_it = ESC_END;
                    out_it++;
                }
            } else if (byte == ESC) {
                if (etl::distance(out_it, output.end()) >= 2) {
                    *out_it = ESC;
                    out_it++;
                    *out_it = ESC_ESC;
                    out_it++;
                }
            } else {
                *out_it = byte;
                out_it++;
            }
        });
        
        return etl::distance(output.begin(), out_it);
    }

    /**
     * @brief Decode a SLIP-encoded buffer.
     */
    etl::expected<size_t, ErrorCode> decode_impl(etl::span<const uint8_t> input, etl::span<uint8_t> output) {
        auto out_it = output.begin();
        bool escape_next = false;

        etl::for_each(input.begin(), input.end(), [this, &out_it, &output, &escape_next](uint8_t byte) {
            if (byte == END) return;

            if (escape_next) {
                if (out_it != output.end()) {
                    if (byte == ESC_END) {
                        *out_it = END;
                        out_it++;
                    }
                    else if (byte == ESC_ESC) {
                        *out_it = ESC;
                        out_it++;
                    }
                }
                escape_next = false;
            } else if (byte == ESC) {
                escape_next = true;
            } else {
                if (out_it != output.end()) {
                    *out_it = byte;
                    out_it++;
                }
            }
        });
        
        return etl::distance(output.begin(), out_it);
    }

    /**
     * @brief Get the maximum encoded buffer size for SLIP.
     */
    static constexpr size_t getEncodedBufferSize_impl(size_t unencodedBufferSize) {
        // Worst case: every byte is escaped, plus leading END.
        return (unencodedBufferSize * 2) + 1;
    }
};

} // namespace PacketSerial2
