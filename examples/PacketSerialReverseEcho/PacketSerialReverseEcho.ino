/**
 * @file PacketSerialReverseEcho.ino
 * @brief Simple reverse echo example using Modern PacketSerial (v2.1).
 * 
 * New in v2.1: ISR-Safe locking policy.
 */

#include <stdint.h>
#include <etl/array.h>
#include <PacketSerial.h>
#include <Codecs/COBS.h>

using namespace PacketSerial2;

// 1. ALLOCATE STATIC MEMORY
etl::array<uint8_t, 128> rxStorage;
etl::array<uint8_t, 256> workBuffer;

// 2. INSTANTIATE WITH SAFETY POLICY:
// We use ArduinoAtomicLock to make the engine ISR-Safe.
// This prevents buffer corruption if update() and send() are called 
// from different contexts (e.g. main loop vs timer interrupt).
PacketSerial<COBS, NoCRC, ArduinoAtomicLock> ps(rxStorage, workBuffer);

void onPacketReceived(etl::span<const uint8_t> packet) {
    uint8_t reversed[packet.size()];
    for (size_t i = 0; i < packet.size(); i++) {
        reversed[i] = packet[packet.size() - 1 - i];
    }
    ps.send(Serial, etl::span<const uint8_t>(reversed, packet.size()));
}

void setup() {
    Serial.begin(115200);
    ps.setPacketHandler(etl::make_delegate(onPacketReceived));
}

void loop() {
    ps.update(Serial);
}
