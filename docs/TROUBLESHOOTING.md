# Troubleshooting PacketSerial v2.2

## ❌ Compilation Errors

### "etl/array.h (or span.h, delegate.h): No such file or directory"
**Solution**: PacketSerial requires the **Embedded Template Library (ETL)**. Install it via the Arduino Library Manager.

### "no matching function for call to PacketSerial"
**Solution**: Version 2.2 requires explicit buffer injection in the constructor. Ensure you are passing two `etl::span` or `etl::array` objects.

---

## 🧪 Unit Testing

If you suspect a logic bug, you can run the PC-based unit test suite:
1. Navigate to the `tests/` directory.
2. Run `make`.
3. Verify that all COBS/SLIP/CRC tests pass.

---

## ⚠️ Runtime Errors

### `ErrorCode::BufferOverflow`
**Cause**: The internal circular buffer (`rx_storage`) or the `work_buffer` you provided is full. 
**Solution**: 
- Increase the size of your `etl::array`s.
- Call `ps.update(Serial)` more frequently.

### `ErrorCode::InvalidChecksum`
**Cause**: Data corruption or CRC algorithm mismatch.
**Solution**:
1. Check electrical noise.
2. Ensure both sides use the **same CRC type**.

### Packets are not being received
**Solution**:
1. Verify the **Packet Marker** (delimiter).
2. Ensure `ps.update()` is being called in `loop()`.
3. Check that a handler is registered with `ps.setPacketHandler()`.
