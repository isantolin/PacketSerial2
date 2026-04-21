/**
 * @file PacketSerialReverseEcho.ino
 * @brief Simple reverse echo example using Modern PacketSerial (v2.2).
 */

#include <stdint.h>
#include <etl/array.h>
#include <etl/algorithm.h>
#include <PacketSerial.h>
#include <Codecs/COBS.h>

using namespace PacketSerial2;

// 1. ALLOCATE STATIC MEMORY
etl::array<uint8_t, 128> rxStorage;
etl::array<uint8_t, 256> workBuffer;

// 2. INSTANTIATE WITH SAFETY POLICY
PacketSerial<COBS, NoCRC, ArduinoAtomicLock> ps(rxStorage, workBuffer);

// 3. ZERO-STL REVERSE BUFFER
etl::array<uint8_t, 256> reverseBuffer;

void onPacketReceived(etl::span<const uint8_t> packet) {
    if (packet.size() > reverseBuffer.size()) return;

    // Use ETL algorithm instead of manual for loop
    etl::reverse_copy(packet.begin(), packet.end(), reverseBuffer.begin());
    
    // Send it back
    ps.send(Serial, etl::span<const uint8_t>(reverseBuffer.data(), packet.size()));
}

void setup() {
    Serial.begin(115200);
    ps.setPacketHandler(etl::make_delegate(onPacketReceived));
}

void loop() {
    ps.update(Serial);
}
