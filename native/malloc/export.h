void* malloc(size_t size) { return chi_malloc(size); }
void* calloc(size_t count, size_t size) { return chi_calloc(count, size); }
void* realloc(void* mem, size_t size) { return chi_realloc(mem, size); }
void free(void* mem) { return chi_free(mem); }
