/**
 * @file PacketSerialReverseEchoAdvanced.ino
 * @brief Advanced example with Integrity Check (CRC) and Error Handling (v2.0).
 * 
 * This example demonstrates:
 * 1. Data Integrity: Using etl::crc16 for packet validation.
 * 2. Error Handling: Catching InvalidChecksum or BufferOverflow.
 * 3. Industrial Robustness: Automatically discarding corrupt packets.
 */

#include <stdint.h>
#include <etl/array.h>
#include <etl/crc16.h> // Import CRC16 from ETL
#include <PacketSerial.h>
#include <Codecs/COBS.h>

// Use the library namespace
using namespace PacketSerial2;

// 1. ALLOCATE STATIC MEMORY:
etl::array<uint8_t, 128> rxStorage;
etl::array<uint8_t, 256> workBuffer;

// 2. INSTANTIATE WITH CRC16:
// The 4th template parameter enables the CRC engine.
// All packets sent will have a 2-byte CRC appended.
// All incoming packets will be validated before being passed to onPacketReceived.
PacketSerial<COBS, etl::crc16> ps(rxStorage, workBuffer);

/**
 * @brief Handle valid packets.
 */
void onPacketReceived(etl::span<const uint8_t> packet) {
    // If we are here, the packet is ALREADY validated by CRC.
    // We just echo it back.
    ps.send(Serial, packet);
}

/**
 * @brief Handle communication errors.
 * @param error The specific ErrorCode.
 */
void onError(ErrorCode error) {
    switch (error) {
        case ErrorCode::InvalidChecksum:
            // This happens when a packet is received but the CRC doesn't match.
            // PacketSerial automatically discards the packet.
            Serial.println("Error: Corrupt packet detected (CRC mismatch).");
            break;
        case ErrorCode::BufferOverflow:
            Serial.println("Error: Receive buffer overflow.");
            break;
        case ErrorCode::MalformedFrame:
            Serial.println("Error: malformed COBS frame.");
            break;
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);

    // Register both packet and error handlers.
    ps.setPacketHandler(etl::make_delegate(onPacketReceived));
    ps.setErrorHandler(etl::make_delegate(onError));
}

void loop() {
    // Keep the engine running.
    ps.update(Serial);
}
