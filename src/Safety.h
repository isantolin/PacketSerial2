#pragma once

#include <stdint.h>
#include <stddef.h>

namespace PacketSerial2 {

/**
 * @brief Default policy: No locking (performance-oriented, not ISR-safe).
 */
struct NoLock {
    inline void lock() const {}
    inline void unlock() const {}
};

/**
 * @brief Default policy: No watchdog heartbeat.
 */
struct NoWatchdog {
    inline void feed() const {}
};

/**
 * @brief Default policy: No CRC validation.
 */
struct NoCRC {
    static constexpr size_t ByteSize = 0;
    inline void reset() {}
    inline void add(uint8_t) {}
    inline uint32_t value() const { return 0; }
};

/**
 * @brief Example of an Arduino Atomic Lock.
 * This can be used as a template parameter for PacketSerial.
 */
#ifdef ARDUINO
#include <Arduino.h>
struct ArduinoAtomicLock {
    inline void lock() const { noInterrupts(); }
    inline void unlock() const { interrupts(); }
};
#endif

} // namespace PacketSerial2
