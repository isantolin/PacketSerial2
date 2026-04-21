/**
 * @file PacketSerialReverseEchoClass.ino
 * @brief Reverse echo example using classes and ETL (v2.2).
 */

#include <stdint.h>
#include <etl/array.h>
#include <etl/algorithm.h>
#include <PacketSerial.h>
#include <Codecs/COBS.h>

using namespace PacketSerial2;

class MyEchoService {
public:
    MyEchoService() : _ps(_rx_storage, _work_buffer) {}

    void begin() {
        _ps.setPacketHandler(etl::make_delegate(*this, &MyEchoService::onPacketReceived));
    }

    void update() {
        _ps.update(Serial);
    }

    void onPacketReceived(etl::span<const uint8_t> packet) {
        if (packet.size() > _reverse_buffer.size()) return;

        // Pure ETL algorithm
        etl::reverse_copy(packet.begin(), packet.end(), _reverse_buffer.begin());
        
        _ps.send(Serial, etl::span<const uint8_t>(_reverse_buffer.data(), packet.size()));
    }

private:
    etl::array<uint8_t, 128> _rx_storage;
    etl::array<uint8_t, 256> _work_buffer;
    etl::array<uint8_t, 256> _reverse_buffer;

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
