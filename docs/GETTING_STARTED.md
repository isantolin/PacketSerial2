# Getting Started with PacketSerial v2.0

Version 2.0 is a complete rewrite. If you are migrating from v1.x, please read the [Changelog](CHANGELOG.md) for breaking changes.

## 1. Prerequisites

- **Embedded Template Library (ETL)**: This is a MANDATORY dependency. Install it via the Arduino Library Manager or download it from [etlcpp.com](https://www.etlcpp.com/).
- **C++17**: The library uses modern features like `if constexpr`, `string_view`, and `span`.

## 2. Basic Setup (Zero-Heap)

Unlike previous versions, PacketSerial v2.0 does not allocate any memory internally. You must provide the buffers.

### Allocate Buffers
Use `etl::array` to ensure memory is allocated statically in RAM (not on the stack or heap).

```cpp
#include <PacketSerial.h>
#include <Codecs/COBS.h>
#include <etl/array.h>

using namespace PacketSerial2;

// Storage for raw incoming bytes (Stream input)
etl::array<uint8_t, 128> rx_storage;

// Storage for decoding/encoding processes
// Must be larger than your maximum frame (payload + CRC + overhead)
etl::array<uint8_t, 256> work_buffer;

// Instantiate the engine
PacketSerial<COBS> ps(rx_storage, work_buffer);
```

## 3. Handling Packets

Version 2.0 uses **Delegates** instead of raw function pointers. This allows you to bind to both global functions and class methods easily.

```cpp
void onPacket(etl::span<const uint8_t> packet) {
    // 'packet' is a read-only view of the decoded data.
    // It exists only during this callback. If you need it later, copy it.
    Serial.print("Received bytes: ");
    Serial.println(packet.size());
}

void setup() {
    Serial.begin(115200);
    
    // Register the callback
    ps.setPacketHandler(etl::make_delegate(onPacket));
}

void loop() {
    // Keep processing the stream
    ps.update(Serial);
}
```

## 4. Integrity (Optional CRC)

To enable CRC validation, pass an ETL CRC type as the second template parameter.

```cpp
#include <etl/crc16.h>

// Now every packet is validated with CRC16 automatically.
PacketSerial<COBS, etl::crc16> ps(rx_storage, work_buffer);
```

## 5. Error Handling

Register an error handler to detect overflows or corrupt packets.

```cpp
void onError(ErrorCode error) {
    if (error == ErrorCode::InvalidChecksum) {
        // Handle corrupt packet
    }
}

void setup() {
    ps.setErrorHandler(etl::make_delegate(onError));
}
```
