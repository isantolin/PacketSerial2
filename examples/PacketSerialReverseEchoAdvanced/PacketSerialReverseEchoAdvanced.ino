/**
 * @file PacketSerialReverseEchoAdvanced.ino
 * @brief Industrial Advanced configuration (v2.2).
 * 
 * Demonstrates:
 * 1. Data Integrity: using etl::crc16.
 * 2. ISR Protection: using ArduinoAtomicLock.
 * 3. Functional Safety: Full state protection.
 */

#include <stdint.h>
#include <etl/array.h>
#include <etl/crc16.h>
#include <PacketSerial.h>
#include <Codecs/COBS.h>

using namespace PacketSerial2;

etl::array<uint8_t, 128> rxStorage;
etl::array<uint8_t, 256> workBuffer;

// THE GOLD STANDARD CONFIGURATION:
// - COBS for framing.
// - CRC16 for data integrity.
// - ArduinoAtomicLock for safe multi-context access.
PacketSerial<COBS, etl::crc16, ArduinoAtomicLock> ps(rxStorage, workBuffer);

void onPacketReceived(etl::span<const uint8_t> packet) {
    // Only valid packets (with correct CRC) will trigger this.
    ps.send(Serial, packet);
}

void onError(ErrorCode error) {
    if (error == ErrorCode::InvalidChecksum) {
        Serial.println("Warning: Corrupt packet dropped.");
    }
}

void setup() {
    Serial.begin(115200);
    ps.setPacketHandler(etl::make_delegate(onPacketReceived));
    ps.setErrorHandler(etl::make_delegate(onError));
}

void loop() {
    ps.update(Serial);
}
