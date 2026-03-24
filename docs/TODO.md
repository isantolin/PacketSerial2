# TODO

## Version 2.x Goals

- [ ] **Unit Tests**: Implement a comprehensive test suite using GTest or Catch2 that can run on Linux/Windows.
- [ ] **Mock Streams**: Provide a standard MockStream class for easy testing of protocol logic.
- [ ] **More Codecs**:
    - [ ] **COBS/R**: Support for Reduced overhead COBS.
    - [ ] **Consistent-Framing**: Implementation of more framing standards.
- [ ] **In-Place Encoding**: Investigate if COBS can be encoded in-place safely without doubling buffer size.
- [ ] **Auto-Discovery**: Support for hardware-specific optimizations using `if constexpr`.
- [ ] **DMA Support**: Hooks for non-blocking DMA-based Stream operations on ARM/ESP32.

## Done

- [x] **Zero-Heap Implementation**: Complete rewrite without dynamic memory.
- [x] **ETL Integration**: Full support for Embedded Template Library.
- [x] **C++17 Migration**: Usage of `if constexpr`, `string_view`, `span`, and `expected`.
- [x] **Integrated CRC**: Native support for 8/16/32-bit CRC.
- [x] **CRTP Abstraction**: Zero-cost abstraction for custom codecs.
- [x] **Deterministic Memory**: User-provided buffers via injected spans.
