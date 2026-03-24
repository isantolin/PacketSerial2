/**
 * @file PacketSerialReverseEcho.ino
 * @brief Simple reverse echo example using Modern PacketSerial (v2.0).
 * 
 * This example demonstrates:
 * 1. Zero-Heap / Deterministic Memory (User-provided buffers).
 * 2. etl::delegate for global function callbacks.
 * 3. etl::span for safe buffer access.
 */

#include <stdint.h>
#include <etl/array.h>
#include <PacketSerial.h>
#include <Codecs/COBS.h>

// Use the library namespace
using namespace PacketSerial2;

// 1. ALLOCATE STATIC MEMORY:
// These buffers are allocated in RAM at compile time (no heap usage).
etl::array<uint8_t, 128> rxStorage;  // For raw incoming data
etl::array<uint8_t, 256> workBuffer; // For decoding and encoding

// 2. INSTANTIATE WITH INJECTED MEMORY:
PacketSerial<COBS> ps(rxStorage, workBuffer);

/**
 * @brief Handle incoming packets.
 * @param packet A safe view of the decoded data.
 */
void onPacketReceived(etl::span<const uint8_t> packet) {
    // We will reverse the received packet and send it back.
    // Note: Since PacketSerial is Zero-Heap, we must use a local or 
    // static buffer if we want to modify the data before sending.
    
    // In this simple example, we reverse the packet using the same buffer 
    // (but carefully, since 'packet' is const, we need a local copy or 
    // we can use the send method directly if it supported it).
    
    // Create a temporary stack buffer for reversing (only for small packets)
    uint8_t reversed[packet.size()];
    for (size_t i = 0; i < packet.size(); i++) {
        reversed[i] = packet[packet.size() - 1 - i];
    }

    // Send the reversed packet back through Serial.
    ps.send(Serial, etl::span<const uint8_t>(reversed, packet.size()));
}

void setup() {
    Serial.begin(115200);

    // Register our callback function using etl::make_delegate.
    ps.setPacketHandler(etl::make_delegate(onPacketReceived));
}

void loop() {
    // Update the PacketSerial engine.
    ps.update(Serial);
}
