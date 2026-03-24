# PacketSerial (v2.1)

[![Build Status](https://travis-ci.org/bakercp/PacketSerial.svg?branch=master)](https://travis-ci.org/bakercp/PacketSerial)
[![Software License](https://img.shields.io/badge/license-MIT-brightgreen.svg)](LICENSE.md)

**PacketSerial** is an industrial-grade library that facilitates packet-based serial communication (COBS, SLIP) for Arduino and other embedded platforms. 

Version 2.0+ is a complete modernization following **SIL-2 compliance** principles: **Zero-Heap**, **Deterministic Memory**, and **Functional Safety**.

---

## ⚖️ Premisas de Modernización (v2.0+)

Esta biblioteca ha sido rediseñada desde cero siguiendo principios de ingeniería para sistemas críticos y de grado industrial:

### 1. Gestión de Memoria Determinista (Zero-Heap / No STL)
*   **Prohibición de Asignación Dinámica**: No se utiliza `malloc`, `free`, `new`, `delete` ni contenedores de la STL (`std::vector`, `std::string`).
*   **Buffers Estáticos**: Toda la memoria para RX/TX debe ser provista por el usuario mediante `etl::span` o `etl::array`, garantizando un uso de RAM conocido en tiempo de compilación.

### 2. Integración Nativa con ETL (Embedded Template Library)
*   **Callbacks mediante `etl::delegate`**: Sustitución de punteros a funciones por delegados para permitir callbacks a métodos de clase sin el overhead de punteros virtuales.
*   **Estructuras de Datos ETL**: Uso interno de `etl::circular_buffer` y `etl::bitset` para la gestión de la FSM (Finite State Machine).
*   **Manejo de Errores con `etl::expected`**: Gestión de errores tipada y sin excepciones.

### 3. Estándar C++17 "Zero-Cost Abstractions"
*   **Polimorfismo en Tiempo de Compilación**: Uso de Templates y **CRTP** (Curiously Recurring Template Pattern) en lugar de funciones virtuales, eliminando el VTable.
*   **Uso de `if constexpr`**: Rutas de código optimizadas según capacidades del hardware sin penalización en tiempo de ejecución.
*   **Zero-Copy**: Uso extensivo de `etl::span` para evitar copias innecesarias de datos.

### 4. Framing de Grado Industrial y Robustez
*   **COBS de Primera Clase**: Implementación optimizada con soporte para decodificación In-Place.
*   **Integridad (CRC)**: Capa opcional de validación CRC (8/16/32) integrada en el proceso de framing.
*   **Resiliencia**: Re-sincronización automática tras detectar delimitadores, con reporte de errores por desbordamiento o tramas malformadas.

### 5. Abstracción de Hardware y Testabilidad
*   **Platform Agnostic**: La lógica core es independiente de `Arduino.h`, permitiendo pruebas unitarias en Linux/Windows.
*   **Interfaz Genérica**: Compatible con `Stream` de Arduino pero permitiendo inyectar Mocks para testing.

### 6. Seguridad Funcional (SIL-2 Compliant)
*   **Hardware Watchdog Heartbeat**: Hooks opcionales para alimentar el WDT durante procesos largos.
*   **Protección contra ISR**: Acceso seguro a buffers compartidos mediante secciones atómicas configurables (Lock Policies).

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
// v2.1: Support for ArduinoAtomicLock for ISR safety
PacketSerial<COBS, NoCRC, ArduinoAtomicLock> ps(rxStorage, workBuffer);

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
