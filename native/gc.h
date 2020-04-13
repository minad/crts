#pragma once

#include "localgc.h"
#include "vec.h"

typedef struct ChiGCWorker_ ChiGCWorker;
typedef struct ChiHeap_ ChiHeap;
typedef struct ChiProcessor_ ChiProcessor;
typedef struct ChiRuntime_ ChiRuntime;
typedef struct ChiTimeout_ ChiTimeout;

/**
 * Global garbage collector state
 */
typedef struct ChiGC_ {
    ChiMutex               mutex;
    ChiVec                 root;      ///< Global list of GC roots
    CHI_IF(CHI_GC_CONC_ENABLED, ChiGCWorker *firstWorker, *lastWorker;)
    struct {
        ChiGrayVec         vec;
        ChiBlockManager    manager;
    } gray;
    struct {
        ChiHeapSegmentList segmentUnswept, segmentPartial;
        ChiChunkList       largeList;
        ChiTrigger         needed;
    } sweep;
    _Atomic(ChiGCPhase)    phase;
    ChiTrigger             trigger;
} ChiGC;

CHI_INTERN void chiGCTrigger(ChiRuntime*);
CHI_INTERN void chiGCSetup(ChiRuntime*);
CHI_INTERN void chiGCDestroy(ChiRuntime*);
CHI_INTERN void chiGCService(ChiProcessor*);
CHI_INTERN void chiGCRoot(ChiRuntime*, Chili);
CHI_INTERN void chiGCUnroot(ChiRuntime*, Chili);
CHI_INTERN void chiScavenger(ChiProcessor*, uint32_t, bool, ChiScavengerStats*);
CHI_INTERN void chiSweepSlice(ChiHeap*, ChiGC*, ChiMarkState, ChiSweepStats*, ChiTimeout*);
CHI_INTERN void chiMarkSlice(ChiGrayVec*, uint32_t, bool, ChiMarkState, ChiScanStats*, ChiTimeout*);
