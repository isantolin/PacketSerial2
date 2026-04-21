/**
 * @file PacketModern.ino
 * @brief Simple usage of PacketSerial2 (v2.2) with COBS.
 */

#include <stdint.h>
#include <PacketSerial.h>
#include <Codecs/COBS.h>
#include <etl/array.h>

using namespace PacketSerial2;

// Static allocation for maximum reliability
etl::array<uint8_t, 64> rx_storage;
etl::array<uint8_t, 128> work_buffer;

// Instantiate the engine with default policies
PacketSerial<COBS> ps(rx_storage, work_buffer);

void onPacket(etl::span<const uint8_t> packet) {
    // Functional style: process and echo
    if (!packet.empty()) {
        ps.send(Serial, packet);
    }
}

void setup() {
    Serial.begin(115200);
    
    // Register callback using ETL delegate
    ps.setPacketHandler(etl::make_delegate(onPacket));
}

void loop() {
    // Hardware-agnostic update
    ps.update(Serial);
}
