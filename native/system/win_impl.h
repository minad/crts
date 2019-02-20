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
#include <io.h>
#include <process.h>
#include <psapi.h>
#include <sysinfoapi.h>
#include "../sink.h"
#include "interrupt.h"
#include "tasklocal.h"

static HANDLE timer;

// We use the user defined bit to encode windows errors in errno
#define CHI_WIN_ERROR (1 << 29)

void chiWinErr(const char* call) {
    chiWinSetErr();
    chiSysErr(call);
}

void chiWinSetErr(void) {
    errno = CHI_WIN_ERROR | (int)GetLastError();
}

static ChiNanos filetimeToNanos(FILETIME f) {
    return (ChiNanos){ (((uint64_t)f.dwHighDateTime << 32) | f.dwLowDateTime) * 100 };
}

ChiNanos chiClock(ChiClock c) {
    if (c == CHI_CLOCK_CPU) {
        FILETIME creation, exit, system, user;
        if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit,
                             &system, &user))
            chiWinErr("GetProcessTimes");
        return chiNanosAdd(filetimeToNanos(system), filetimeToNanos(user));
    }
    static LARGE_INTEGER freq = { .QuadPart = 0 };
    if (!freq.QuadPart) {
        if (!QueryPerformanceFrequency(&freq))
            chiWinErr("QueryPerformanceFrequency");
    }
    LARGE_INTEGER counter;
    if (!QueryPerformanceCounter(&counter))
        chiWinErr("QueryPerformanceCounter");
    return (ChiNanos){ (uint64_t)counter.QuadPart * (1000000000ULL / (uint64_t)freq.QuadPart) };
}

ChiNanos chiCondTimedWait(ChiCond* c, ChiMutex* m, ChiNanos ns) {
    ChiNanos begin = chiClock(CHI_CLOCK_REAL_FINE);
    if (!SleepConditionVariableCS(CHI_UNP(Cond, c), CHI_UNP(Mutex, m), (DWORD)CHI_UN(Millis, chiNanosToMillis(ns)))
        && GetLastError() != ERROR_TIMEOUT)
        chiWinErr("SleepConditionVariableCS");
    return chiNanosDelta(chiClock(CHI_CLOCK_REAL_FINE), begin);
}

typedef struct {
    ChiTaskRun run;
    void* arg;
} TaskArg;

static WINAPI unsigned taskRun(void* arg) {
    TaskArg ta = *(TaskArg*)arg;
    chiFree(arg);
    ta.run(ta.arg);
    return 0;
}

ChiTask chiTaskTryCreate(ChiTaskRun run, void* arg) {
    TaskArg* ta = chiAllocObj(TaskArg);
    ta->run = run;
    ta->arg = arg;
    HANDLE h = (HANDLE)_beginthreadex(0,                      // default security attributes
                                      0,                      // use default stack size
                                      taskRun,                // thread function name
                                      ta,                     // argument to thread function
                                      0,                      // use default creation flags
                                      0);                     // returns the thread identifier
    if (!h)
        chiFree(ta);
    return (ChiTask){ h };
}

ChiTask chiTaskCreate(ChiTaskRun run, void* arg) {
    ChiTask t = chiTaskTryCreate(run, arg);
    if (chiTaskNull(t))
        chiSysErr("_beginthreadex");
    return t;
}

static DWORD taskLocalKey = 0;

void* chiTaskLocal(size_t size, ChiDestructor destructor) {
    TaskLocal* oldLocal = (TaskLocal*)FlsGetValue(taskLocalKey);
    TaskLocal* newLocal = (TaskLocal*)chiAlloc(sizeof (TaskLocal) + size);
    newLocal->destructor = destructor;
    newLocal->next = oldLocal;
    if (!FlsSetValue(taskLocalKey, newLocal))
        chiWinErr("FlsSetValue");
    return newLocal->data;
}

static WINAPI BOOL ctrlHandler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
        dispatchSig(type == CTRL_C_EVENT ? CHI_SIG_INTERRUPT : CHI_SIG_DUMPHEAP);
        return TRUE;
    }
    return FALSE;
}

static WINAPI void destroyTaskLocal_(void* data) {
    destroyTaskLocal(data);
}

static void redirectOutput(void) {
    int fdout = _fileno(stdout), fderr = _fileno(stderr);
    if (fdout >= 0 && fderr >= 0 && _get_osfhandle(fdout) >= 0 && _get_osfhandle(fderr) >= 0)
        return;
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;
    if (_wfreopen(L"CONOUT$", L"w", stdout))
        setvbuf(stdout, 0, _IONBF, 0);
    if (_wfreopen(L"CONOUT$", L"w", stderr))
        setvbuf(stderr, 0, _IONBF, 0);
}

static bool drinkingWine(void) {
    HMODULE h;
    return (h = GetModuleHandleW(L"ntdll.dll")) && !!GetProcAddress(h, "wine_get_version");
}

static void enableColor(DWORD fd) {
    HANDLE h = GetStdHandle(fd);
    DWORD mode;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | 4);
}

static void dispatcherNotify(void) {
    ChiMicros timeout = dispatchTimers();
    DWORD due = chiMicrosZero(timeout) ? INFINITE : CHI_MAX((DWORD)CHI_UN(Millis, chiMicrosToMillis(timeout)), 1);
    if (!ChangeTimerQueueTimer(0, timer, 0, due))
        chiWinErr("ChangeTimerQueueTimer");
}

static void WINAPI timerCallback(void* CHI_UNUSED(param), BOOLEAN CHI_UNUSED(fired)) {
    dispatcherNotify();
}

void chiSystemSetup(void) {
    interruptSetup();
    if (!drinkingWine()) {
        redirectOutput();
        SetConsoleOutputCP(CP_UTF8);
        enableColor(STD_OUTPUT_HANDLE);
        enableColor(STD_ERROR_HANDLE);
    }
    taskLocalKey = FlsAlloc(destroyTaskLocal_);
    if (!taskLocalKey)
        chiWinErr("FlsAlloc");
     if (!CreateTimerQueueTimer(&timer, 0, timerCallback, 0, 0, INFINITE, WT_EXECUTEINTIMERTHREAD))
        chiWinErr("CreateTimerQueueTimer");
    if (!SetConsoleCtrlHandler(ctrlHandler, TRUE))
        chiWinErr("SetConsoleCtrlHandler");
}

ChiTask chiTaskCurrent(void) {
    HANDLE h;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                         GetCurrentProcess(), &h, 0, FALSE, DUPLICATE_SAME_ACCESS))
        chiWinErr("DuplicateHandle");
    return (ChiTask){ h };
}

_Noreturn void chiTaskExit(void) {
    _endthreadex(0);
}

void chiTaskYield(void) {
    if (!SwitchToThread())
        chiWinErr("SwitchToThread");
}

void chiTaskJoin(ChiTask t) {
    HANDLE h = CHI_UN(Task, t);
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
}

void chiTaskCancel(ChiTask t) {
    CancelSynchronousIo(CHI_UN(Task, t));
}

void chiTaskClose(ChiTask t) {
    CloseHandle(CHI_UN(Task, t));
}

bool chiTaskEqual(ChiTask a, ChiTask b) {
    return CHI_UN(Task, a) == CHI_UN(Task, b);
}

bool chiTaskNull(ChiTask a) {
    return !CHI_UN(Task, a);
}

void chiTaskName(const char* CHI_UNUSED(name)) {
    // TODO: SetThreadDescription is only available on Windows 10
}

void* chiVirtAlloc(void* ptr, size_t size, int32_t CHI_UNUSED(flags)) {
    void* p = VirtualAlloc(ptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p)
        chiWinSetErr();
    if (p && ptr && p != ptr)
        chiErr("Invalid memory address %p, but expected %p", p, ptr);
    return p;
}

void chiVirtFree(void* p, size_t CHI_UNUSED(s)) {
    if (!VirtualFree(p, 0, MEM_RELEASE))
        chiWinErr("VirtualFree");
}


uint32_t chiPhysProcessors(void) {
    static uint32_t cachedProcessors = 0;
    if (!cachedProcessors) {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        cachedProcessors = info.dwNumberOfProcessors;
    }
    return cachedProcessors;
}

uint64_t chiPhysMemory(void) {
    uint64_t physMemory = 0;
    if (!physMemory) {
        MEMORYSTATUSEX mem = { .dwLength = sizeof (mem) };
        if (!GlobalMemoryStatusEx(&mem))
            chiWinErr("GlobalMemoryStatusEx");
        physMemory = mem.ullTotalPhys / CHI_MiB(1) * CHI_MiB(1);
    }
    return physMemory;
}

bool chiFilePerm(const char* file, int32_t CHI_UNUSED(perm)) {
    CHI_AUTO_FREE(fileW, chiAllocUtf8To16(file));
    return !_waccess(fileW, 4);
}

void chiActivity(ChiActivity* a) {
    CHI_CLEAR(a);

    PROCESS_MEMORY_COUNTERS mem;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &mem, sizeof(mem)))
        chiWinErr("GetProcessMemoryInfo");

    FILETIME creation, exit, system, user;
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit,
                         &system, &user))
        chiWinErr("GetProcessTimes");

    a->cpuTimeUser = filetimeToNanos(user);
    a->cpuTimeSystem = filetimeToNanos(system);
    a->pageFault = mem.PageFaultCount;
    a->residentSize = mem.PeakWorkingSetSize;
}

uint32_t chiPid(void) {
    return GetCurrentProcessId();
}

bool chiTerminal(int fd) {
    return !!_isatty(fd);
}

FILE* chiFileOpenWrite(const char* file) {
    CHI_AUTO_FREE(fileW, chiAllocUtf8To16(file));
    FILE* fp = _wfopen(fileW, L"abN");
    if (!fp)
        return 0;
    off_t off = ftello(fp);
    if (off) {
        int err = off > 0 ? EEXIST : errno;
        fclose(fp);
        errno = err;
        return 0;
    }
    return fp;
}

FILE* chiFileOpenRead(const char* file) {
    CHI_AUTO_FREE(fileW, chiAllocUtf8To16(file));
    return _wfopen(fileW, L"rbN");
}

bool chiWinErrorString(uint32_t err, char* buf, size_t bufsiz) {
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

bool chiErrorString(char* buf, size_t bufsiz) {
    int err = errno;
    return err & CHI_WIN_ERROR
        ? chiWinErrorString((uint32_t)(err & ~CHI_WIN_ERROR), buf, bufsiz)
        : !strerror_s(buf, bufsiz, err);
}

FILE* chiOpenPipe(const char* cmd) {
    CHI_AUTO_FREE(cmdW, chiAllocUtf8To16(cmd));
    return _wpopen(cmdW, L"wb");
}

#if CHI_ENV_ENABLED
char* chiGetEnv(const char* var) {
    CHI_AUTO_FREE(varW, chiAllocUtf8To16(var));
    wchar_t* val = _wgetenv(varW);
    return val ? chiAllocUtf16To8(val) : 0;
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
