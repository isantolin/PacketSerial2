#pragma once

#include <stdint.h>
#include <stddef.h>
#include <etl/vector.h>
#include <etl/deque.h>
#include <etl/algorithm.h>

namespace PacketSerial2 {
namespace Mocks {

/**
 * @brief Platform-agnostic Mock Stream for unit testing.
 * 
 * This class fulfills the "Hardware Abstraction" premise at 100%, 
 * allowing developers to test protocol logic on PC (Linux/Windows) 
 * without Arduino hardware.
 */
template <size_t Capacity>
class MockStream {
public:
    /**
     * @brief Mimics Arduino's Stream::write.
     */
    size_t write(uint8_t data) {
        if (_tx_buffer.full()) return 0;
        _tx_buffer.push_back(data);
        return 1;
    }

    /**
     * @brief Mimics Arduino's Stream::write for bulk data.
     */
    size_t write(const uint8_t* buffer, size_t size) {
        size_t written = 0;
        etl::for_each(buffer, buffer + size, [this, &written](uint8_t byte) {
            if (this->write(byte) != 0) {
                written++;
            }
        });
        return written;
    }

    /**
     * @brief Mimics Arduino's Stream::read.
     */
    int read() {
        if (_rx_buffer.empty()) return -1;
        uint8_t data = _rx_buffer.front();
        _rx_buffer.pop_front();
        return data;
    }

    /**
     * @brief Mimics Arduino's Stream::available.
     */
    int available() {
        return static_cast<int>(_rx_buffer.size());
    }

    // --- Test Injection Methods ---

    /**
     * @brief Inject raw data to simulate incoming bytes from hardware.
     */
    void injectIncomingData(etl::span<const uint8_t> data) {
        const size_t space = _rx_buffer.max_size() - _rx_buffer.size();
        const size_t count = (data.size() < space) ? data.size() : space;
        
        etl::for_each(data.begin(), data.begin() + count, [this](uint8_t b) {
            _rx_buffer.push_back(b);
        });
    }

    /**
     * @brief Get the data that the application tried to send.
     */
    const etl::vector<uint8_t, Capacity>& getTransmittedData() const {
        return _tx_buffer;
    }

    void clear() {
        _tx_buffer.clear();
        _rx_buffer.clear();
    }

private:
    etl::vector<uint8_t, Capacity> _tx_buffer;
    etl::deque<uint8_t, Capacity> _rx_buffer;
};

} // namespace Mocks
} // namespace PacketSerial2
