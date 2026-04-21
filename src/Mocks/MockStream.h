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
        size_t initial_size = _tx_buffer.size();
        etl::for_each(buffer, buffer + size, [this](uint8_t byte) {
            this->write(byte);
        });
        return _tx_buffer.size() - initial_size;
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

    /**
     * @brief Inject raw data to simulate incoming bytes from hardware.
     */
    void injectIncomingData(etl::span<const uint8_t> data) {
        etl::for_each(data.begin(), data.end(), [this](uint8_t b) {
            if (!this->_rx_buffer.full()) {
                this->_rx_buffer.push_back(b);
            }
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
