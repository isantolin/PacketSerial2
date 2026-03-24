#pragma once

/**
 * @file Safety.h
 * @brief Safety policies for ISR protection and Watchdog integration.
 */

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
