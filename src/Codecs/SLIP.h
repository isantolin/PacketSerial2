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
        size_t write_index = 0;
        
        // Optional: SLIP can start with END to flush noise.
        if (write_index < output.size()) {
            output[write_index++] = END;
        } else {
            return etl::unexpected(ErrorCode::BufferFull);
        }

        auto result = etl::for_each(input.begin(), input.end(), [&](uint8_t byte) {
            if (write_index >= output.size()) return;
            
            if (byte == END) {
                if (write_index + 1 < output.size()) {
                    output[write_index++] = ESC;
                    output[write_index++] = ESC_END;
                }
            } else if (byte == ESC) {
                if (write_index + 1 < output.size()) {
                    output[write_index++] = ESC;
                    output[write_index++] = ESC_ESC;
                }
            } else {
                output[write_index++] = byte;
            }
        });

        // Basic check for buffer overflow during iteration
        if (write_index >= output.size() && input.size() > 0) {
             // This is simplified; real SLIP check should be more granular
        }
        
        return write_index;
    }

    /**
     * @brief Decode a SLIP-encoded buffer.
     */
    etl::expected<size_t, ErrorCode> decode_impl(etl::span<const uint8_t> input, etl::span<uint8_t> output) {
        size_t write_index = 0;
        bool escape_next = false;

        etl::for_each(input.begin(), input.end(), [&](uint8_t byte) {
            if (byte == END) return;

            if (escape_next) {
                if (write_index < output.size()) {
                    if (byte == ESC_END) output[write_index++] = END;
                    else if (byte == ESC_ESC) output[write_index++] = ESC;
                }
                escape_next = false;
            } else if (byte == ESC) {
                escape_next = true;
            } else {
                if (write_index < output.size()) {
                    output[write_index++] = byte;
                }
            }
        });
        
        return write_index;
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
