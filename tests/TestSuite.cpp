#include <stdint.h>
#include <iostream>
#include <iomanip>
#include <etl/array.h>
#include <etl/span.h>
#include <etl/algorithm.h>
#include <etl/crc16.h>
#include <etl/crc32.h>

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

// --- Global Flags for Callbacks ---
static bool packet_received = false;
static size_t last_packet_size = 0;
static ErrorCode last_error = ErrorCode::None;

void reset_flags() {
    packet_received = false;
    last_packet_size = 0;
    last_error = ErrorCode::None;
}

void onPacket(etl::span<const uint8_t> packet) {
    packet_received = true;
    last_packet_size = packet.size();
}

void onError(ErrorCode error) {
    last_error = error;
}

// --- Codec Roundtrip Tests (Low Level) ---

bool test_cobs_roundtrip() {
    COBS codec;
    uint8_t raw[] = { 0x01, 0x00, 0x02, 0x00, 0x03 };
    etl::array<uint8_t, 5> input;
    etl::copy(raw, raw + 5, input.begin());
    etl::array<uint8_t, 16> enc, dec;

    auto res_e = codec.encode(input, enc);
    if (!res_e.has_value()) return false;

    auto res_d = codec.decode(etl::span<const uint8_t>(enc.data(), res_e.value()), dec);
    return res_d.has_value() && res_d.value() == 5 && etl::equal(input.begin(), input.end(), dec.begin());
}

bool test_slip_roundtrip() {
    SLIP codec;
    uint8_t raw[] = { 0x01, 0xC0, 0xDB, 0x02 };
    etl::array<uint8_t, 4> input;
    etl::copy(raw, raw + 4, input.begin());
    etl::array<uint8_t, 16> enc, dec;

    auto res_e = codec.encode(input, enc);
    if (!res_e.has_value()) return false;

    auto res_d = codec.decode(etl::span<const uint8_t>(enc.data(), res_e.value()), dec);
    return res_d.has_value() && res_d.value() == 4 && etl::equal(input.begin(), input.end(), dec.begin());
}

// --- High Level Integration Tests ---

bool test_ps_cobs_no_crc() {
    etl::array<uint8_t, 64> rx; etl::array<uint8_t, 128> work;
    PacketSerial<COBS, NoCRC> ps(rx, work);
    MockStream<256> stream;
    reset_flags();
    ps.setPacketHandler(etl::make_delegate<onPacket>());

    uint8_t data[] = { 1, 2, 3 };
    ps.send(stream, etl::span<const uint8_t>(data, 3));
    stream.injectIncomingData(etl::span<const uint8_t>(stream.getTransmittedData().data(), stream.getTransmittedData().size()));
    ps.update(stream);

    return packet_received && last_packet_size == 3;
}

bool test_ps_cobs_crc32() {
    etl::array<uint8_t, 64> rx; etl::array<uint8_t, 128> work;
    PacketSerial<COBS, etl::crc32> ps(rx, work);
    MockStream<256> stream;
    reset_flags();
    ps.setPacketHandler(etl::make_delegate<onPacket>());

    uint8_t data[] = { 0xAA, 0xBB, 0xCC, 0xDD };
    ps.send(stream, etl::span<const uint8_t>(data, 4));
    stream.injectIncomingData(etl::span<const uint8_t>(stream.getTransmittedData().data(), stream.getTransmittedData().size()));
    ps.update(stream);

    return packet_received && last_packet_size == 4;
}

bool test_ps_slip_no_crc() {
    etl::array<uint8_t, 64> rx; etl::array<uint8_t, 128> work;
    PacketSerial<SLIP, NoCRC> ps(rx, work);
    MockStream<256> stream;
    reset_flags();
    ps.setPacketHandler(etl::make_delegate<onPacket>());

    uint8_t data[] = { 0xC0, 0x01, 0xDB };
    ps.send(stream, etl::span<const uint8_t>(data, 3));
    stream.injectIncomingData(etl::span<const uint8_t>(stream.getTransmittedData().data(), stream.getTransmittedData().size()));
    ps.update(stream);

    return packet_received && last_packet_size == 3;
}

bool test_ps_slip_crc16() {
    etl::array<uint8_t, 64> rx; etl::array<uint8_t, 128> work;
    PacketSerial<SLIP, etl::crc16> ps(rx, work);
    MockStream<256> stream;
    reset_flags();
    ps.setPacketHandler(etl::make_delegate<onPacket>());

    uint8_t data[] = { 10, 20, 30 };
    ps.send(stream, etl::span<const uint8_t>(data, 3));
    stream.injectIncomingData(etl::span<const uint8_t>(stream.getTransmittedData().data(), stream.getTransmittedData().size()));
    ps.update(stream);

    return packet_received && last_packet_size == 3;
}

bool test_ps_invalid_crc() {
    etl::array<uint8_t, 64> rx; etl::array<uint8_t, 128> work;
    PacketSerial<COBS, etl::crc16> ps(rx, work);
    MockStream<256> stream;
    reset_flags();
    ps.setPacketHandler(etl::make_delegate<onPacket>());
    ps.setErrorHandler(etl::make_delegate<onError>());

    uint8_t data[] = { 1, 2, 3, 4 };
    ps.send(stream, etl::span<const uint8_t>(data, 4));

    // Corrupt one byte of the transmitted data (the CRC part or payload)
    auto& tx = const_cast<etl::vector<uint8_t, 256>&>(stream.getTransmittedData());
    tx[2] ^= 0xFF; 

    stream.injectIncomingData(etl::span<const uint8_t>(tx.data(), tx.size()));
    ps.update(stream);

    return !packet_received && last_error == ErrorCode::InvalidChecksum;
}

// --- Main Runner ---

int main() {
    std::cout << "PacketSerial2 Extended Test Suite" << std::endl;
    std::cout << "=================================" << std::endl;

    etl::array<TestResult, 7> results = {{
        RUN_TEST(test_cobs_roundtrip),
        RUN_TEST(test_slip_roundtrip),
        RUN_TEST(test_ps_cobs_no_crc),
        RUN_TEST(test_ps_cobs_crc32),
        RUN_TEST(test_ps_slip_no_crc),
        RUN_TEST(test_ps_slip_crc16),
        RUN_TEST(test_ps_invalid_crc)
    }};

    bool all_passed = true;
    etl::for_each(results.begin(), results.end(), [&](const TestResult& r) {
        print_result(r);
        if (!r.success) all_passed = false;
    });

    std::cout << "=================================" << std::endl;
    if (all_passed) {
        std::cout << "ALL TESTS PASSED" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED" << std::endl;
    }

    return all_passed ? 0 : 1;
}
