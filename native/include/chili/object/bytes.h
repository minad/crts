#pragma once

#include "../../chili.h"

/**
 * Reference to byte array with explicit size field.
 * We pass ChiBytesRef generally by value when calling inlined functions.
 */
typedef struct {
    uint32_t       size;
    const uint8_t* bytes;
} ChiBytesRef;

/**
 * Variable-size byte array with explicit size field
 */
typedef const struct CHI_PACKED {
    uint32_t      size;
    const uint8_t bytes[];
} ChiStaticBytes;

#define CHI_STATIC_BYTES(s) { .size = CHI_STRSIZE(s), .bytes = { s } }

CHI_INL CHI_WU uint32_t CHI_PRIVATE(chiByteSize)(const uint8_t* bytes, size_t sizeW) {
    uint32_t pad = bytes[sizeW * CHI_WORDSIZE - 1];
    CHI_ASSERT(pad < CHI_WORDSIZE);
    return CHI_WORDSIZE * (uint32_t)sizeW - 1 - pad;
}

// Store bytesize in trailing byte like Ocaml does it.
CHI_INL void CHI_PRIVATE(chiSetByteSize)(uint8_t* bytes, uint32_t size) {
    uint8_t pad = (uint8_t)(CHI_WORDSIZE - 1 - (size & (CHI_WORDSIZE - 1)));
    memset(bytes + size, 0, pad);
    bytes[size + pad] = pad;
}
