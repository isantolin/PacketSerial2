# TODO

## Version 2.x Goals

- [ ] **More Codecs**:
    - [ ] **COBS/R**: Support for Reduced overhead COBS.
    - [ ] **Consistent-Framing**: Implementation of more framing standards.
- [ ] **DMA Support**: Hooks for non-blocking DMA-based Stream operations on ARM/ESP32.
- [ ] **In-Place Encoding**: Optimize COBS encoding to reduce intermediate buffer requirements.

## Done

- [x] **Zero-Heap Implementation**: Complete rewrite without dynamic memory.
- [x] **ETL Integration**: Full support for Embedded Template Library.
- [x] **C++17 Migration**: Usage of `if constexpr`, `span`, and `expected`.
- [x] **Integrated CRC**: Native support for symmetric 8/16/32-bit CRC.
- [x] **Pure ETL Implementation**: Eradication of manual loops in core logic.
- [x] **Unit Tests**: PC-based test suite for protocol logic verification.
- [x] **Mock Streams**: Hardware-agnostic Stream mock for testing.
- [x] **Bulk-Write Optimization**: 32-byte chunked transmission for AVR.
