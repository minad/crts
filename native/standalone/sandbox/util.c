#include <sandbox.h>
#include <string.h>

void sb_stream_write_all(uint32_t id, const void* buf, size_t size) {
    size_t written = 0;
    while (written < size) {
        size_t n = sb_stream_write(id, (const uint8_t*)buf + written, size - written);
        if (n)
            written += n;
        else
            sb_yield(~0ULL, &(struct sb_mask){ .stream_write = 1U << id });
    }
}

void sb_stream_read_all(uint32_t id, void* buf, size_t size) {
    size_t read = 0;
    while (read < size) {
        size_t n = sb_stream_read(id, (uint8_t*)buf + read, size - read);
        if (n)
            read += n;
        else
            sb_yield(~0ULL, &(struct sb_mask){ .stream_read = 1U << id });
    }
}

void sb_stream_puts(uint32_t id, const char* s) {
    sb_stream_write_all(id, s, strlen(s));
}

void sb_die(const char* s) {
    sb_warn(s);
    sb_exit(1);
}

void sb_warn(const char* s) {
    sb_stream_puts(SB_STREAM_ERR, "sandbox: ");
    sb_stream_puts(SB_STREAM_ERR, s);
    sb_stream_puts(SB_STREAM_ERR, "\n");
}
