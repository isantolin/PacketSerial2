#pragma once

#include <stddef.h>
#include <etl/expected.h>
#include <etl/span.h>
#include "ErrorCode.h"

namespace PacketSerial2 {

/**
 * @brief Base class for codecs using CRTP (Curiously Recurring Template Pattern).
 * 
 * @tparam Derived The derived codec class.
 */
template <typename Derived>
class ICodec {
public:
    /**
     * @brief Encode a byte buffer.
     * 
     * @param input The unencoded input buffer.
     * @param output The buffer for the encoded bytes.
     * @return etl::expected<size_t, ErrorCode> Number of bytes written to the output buffer, or an error code.
     */
    etl::expected<size_t, ErrorCode> encode(etl::span<const uint8_t> input, etl::span<uint8_t> output) {
        return static_cast<Derived*>(this)->encode_impl(input, output);
    }

    /**
     * @brief Decode an encoded buffer.
     * 
     * @param input The encoded input buffer.
     * @param output The target buffer for the decoded bytes.
     * @return etl::expected<size_t, ErrorCode> Number of bytes written to the output buffer, or an error code.
     */
    etl::expected<size_t, ErrorCode> decode(etl::span<const uint8_t> input, etl::span<uint8_t> output) {
        return static_cast<Derived*>(this)->decode_impl(input, output);
    }

    /**
     * @brief Get the maximum encoded buffer size for an unencoded buffer size.
     * 
     * @param unencodedBufferSize The size of the buffer to be encoded.
     * @return size_t The maximum size of the required encoded buffer.
     */
    static size_t getEncodedBufferSize(size_t unencodedBufferSize) {
        return Derived::getEncodedBufferSize_impl(unencodedBufferSize);
    }
};

} // namespace PacketSerial2
