# Design Background

**PacketSerial 2.0** is a complete evolution of the original library. While the goal remains the same—to provide reliable, packet-based communication over a byte-stream—the internal philosophy has shifted towards **Industrial Grade Robustness**.

## Why Version 2.0?

Traditional Arduino libraries often rely on `malloc/free`, `new/delete`, or the Standard Template Library (STL), which lead to **Heap Fragmentation**. In high-integrity or long-running systems (like industrial sensors, robotics, or aerospace), heap fragmentation is a primary source of non-deterministic failures.

### Design Pillars

1.  **Zero-Heap (Deterministic Memory)**: Every single byte of RAM used by PacketSerial is provided by the user at compile-time. This ensures that your system's memory usage is perfectly predictable and won't crash after weeks of uptime due to fragmentation.
2.  **Functional Safety**: We use `etl::span` and `etl::expected` to ensure that data access is always safe and errors (like corrupt packets or overflows) are handled explicitly and type-safely.
3.  **Zero-Cost Abstractions**: By using the **Curiously Recurring Template Pattern (CRTP)** and `if constexpr`, the compiler can optimize the code specifically for your configuration. For example, if you don't use a CRC, the CRC-related code is completely removed by the linker.
4.  **ETL Native Integration**: Instead of reinventing the wheel, we leverage the battle-tested **Embedded Template Library (ETL)**, which is the industry standard for modern, safety-critical C++ on microcontrollers.

## How it Works

The PacketSerial engine acts as a **State Machine** that consumes bytes from a stream. It stores them in a circular buffer (`rx_storage`) until a delimiter (like `0x00`) is found. 

Once a delimiter is found, it "linearizes" the frame into the `work_buffer`, validates the integrity (CRC), and decodes the payload (COBS/SLIP). Finally, it triggers an `etl::delegate` to your application code.

This entire process happens **without a single dynamic allocation**.
