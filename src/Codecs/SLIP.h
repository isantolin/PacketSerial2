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

        for (uint8_t byte : input) {
            if (byte == END) {
                if (write_index + 1 < output.size()) {
                    output[write_index++] = ESC;
                    output[write_index++] = ESC_END;
                } else {
                    return etl::unexpected(ErrorCode::BufferFull);
                }
            } else if (byte == ESC) {
                if (write_index + 1 < output.size()) {
                    output[write_index++] = ESC;
                    output[write_index++] = ESC_ESC;
                } else {
                    return etl::unexpected(ErrorCode::BufferFull);
                }
            } else {
                if (write_index < output.size()) {
                    output[write_index++] = byte;
                } else {
                    return etl::unexpected(ErrorCode::BufferFull);
                }
            }
        }
        
        return write_index;
    }

    /**
     * @brief Decode a SLIP-encoded buffer.
     */
    etl::expected<size_t, ErrorCode> decode_impl(etl::span<const uint8_t> input, etl::span<uint8_t> output) {
        size_t write_index = 0;
        for (size_t i = 0; i < input.size(); ++i) {
            uint8_t byte = input[i];
            
            // Skip leading/trailing END markers if they appear in the middle of the frame
            if (byte == END) continue;

            if (byte == ESC) {
                if (i + 1 < input.size()) {
                    uint8_t next_byte = input[++i];
                    if (next_byte == ESC_END) {
                        byte = END;
                    } else if (next_byte == ESC_ESC) {
                        byte = ESC;
                    } else {
                        return etl::unexpected(ErrorCode::MalformedFrame);
                    }
                } else {
                    return etl::unexpected(ErrorCode::MalformedFrame);
                }
            }
            
            if (write_index < output.size()) {
                output[write_index++] = byte;
            } else {
                return etl::unexpected(ErrorCode::BufferOverflow);
            }
        }
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
