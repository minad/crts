/**
 * Minimal Hypervisor API based on the Solo5 API.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

enum {
    SB_STREAM_IN,
    SB_STREAM_OUT,
    SB_STREAM_ERR,
    SB_STREAM_AUX,
};

enum {
    SB_MODE_READ  = 1U,
    SB_MODE_WRITE = 2U,
    SB_MODE_RW    = SB_MODE_READ | SB_MODE_WRITE,
};

struct sb_net {
    uint8_t mac[6];
    size_t  mtu;
};

struct sb_block {
    uint64_t size;
    size_t   unit;
};

struct sb_heap {
    uintptr_t start;
    size_t    size;
};

struct sb_stream {
    uint32_t mode;
};

struct sb {
    struct sb_heap    heap;
    struct sb_block*  block;
    struct sb_net*    net;
    struct sb_stream* stream;
    uint32_t          block_count;
    uint32_t          net_count;
    uint32_t          stream_count; // always >= 3
};

struct sb_mask {
    uint32_t net_read;
    uint32_t stream_write;
    uint32_t stream_read;
};

extern const struct sb* sb_info;
int sb_main(int argc, char** argv);
_Noreturn void sb_exit(int status);
uint64_t sb_clock_monotonic(void);
uint64_t sb_clock_wall(void);
void sb_yield(uint64_t ns, struct sb_mask*);
size_t sb_stream_write(uint32_t id, const void *buf, size_t size);
size_t sb_stream_read(uint32_t id, void *buf, size_t size);
void sb_net_write(uint32_t id, const void *buf, size_t size);
size_t sb_net_read(uint32_t id, void *buf, size_t size);
void sb_block_write(uint32_t id, uint64_t off, const void *buf, size_t size); // must be aligned by unit
void sb_block_read(uint32_t id, uint64_t off, void *buf, size_t size); // must be aligned by unit
void sb_block_discard(uint32_t id, uint64_t begin, uint64_t end); // must be aligned by unit
void sb_block_sync(uint32_t id);

// Non primitives, utilities
void sb_stream_read_all(uint32_t id, void* buf, size_t size);
void sb_stream_write_all(uint32_t id, const void *buf, size_t size);
void sb_stream_puts(uint32_t id, const char* s);
void sb_die(const char* s);
void sb_warn(const char* s);
