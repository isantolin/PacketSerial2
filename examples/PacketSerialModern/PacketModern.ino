/**
 * @file PacketSerialModern.ino
 * @brief Modern usage example for PacketSerial.
 * 
 * This example demonstrates:
 * 1. Zero-Heap / Deterministic Memory Management.
 * 2. User-provided buffers via etl::array/etl::span.
 * 3. etl::delegate for class-method callbacks.
 * 4. COBS encoding with zero stack overhead.
 */

#include <stdint.h>
#include <etl/array.h>
#include <PacketSerial.h>
#include <Codecs/COBS.h>

// Use the library namespace
using namespace PacketSerial2;

class MyService {
public:
    /**
     * @brief Initialize PacketSerial by injecting the required buffers.
     * 
     * This ensures that PacketSerial uses ONLY the memory we provide here,
     * making its RAM usage perfectly predictable and avoiding the heap.
     */
    MyService() : _ps(_rx_storage, _work_buffer) {}

    void begin() {
        // Initialize the library with etl::delegates.
        _ps.setPacketHandler(etl::make_delegate(*this, &MyService::onPacket));
        _ps.setErrorHandler(etl::make_delegate(*this, &MyService::onError));
    }

    void loop() {
        // Update the serial communication using Arduino's Serial.
        _ps.update(Serial);
    }

    /**
     * @brief Handle incoming packets.
     * @param packet A safe view of the decoded data in the work_buffer.
     */
    void onPacket(etl::span<const uint8_t> packet) {
        // In this example, we just echo the packet back.
        // The .send() method will use the same _work_buffer to encode the data.
        _ps.send(Serial, packet);
    }

    /**
     * @brief Handle errors.
     * @param error The typed error code.
     */
    void onError(ErrorCode error) {
        // Simple error feedback
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }

private:
    // DETERMINISTIC MEMORY MANAGEMENT:
    // These buffers are allocated as part of the class instance in the DATA/BSS segment.
    
    // Internal buffer to store raw incoming bytes from the Stream.
    etl::array<uint8_t, 128> _rx_storage;
    
    // Buffer used for linearization, decoding, and encoding processes.
    // It must be large enough to hold the maximum expected frame (payload + CRC + overhead).
    etl::array<uint8_t, 256> _work_buffer;

    // The PacketSerial instance (using COBS and NoCRC by default).
    PacketSerial<COBS> _ps;
};

// Global instance of the service.
MyService service;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);
    
    // Start the service
    service.begin();
}

void loop() {
    service.loop();
}
