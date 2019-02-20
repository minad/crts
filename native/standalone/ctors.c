#include <bits/ctors.h>

typedef struct {
    void (*fn)(void*);
    void* arg;
} atexit_t;

static atexit_t* atexit_entry = 0;
static size_t atexit_capacity = 0, atexit_count = 0;

int __cxa_atexit(void (*fn)(void*), void* arg, void* dso) {
    (void)dso;
    if (atexit_count >= atexit_capacity) {
        atexit_capacity = atexit_capacity ? 2 * atexit_capacity : 8;
        atexit_entry = (atexit_t*)realloc(atexit_entry, sizeof (atexit_t) * atexit_capacity);
    }
    atexit_entry[atexit_count++] = (atexit_t){ fn, arg };
    return 0;
}

void __cxa_finalize(void* dso) {
    (void)dso;
    for (size_t i = atexit_count; i --> 0;)
        atexit_entry[i].fn(atexit_entry[i].arg);
    atexit_count = 0;
}
