/**
 * @file PacketSerialModern.ino
 * @brief Modern usage example for PacketSerial2 (v2.2).
 */

#include <stdint.h>
#include <PacketSerial.h>
#include <Codecs/COBS.h>
#include <etl/array.h>

using namespace PacketSerial2;

class MyService {
public:
    // v2.2: Initialize with ETL arrays.
    MyService() : _ps(_rx_storage, _work_buffer) {}

    void begin() {
        _ps.setPacketHandler(etl::make_delegate(*this, &MyService::onPacket));
        _ps.setErrorHandler(etl::make_delegate(*this, &MyService::onError));
    }

    void loop() {
        _ps.update(Serial);
    }

    void onPacket(etl::span<const uint8_t> packet) {
        if (!packet.empty()) {
            _ps.send(Serial, packet);
        }
    }

    void onError(ErrorCode error) {
        digitalWrite(LED_BUILTIN, HIGH);
    }

private:
    // v2.2: Pure ETL static memory management.
    etl::array<uint8_t, 128> _rx_storage;
    etl::array<uint8_t, 256> _work_buffer;
    PacketSerial<COBS> _ps;
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
