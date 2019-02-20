#pragma once

#include "blockvec.h"

typedef enum {
    CHI_MS_PHASE_IDLE,
    CHI_MS_PHASE_MARK,
    CHI_MS_PHASE_SWEEP,
} ChiMSPhase;

typedef struct ChiGCSweeper_ ChiGCSweeper;
typedef struct ChiGCMarker_ ChiGCMarker;
typedef struct ChiRuntime_ ChiRuntime;
typedef struct ChiProcessor_ ChiProcessor;

typedef enum {
    CHI_NODUMP,
    CHI_DUMP,
} ChiDump;

typedef struct {
    ChiMutex mutex;
    size_t used, capacity;
    Chili* elem;
} ChiRootVec;

/**
 * Global garbage collector state
 */
typedef struct {
    ChiRootVec        roots;      ///< Global list of GC roots
    _Atomic(int32_t)  blocked;    ///< Garbage collection is blocked if != 0
    uint32_t          dumpId;     ///< Number of heap dumps
    uint32_t          scavTrip;   ///< Scavenger trip counter
    ChiMSPhase        msPhase;    ///< Current mark and sweep phase
    _Atomic(ChiDump)  dumpHeap;   ///< Dump heap after next scavenger run
    _Atomic(ChiGCTrigger) trigger;
    struct {
        ChiGCSweeper* sweeper;    ///< State of the sweeper workers
        ChiGCMarker*  marker;     ///< State of the marker workers
    } conc[CHI_GC_CONC_ENABLED];
} ChiGC;

/**
 * Per-processor state of the garbage collector
 */
typedef struct {
    /**
     *< Generational GC: The dirtyset contains unique values across (!) all processors.
     * List 0 contains the dirty major objects. The lists 1 to CHI_GEN_MAX-1 contains dirty tenured objects.
     */
    ChiBlockVec dirty[CHI_GEN_MAX];
    ChiBlockVec grayList;           ///< Gray list used for marking
} ChiGCPS;

void chiGCTrigger(ChiRuntime*, ChiGCRequestor, ChiGCTrigger, ChiDump);
void chiGCSetup(ChiRuntime*);
void chiGCDestroy(ChiRuntime*);
void chiGCProcStart(ChiProcessor*);
void chiGCProcStop(ChiProcessor*);
void chiGCSlice(ChiProcessor*, ChiGCTrigger);
CHI_WU Chili chiRoot(ChiRuntime*, Chili);
void chiUnroot(ChiRuntime*, Chili);
