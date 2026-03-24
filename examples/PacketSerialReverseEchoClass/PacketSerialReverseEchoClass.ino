/**
 * @file PacketSerialReverseEchoClass.ino
 * @brief Reverse echo example inside a class using Modern PacketSerial (v2.0).
 * 
 * This example shows:
 * 1. Encapsulating PacketSerial inside a class.
 * 2. Deterministic memory (buffers as class members).
 * 3. etl::delegate for class method callbacks.
 */

#include <stdint.h>
#include <etl/array.h>
#include <PacketSerial.h>
#include <Codecs/COBS.h>

// Use the library namespace
using namespace PacketSerial2;

class MyEchoService {
public:
    /**
     * @brief Initialize PacketSerial with internal buffers.
     */
    MyEchoService() : _ps(_rx_storage, _work_buffer) {}

    void begin() {
        // Set the packet handler using etl::make_delegate to bind 'this' instance.
        // No need for static methods or 'void* sender' anymore.
        _ps.setPacketHandler(etl::make_delegate(*this, &MyEchoService::onPacketReceived));
    }

    void update() {
        // We pass the Serial stream to update the engine.
        _ps.update(Serial);
    }

    /**
     * @brief Handle incoming packets (class method).
     * @param packet A safe view of the decoded data.
     */
    void onPacketReceived(etl::span<const uint8_t> packet) {
        // Reverse the packet in-place using a local buffer.
        uint8_t reversed[packet.size()];
        for (size_t i = 0; i < packet.size(); i++) {
            reversed[i] = packet[packet.size() - 1 - i];
        }

        // Send the reversed data back.
        _ps.send(Serial, etl::span<const uint8_t>(reversed, packet.size()));
    }

private:
    // ALLOCATE MEMORY AS CLASS MEMBERS:
    // These buffers are part of the MyEchoService instance (no heap).
    etl::array<uint8_t, 128> _rx_storage;
    etl::array<uint8_t, 256> _work_buffer;

    // Instance of the PacketSerial engine using COBS codec.
    PacketSerial<COBS> _ps;
};

// Global instance of the service.
MyEchoService echoService;

void setup() {
    Serial.begin(115200);
    echoService.begin();
}

void loop() {
    // Service logic update.
    echoService.update();
}
