# Getting Started with PacketSerial v2.2

Version 2.2 is an optimized release for high-performance and safety-critical systems.

## 1. Prerequisites

- **Embedded Template Library (ETL)**: This is a MANDATORY dependency. Install it via the Arduino Library Manager or download it from [etlcpp.com](https://www.etlcpp.com/).
- **C++17**: The library uses modern features like `if constexpr` and `span`.
- **Zero-STL**: This library does NOT require the C++ Standard Template Library.

## 2. Basic Setup (Zero-Heap)

PacketSerial v2.2 does not allocate any memory internally. You must provide the buffers statically.

### Allocate Buffers
Use `etl::array` to ensure memory is allocated in RAM (not on the stack or heap).

```cpp
#include <PacketSerial.h>
#include <Codecs/COBS.h>
#include <etl/array.h>

using namespace PacketSerial2;

// Storage for raw incoming bytes (Stream input)
etl::array<uint8_t, 128> rx_storage;

// Storage for decoding/encoding processes
etl::array<uint8_t, 256> work_buffer;

// Instantiate the engine
PacketSerial<COBS> ps(rx_storage, work_buffer);
```

## 3. Handling Packets

Register one or more callback functions (Delegates) to handle incoming decoded packets.

### Simple Registration
```cpp
void onPacket(etl::span<const uint8_t> packet) {
    // Process your data
}

void setup() {
    Serial.begin(115200);
    // Bind your function using etl::make_delegate
    ps.setPacketHandler(etl::make_delegate(onPacket));
}
```

### Multi-Subscriber (Advanced)
PacketSerial v2.2 supports multiple independent handlers. This is useful for decoupling logic (e.g., one handler for data processing, another for logging).

```cpp
void onPacketData(etl::span<const uint8_t> packet) { /* ... */ }
void onPacketLog(etl::span<const uint8_t> packet) { /* ... */ }

void setup() {
    ps.addPacketHandler(etl::make_delegate(onPacketData));
    ps.addPacketHandler(etl::make_delegate(onPacketLog));
}
```

## 4. Compile-Time Safety

The library uses C++17 `static_assert` to ensure that your configuration is valid. If you try to use a Codec that does not inherit from `ICodec`, the compiler will generate a clear error message.

## 5. Advanced: Using CRC

PacketSerial v2.2 supports full symmetric data integrity. When a CRC is specified, it is automatically calculated and appended during `send()`, and verified during `update()`.

```cpp
#include <etl/crc16.h>

// v2.2 supports any ETL CRC type seamlessly (crc8, crc16, crc32)
PacketSerial<COBS, etl::crc16> ps(rx_storage, work_buffer);
```

## 5. Industrial Safety (SIL-2 Ready)

For safety-critical systems, you can inject locking policies (e.g., for ISR safety) and watchdog heartbeat hooks.

```cpp
// Example: ISR-Safe with Watchdog support
PacketSerial<COBS, NoCRC, ArduinoAtomicLock> ps(rx_storage, work_buffer);
```
