# Troubleshooting PacketSerial 2.0

## ❌ Compilation Errors

### "etl/array.h (or span.h, delegate.h): No such file or directory"
**Solution**: PacketSerial 2.0 requires the **Embedded Template Library (ETL)**. Install it via the Arduino Library Manager (search for "Embedded Template Library") or download it from [etlcpp.com](https://www.etlcpp.com/).

### "namespace 'PacketSerial2' does not exist"
**Solution**: Ensure you are including `<PacketSerial.h>` and using the `PacketSerial2::` namespace or a `using namespace PacketSerial2;` statement.

---

## ⚠️ Runtime Errors

### `ErrorCode::BufferOverflow`
**Cause**: The internal circular buffer (`rx_storage`) or the `work_buffer` you provided is full. This happens when:
1.  Your packets are larger than the buffers you allocated.
2.  You are receiving data faster than you are calling `.update()`.
3.  The packet delimiter (e.g., `0x00`) is missing or corrupt.

**Solution**: 
- Increase the size of your `etl::array`s.
- Call `ps.update(Serial)` more frequently in your main loop.

### `ErrorCode::InvalidChecksum` (CRC Errors)
**Cause**: A packet was received, but the CRC validation failed.
**Solution**:
1.  Check for electrical noise on your serial lines (RS-232, RS-485, etc.).
2.  Ensure both the sender and receiver are using the **same CRC algorithm** (e.g., both use `etl::crc16`).
3.  Verify the baud rate and cable integrity.

### `ErrorCode::MalformedFrame` (COBS/SLIP Errors)
**Cause**: The incoming data does not follow the encoding protocol (e.g., COBS detected a 0x00 where it shouldn't be).
**Solution**: This is often caused by line noise or desynchronization. PacketSerial 2.0 will automatically attempt to re-sync at the next delimiter.

### Packets are not being received
**Solution**:
1.  Check your hardware connections (TX/RX).
2.  Ensure you have registered a `PacketHandler` using `ps.setPacketHandler()`.
3.  Verify that your sender is actually sending the **packet delimiter** (e.g., `0x00` for COBS or `0xC0` for SLIP).
