#pragma once

// Disable C++ warning for zero-size structs
#define _DATA(d, a) (sizeof (*(d)) ? &(d)->a : 0)
#define INSTRUMENT_APPEND(s, u, v)                                      \
    typedef struct { u##Data a; v##Data b; } s##Data;          \
    CHI_INL void s##Alloc(ChiProcessor* proc, s##Data *l, Chili fn, Chili* frame, size_t size)           { u##Alloc(proc, _DATA(l, a), fn, frame, size);      v##Alloc(proc, _DATA(l, b), fn, frame, size);      } \
    CHI_INL void s##Enter(ChiProcessor* proc, s##Data *l, Chili fn, Chili* frame, bool jmp)              { u##Enter(proc, _DATA(l, a), fn, frame, jmp);       v##Enter(proc, _DATA(l, b), fn, frame, jmp);       } \
    CHI_INL void s##Leave(ChiProcessor* proc, s##Data *l, Chili fn, Chili* frame)                        { u##Leave(proc, _DATA(l, a), fn, frame);            v##Leave(proc, _DATA(l, b), fn, frame);            } \
    CHI_INL void s##FFITail(ChiProcessor* proc, s##Data *l, Chili fn, Chili* frame, const CbyCode* ffi)  { u##FFITail(proc, _DATA(l, a), fn, frame, ffi);     v##FFITail(proc, _DATA(l, b), fn, frame, ffi);    } \
    CHI_INL void s##FFIBegin(ChiProcessor* proc, s##Data *l, Chili fn, Chili* frame, const CbyCode* ffi) { u##FFIBegin(proc, _DATA(l, a), fn, frame, ffi);    v##FFIBegin(proc, _DATA(l, b), fn, frame, ffi);    } \
    CHI_INL void s##FFIEnd(ChiProcessor* proc, s##Data *l, Chili fn, Chili* frame, const CbyCode* ffi)   { u##FFIEnd(proc, _DATA(l, a), fn, frame, ffi);      v##FFIEnd(proc, _DATA(l, b), fn, frame, ffi);      } \
    CHI_INL void s##InsnBegin(ChiProcessor* proc, s##Data *l, const CbyCode* ip, Opcode op)              { u##InsnBegin(proc, _DATA(l, a), ip, op);           v##InsnBegin(proc, _DATA(l, b), ip, op);           } \
    CHI_INL void s##InsnEnd(ChiProcessor* proc, s##Data *l, Opcode op)                                   { u##InsnEnd(proc, _DATA(l, a), op);                 v##InsnEnd(proc, _DATA(l, b), op);                 } \
    CHI_INL void s##Setup(CbyInterpreter* interp, s##Data *g)                                            { u##Setup(interp, _DATA(g, a));                     v##Setup(interp, _DATA(g, b));                     } \
    CHI_INL void s##Destroy(CbyInterpreter* interp, s##Data *g)                                          { u##Destroy(interp, _DATA(g, a));                   v##Destroy(interp, _DATA(g, b));                   } \
    CHI_INL void s##ProcStart(ChiProcessor* proc, s##Data *l, s##Data *g)                              { u##ProcStart(proc, _DATA(l, a), _DATA(g, a));      v##ProcStart(proc, _DATA(l, b), _DATA(g, b));      } \
    CHI_INL void s##ProcStop(ChiProcessor* proc, s##Data *l, s##Data *g)                               { u##ProcStop(proc, _DATA(l, a), _DATA(g, a));       v##ProcStop(proc, _DATA(l, b), _DATA(g, b));       }
