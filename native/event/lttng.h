// Generated by generate.pl from defs.in

#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER chili

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "../event/lttng.h"

#if !defined(_CHI_EVENT_LTTNG_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _CHI_EVENT_LTTNG_H

#include "../processor.h"
#include <chili/object/thread.h>
#include <lttng/tracepoint.h>

TRACEPOINT_EVENT(
    chili,
    BEGIN,
    TP_ARGS(const ChiRuntime*, rt, const ChiEventVersion*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
        ctf_integer(uint32_t, version, d->version)
    )
)

TRACEPOINT_EVENT(
    chili,
    END,
    TP_ARGS(const ChiRuntime*, rt),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
    )
)

TRACEPOINT_EVENT(
    chili,
    PROC_INIT,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    PROC_DESTROY,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    PROC_RESUME_REQ,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventResumeReq*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_integer(uint32_t, count, d->count)
    )
)

TRACEPOINT_EVENT(
    chili,
    PROC_TICK,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    WORKER_INIT,
    TP_ARGS(const ChiWorker*, worker),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)worker->rt)
        ctf_integer(uint32_t, wid, worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    WORKER_DESTROY,
    TP_ARGS(const ChiWorker*, worker),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)worker->rt)
        ctf_integer(uint32_t, wid, worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    WORKER_NAME,
    TP_ARGS(const ChiWorker*, worker, const ChiEventWorkerName*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)worker->rt)
        ctf_integer(uint32_t, wid, worker->wid)
        ctf_sequence_text(uint8_t, name, d->name.bytes, uint32_t, d->name.size)
    )
)

TRACEPOINT_EVENT(
    chili,
    MODULE_LOAD,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventModuleLoad*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_sequence_text(uint8_t, module, d->module.bytes, uint32_t, d->module.size)
        ctf_sequence_text(uint8_t, file, d->file.bytes, uint32_t, d->file.size)
        ctf_sequence_text(uint8_t, path, d->path.bytes, uint32_t, d->path.size)
    )
)

TRACEPOINT_EVENT(
    chili,
    MODULE_UNLOAD,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventModuleName*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_sequence_text(uint8_t, module, d->module.bytes, uint32_t, d->module.size)
    )
)

TRACEPOINT_EVENT(
    chili,
    MODULE_INIT,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventModuleName*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_sequence_text(uint8_t, module, d->module.bytes, uint32_t, d->module.size)
    )
)

TRACEPOINT_EVENT(
    chili,
    FFI_LOAD,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventFFI*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_sequence_text(uint8_t, module, d->module.bytes, uint32_t, d->module.size)
        ctf_sequence_text(uint8_t, name, d->name.bytes, uint32_t, d->name.size)
    )
)

TRACEPOINT_EVENT(
    chili,
    TRACE_FFI,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventTraceFFI*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_sequence_text(uint8_t, name, d->name.bytes, uint32_t, d->name.size)
    )
)

TRACEPOINT_EVENT(
    chili,
    TRACE_ENTER,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventTrace*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_sequence_text(uint8_t, module, d->module.bytes, uint32_t, d->module.size)
        ctf_sequence_text(uint8_t, fn, d->fn.bytes, uint32_t, d->fn.size)
        ctf_sequence_text(uint8_t, file, d->file.bytes, uint32_t, d->file.size)
        ctf_integer(size_t, size, d->size)
        ctf_integer(uint32_t, line, d->line)
        ctf_integer(bool, interp, d->interp)
    )
)

TRACEPOINT_EVENT(
    chili,
    TRACE_LEAVE,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventTrace*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_sequence_text(uint8_t, module, d->module.bytes, uint32_t, d->module.size)
        ctf_sequence_text(uint8_t, fn, d->fn.bytes, uint32_t, d->fn.size)
        ctf_sequence_text(uint8_t, file, d->file.bytes, uint32_t, d->file.size)
        ctf_integer(size_t, size, d->size)
        ctf_integer(uint32_t, line, d->line)
        ctf_integer(bool, interp, d->interp)
    )
)

TRACEPOINT_EVENT(
    chili,
    TRACE_ENTER_JMP,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventTrace*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_sequence_text(uint8_t, module, d->module.bytes, uint32_t, d->module.size)
        ctf_sequence_text(uint8_t, fn, d->fn.bytes, uint32_t, d->fn.size)
        ctf_sequence_text(uint8_t, file, d->file.bytes, uint32_t, d->file.size)
        ctf_integer(size_t, size, d->size)
        ctf_integer(uint32_t, line, d->line)
        ctf_integer(bool, interp, d->interp)
    )
)

TRACEPOINT_EVENT(
    chili,
    EXCEPTION_HANDLED,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventException*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_sequence_text(uint8_t, name, d->name.bytes, uint32_t, d->name.size)
        ctf_sequence_text(uint8_t, trace, d->trace.bytes, uint32_t, d->trace.size)
    )
)

TRACEPOINT_EVENT(
    chili,
    EXCEPTION_UNHANDLED,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventException*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_sequence_text(uint8_t, name, d->name.bytes, uint32_t, d->name.size)
        ctf_sequence_text(uint8_t, trace, d->trace.bytes, uint32_t, d->trace.size)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_BLOCK,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_UNBLOCK,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_REQ,
    TP_ARGS(const ChiRuntime*, rt, const ChiEventGCRequest*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
        ctf_integer(uint32_t, trigger, d->trigger)
        ctf_integer(uint32_t, requestor, d->requestor)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_MARK_STATS,
    TP_ARGS(const ChiWorker*, worker, const ChiEventMark*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)worker->rt)
        ctf_integer(uint32_t, wid, worker->wid)
        ctf_integer(size_t, object_count, d->object.count)
        ctf_integer(size_t, object_words, d->object.words)
        ctf_integer(size_t, stack_count, d->stack.count)
        ctf_integer(size_t, stack_words, d->stack.words)
        ctf_integer(size_t, collapsed, d->collapsed)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_SWEEP_STATS,
    TP_ARGS(const ChiWorker*, worker, const ChiEventSweep*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)worker->rt)
        ctf_integer(uint32_t, wid, worker->wid)
        ctf_integer(size_t, small_live_count, d->small.live.count)
        ctf_integer(size_t, small_live_words, d->small.live.words)
        ctf_integer(size_t, small_free_count, d->small.free.count)
        ctf_integer(size_t, small_free_words, d->small.free.words)
        ctf_integer(size_t, medium_live_count, d->medium.live.count)
        ctf_integer(size_t, medium_live_words, d->medium.live.words)
        ctf_integer(size_t, medium_free_count, d->medium.free.count)
        ctf_integer(size_t, medium_free_words, d->medium.free.words)
        ctf_integer(size_t, large_live_count, d->large.live.count)
        ctf_integer(size_t, large_live_words, d->large.live.words)
        ctf_integer(size_t, large_free_count, d->large.free.count)
        ctf_integer(size_t, large_free_words, d->large.free.words)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_SWEEP_NOTIFY,
    TP_ARGS(const ChiRuntime*, rt),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
    )
)

TRACEPOINT_EVENT(
    chili,
    NURSERY_RESIZE,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventNursery*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(size_t, oldLimit, d->oldLimit)
        ctf_integer(size_t, newLimit, d->newLimit)
    )
)

TRACEPOINT_EVENT(
    chili,
    HEAP_BEFORE_SCAV,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventHeapUsage*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(size_t, major_small_allocSinceSweep, d->major.small.allocSinceSweep)
        ctf_integer(uint64_t, major_small_allocSinceStart, d->major.small.allocSinceStart)
        ctf_integer(size_t, major_small_totalWords, d->major.small.totalWords)
        ctf_integer(size_t, major_medium_allocSinceSweep, d->major.medium.allocSinceSweep)
        ctf_integer(uint64_t, major_medium_allocSinceStart, d->major.medium.allocSinceStart)
        ctf_integer(size_t, major_medium_totalWords, d->major.medium.totalWords)
        ctf_integer(size_t, major_large_allocSinceSweep, d->major.large.allocSinceSweep)
        ctf_integer(uint64_t, major_large_allocSinceStart, d->major.large.allocSinceStart)
        ctf_integer(size_t, major_large_totalWords, d->major.large.totalWords)
        ctf_integer(size_t, minor_usedWords, d->minor.usedWords)
        ctf_integer(size_t, minor_totalWords, d->minor.totalWords)
        ctf_integer(size_t, totalWords, d->totalWords)
    )
)

TRACEPOINT_EVENT(
    chili,
    HEAP_AFTER_SCAV,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventHeapUsage*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(size_t, major_small_allocSinceSweep, d->major.small.allocSinceSweep)
        ctf_integer(uint64_t, major_small_allocSinceStart, d->major.small.allocSinceStart)
        ctf_integer(size_t, major_small_totalWords, d->major.small.totalWords)
        ctf_integer(size_t, major_medium_allocSinceSweep, d->major.medium.allocSinceSweep)
        ctf_integer(uint64_t, major_medium_allocSinceStart, d->major.medium.allocSinceStart)
        ctf_integer(size_t, major_medium_totalWords, d->major.medium.totalWords)
        ctf_integer(size_t, major_large_allocSinceSweep, d->major.large.allocSinceSweep)
        ctf_integer(uint64_t, major_large_allocSinceStart, d->major.large.allocSinceStart)
        ctf_integer(size_t, major_large_totalWords, d->major.large.totalWords)
        ctf_integer(size_t, minor_usedWords, d->minor.usedWords)
        ctf_integer(size_t, minor_totalWords, d->minor.totalWords)
        ctf_integer(size_t, totalWords, d->totalWords)
    )
)

TRACEPOINT_EVENT(
    chili,
    BLOCK_CHUNK_NEW,
    TP_ARGS(const ChiRuntime*, rt, const ChiEventChunk*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
        ctf_integer(size_t, heapSize, d->heapSize)
        ctf_integer(size_t, size, d->size)
        ctf_integer(size_t, align, d->align)
        ctf_integer_hex(uintptr_t, start, d->start)
    )
)

TRACEPOINT_EVENT(
    chili,
    BLOCK_CHUNK_FREE,
    TP_ARGS(const ChiRuntime*, rt, const ChiEventChunk*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
        ctf_integer(size_t, heapSize, d->heapSize)
        ctf_integer(size_t, size, d->size)
        ctf_integer(size_t, align, d->align)
        ctf_integer_hex(uintptr_t, start, d->start)
    )
)

TRACEPOINT_EVENT(
    chili,
    HEAP_CHUNK_NEW,
    TP_ARGS(const ChiRuntime*, rt, const ChiEventChunk*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
        ctf_integer(size_t, heapSize, d->heapSize)
        ctf_integer(size_t, size, d->size)
        ctf_integer(size_t, align, d->align)
        ctf_integer_hex(uintptr_t, start, d->start)
    )
)

TRACEPOINT_EVENT(
    chili,
    HEAP_CHUNK_FREE,
    TP_ARGS(const ChiRuntime*, rt, const ChiEventChunk*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
        ctf_integer(size_t, heapSize, d->heapSize)
        ctf_integer(size_t, size, d->size)
        ctf_integer(size_t, align, d->align)
        ctf_integer_hex(uintptr_t, start, d->start)
    )
)

TRACEPOINT_EVENT(
    chili,
    HEAP_LIMIT,
    TP_ARGS(const ChiRuntime*, rt, const ChiEventHeapLimit*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
        ctf_integer(uint32_t, limit, d->limit)
        ctf_integer(size_t, heapSize, d->heapSize)
        ctf_integer(size_t, softLimit, d->softLimit)
        ctf_integer(size_t, hardLimit, d->hardLimit)
    )
)

TRACEPOINT_EVENT(
    chili,
    HEAP_ALLOC_FAILED,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventHeapAlloc*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_sequence_text(uint8_t, type, d->type.bytes, uint32_t, d->type.size)
        ctf_integer(size_t, size, d->size)
    )
)

TRACEPOINT_EVENT(
    chili,
    PAR,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
    )
)

TRACEPOINT_EVENT(
    chili,
    STACK_GROW,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventStackSize*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_integer(size_t, reqSize, d->reqSize)
        ctf_integer(size_t, oldSize, d->oldSize)
        ctf_integer(size_t, newSize, d->newSize)
        ctf_integer(size_t, usedSize, d->usedSize)
    )
)

TRACEPOINT_EVENT(
    chili,
    STACK_SHRINK,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventStackSize*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_integer(size_t, reqSize, d->reqSize)
        ctf_integer(size_t, oldSize, d->oldSize)
        ctf_integer(size_t, newSize, d->newSize)
        ctf_integer(size_t, usedSize, d->usedSize)
    )
)

TRACEPOINT_EVENT(
    chili,
    STACK_TRACE,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventStackTrace*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_sequence_text(uint8_t, trace, d->trace.bytes, uint32_t, d->trace.size)
    )
)

TRACEPOINT_EVENT(
    chili,
    STACK_BLACKHOLE,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventBlackhole*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_integer(size_t, count, d->count)
    )
)

TRACEPOINT_EVENT(
    chili,
    PROF_TRACE,
    TP_ARGS(const ChiWorker*, worker, const ChiEventStackTrace*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)worker->rt)
        ctf_integer(uint32_t, wid, worker->wid)
        ctf_sequence_text(uint8_t, trace, d->trace.bytes, uint32_t, d->trace.size)
    )
)

TRACEPOINT_EVENT(
    chili,
    PROF_ENABLED,
    TP_ARGS(const ChiWorker*, worker),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)worker->rt)
        ctf_integer(uint32_t, wid, worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    PROF_DISABLED,
    TP_ARGS(const ChiWorker*, worker),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)worker->rt)
        ctf_integer(uint32_t, wid, worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    THREAD_NAME,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventThreadName*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint32_t, tid, d->tid)
        ctf_sequence_text(uint8_t, name, d->name.bytes, uint32_t, d->name.size)
    )
)

TRACEPOINT_EVENT(
    chili,
    THREAD_NEW,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventThreadNew*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_integer(uint32_t, tid, d->tid)
        ctf_integer(uint32_t, count, d->count)
    )
)

TRACEPOINT_EVENT(
    chili,
    THREAD_TERMINATED,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventThreadTerm*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_integer(uint32_t, count, d->count)
    )
)

TRACEPOINT_EVENT(
    chili,
    THREAD_BLACKHOLE,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
    )
)

TRACEPOINT_EVENT(
    chili,
    BIGINT_OVERFLOW,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
    )
)

TRACEPOINT_EVENT(
    chili,
    STRBUILDER_OVERFLOW,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
    )
)

TRACEPOINT_EVENT(
    chili,
    TICK,
    TP_ARGS(const ChiRuntime*, rt),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
    )
)

TRACEPOINT_EVENT(
    chili,
    SIGNAL,
    TP_ARGS(const ChiRuntime*, rt, const ChiEventSignal*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
        ctf_integer(uint32_t, sig, d->sig)
    )
)

TRACEPOINT_EVENT(
    chili,
    ACTIVITY,
    TP_ARGS(const ChiRuntime*, rt, const ChiEventActivity*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
        ctf_integer(uint64_t, cpuTimeUser, CHI_UN(Nanos, d->cpuTimeUser))
        ctf_integer(uint64_t, cpuTimeSystem, CHI_UN(Nanos, d->cpuTimeSystem))
        ctf_integer(size_t, residentSize, d->residentSize)
        ctf_integer(uint64_t, pageFault, d->pageFault)
        ctf_integer(uint64_t, pageSwap, d->pageSwap)
        ctf_integer(uint64_t, contextSwitch, d->contextSwitch)
        ctf_integer(uint64_t, diskRead, d->diskRead)
        ctf_integer(uint64_t, diskWrite, d->diskWrite)
    )
)

TRACEPOINT_EVENT(
    chili,
    USER,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventUser*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_sequence_text(uint8_t, data, d->data.bytes, uint32_t, d->data.size)
    )
)

TRACEPOINT_EVENT(
    chili,
    HEAP_CHECK_BEGIN,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    HEAP_CHECK_END,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    HEAP_DUMP_BEGIN,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    HEAP_DUMP_END,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventHeapDump*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_sequence_text(uint8_t, file, d->file.bytes, uint32_t, d->file.size)
    )
)

TRACEPOINT_EVENT(
    chili,
    HEAP_PROF_BEGIN,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    HEAP_PROF_END,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventHeapProf*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(size_t, array_count, d->array.count)
        ctf_integer(size_t, array_words, d->array.words)
        ctf_integer(size_t, bigint_count, d->bigint.count)
        ctf_integer(size_t, bigint_words, d->bigint.words)
        ctf_integer(size_t, box64_count, d->box64.count)
        ctf_integer(size_t, box64_words, d->box64.words)
        ctf_integer(size_t, buffer_count, d->buffer.count)
        ctf_integer(size_t, buffer_words, d->buffer.words)
        ctf_integer(size_t, data_count, d->data.count)
        ctf_integer(size_t, data_words, d->data.words)
        ctf_integer(size_t, ffi_count, d->ffi.count)
        ctf_integer(size_t, ffi_words, d->ffi.words)
        ctf_integer(size_t, fn_count, d->fn.count)
        ctf_integer(size_t, fn_words, d->fn.words)
        ctf_integer(size_t, raw_count, d->raw.count)
        ctf_integer(size_t, raw_words, d->raw.words)
        ctf_integer(size_t, stack_count, d->stack.count)
        ctf_integer(size_t, stack_words, d->stack.words)
        ctf_integer(size_t, string_count, d->string.count)
        ctf_integer(size_t, string_words, d->string.words)
        ctf_integer(size_t, stringbuilder_count, d->stringbuilder.count)
        ctf_integer(size_t, stringbuilder_words, d->stringbuilder.words)
        ctf_integer(size_t, thread_count, d->thread.count)
        ctf_integer(size_t, thread_words, d->thread.words)
        ctf_integer(size_t, thunk_count, d->thunk.count)
        ctf_integer(size_t, thunk_words, d->thunk.words)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_SLICE_BEGIN,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_SLICE_END,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventGCSlice*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint32_t, trigger, d->trigger)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_MARKSWEEP_BEGIN,
    TP_ARGS(const ChiRuntime*, rt),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_MARKSWEEP_END,
    TP_ARGS(const ChiRuntime*, rt),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_MARK_PHASE_BEGIN,
    TP_ARGS(const ChiRuntime*, rt),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_MARK_PHASE_END,
    TP_ARGS(const ChiRuntime*, rt),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_MARK_SLICE_BEGIN,
    TP_ARGS(const ChiWorker*, worker),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)worker->rt)
        ctf_integer(uint32_t, wid, worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_MARK_SLICE_END,
    TP_ARGS(const ChiWorker*, worker),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)worker->rt)
        ctf_integer(uint32_t, wid, worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_SWEEP_PHASE_BEGIN,
    TP_ARGS(const ChiRuntime*, rt),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_SWEEP_PHASE_END,
    TP_ARGS(const ChiRuntime*, rt),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_SWEEP_SLICE_BEGIN,
    TP_ARGS(const ChiWorker*, worker),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)worker->rt)
        ctf_integer(uint32_t, wid, worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_SWEEP_SLICE_END,
    TP_ARGS(const ChiWorker*, worker),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)worker->rt)
        ctf_integer(uint32_t, wid, worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_SCAVENGER_BEGIN,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    GC_SCAVENGER_END,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventScavenger*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(size_t, dirty_object_count, d->dirty.object.count)
        ctf_integer(size_t, dirty_object_words, d->dirty.object.words)
        ctf_integer(size_t, dirty_stack_count, d->dirty.stack.count)
        ctf_integer(size_t, dirty_stack_words, d->dirty.stack.words)
        ctf_integer(size_t, raw_promoted_count, d->raw.promoted.count)
        ctf_integer(size_t, raw_promoted_words, d->raw.promoted.words)
        ctf_integer(size_t, raw_copied_count, d->raw.copied.count)
        ctf_integer(size_t, raw_copied_words, d->raw.copied.words)
        ctf_integer(size_t, object_promoted_count, d->object.promoted.count)
        ctf_integer(size_t, object_promoted_words, d->object.promoted.words)
        ctf_integer(size_t, object_copied_count, d->object.copied.count)
        ctf_integer(size_t, object_copied_words, d->object.copied.words)
        ctf_integer(size_t, object_copied1_count, d->object.copied1.count)
        ctf_integer(size_t, object_copied1_words, d->object.copied1.words)
        ctf_integer(size_t, collapsed, d->collapsed)
        ctf_integer(uint32_t, gen, d->gen)
        ctf_integer(bool, snapshot, d->snapshot)
    )
)

TRACEPOINT_EVENT(
    chili,
    PROC_RUN_BEGIN,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    PROC_RUN_END,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    PROC_SUSPEND_BEGIN,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    PROC_SUSPEND_END,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    PROC_WAIT_SYNC_BEGIN,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    PROC_WAIT_SYNC_END,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    PROC_SYNC_BEGIN,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    PROC_SYNC_END,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
    )
)

TRACEPOINT_EVENT(
    chili,
    THREAD_SCHED_BEGIN,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
    )
)

TRACEPOINT_EVENT(
    chili,
    THREAD_SCHED_END,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
    )
)

TRACEPOINT_EVENT(
    chili,
    THREAD_RUN_BEGIN,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
    )
)

TRACEPOINT_EVENT(
    chili,
    THREAD_RUN_END,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
    )
)

TRACEPOINT_EVENT(
    chili,
    USER_DURATION_BEGIN,
    TP_ARGS(const ChiProcessor*, proc),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
    )
)

TRACEPOINT_EVENT(
    chili,
    USER_DURATION_END,
    TP_ARGS(const ChiProcessor*, proc, const ChiEventUser*, d),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)
        ctf_integer(uint32_t, wid, proc->worker->wid)
        ctf_integer(uint64_t, tid, chiToUnboxed(chiToThread(proc->thread)->tid))
        ctf_sequence_text(uint8_t, data, d->data.bytes, uint32_t, d->data.size)
    )
)

TRACEPOINT_EVENT(
    chili,
    STARTUP_BEGIN,
    TP_ARGS(const ChiRuntime*, rt),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
    )
)

TRACEPOINT_EVENT(
    chili,
    STARTUP_END,
    TP_ARGS(const ChiRuntime*, rt),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
    )
)

TRACEPOINT_EVENT(
    chili,
    SHUTDOWN_BEGIN,
    TP_ARGS(const ChiRuntime*, rt),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
    )
)

TRACEPOINT_EVENT(
    chili,
    SHUTDOWN_END,
    TP_ARGS(const ChiRuntime*, rt),
    TP_FIELDS(
        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)
    )
)

#endif

#include <lttng/tracepoint-event.h>
