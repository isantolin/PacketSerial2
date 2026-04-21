# Background

PacketSerial was originally developed to provide a simple, reliable way to send packets of data over a serial connection. The original version relied on dynamic memory allocation and was tightly coupled with the Arduino environment.

## The Evolution to v2.2

With the rise of safety-critical embedded systems and the need for higher performance on resource-constrained hardware, PacketSerial has evolved into a sophisticated industrial-grade engine.

### Modern C++ and ETL

Starting with version 2.0, the library moved to a **Zero-Heap** architecture. By integrating the **Embedded Template Library (ETL)**, we achieved deterministic behavior and absolute control over RAM usage.

### Pure Functional Design (v2.2)

The v2.2 release marks the transition to a **Pure ETL** design. Every cycle of the CPU is optimized by delegating flow control to ETL algorithms (`etl::algorithm`), eliminating manual loops and raw pointer arithmetic. This ensures that the code is not only faster but also mathematically more predecible and easier to certify for safety standards like **SIL-2**.

### Industrial Strength

Today, PacketSerial2 provides:
1.  **Framing**: Efficient COBS and SLIP implementations.
2.  **Integrity**: Template-based CRC injection.
3.  **Safety**: Atomic lock policies and watchdog hooks.
4.  **Performance**: Block-based serial transmission optimized for 8-bit and 32-bit microcontrollers.
