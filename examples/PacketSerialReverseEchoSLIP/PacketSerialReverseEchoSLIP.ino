/**
 * @file PacketSerialReverseEchoSLIP.ino
 * @brief Simple reverse echo example using SLIP and ETL (v2.2).
 */

#include <stdint.h>
#include <etl/array.h>
#include <etl/algorithm.h>
#include <PacketSerial.h>
#include <Codecs/SLIP.h>

using namespace PacketSerial2;

etl::array<uint8_t, 128> rxStorage;
etl::array<uint8_t, 256> workBuffer;
etl::array<uint8_t, 256> reverseBuffer;

// Using SLIP codec with atomic protection.
PacketSerial<SLIP, NoCRC, ArduinoAtomicLock> ps(rxStorage, workBuffer);

void onPacketReceived(etl::span<const uint8_t> packet) {
    if (packet.size() > reverseBuffer.size()) return;

    // Pure ETL algorithm
    etl::reverse_copy(packet.begin(), packet.end(), reverseBuffer.begin());
    
    ps.send(Serial, etl::span<const uint8_t>(reverseBuffer.data(), packet.size()));
}

void setup() {
    Serial.begin(115200);
    ps.setPacketHandler(etl::make_delegate(onPacketReceived));
}

void loop() {
    ps.update(Serial);
}
