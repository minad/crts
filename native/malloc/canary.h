static void* checkCanary(void* p) {
    if (CHI_POISON_ENABLED && p) {
        uint8_t* q = (uint8_t*)p - 16;
        CHI_ASSERT(chiPeekUInt64(q + 8) == CHI_POISON_MALLOC_BEGIN);
        CHI_ASSERT(chiPeekUInt64(q + 16 + chiPeekUInt64(q)) == CHI_POISON_MALLOC_END);
        return q;
    }
    return p;
}

static void* writeCanary(void* p, size_t size) {
    if (CHI_POISON_ENABLED && p) {
        uint8_t* q = (uint8_t*)p;
        chiPokeUInt64(q, size);
        chiPokeUInt64(q + 8, CHI_POISON_MALLOC_BEGIN);
        chiPokeUInt64(q + 16 + size, CHI_POISON_MALLOC_END);
        return q + 16;
    }
    return p;
}

static size_t addCanary(size_t size) {
    return CHI_POISON_ENABLED ? size + 24 : size;
}

static void* chi_malloc(size_t size) {
    return writeCanary(chi_system_malloc(addCanary(size)), size);
}

static void* chi_calloc(size_t count, size_t size) {
    if (CHI_POISON_ENABLED) {
        size_t n;
        if (__builtin_mul_overflow(count, size, &n))
            return 0;
        return writeCanary(chi_system_calloc(1, addCanary(n)), n);
    }
    return chi_system_calloc(count, size);
}

static void* chi_realloc(void* p, size_t size) {
    return writeCanary(chi_system_realloc(checkCanary(p), size ? addCanary(size) : 0), size);
}

static void chi_free(void* p) {
    chi_system_free(checkCanary(p));
}
