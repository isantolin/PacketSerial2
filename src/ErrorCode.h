#pragma once

#include <cstdint>

namespace PacketSerial2 {

enum class ErrorCode : uint8_t {
    None = 0,
    BufferFull,
    BufferOverflow,
    MalformedFrame,
    InvalidChecksum,
    InvalidEncoding,
    EncodingError,
    DecodingError,
    IncompletePacket,
    InternalError
};

} // namespace PacketSerial2
