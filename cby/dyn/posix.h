#include <dlfcn.h>

#define CBY_DYN_DEFAULT_HANDLE ((CbyDyn*)RTLD_DEFAULT)

static CbyDyn* cbyDynOpen(const char* name) {
    return (CbyDyn*)dlopen(name, RTLD_NOW);
}

static CbyDyn* cbyDynOpenFile(const char* file) {
    CHI_STRING_SINK(path);
    if (!strchr(file, '/'))
        chiSinkPuts(path, "./");
    chiSinkPuts(path, file);
    return cbyDynOpen(chiSinkCString(path));
}

static const char* cbyDynError(void) {
    const char* s = dlerror();
    return s ? s : "Unknown error"; // dlerror returns 0
}

static void cbyDynClose(CbyDyn* h) {
    dlclose(h);
}

static void* cbyDynSym(CbyDyn* h, const char* s) {
    return dlsym(h, s);
}
