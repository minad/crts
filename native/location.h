#pragma once

#include "event/defs.h"
#include "private.h"

enum {
    CHI_LOC_NATIVE,
    CHI_LOC_FFI,        // used by the profiler
    CHI_LOC_WORKER,     // used by the profiler
    CHI_LOC_THREAD,     // used by the profiler
    CHI_LOC_NAME,       // used by the profiler
    CHI_LOC_INCOMPLETE, // used by the profiler
};

enum {
    CHI_LOCFMT_FN      = 1,
    CHI_LOCFMT_MODFN   = 2,
    CHI_LOCFMT_INTERP  = 4,
    CHI_LOCFMT_FILE    = 8,
    CHI_LOCFMT_FILESEP = 16,
    CHI_LOCFMT_ALL     = ~CHI_LOCFMT_FILESEP,
};

typedef union {
    struct {
        uintptr_t   type;
        const void* id;
    };
    struct {
        uintptr_t   type;
        ChiCont     cont;
    } native;
    struct {
        const uint8_t* code;
        const uint8_t* ip;
    } interp;
} ChiLoc;

typedef struct {
    ChiLocInfo loc;
    char       buf[256];
} ChiLocLookup;

typedef struct ChiSink_ ChiSink;

size_t chiLocFmt(ChiSink*, const ChiLocInfo*, int32_t);
void chiLocLookup(ChiLocLookup*, ChiLoc);
void chiLocLookupDefault(ChiLocLookup*, ChiLoc);
CHI_WU ChiLoc chiLocateFn(Chili);
CHI_WU ChiLoc chiLocateFnDefault(Chili);
CHI_WU ChiLoc chiLocateFrame(const Chili*);
CHI_WU ChiLoc chiLocateFrameDefault(const Chili*);
