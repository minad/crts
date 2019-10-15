#include <psapi.h>

#define CBY_DYN_DEFAULT_HANDLE ((CbyDyn*)-1)

static CbyDyn* cbyDynOpen(const char* name) {
    CHI_AUTO_FREE(nameW, chiAllocUtf8To16(name));
    return (CbyDyn*)LoadLibraryExW(nameW, 0, 0);
}

static CbyDyn* cbyDynOpenFile(const char* file) {
    CHI_STRING_SINK(path);
    if (!strchr(file, '\\'))
        chiSinkPuts(path, ".\\");
    chiSinkPuts(path, file);
    return cbyDynOpen(chiSinkCString(path));
}

static const char* cbyDynError(void) {
    static _Thread_local char buf[64];
    return chiWinErrorString(GetLastError(), buf, sizeof (buf)) ? buf : "Unknown error";
}

static void* cbyDynSym(CbyDyn* h, const char* s) {
    if (h == CBY_DYN_DEFAULT_HANDLE) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                   FALSE, GetCurrentProcessId());
        if (!hProc)
            return 0;

        void* p = 0;
        DWORD needed;
        HMODULE mods[256];
        if (EnumProcessModules(hProc, mods, sizeof(mods), &needed)) {
            for (size_t i = 0; i < needed / sizeof(HMODULE); ++i) {
                void* q = GetProcAddress(mods[i], s);
                if (q) {
                    p = q;
                    break;
                }
            }
        }

        CloseHandle(hProc);
        return p;
    }

    return GetProcAddress((HMODULE)h, s);
}

static void cbyDynClose(CbyDyn* h) {
    CloseHandle(h);
}
