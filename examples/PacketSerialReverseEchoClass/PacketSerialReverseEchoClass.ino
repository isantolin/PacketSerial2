/**
 * @file PacketSerialReverseEchoClass.ino
 * @brief Reverse echo example inside a class (v2.1).
 * 
 * Demonstrates:
 * 1. Safe encapsulation in an OOP environment.
 * 2. Atomic sections for high-reliability applications.
 */

#include <stdint.h>
#include <etl/array.h>
#include <PacketSerial.h>
#include <Codecs/COBS.h>

using namespace PacketSerial2;

class MyEchoService {
public:
    /**
     * @brief Constructor with ArduinoAtomicLock (v2.1).
     */
    MyEchoService() : _ps(_rx_storage, _work_buffer) {}

    void begin() {
        _ps.setPacketHandler(etl::make_delegate(*this, &MyEchoService::onPacketReceived));
    }

    void update() {
        _ps.update(Serial);
    }

    void onPacketReceived(etl::span<const uint8_t> packet) {
        uint8_t reversed[packet.size()];
        for (size_t i = 0; i < packet.size(); i++) {
            reversed[i] = packet[packet.size() - 1 - i];
        }
        _ps.send(Serial, etl::span<const uint8_t>(reversed, packet.size()));
    }

private:
    etl::array<uint8_t, 128> _rx_storage;
    etl::array<uint8_t, 256> _work_buffer;

    // We can also pass policies as template parameters here.
    PacketSerial<COBS, NoCRC, ArduinoAtomicLock> _ps;
};

MyEchoService echoService;

void setup() {
    Serial.begin(115200);
    echoService.begin();
}

void loop() {
    echoService.update();
}
