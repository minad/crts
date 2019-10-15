static void* chi_system_malloc(size_t size) { return malloc(size); }
static void* chi_system_calloc(size_t count, size_t size) { return calloc(count, size); }
static void* chi_system_realloc(void* mem, size_t size) { return realloc(mem, size); }
static void chi_system_free(void* mem) { return free(mem); }
