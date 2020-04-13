/*
 * The Windows port is mostly functional with a few minor issues.
 *
 * We are relying on Windows Vista for condition variables, rwlocks and InitOnceExecuteOnce.
 * No newer APIs are used.
 *
 * The Windows port can be compiled by mingw64. Unfortunately
 * native thread local storage (available since Vista)
 * is not yet supported by mingw64. Thread local variables
 * are accessed via a call to __emutls_get_address.
 */

#include <process.h>
#include <psapi.h>
#include <sysinfoapi.h>
#include "../mem.h"
#include "../sink.h"
#include "interrupt.h"

static HANDLE timer;
static uint64_t clockScale;

// We use the user defined bit to encode windows errors in errno
#define CHI_WIN_ERROR (1 << 29)

static void winSetErr(void) {
    errno = CHI_WIN_ERROR | (int)GetLastError();
}

static void winErr(const char* call) {
    winSetErr();
    chiSysErr(call);
}

static ChiNanos filetimeToNanos(FILETIME f) {
    return chiNanos((((uint64_t)f.dwHighDateTime << 32) | f.dwLowDateTime) * 100);
}

ChiNanos chiClockCpu(void) {
    FILETIME creation, exit, system, user;
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit,
                         &system, &user))
        winErr("GetProcessTimes");
    return chiNanosAdd(filetimeToNanos(system), filetimeToNanos(user));
}

ChiNanos chiClockMonotonicFine(void) {
    LARGE_INTEGER counter;
    if (!QueryPerformanceCounter(&counter))
        winErr("QueryPerformanceCounter");
    return chiNanos((uint64_t)counter.QuadPart * clockScale);
}

ChiNanos chiCondTimedWait(ChiCond* c, ChiMutex* m, ChiNanos ns) {
    ChiNanos begin = chiClockMonotonicFine();
    if (!SleepConditionVariableSRW(CHI_UNP(Cond, c), CHI_UNP(Mutex, m), (DWORD)CHI_UN(Millis, chiNanosToMillis(ns)), 0)
        && GetLastError() != ERROR_TIMEOUT)
        winErr("SleepConditionVariableCS");
    return chiNanosDelta(chiClockMonotonicFine(), begin);
}

static WINAPI BOOL ctrlHandler(DWORD type) {
    if (type == CTRL_C_EVENT) {
        dispatchSig(type == CTRL_C_EVENT ? CHI_SIG_USERINTERRUPT : CHI_SIG_DUMPSTACK);
        return TRUE;
    }
    return FALSE;
}

static void enableColor(DWORD fd) {
    HANDLE h = GetStdHandle(fd);
    DWORD mode;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | 4);
}

static void consoleSetup(void) {
    AttachConsole(ATTACH_PARENT_PROCESS);
    SetConsoleOutputCP(CP_UTF8);
    enableColor(STD_OUTPUT_HANDLE);
    enableColor(STD_ERROR_HANDLE);
}

static void dispatcherNotify(void) {
    ChiMicros timeout = dispatchTimers();
    DWORD due = chiMicrosZero(timeout) ? INFINITE : CHI_MAX((DWORD)CHI_UN(Millis, chiMicrosToMillis(timeout)), 1);
    if (!ChangeTimerQueueTimer(0, timer, 0, due))
        winErr("ChangeTimerQueueTimer");
}

static void WINAPI timerCallback(void* CHI_UNUSED(param), BOOLEAN CHI_UNUSED(fired)) {
    dispatcherNotify();
}

static void clockSetup(void) {
    LARGE_INTEGER freq;
    if (!QueryPerformanceFrequency(&freq))
        winErr("QueryPerformanceFrequency");
    clockScale = UINT64_C(1000000000) / (uint64_t)freq.QuadPart;
}

void chiSystemSetup(void) {
    CHI_ASSERT_ONCE;
    clockSetup();
    consoleSetup();
    if (!CreateTimerQueueTimer(&timer, 0, timerCallback, 0, 0, INFINITE, WT_EXECUTEINTIMERTHREAD))
        winErr("CreateTimerQueueTimer");
    if (!SetConsoleCtrlHandler(ctrlHandler, TRUE))
        winErr("SetConsoleCtrlHandler");
}

ChiTask chiTaskCurrent(void) {
    HANDLE h;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                         GetCurrentProcess(), &h, 0, FALSE, DUPLICATE_SAME_ACCESS))
        winErr("DuplicateHandle");
    return CHI_WRAP(Task, h);
}

_Noreturn void chiTaskExit(void) {
    _endthreadex(0);
}

void chiTaskJoin(ChiTask t) {
    HANDLE h = CHI_UN(Task, t);
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
}

static void* virtAlloc(void* ptr, size_t size, DWORD type, DWORD access) {
    void* p = VirtualAlloc(ptr, size, type, access);
    if (!p)
        winSetErr();
    return p;
}

static void virtFree(void* ptr, size_t size, DWORD type) {
    if (!VirtualFree(ptr, size, type))
        winErr("VirtualFree");
}

bool chiVirtReserve(ChiVirtMem* CHI_UNUSED(mem), void* ptr, size_t size) {
    void* res = virtAlloc(ptr, size, MEM_RESERVE, PAGE_NOACCESS);
    if (!res)
        return false;
    if (res != ptr)
        chiErr("Could not reserve memory %p - %p", ptr, (uint8_t*)ptr + size);
    return true;
}

bool chiVirtCommit(ChiVirtMem* CHI_UNUSED(mem), void* ptr, size_t size, bool CHI_UNUSED(huge)) {
    CHI_ASSERT(ptr);
    CHI_ASSERT(size);
    return !!virtAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
}

void chiVirtDecommit(ChiVirtMem* CHI_UNUSED(mem), void* ptr, size_t size) {
    CHI_ASSERT(ptr);
    CHI_ASSERT(size);
    virtFree(ptr, size, MEM_DECOMMIT);
}

void* chiVirtAlloc(void* ptr, size_t size, bool CHI_UNUSED(huge)) {
    return virtAlloc(ptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void chiVirtFree(void* ptr, size_t CHI_UNUSED(s)) {
    virtFree(ptr, 0, MEM_RELEASE);
}

uint32_t chiPhysProcessors(void) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
}

uint64_t chiPhysMemory(void) {
    MEMORYSTATUSEX mem = { .dwLength = sizeof (mem) };
    if (!GlobalMemoryStatusEx(&mem))
        winErr("GlobalMemoryStatusEx");
    return mem.ullTotalPhys / CHI_MiB(1) * CHI_MiB(1);
}

bool chiFilePerm(const char* file, int32_t CHI_UNUSED(perm)) {
    CHI_AUTO_FREE(fileW, chiAllocUtf8To16(file));
    DWORD attrs = GetFileAttributesW(fileW);
    if (attrs) {
        winSetErr();
        return false;
    }
    return true;
}

void chiSystemStats(ChiSystemStats* a) {
    CHI_ZERO_STRUCT(a);

    PROCESS_MEMORY_COUNTERS mem;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &mem, sizeof(mem)))
        winErr("GetProcessMemoryInfo");

    FILETIME creation, exit, system, user;
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit,
                         &system, &user))
        winErr("GetProcessTimes");

    a->cpuTimeUser = filetimeToNanos(user);
    a->cpuTimeSystem = filetimeToNanos(system);
    a->pageFault = mem.PageFaultCount;
    a->residentSize = mem.PeakWorkingSetSize; // TODO: Current RSS!
}

bool chiFileRead(ChiFile file, void* buf, size_t size, size_t* nread) {
    DWORD dwRead;
    if (!ReadFile(CHI_UN(File, file), buf, (DWORD)size, &dwRead, 0)) {
        winSetErr();
        return false;
    }
    *nread = dwRead;
    return true;
}

bool chiFileWrite(ChiFile file, const void* buf, size_t size) {
    DWORD dwWrite;
    if (!WriteFile(CHI_UN(File, file), buf, (DWORD)size, &dwWrite, 0) || size != (size_t)dwWrite) {
        winSetErr();
        return false;
    }
    return true;
}

bool chiFileReadOff(ChiFile file, void* buf, size_t size, uint64_t off) {
    DWORD dwRead;
    OVERLAPPED overlapped = { .Offset = (DWORD)(off & UINT32_MAX), .OffsetHigh = (DWORD)(off >> 32) };
    if (!ReadFile(CHI_UN(File, file), buf, (DWORD)size, &dwRead, &overlapped) || (size_t)dwRead != size) {
        winSetErr();
        return false;
    }
    return true;
}

ChiFile chiFileOpen(const char* file) {
    CHI_AUTO_FREE(fileW, chiAllocUtf8To16(file));
    HANDLE h = CreateFileW(fileW, GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE) {
        winSetErr();
        return CHI_WRAP(File, 0);
    }
    return CHI_WRAP(File, h);
}

ChiFile chiFileOpenRead(const char* file) {
    CHI_AUTO_FREE(fileW, chiAllocUtf8To16(file));
    HANDLE h = CreateFileW(fileW, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE) {
        winSetErr();
        return CHI_WRAP(File, 0);
    }
    return CHI_WRAP(File, h);
}

uint64_t chiFileSize(ChiFile file) {
    LARGE_INTEGER size;
    if (!GetFileSizeEx(CHI_UN(File, file), &size))
        winSetErr();
    return (uint64_t)size.QuadPart;
}

bool chiWinErrorString(DWORD err, char* buf, size_t bufsiz) {
    wchar_t bufW[256];
    if (!FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        0,                      // message source
                        err,                    // error number
                        0,                      // default language
                        bufW,                   // destination
                        sizeof(bufW),           // size of destination
                        0))                     // no inserts
        return false;
    chiUtf16To8(bufW, buf, bufsiz);
    size_t n = strlen(buf);
    if (n >= 3 && buf[n - 1] == '\n' && buf[n - 2] == '\r' && buf[n - 3] == '.')
        buf[n - 3] = 0;
    return true;
}

bool chiErrnoString(char* buf, size_t bufsiz) {
    int err = errno;
    return err & CHI_WIN_ERROR
        ? chiWinErrorString((DWORD)(err & ~CHI_WIN_ERROR), buf, bufsiz)
        : !strerror_s(buf, bufsiz, err);
}

ChiDir chiDirOpen(const char* path) {
    CHI_AUTO_FREE(pathW, chiAllocUtf8To16(path));
    WIN32_FIND_DATAW data;
    HANDLE h = FindFirstFileW(pathW, &data);
    if (h == INVALID_HANDLE_VALUE) {
        winSetErr();
        return 0;
    }
    ChiDir dir = chiAllocObj(ChiDirStruct);
    dir->handle = h;
    dir->current = 0;
    dir->next = chiAllocUtf16To8(data.cFileName);
    return dir;
}

const char* chiDirEntry(ChiDir dir) {
    WIN32_FIND_DATAW data;
    if (dir->current)
        chiFree(dir->current);
    dir->current = dir->next;
    dir->next = FindNextFile(dir->handle, &data) ? chiAllocUtf16To8(data.cFileName) : 0;
    return dir->current;
}

void chiDirClose(ChiDir dir) {
    if (dir->current)
        chiFree(dir->current);
    if (dir->next)
        chiFree(dir->next);
    FindClose(dir->handle);
    chiFree(dir);
}

#if CHI_ENV_ENABLED
char* chiGetEnv(const char* var) {
    CHI_AUTO_FREE(varW, chiAllocUtf8To16(var));
    DWORD n = GetEnvironmentVariableW(varW, 0, 0);
    if (!n) {
        winSetErr();
        return 0;
    }
    CHI_AUTO_ALLOC(WCHAR, buf, n + 1);
    if (!GetEnvironmentVariableW(varW, buf, n + 1)) {
        winSetErr();
        return 0;
    }
    return chiAllocUtf16To8(buf);
}
#endif

int main(int, char**);
int wmain(int, wchar_t**, wchar_t**);
int wmain(int argc, wchar_t** wargv, wchar_t** CHI_UNUSED(wenvp)) {
    char** argv = chiAllocArr(char*, (size_t)argc + 1);
    for (int i = 0; i < argc; ++i)
        argv[i] = chiAllocUtf16To8(wargv[i]);
    argv[argc] = 0;
    return main(argc, argv);
}
