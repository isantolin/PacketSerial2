#include <stdint.h>
#include <iostream>
#include <iomanip>
#include <etl/array.h>
#include <etl/span.h>
#include <etl/algorithm.h>
#include <etl/crc16.h>

#include "../src/PacketSerial.h"
#include "../src/Codecs/COBS.h"
#include "../src/Codecs/SLIP.h"
#include "../src/Mocks/MockStream.h"

using namespace PacketSerial2;
using namespace PacketSerial2::Mocks;

// --- Test Infrastructure ---

struct TestResult {
    const char* name;
    bool success;
};

#define RUN_TEST(test_func) { #test_func, test_func() }

void print_result(const TestResult& result) {
    std::cout << std::left << std::setw(40) << result.name 
              << ": " << (result.success ? "PASSED" : "FAILED") << std::endl;
}

// --- COBS Tests ---

bool test_cobs_roundtrip() {
    COBS codec;
    uint8_t raw_data[] = { 0x01, 0x02, 0x00, 0x03, 0x00, 0x04 };
    etl::array<uint8_t, sizeof(raw_data)> input;
    etl::copy(raw_data, raw_data + sizeof(raw_data), input.begin());
    
    etl::array<uint8_t, 16> encoded;
    etl::array<uint8_t, 16> decoded;

    auto enc_res = codec.encode(input, encoded);
    if (!enc_res.has_value()) return false;

    auto dec_res = codec.decode(etl::span<const uint8_t>(encoded.data(), enc_res.value()), decoded);
    if (!dec_res.has_value() || dec_res.value() != input.size()) return false;

    return etl::equal(input.begin(), input.end(), decoded.begin());
}

// --- SLIP Tests ---

bool test_slip_roundtrip() {
    SLIP codec;
    uint8_t raw_data[] = { 0x01, 0xC0, 0xDB, 0x02 };
    etl::array<uint8_t, sizeof(raw_data)> input;
    etl::copy(raw_data, raw_data + sizeof(raw_data), input.begin());

    etl::array<uint8_t, 16> encoded;
    etl::array<uint8_t, 16> decoded;

    auto enc_res = codec.encode(input, encoded);
    if (!enc_res.has_value()) return false;

    auto dec_res = codec.decode(etl::span<const uint8_t>(encoded.data(), enc_res.value()), decoded);
    if (!dec_res.has_value() || dec_res.value() != input.size()) return false;

    return etl::equal(input.begin(), input.end(), decoded.begin());
}

// --- PacketSerial Tests ---

bool test_packet_serial_basic() {
    etl::array<uint8_t, 64> rx_storage;
    etl::array<uint8_t, 128> work_buffer;
    PacketSerial<COBS> ps(rx_storage, work_buffer);
    MockStream<256> stream;

    bool packet_received = false;
    size_t received_size = 0;

    auto handler = etl::make_delegate([&](etl::span<const uint8_t> packet) {
        packet_received = true;
        received_size = packet.size();
    });
    ps.setPacketHandler(handler);

    uint8_t payload[] = { 0xAA, 0xBB, 0xCC };
    ps.send(stream, etl::span<const uint8_t>(payload, sizeof(payload)));

    // MockStream now contains the encoded packet.
    // Inject it back to simulate reception.
    stream.injectIncomingData(etl::span<const uint8_t>(stream.getTransmittedData().data(), stream.getTransmittedData().size()));
    
    ps.update(stream);

    return packet_received && received_size == sizeof(payload);
}

bool test_packet_serial_crc() {
    etl::array<uint8_t, 64> rx_storage;
    etl::array<uint8_t, 128> work_buffer;
    PacketSerial<COBS, etl::crc16> ps(rx_storage, work_buffer);
    MockStream<256> stream;

    bool packet_received = false;
    auto handler = etl::make_delegate([&](etl::span<const uint8_t>) {
        packet_received = true;
    });
    ps.setPacketHandler(handler);

    uint8_t payload[] = { 0x01, 0x02, 0x03, 0x04 };
    ps.send(stream, etl::span<const uint8_t>(payload, sizeof(payload)));

    // Inject back
    stream.injectIncomingData(etl::span<const uint8_t>(stream.getTransmittedData().data(), stream.getTransmittedData().size()));
    
    ps.update(stream);

    return packet_received;
}

// --- Main Runner ---

int main() {
    std::cout << "PacketSerial2 Unit Test Suite" << std::endl;
    std::cout << "=============================" << std::endl;

    etl::array<TestResult, 4> results = {{
        RUN_TEST(test_cobs_roundtrip),
        RUN_TEST(test_slip_roundtrip),
        RUN_TEST(test_packet_serial_basic),
        RUN_TEST(test_packet_serial_crc)
    }};

    bool all_passed = true;
    etl::for_each(results.begin(), results.end(), [&](const TestResult& r) {
        print_result(r);
        if (!r.success) all_passed = false;
    });

    std::cout << "=============================" << std::endl;
    if (all_passed) {
        std::cout << "ALL TESTS PASSED" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED" << std::endl;
    }

    return all_passed ? 0 : 1;
}
