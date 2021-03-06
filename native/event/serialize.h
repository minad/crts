// Generated by generate.pl from defs.in

SPAYLOAD_BEGIN(Chunk, ChiEventChunk* d)
    SFIELD(heapSize, SDEC(d->heapSize));
    SFIELD(size, SDEC(d->size));
    SFIELD(align, SDEC(d->align));
    SFIELD(start, SHEX(d->start));
SPAYLOAD_END

SPAYLOAD_BEGIN(Exception, ChiEventException* d)
    SFIELD(name, SSTR(d->name));
    SFIELD(trace, SSTR(d->trace));
SPAYLOAD_END

SPAYLOAD_BEGIN(FFI, ChiEventFFI* d)
    SFIELD(module, SSTR(d->module));
    SFIELD(name, SSTR(d->name));
SPAYLOAD_END

SPAYLOAD_BEGIN(FnLog, ChiEventFnLog* d)
    SFIELD(fn, SSTR(d->fn));
    SFIELD(file, SSTR(d->file));
    SFIELD(size, SDEC(d->size));
    SFIELD(line, SDEC(d->line));
    SFIELD(interp, SBOOL(d->interp));
SPAYLOAD_END

SPAYLOAD_BEGIN(FnLogFFI, ChiEventFnLogFFI* d)
    SFIELD(name, SSTR(d->name));
SPAYLOAD_END

SPAYLOAD_BEGIN(GCPhase, ChiEventGCPhase* d)
    SFIELD(phase, SENUM(ChiGCPhase, d->phase));
SPAYLOAD_END

SPAYLOAD_BEGIN(HeapAlloc, ChiEventHeapAlloc* d)
    SFIELD(type, SSTR(d->type));
    SFIELD(size, SDEC(d->size));
SPAYLOAD_END

SPAYLOAD_BEGIN(HeapUsage, ChiEventHeapUsage* d)
    SBLOCK_BEGIN(small);
        SFIELD(allocSize, SDEC(d->small.allocSize));
        SFIELD(totalSize, SDEC(d->small.totalSize));
    SBLOCK_END(small);
    SBLOCK_BEGIN(medium);
        SFIELD(allocSize, SDEC(d->medium.allocSize));
        SFIELD(totalSize, SDEC(d->medium.totalSize));
    SBLOCK_END(medium);
    SBLOCK_BEGIN(large);
        SFIELD(allocSize, SDEC(d->large.allocSize));
        SFIELD(totalSize, SDEC(d->large.totalSize));
    SBLOCK_END(large);
    SFIELD(totalSize, SDEC(d->totalSize));
SPAYLOAD_END

SPAYLOAD_BEGIN(Mark, ChiEventMark* d)
    SBLOCK_BEGIN(object);
        SFIELD(count, SDEC(d->object.count));
        SFIELD(words, SDEC(d->object.words));
    SBLOCK_END(object);
    SBLOCK_BEGIN(stack);
        SFIELD(count, SDEC(d->stack.count));
        SFIELD(words, SDEC(d->stack.words));
    SBLOCK_END(stack);
    SFIELD(collapsed, SDEC(d->collapsed));
SPAYLOAD_END

SPAYLOAD_BEGIN(ModuleInit, ChiEventModuleInit* d)
    SFIELD(fn, SSTR(d->fn));
    SFIELD(file, SSTR(d->file));
    SFIELD(size, SDEC(d->size));
    SFIELD(line, SDEC(d->line));
    SFIELD(interp, SBOOL(d->interp));
SPAYLOAD_END

SPAYLOAD_BEGIN(ModuleLoad, ChiEventModuleLoad* d)
    SFIELD(module, SSTR(d->module));
    SFIELD(file, SSTR(d->file));
    SFIELD(path, SSTR(d->path));
SPAYLOAD_END

SPAYLOAD_BEGIN(ModuleName, ChiEventModuleName* d)
    SFIELD(module, SSTR(d->module));
SPAYLOAD_END

SPAYLOAD_BEGIN(ProcMsgRecv, ChiEventProcMsgRecv* d)
    SFIELD(message, SENUM(ChiProcessorMessage, d->message));
SPAYLOAD_END

SPAYLOAD_BEGIN(ProcMsgSend, ChiEventProcMsgSend* d)
    SFIELD(receiverWid, SDEC(d->receiverWid));
    SFIELD(message, SENUM(ChiProcessorMessage, d->message));
SPAYLOAD_END

SPAYLOAD_BEGIN(ProcNotify, ChiEventProcNotify* d)
    SFIELD(notifyWid, SDEC(d->notifyWid));
SPAYLOAD_END

SPAYLOAD_BEGIN(ProcRequest, ChiEventProcRequest* d)
    SFIELD(request, SENUM(ChiProcessorRequest, d->request));
SPAYLOAD_END

SPAYLOAD_BEGIN(ProcStall, ChiEventProcStall* d)
    SFIELD(wid, SDEC(d->wid));
SPAYLOAD_END

SPAYLOAD_BEGIN(ProcSuspend, ChiEventProcSuspend* d)
    SFIELD(ms, SDEC(d->ms));
SPAYLOAD_END

SPAYLOAD_BEGIN(Scavenger, ChiEventScavenger* d)
    SBLOCK_BEGIN(promoted);
        SBLOCK_BEGIN(copied);
            SFIELD(count, SDEC(d->promoted.copied.count));
            SFIELD(words, SDEC(d->promoted.copied.words));
        SBLOCK_END(copied);
        SBLOCK_BEGIN(scanned);
            SFIELD(count, SDEC(d->promoted.scanned.count));
            SFIELD(words, SDEC(d->promoted.scanned.words));
        SBLOCK_END(scanned);
        SFIELD(thunk, SDEC(d->promoted.thunk));
    SBLOCK_END(promoted);
    SBLOCK_BEGIN(scavenger);
        SBLOCK_BEGIN(dirty);
            SBLOCK_BEGIN(object);
                SFIELD(count, SDEC(d->scavenger.dirty.object.count));
                SFIELD(words, SDEC(d->scavenger.dirty.object.words));
            SBLOCK_END(object);
            SBLOCK_BEGIN(stack);
                SFIELD(count, SDEC(d->scavenger.dirty.stack.count));
                SFIELD(words, SDEC(d->scavenger.dirty.stack.words));
            SBLOCK_END(stack);
            SBLOCK_BEGIN(card);
                SFIELD(count, SDEC(d->scavenger.dirty.card.count));
                SFIELD(words, SDEC(d->scavenger.dirty.card.words));
            SBLOCK_END(card);
        SBLOCK_END(dirty);
        SBLOCK_BEGIN(raw);
            SBLOCK_BEGIN(promoted);
                SFIELD(count, SDEC(d->scavenger.raw.promoted.count));
                SFIELD(words, SDEC(d->scavenger.raw.promoted.words));
            SBLOCK_END(promoted);
            SBLOCK_BEGIN(copied);
                SFIELD(count, SDEC(d->scavenger.raw.copied.count));
                SFIELD(words, SDEC(d->scavenger.raw.copied.words));
            SBLOCK_END(copied);
        SBLOCK_END(raw);
        SBLOCK_BEGIN(object);
            SBLOCK_BEGIN(promoted);
                SFIELD(count, SDEC(d->scavenger.object.promoted.count));
                SFIELD(words, SDEC(d->scavenger.object.promoted.words));
            SBLOCK_END(promoted);
            SBLOCK_BEGIN(copied);
                SFIELD(count, SDEC(d->scavenger.object.copied.count));
                SFIELD(words, SDEC(d->scavenger.object.copied.words));
            SBLOCK_END(copied);
        SBLOCK_END(object);
        SFIELD(collapsed, SDEC(d->scavenger.collapsed));
        SFIELD(aging, SDEC(d->scavenger.aging));
        SFIELD(snapshot, SBOOL(d->scavenger.snapshot));
    SBLOCK_END(scavenger);
    SBLOCK_BEGIN(minorHeapBefore);
        SFIELD(usedSize, SDEC(d->minorHeapBefore.usedSize));
        SFIELD(totalSize, SDEC(d->minorHeapBefore.totalSize));
    SBLOCK_END(minorHeapBefore);
    SBLOCK_BEGIN(minorHeapAfter);
        SFIELD(usedSize, SDEC(d->minorHeapAfter.usedSize));
        SFIELD(totalSize, SDEC(d->minorHeapAfter.totalSize));
    SBLOCK_END(minorHeapAfter);
SPAYLOAD_END

SPAYLOAD_BEGIN(Signal, ChiEventSignal* d)
    SFIELD(sig, SENUM(ChiSig, d->sig));
SPAYLOAD_END

SPAYLOAD_BEGIN(Stack, ChiEventStack* d)
    SFIELD(stack, SHEX(d->stack));
SPAYLOAD_END

SPAYLOAD_BEGIN(StackSize, ChiEventStackSize* d)
    SFIELD(stack, SHEX(d->stack));
    SFIELD(size, SDEC(d->size));
    SFIELD(step, SDEC(d->step));
    SFIELD(copied, SDEC(d->copied));
SPAYLOAD_END

SPAYLOAD_BEGIN(StackTrace, ChiEventStackTrace* d)
    SFIELD(stack, SHEX(d->stack));
    SFIELD(trace, SSTR(d->trace));
SPAYLOAD_END

SPAYLOAD_BEGIN(Sweep, ChiEventSweep* d)
    SBLOCK_BEGIN(small);
        SBLOCK_BEGIN(alive);
            SFIELD(count, SDEC(d->small.alive.count));
            SFIELD(words, SDEC(d->small.alive.words));
        SBLOCK_END(alive);
        SBLOCK_BEGIN(garbage);
            SFIELD(count, SDEC(d->small.garbage.count));
            SFIELD(words, SDEC(d->small.garbage.words));
        SBLOCK_END(garbage);
    SBLOCK_END(small);
    SBLOCK_BEGIN(medium);
        SBLOCK_BEGIN(alive);
            SFIELD(count, SDEC(d->medium.alive.count));
            SFIELD(words, SDEC(d->medium.alive.words));
        SBLOCK_END(alive);
        SBLOCK_BEGIN(garbage);
            SFIELD(count, SDEC(d->medium.garbage.count));
            SFIELD(words, SDEC(d->medium.garbage.words));
        SBLOCK_END(garbage);
    SBLOCK_END(medium);
    SBLOCK_BEGIN(large);
        SBLOCK_BEGIN(alive);
            SFIELD(count, SDEC(d->large.alive.count));
            SFIELD(words, SDEC(d->large.alive.words));
        SBLOCK_END(alive);
        SBLOCK_BEGIN(garbage);
            SFIELD(count, SDEC(d->large.garbage.count));
            SFIELD(words, SDEC(d->large.garbage.words));
        SBLOCK_END(garbage);
    SBLOCK_END(large);
SPAYLOAD_END

SPAYLOAD_BEGIN(SystemStats, ChiEventSystemStats* d)
    SFIELD(cpuTimeUser, SDEC(CHI_UN(Nanos, d->cpuTimeUser)));
    SFIELD(cpuTimeSystem, SDEC(CHI_UN(Nanos, d->cpuTimeSystem)));
    SFIELD(maxResidentSize, SDEC(d->maxResidentSize));
    SFIELD(currResidentSize, SDEC(d->currResidentSize));
    SFIELD(pageFault, SDEC(d->pageFault));
    SFIELD(voluntaryContextSwitch, SDEC(d->voluntaryContextSwitch));
    SFIELD(involuntaryContextSwitch, SDEC(d->involuntaryContextSwitch));
SPAYLOAD_END

SPAYLOAD_BEGIN(ThreadEnqueue, ChiEventThreadEnqueue* d)
    SFIELD(enqTid, SDEC(d->enqTid));
    SFIELD(phase, SENUM(ChiThreadPhase, d->phase));
SPAYLOAD_END

SPAYLOAD_BEGIN(ThreadMigrate, ChiEventThreadMigrate* d)
    SFIELD(migratedTid, SDEC(d->migratedTid));
    SFIELD(newOwnerWid, SDEC(d->newOwnerWid));
SPAYLOAD_END

SPAYLOAD_BEGIN(ThreadName, ChiEventThreadName* d)
    SFIELD(nameTid, SDEC(d->nameTid));
    SFIELD(name, SSTR(d->name));
SPAYLOAD_END

SPAYLOAD_BEGIN(ThreadNew, ChiEventThreadNew* d)
    SFIELD(newTid, SDEC(d->newTid));
    SFIELD(newStack, SHEX(d->newStack));
SPAYLOAD_END

SPAYLOAD_BEGIN(ThreadNext, ChiEventThreadNext* d)
    SFIELD(nextTid, SDEC(d->nextTid));
SPAYLOAD_END

SPAYLOAD_BEGIN(ThreadYield, ChiEventThreadYield* d)
    SFIELD(phase, SENUM(ChiThreadPhase, d->phase));
SPAYLOAD_END

SPAYLOAD_BEGIN(UserBuffer, ChiEventUserBuffer* d)
    SFIELD(buffer, SBYTES(d->buffer));
SPAYLOAD_END

SPAYLOAD_BEGIN(UserString, ChiEventUserString* d)
    SFIELD(string, SSTR(d->string));
SPAYLOAD_END

SPAYLOAD_BEGIN(WorkerName, ChiEventWorkerName* d)
    SFIELD(name, SSTR(d->name));
SPAYLOAD_END

SEVENT_BEGIN
   SFIELD(ts, SDEC(CHI_UN(Nanos, e->ts)));
   if (eventDesc[e->type].cls == CLASS_END)
       SFIELD(dur, SDEC(CHI_UN(Nanos, e->dur)));
   if (eventDesc[e->type].ctx != CTX_RUNTIME)
       SFIELD(wid, SDEC(e->wid));
   if (eventDesc[e->type].ctx == CTX_THREAD)
       SFIELD(tid, SDEC(e->tid));
   switch (e->type) {
   case CHI_EVENT_GC_MARK_PHASE_BEGIN: break;
   case CHI_EVENT_GC_MARK_PHASE_END: break;
   case CHI_EVENT_GC_MARK_SLICE_BEGIN: break;
   case CHI_EVENT_GC_MARK_SLICE_END: SPAYLOAD(Mark, &e->payload->GC_MARK_SLICE_END); break;
   case CHI_EVENT_GC_SCAVENGER_BEGIN: break;
   case CHI_EVENT_GC_SCAVENGER_END: SPAYLOAD(Scavenger, &e->payload->GC_SCAVENGER_END); break;
   case CHI_EVENT_GC_SWEEP_SLICE_BEGIN: break;
   case CHI_EVENT_GC_SWEEP_SLICE_END: SPAYLOAD(Sweep, &e->payload->GC_SWEEP_SLICE_END); break;
   case CHI_EVENT_PROC_PARK_BEGIN: break;
   case CHI_EVENT_PROC_PARK_END: break;
   case CHI_EVENT_PROC_RUN_BEGIN: break;
   case CHI_EVENT_PROC_RUN_END: break;
   case CHI_EVENT_PROC_SERVICE_BEGIN: break;
   case CHI_EVENT_PROC_SERVICE_END: break;
   case CHI_EVENT_SHUTDOWN_BEGIN: break;
   case CHI_EVENT_SHUTDOWN_END: break;
   case CHI_EVENT_STARTUP_BEGIN: break;
   case CHI_EVENT_STARTUP_END: break;
   case CHI_EVENT_THREAD_RUN_BEGIN: break;
   case CHI_EVENT_THREAD_RUN_END: break;
   case CHI_EVENT_THREAD_SCHED_BEGIN: break;
   case CHI_EVENT_THREAD_SCHED_END: SPAYLOAD(ThreadNext, &e->payload->THREAD_SCHED_END); break;
   case CHI_EVENT_USER_DURATION_BEGIN: break;
   case CHI_EVENT_USER_DURATION_END: break;
   case CHI_EVENT_BIGINT_OVERFLOW: break;
   case CHI_EVENT_BLOCK_CHUNK_FREE: SPAYLOAD(Chunk, &e->payload->BLOCK_CHUNK_FREE); break;
   case CHI_EVENT_BLOCK_CHUNK_NEW: SPAYLOAD(Chunk, &e->payload->BLOCK_CHUNK_NEW); break;
   case CHI_EVENT_ENTRY_BLACKHOLE: break;
   case CHI_EVENT_ENTRY_NOTIFY_INT: break;
   case CHI_EVENT_ENTRY_START: break;
   case CHI_EVENT_ENTRY_TIMER_INT: break;
   case CHI_EVENT_ENTRY_UNHANDLED: break;
   case CHI_EVENT_ENTRY_USER_INT: break;
   case CHI_EVENT_EXCEPTION: SPAYLOAD(Exception, &e->payload->EXCEPTION); break;
   case CHI_EVENT_FFI_LOAD: SPAYLOAD(FFI, &e->payload->FFI_LOAD); break;
   case CHI_EVENT_FNLOG_CONT: SPAYLOAD(FnLog, &e->payload->FNLOG_CONT); break;
   case CHI_EVENT_FNLOG_ENTER: SPAYLOAD(FnLog, &e->payload->FNLOG_ENTER); break;
   case CHI_EVENT_FNLOG_ENTER_JMP: SPAYLOAD(FnLog, &e->payload->FNLOG_ENTER_JMP); break;
   case CHI_EVENT_FNLOG_FFI: SPAYLOAD(FnLogFFI, &e->payload->FNLOG_FFI); break;
   case CHI_EVENT_FNLOG_LEAVE: SPAYLOAD(FnLog, &e->payload->FNLOG_LEAVE); break;
   case CHI_EVENT_GC_PHASE_GLOBAL: SPAYLOAD(GCPhase, &e->payload->GC_PHASE_GLOBAL); break;
   case CHI_EVENT_GC_PHASE_LOCAL: SPAYLOAD(GCPhase, &e->payload->GC_PHASE_LOCAL); break;
   case CHI_EVENT_GC_TRIGGER: break;
   case CHI_EVENT_HEAP_ALLOC_FAILED: SPAYLOAD(HeapAlloc, &e->payload->HEAP_ALLOC_FAILED); break;
   case CHI_EVENT_HEAP_CHUNK_FREE: SPAYLOAD(Chunk, &e->payload->HEAP_CHUNK_FREE); break;
   case CHI_EVENT_HEAP_CHUNK_NEW: SPAYLOAD(Chunk, &e->payload->HEAP_CHUNK_NEW); break;
   case CHI_EVENT_HEAP_LIMIT_GC: break;
   case CHI_EVENT_HEAP_LIMIT_OVERFLOW: break;
   case CHI_EVENT_HEAP_USAGE: SPAYLOAD(HeapUsage, &e->payload->HEAP_USAGE); break;
   case CHI_EVENT_LOG_BEGIN: break;
   case CHI_EVENT_LOG_END: break;
   case CHI_EVENT_MODULE_INIT: SPAYLOAD(ModuleInit, &e->payload->MODULE_INIT); break;
   case CHI_EVENT_MODULE_LOAD: SPAYLOAD(ModuleLoad, &e->payload->MODULE_LOAD); break;
   case CHI_EVENT_MODULE_UNLOAD: SPAYLOAD(ModuleName, &e->payload->MODULE_UNLOAD); break;
   case CHI_EVENT_PROC_DESTROY: break;
   case CHI_EVENT_PROC_INIT: break;
   case CHI_EVENT_PROC_MSG_RECV: SPAYLOAD(ProcMsgRecv, &e->payload->PROC_MSG_RECV); break;
   case CHI_EVENT_PROC_MSG_SEND: SPAYLOAD(ProcMsgSend, &e->payload->PROC_MSG_SEND); break;
   case CHI_EVENT_PROC_NOTIFY: SPAYLOAD(ProcNotify, &e->payload->PROC_NOTIFY); break;
   case CHI_EVENT_PROC_REQUEST: SPAYLOAD(ProcRequest, &e->payload->PROC_REQUEST); break;
   case CHI_EVENT_PROC_STALL: SPAYLOAD(ProcStall, &e->payload->PROC_STALL); break;
   case CHI_EVENT_PROC_SUSPEND: SPAYLOAD(ProcSuspend, &e->payload->PROC_SUSPEND); break;
   case CHI_EVENT_PROF_DISABLED: break;
   case CHI_EVENT_PROF_ENABLED: break;
   case CHI_EVENT_PROF_TRACE: SPAYLOAD(StackTrace, &e->payload->PROF_TRACE); break;
   case CHI_EVENT_SIGNAL: SPAYLOAD(Signal, &e->payload->SIGNAL); break;
   case CHI_EVENT_STACK_ACTIVATE: SPAYLOAD(Stack, &e->payload->STACK_ACTIVATE); break;
   case CHI_EVENT_STACK_DEACTIVATE: SPAYLOAD(Stack, &e->payload->STACK_DEACTIVATE); break;
   case CHI_EVENT_STACK_GROW: SPAYLOAD(StackSize, &e->payload->STACK_GROW); break;
   case CHI_EVENT_STACK_SCANNED: SPAYLOAD(Stack, &e->payload->STACK_SCANNED); break;
   case CHI_EVENT_STACK_SHRINK: SPAYLOAD(StackSize, &e->payload->STACK_SHRINK); break;
   case CHI_EVENT_STACK_TRACE: SPAYLOAD(StackTrace, &e->payload->STACK_TRACE); break;
   case CHI_EVENT_STRBUILDER_OVERFLOW: break;
   case CHI_EVENT_SYSTEM_STATS: SPAYLOAD(SystemStats, &e->payload->SYSTEM_STATS); break;
   case CHI_EVENT_THREAD_ENQUEUE: SPAYLOAD(ThreadEnqueue, &e->payload->THREAD_ENQUEUE); break;
   case CHI_EVENT_THREAD_MIGRATE: SPAYLOAD(ThreadMigrate, &e->payload->THREAD_MIGRATE); break;
   case CHI_EVENT_THREAD_NAME: SPAYLOAD(ThreadName, &e->payload->THREAD_NAME); break;
   case CHI_EVENT_THREAD_NEW: SPAYLOAD(ThreadNew, &e->payload->THREAD_NEW); break;
   case CHI_EVENT_THREAD_SWITCH: SPAYLOAD(ThreadNext, &e->payload->THREAD_SWITCH); break;
   case CHI_EVENT_THREAD_TAKEOVER: SPAYLOAD(ThreadMigrate, &e->payload->THREAD_TAKEOVER); break;
   case CHI_EVENT_THREAD_TERMINATED: break;
   case CHI_EVENT_THREAD_YIELD: SPAYLOAD(ThreadYield, &e->payload->THREAD_YIELD); break;
   case CHI_EVENT_USER_BUFFER: SPAYLOAD(UserBuffer, &e->payload->USER_BUFFER); break;
   case CHI_EVENT_USER_STRING: SPAYLOAD(UserString, &e->payload->USER_STRING); break;
   case CHI_EVENT_WORKER_DESTROY: break;
   case CHI_EVENT_WORKER_INIT: break;
   case CHI_EVENT_WORKER_NAME: SPAYLOAD(WorkerName, &e->payload->WORKER_NAME); break;
   default: break;
   }
SEVENT_END

#undef SBLOCK_BEGIN
#undef SBLOCK_END
#undef SBOOL
#undef SBYTES
#undef SENUM
#undef SEVENT_BEGIN
#undef SEVENT_END
#undef SFIELD
#undef SDEC
#undef SHEX
#undef SPAYLOAD
#undef SPAYLOAD_BEGIN
#undef SPAYLOAD_END
#undef SSTR
