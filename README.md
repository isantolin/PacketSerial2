# PacketSerial (v2.0)

[![Build Status](https://travis-ci.org/bakercp/PacketSerial.svg?branch=master)](https://travis-ci.org/bakercp/PacketSerial)
[![Software License](https://img.shields.io/badge/license-MIT-brightgreen.svg)](LICENSE.md)

**PacketSerial** is an industrial-grade library that facilitates packet-based serial communication (COBS, SLIP) for Arduino and other embedded platforms. 

Version 2.0 is a complete modernization following **SIL-2 compliance** principles: **Zero-Heap**, **Deterministic Memory**, and **Functional Safety**.

---

## 🚀 Key Modern Features

1.  **Zero-Heap Architecture**: No `malloc`, `new`, or dynamic allocation. No `std::vector` or `std::string`.
2.  **Deterministic RAM**: All RX/TX buffers are provided by the user at compile-time via `etl::array` or `etl::span`.
3.  **ETL Native**: Powered by the [Embedded Template Library](https://www.etlcpp.com/). 
    - Callbacks via `etl::delegate` (No heavy virtual functions).
    - Internal state managed by `etl::bitset` and `etl::circular_buffer`.
4.  **Integrated Integrity (CRC)**: Optional 8/16/32-bit CRC validation built into the framing process.
5.  **C++17 Optimized**: Uses `if constexpr` and CRTP for "Zero-Cost" abstractions.
6.  **Platform Agnostic**: Compiles on AVR, ESP32, ARM (Teensy/STM32), and Linux/Windows for unit testing.

---

## 🛠 Quick Start

### 1. Define your buffers (Static)
```cpp
#include <PacketSerial.h>
#include <Codecs/COBS.h>
#include <etl/array.h>

using namespace PacketSerial2;

// These buffers are allocated in RAM (not stack, not heap)
etl::array<uint8_t, 128> rxStorage;
etl::array<uint8_t, 256> workBuffer;
```

### 2. Instantiate and Register
```cpp
PacketSerial<COBS> ps(rxStorage, workBuffer);

void onPacket(etl::span<const uint8_t> packet) {
    // Process your safe data view here
}

void setup() {
    Serial.begin(115200);
    ps.setPacketHandler(etl::make_delegate(onPacket));
}

void loop() {
    ps.update(Serial);
}
```

---

## 📦 Requirements

- **C++17** compatible compiler.
- **Embedded Template Library (ETL)**: Must be installed in your environment.

## 📖 Documentation

- [Getting Started](docs/GETTING_STARTED.md)
- [Industrial Robustness (CRC & Errors)](examples/PacketSerialReverseEchoAdvanced/PacketSerialReverseEchoAdvanced.ino)
- [Changelog](docs/CHANGELOG.md)

## License

MIT License. See [LICENSE.md](LICENSE.md) for details.
