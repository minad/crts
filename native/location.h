#pragma once

#include "event/defs.h"
#include "private.h"

typedef enum {
    CHI_LOC_NATIVE,
    CHI_LOC_INTERP,
    CHI_LOC_FFI,        // used by the profiler
    CHI_LOC_WORKER,     // used by the profiler
    CHI_LOC_THREAD,     // used by the profiler
    CHI_LOC_NAME,       // used by the profiler
    CHI_LOC_INCOMPLETE, // used by the profiler
} ChiLocType;

CHI_FLAGTYPE(ChiLocFmt,
             CHI_LOCFMT_FN      = 1,
             CHI_LOCFMT_INTERP  = 2,
             CHI_LOCFMT_FILE    = 4,
             CHI_LOCFMT_FILESEP = 8,
             CHI_LOCFMT_ALL     = 1|2|4)

typedef struct {
    union {
        const void* id;
        ChiCont cont;
    };
    ChiLocType type;
} ChiLoc;

typedef struct {
    ChiLocInfo loc;
    char       buf[256];
} ChiLocResolve;

typedef struct ChiSink_ ChiSink;

CHI_INTERN size_t chiLocFmt(ChiSink*, const ChiLocInfo*, ChiLocFmt);
CHI_INTERN void chiLocResolve(ChiLocResolve*, ChiLoc, bool);
CHI_INTERN void chiLocResolveDefault(ChiLocResolve*, ChiLoc, bool);
CHI_INTERN CHI_WU ChiLoc chiLocateFrame(const Chili*);
CHI_INTERN CHI_WU ChiLoc chiLocateFrameDefault(const Chili*);
