/**
 * @file PacketModern.ino
 * @brief Modern usage example with SIL-2 safety (v2.1).
 * 
 * This is the recommended configuration for new projects.
 */

#include <stdint.h>
#include <etl/array.h>
#include <PacketSerial.h>
#include <Codecs/COBS.h>

using namespace PacketSerial2;

class MyService {
public:
    /**
     * @brief Initialize with all safety features.
     */
    MyService() : _ps(_rx_storage, _work_buffer) {}

    void begin() {
        _ps.setPacketHandler(etl::make_delegate(*this, &MyService::onPacket));
        _ps.setErrorHandler(etl::make_delegate(*this, &MyService::onError));
    }

    void loop() {
        _ps.update(Serial);
    }

    void onPacket(etl::span<const uint8_t> packet) {
        // Echo
        _ps.send(Serial, packet);
    }

    void onError(ErrorCode error) {
        // Handle overflow, CRC, etc.
    }

private:
    etl::array<uint8_t, 128> _rx_storage;
    etl::array<uint8_t, 256> _work_buffer;

    // Use ArduinoAtomicLock for ISR safety.
    PacketSerial<COBS, NoCRC, ArduinoAtomicLock> _ps;
};

MyService service;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);
    service.begin();
}

void loop() {
    service.loop();
}
