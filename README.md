# PacketSerial (v2.2)

[![Build Status](https://travis-ci.org/bakercp/PacketSerial.svg?branch=master)](https://travis-ci.org/bakercp/PacketSerial)
[![Software License](https://img.shields.io/badge/license-MIT-brightgreen.svg)](LICENSE.md)

**PacketSerial** is an industrial-grade library that facilitates packet-based serial communication (COBS, SLIP) for Arduino and other embedded platforms. 

Version 2.2 is a major optimization following **SIL-2 compliance** principles: **Zero-Heap**, **Zero-STL**, and **Deterministic Performance**.

---

## ⚖️ Premisas de Modernización (v2.2)

Esta biblioteca ha sido rediseñada siguiendo principios de ingeniería para sistemas críticos y de grado industrial:

### 1. Gestión de Memoria Determinista (Zero-Heap / Zero-STL)
*   **Prohibición de Asignación Dinámica**: No se utiliza `malloc`, `free`, `new`, `delete` ni contenedores de la STL.
*   **Zero-STL**: No depende de las cabeceras estándar de C++ (`<cstdint>`, `<algorithm>`, etc.), utilizando cabeceras de C estándar (`<stdint.h>`, `<stddef.h>`) para máxima portabilidad en entornos *freestanding*.
*   **Buffers Estáticos**: Toda la memoria para RX/TX debe ser provista por el usuario, garantizando un uso de RAM conocido en tiempo de compilación.

### 2. Integración Profunda con ETL (Embedded Template Library)
*   **Algoritmos Optimizados**: Uso de `etl::algorithm` (`copy`, `for_each`) para el procesamiento de datos, permitiendo optimizaciones de hardware específicas.
*   **Callbacks mediante `etl::delegate`**: Sustitución de punteros a funciones por delegados eficientes.
*   **Manejo de Errores con `etl::expected`**: Gestión de errores tipada y robusta.

### 3. Estándar C++17 "Zero-Cost Abstractions"
*   **Polimorfismo en Tiempo de Compilación**: Uso de **CRTP** (Curiously Recurring Template Pattern) para eliminar el overhead de VTables.
*   **Zero-Copy**: Uso extensivo de `etl::span` para vistas de datos sin copias innecesarias.

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
// v2.2: Support for Lock Policies and Watchdog Heartbeat
PacketSerial<COBS, NoCRC, NoLock, NoWatchdog> ps(rxStorage, workBuffer);

void onPacket(etl::span<const uint8_t> packet) {
    // Process your data safe view
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
