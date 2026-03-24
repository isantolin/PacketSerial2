/**
 * @file PacketSerialReverseEchoSLIP.ino
 * @brief SLIP Reverse echo example (v2.1).
 * 
 * Safety features:
 * 1. SLIP protocol with framing markers.
 * 2. ArduinoAtomicLock for thread/ISR safety.
 */

#include <stdint.h>
#include <etl/array.h>
#include <PacketSerial.h>
#include <Codecs/SLIP.h>

using namespace PacketSerial2;

etl::array<uint8_t, 128> rxStorage;
etl::array<uint8_t, 256> workBuffer;

// Using SLIP codec with atomic protection.
PacketSerial<SLIP, NoCRC, ArduinoAtomicLock> ps(rxStorage, workBuffer);

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
