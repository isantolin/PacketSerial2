# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [2.2.0] 2026-04-26

### Added
- **Pure-ETL Implementation**: Completely eradicated all manual `for` and `while` loops in the core logic using `etl::algorithm`.
- **Multi-Subscriber Support**: Added `addPacketHandler` to allow multiple `etl::delegate` subscribers (up to 2 by default) per instance.
- **Optimized Marker Search**: Integrated `etl::find` for accelerated packet boundary detection in the circular buffer.
- **Compile-Time Contract Validation**: Used C++17 `static_assert` and `etl::is_base_of` to enforce Codec interface integrity at compile time.
- **Internal State Bitset**: Replaced boolean flags with `etl::bitset` for memory-efficient and explicit state management.
- **SIL-2 Compliance Patterns**: Strict determinism with explicit lambda captures and separated increment operations.
- **Bulk-Write Optimization**: Dramatic performance boost (up to 75%) on Arduino by sending data in 32-byte blocks.
- **Symmetric CRC Support**: Full support for CRC in the `send()` pipeline, compatible with any ETL CRC type.
- **PC Unit Test Suite**: Added a hardware-agnostic test suite in `tests/` for logic verification.
- **PacketSerialBase**: New base class to minimize template bloat and Flash consumption.

### Changed
- **API Extension**: `addPacketHandler` allows adding handlers without clearing previous ones.
- **API Consistency**: `setPacketHandler` remains for backward compatibility, clearing existing handlers before adding the new one.
- **Breaking Change**: `send()` is now public and fully supports integrated CRC.
- **API Purism**: Replaced all remaining raw pointer arithmetic and manual indexing `[]` with ETL iterators.
- **Metadata**: Updated `library.properties` to version 2.2.0 and unified maintainer information.

## [2.0.0] 2026-03-24

### Added
- Complete rewrite for Modern C++17 and Industrial Standards.
- **Zero-Heap / Zero-STL** architecture.
- **Deterministic Memory**: Buffers must be provided by the user via `etl::span` or `etl::array`.
- **ETL Integration**: Full support for Embedded Template Library.
- **Integrated CRC**: Optional CRC8/16/32 validation support using templates.
- **Advanced Error Handling**: Typed `ErrorCode` via `etl::expected` and error delegates.
- **ICodec Interface**: CRTP-based zero-cost abstraction for custom codecs.

### Changed
- **Breaking Change**: `PacketSerial` now requires memory injection in the constructor.
- **Breaking Change**: Callbacks now use `etl::delegate` instead of raw function pointers.
- **Breaking Change**: Replaced `PacketHandlerFunction` with `etl::delegate<void(etl::span<const uint8_t>)>`.
- **Namespace**: All core logic moved to `PacketSerial2` namespace.

### Removed
- Removed old `PacketSerial_` template and `Encoding/` directory.
- Removed dynamic buffer allocation and virtual functions.

## [1.4.0] 2019-07-26

### Added

- Added `const Stream* getStream() const` and `Stream* getStream()` methods. Thanks @orgicus.

### Changed

- Updated README.

## [1.3.0] 2019-07-25

### Removed

- Remove the `void begin(unsigned long speed, size_t port)` function.
- Remove the `void begin(Stream* stream)` function.
- `while(!SerialX)` line when `CORE_TEENSY` is defined. This was leading to unexpected behavior where programs would not start until a serial connection was opened.

### Added

- Lambda function packetHandler examples and documentation.
- Add `bool overflow() const` to check for a receive buffer overflow.
- Added API documentation @ [http://bakercp.github.io/PacketSerial/](http://bakercp.github.io/PacketSerial/)

### Changed

- Updated README.md, fixed spelling error, added links, docs.
- Rewrote SLIP enum in terms of decimal notation for consistency.
- Updated documentation folder, .github structure.
- Updated CI testing.

## [1.2.0] 2017-11-09

### Added

- An additional PacketHandler pointer type that includes the sender's pointer e.g. `void onPacketReceived(const void* sender, const uint8_t* buffer, size_t size)`. Either functions can be set. Calling `setPacketHandler()` will always remove all previous function pointers.

### Removed

- Deprecated all but one basic `void begin(Stream* stream)` function. Use `void setStream(Stream* stream)` instead.
- Reverted void `PacketSerial_::begin(unsigned long speed, uint8_t config, size_t port)`.

### Changed

- Updated README.md, fixed errors, spelling and updated examples.

## [1.1.0] 2017-11-09

### Added

- Additional inline documentation.
- Added doxygen configuration file.
- Added CHANGELOG.md file.
- Added the `void PacketSerial_::begin(unsigned long speed, uint8_t config, size_t port)` method to avoid confusion with the standard `Serial.begin(unsigned long speed, uint8_t config)`.

### Changed

- Updated README.md, fixed errors, spelling, byte counts, etc.
- Updated documentation / comments in documentation for clarity.

### Removed

- Deprecated the `void begin(unsigned long speed, size_t port)` method because it could be confused with the standard `Serial.begin(unsigned long speed, uint8_t config)` method.

### Fixed

- Fixed Duplicated SLIP END Packet #[11](https://github.com/bakercp/PacketSerial/issues/11)
- Fix types to remove warnings in examples.
- Add `const` qualifier to the `send()` method.

### Security

- None
