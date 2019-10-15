#include "tracepoint.h"

#if CHI_TRACEPOINTS_CONT_ENABLED
void chiTraceContTime(ChiCont cont, Chili* frame) {
    chiTraceContTimeName(cont, frame, 0);
}

void chiTraceContTimeName(ChiCont cont, Chili* frame, const char* name) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CHI_ASSERT(chiTraceTimeTriggered(proc->worker));
    chiTraceHandler(&(ChiTracePoint){ .proc = proc, .worker = proc->worker, .frame = frame,
                                         .cont = cont, .name = name, .thread = true });
}

void chiTraceContAlloc(ChiCont cont, Chili* frame, size_t alloc) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CHI_ASSERT(chiTraceAllocTriggered(proc->worker));
    chiTraceHandler(&(ChiTracePoint){ .proc = proc, .worker = proc->worker, .frame = frame,
                                         .cont = cont, .alloc = alloc, .thread = true });
}
#endif
