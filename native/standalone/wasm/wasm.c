#include <wasm.h>

void wasm_sink_write_all(uint32_t fd, const void* buf, size_t size) {
    size_t written = 0;
    while (written < size)
        written += wasm_sink_write(fd, (const uint8_t*)buf + written, size - written);
}
