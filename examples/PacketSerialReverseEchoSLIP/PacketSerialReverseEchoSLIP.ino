/**
 * @file PacketSerialReverseEchoSLIP.ino
 * @brief Reverse echo example using SLIP codec (v2.0).
 * 
 * This example shows:
 * 1. Changing the protocol (Codec) via template parameter.
 * 2. SLIP (Serial Line Internet Protocol) usage.
 * 3. Zero-Heap / Deterministic RAM.
 */

#include <stdint.h>
#include <etl/array.h>
#include <PacketSerial.h>
#include <Codecs/SLIP.h>

// Use the library namespace
using namespace PacketSerial2;

// 1. ALLOCATE STATIC MEMORY:
// Buffers are allocated in RAM at compile time.
etl::array<uint8_t, 128> rxStorage;
etl::array<uint8_t, 256> workBuffer;

// 2. INSTANTIATE WITH SLIP CODEC:
// Note how we only change the template parameter 'SLIP'.
PacketSerial<SLIP> ps(rxStorage, workBuffer);

/**
 * @brief Handle incoming packets.
 * @param packet A safe view of the decoded data.
 */
void onPacketReceived(etl::span<const uint8_t> packet) {
    // Create a local copy to reverse the packet.
    uint8_t reversed[packet.size()];
    for (size_t i = 0; i < packet.size(); i++) {
        reversed[i] = packet[packet.size() - 1 - i];
    }

    // Send the reversed packet back using SLIP.
    ps.send(Serial, etl::span<const uint8_t>(reversed, packet.size()));
}

void setup() {
    Serial.begin(115200);

    // Register callback.
    ps.setPacketHandler(etl::make_delegate(onPacketReceived));
}

void loop() {
    // Update the PacketSerial engine.
    ps.update(Serial);
}
