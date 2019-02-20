#include "trace.h"

#if CHI_TRACEPOINTS_ENABLED
void chiTraceTime(ChiCont cont, Chili* frame) {
    chiTraceTimeName(cont, frame, 0);
}

void chiTraceTimeName(ChiCont cont, Chili* frame, const char* name) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CHI_ASSERT(chiTraceTimeTriggered(&proc->worker));
    ChiTracePoint tp = { .frame = frame, .cont = cont, .name = name, .thread = true };
    chiTraceHandler(&proc->worker, &tp);
}

void chiTraceAlloc(ChiCont cont, Chili* frame, size_t alloc) {
    ChiProcessor* proc = CHI_CURRENT_PROCESSOR;
    CHI_ASSERT(chiTraceAllocTriggered(&proc->worker));
    ChiTracePoint tp = { .frame = frame, .cont = cont, .alloc = alloc, .thread = true };
    chiTraceHandler(&proc->worker, &tp);
}
#endif
