/**
 * Reference to byte array with explicit size field.
 */
typedef struct {
    const uint8_t* bytes;
    uint32_t       size;
} ChiBytesRef;

/**
 * Variable-size byte array with explicit size field
 */
typedef const struct CHI_PACKED {
    uint32_t      size;
    const uint8_t bytes[];
} ChiStaticBytes;

#define CHI_STATIC_BYTES(s) { .size = CHI_STRSIZE(s), .bytes = { s } }
