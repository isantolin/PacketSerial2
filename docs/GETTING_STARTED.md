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

Register a callback function (Delegate) to handle incoming decoded packets.

```cpp
void onPacket(etl::span<const uint8_t> packet) {
    // packet.data() contains the data
    // packet.size() contains the size
}

void setup() {
    Serial.begin(115200);
    // Bind your function using etl::make_delegate
    ps.setPacketHandler(etl::make_delegate(onPacket));
}

void loop() {
    ps.update(Serial);
}
```

## 4. Advanced: Using CRC

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
