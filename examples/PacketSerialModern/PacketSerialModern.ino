/**
 * @file PacketSerial2Modern.ino
 * @brief Modern usage example for PacketSerial2.
 * 
 * This example demonstrates:
 * 1. Zero-Heap/No-STL architecture.
 * 2. etl::delegate for class-method callbacks.
 * 3. etl::span for safe buffer access.
 * 4. COBS encoding with static memory.
 */

#include <stdint.h>
#include <PacketSerial.h>
#include <Codecs/COBS.h>

// Use the library namespace
using namespace PacketSerial2;

class MyService {
public:
    void begin() {
        // Initialize the library with etl::delegates.
        // etl::make_delegate binds the method 'onPacket' of this instance 'this'.
        _ps.setPacketHandler(etl::make_delegate(*this, &MyService::onPacket));
        _ps.setErrorHandler(etl::make_delegate(*this, &MyService::onError));
    }

    void loop() {
        // Update the serial communication.
        // Works with any Arduino Stream (Serial, SoftwareSerial, EthernetClient, etc.)
        _ps.update(Serial);
    }

    /**
     * @brief Handle incoming packets.
     * @param packet A safe view of the decoded data.
     */
    void onPacket(etl::span<const uint8_t> packet) {
        // Process the packet (e.g., check command byte)
        if (packet.size() > 0) {
            uint8_t command = packet[0];
            
            // In this example, we just echo the packet back.
            _ps.send(Serial, packet);
        }
    }

    /**
     * @brief Handle errors.
     * @param error The typed error code.
     */
    void onError(ErrorCode error) {
        // Blink internal LED on error
        digitalWrite(LED_BUILTIN, HIGH);
        
        // ErrorCode is an enum class for safety
        if (error == ErrorCode::BufferOverflow) {
            // Take action on overflow (e.g., clear state)
        }
    }

private:
    // Template parameters: <Codec, RxBufferSize, PacketMarker, CRCType>
    // - COBS: Uses 0x00 as delimiter.
    // - 128: Static buffer size in RAM.
    PacketSerial2<COBS, 128> _ps;
};

// Instantiate the service
MyService service;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);
    
    // Start our modern service
    service.begin();
}

void loop() {
    service.loop();
}
