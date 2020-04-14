#pragma once

#define RET(r)                ({ A(0) = (r); JUMP_FN(SP[-1]); })
#define JUMP_FN(c)            JUMP(_chiContFn(c))
#define JUMP_FN0              JUMP_FN(chiIdx(A(0), 0))
#define KNOWN_JUMP(n)         JUMP(CHI_CONT_FN(n))

// read only registers, since we want to track writes in the code explicitly
#define HLRW                (_chiReg->hl)
#define AUX                 (_chiReg->aux)
#define SL                  ((Chili* const)SLRW)
#define HL                  ((ChiWord* const)HLRW)

#define _CHI_TRACE_TRIGGER  CHI_AND(CHI_TRACEPOINTS_CONT_ENABLED, atomic_load_explicit(&AUX.TRACEPOINT, memory_order_relaxed))
#define TRACE_TIME          ({ if (CHI_UNLIKELY(_CHI_TRACE_TRIGGER > 0)) _chiTraceContTime(_chiCurrentCont, SP); })
#define TRACE_TIME_NAME(n)  ({ if (CHI_UNLIKELY(_CHI_TRACE_TRIGGER > 0)) _chiTraceContTimeName(_chiCurrentCont, SP, (n)); })
#define TRACE_ALLOC(n)      ({ if (CHI_UNLIKELY(_CHI_TRACE_TRIGGER < 0)) _chiTraceContAlloc(_chiCurrentCont, SP, (n)); })
#define TRACE_FFI(n)        TRACE_TIME_NAME("ffi " #n)

#define PROTECT(a)          ({ _chiProtectBegin(SP); A(0) = (a); KNOWN_JUMP(_chiProtectEnd); })
#define PROTECT_VOID(a)     PROTECT(({ (a); CHI_FALSE; }))

#ifndef CHI_GUID
#  error CHI_GUID must be defined
#endif

// Use prefix data attribute
#define _CHI_CONT_PREFIX_INFO0(n)   __attribute__ ((aligned(CHI_CONT_PREFIX_ALIGN), used))
#define _CHI_CONT_PREFIX_FN0(n)     __prefix_data__(CHI_CAT(n, _info)) __attribute__ ((aligned(CHI_CONT_PREFIX_ALIGN)))

// Use prefix data sections
#define _CHI_CONT_PREFIX_INFO1(n)   __attribute__ ((aligned(CHI_CONT_PREFIX_ALIGN), section(".text$" CHI_STRINGIZE(CHI_GUID) "." #n ".0"), used))
#define _CHI_CONT_PREFIX_FN1(n)     __attribute__ ((aligned(CHI_CONT_PREFIX_ALIGN), section(".text$" CHI_STRINGIZE(CHI_GUID) "." #n ".1")))

// check prefix data
#define _CHI_CONT_PREFIX_VALID0(n)  true
#define _CHI_CONT_PREFIX_VALID1(n)  ({ const void* __volatile__ addr1 = CHI_CHOICE(CHI_CONT_PREFIX_SECTION, &CHI_CAT(n, _info), 0); \
                                       const void* __volatile__ addr2 = CHI_CHOICE(CHI_CONT_PREFIX_SECTION, (const uint8_t*)n - CHI_CONT_PREFIX_ALIGN, 0); \
                                       (const uint8_t*)addr1 == (const uint8_t*)addr2; })

// Use prefix data
#define _CHI_CONT1(n, static_cont, attr_fn, ...)                        \
    CHI_IF(CHI_LOC_ENABLED,                                             \
    static _ChiContInfoLoc CHI_CAT(n, _loc) = {                         \
        .file = __FILE__, .name = #n, .line = __LINE__                  \
    };)                                                                 \
    CHI_CAT(_CHI_CONT_PREFIX_INFO, CHI_CONT_PREFIX_SECTION)(n) static ChiContInfo CHI_CAT(n, _info) = { \
        CHI_IF(CHI_LOC_ENABLED, .loc = &CHI_CAT(n, _loc),)              \
        .trap = CHI_ARCH_TRAP_OP, ##__VA_ARGS__                         \
    };                                                                  \
    CHI_CAT(_CHI_CONT_PREFIX_FN, CHI_CONT_PREFIX_SECTION)(n) static_cont attr_fn CHI_CONT_PROTO(n)
#define _CHI_CONT_INFO1(n) CHI_CAT(n, _info)

// Use indirection
#define _CHI_CONT0(n, static_cont, attr_fn, ...)                        \
    static_cont attr_fn CHI_CONT_PROTO(CHI_CAT(n, _fn));                \
    CHI_WARN_OFF(c++-compat, redundant-decls)                           \
    static_cont ChiContInfo n = {                                       \
        CHI_IF(CHI_LOC_ENABLED, .file = __FILE__, .name = #n, .line = __LINE__,) \
        .fn = CHI_CAT(n, _fn), ##__VA_ARGS__                            \
    };                                                                  \
    CHI_WARN_ON                                                         \
    static_cont attr_fn CHI_CONT_PROTO(CHI_CAT(n, _fn))
#define _CHI_CONT_INFO0(n) n

#define CONT(n, ...)        ATTR_CONT(n,,, __VA_ARGS__)
#define STATIC_CONT(n, ...) ATTR_CONT(n, static,, __VA_ARGS__)
#define INTERN_CONT(n, ...) ATTR_CONT(n, CHI_INTERN,, __VA_ARGS__)
#define ATTR_CONT(...)      CHI_CAT(_CHI_CONT, CHI_CONT_PREFIX)(__VA_ARGS__)

#define PROLOGUE(c)                                                     \
    CHI_ASSERT(!strcmp(CHI_STRINGIZE(CHI_CONT_FN(c)), __func__));       \
    CHI_ASSERT(CHI_CAT(_CHI_CONT_PREFIX_VALID, CHI_CONT_PREFIX)(c));    \
    _PROLOGUE(CHI_CAT(_CHI_CONT_INFO, CHI_CONT_PREFIX)(c).na);          \
    CHI_IF(CHI_POISON_ENABLED,                                          \
           for (uint32_t i = CHI_CAT(_CHI_CONT_INFO, CHI_CONT_PREFIX)(c).na + 1; i < CHI_AMAX; ++i) \
               A(i) = _CHILI_POISON;)                                   \
    const ChiCont _chiCurrentCont = &c;                                 \
    CHI_IF(CHI_FNLOG_ENABLED, if (AUX.FNLOG) chiFnLog(_chiCurrentCont);); \
    TRACE_TIME

#define NEW_PAYLOAD(type, var) ((type*)_chiNewPayload##var)

#define NEW_LARGE(var, type, size)                      \
    Chili var = chiNew((type), (size));                 \
    void *_chiNewPayload##var = _chiPayload(var);       \
    ({})

#define _CHI_NEW(s, var, type, size)                                    \
    Chili var;                                                          \
    void *_chiNewPayload##var;                                          \
    ({                                                                  \
        const size_t s = (size);                                        \
        CHI_ASSERT(s <= CHI_MAX_UNPINNED);                              \
        var = _chiWrap(_chiNewPayload##var = HP, s, (type), CHI_GEN_NURSERY); \
        HP += s;                                                        \
        ({});                                                           \
    })
#define NEW(var, type, size) _CHI_NEW(CHI_GENSYM, var, (type), (size))

#define NEW_INIT(var, idx, x)  _chiInit(var, NEW_PAYLOAD(Chili, var) + (idx), (x))
#define NEW_INIT_THUNK(var, x) _chiFieldInit(&NEW_PAYLOAD(ChiThunk, var)->val, (x));

#define _CHI_LIMITS_SAVE(sl, hl, params, save, param1, ...)             \
    ({                                                                  \
        const struct { size_t stack, heap; bool interrupt; } params = { param1, ##__VA_ARGS__ }; \
        CHI_ASSERT(params.heap <= CHI_BLOCK_MINLIMIT);                  \
        CHI_ASSERT(!params.interrupt || !params.heap);                  \
        if (params.heap) TRACE_ALLOC(params.heap);                      \
        Chili* sl = SP + params.stack;                                  \
        ChiWord* hl = HP + params.heap;                                 \
        if (CHI_UNLIKELY((params.stack && sl > SL)                      \
                         || (params.interrupt && !HL)                   \
                         || (params.heap && hl > HL))) {                \
            if (params.stack) SLRW = sl;                                \
            if (params.heap) HLRW = hl;                                 \
            save;                                                       \
        }                                                               \
    })
#define LIMITS_SAVE(...) _CHI_LIMITS_SAVE(CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, __VA_ARGS__)
#define LIMITS(...)      _CHI_LIMITS_SAVE(CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, ({ *SP = chiFromCont(_chiCurrentCont); KNOWN_JUMP(_chiInterrupt); }), __VA_ARGS__)

/* Grow heap. Does update HL to ensure that we do not have have HP > HL afterwards.
 * It might be that HL=0 after GROW_HEAP when an interrupt is pending,
 * but it will be guaranteed that enough memory is available.
 */
#define _CHI_GROW_HEAP(hl, n)                   \
    ({                                          \
        ChiWord* hl = HP + (n);                 \
        if (CHI_UNLIKELY(hl > HL))              \
            HP = _chiGrowHeap(HP, hl);          \
    })
#define GROW_HEAP(n) _CHI_GROW_HEAP(CHI_GENSYM, (n))

#define _CHI_FORCE(thk, val, thk_val)           \
    ({                                          \
        Chili thk = (thk_val), val;             \
        if (_chiThunkForced(thk, &val))         \
            RET(val);                           \
        A(0) = val;                             \
        A(1) = thk;                             \
        KNOWN_JUMP(_chiThunkForce);             \
    })
#define FORCE(t) _CHI_FORCE(CHI_GENSYM, CHI_GENSYM, (t))

#define _CHI_APP(args, args_val)                \
    ({                                          \
        const size_t args = (args_val);         \
        CHI_ASSERT(args < CHI_AMAX);            \
        if (args == _chiFnArity(A(0)))          \
            JUMP_FN0;                           \
        if (args - 1 < CHI_DIM(_chiAppTable))   \
            JUMP(_chiAppTable[args - 1]);       \
        AUX.APPN = (uint8_t)args;               \
        KNOWN_JUMP(_chiAppN);                   \
    })
#define APP(args) _CHI_APP(CHI_GENSYM, (args))

#define _CHI_OVERAPP(idx, rest, args, arity, args_val, arity_val)       \
    ({                                                                  \
        const size_t args = (args_val), arity = (arity_val), rest = args - arity; \
        CHI_ASSUME(arity < args);                                       \
        CHI_ASSUME(rest > 0);                                           \
        CHI_ASSUME(rest < args);                                        \
        for (size_t idx = 0; idx < rest; ++idx)                         \
            SP[idx] = A(1 + arity + idx);                               \
        SP += rest;                                                     \
        if ((__builtin_constant_p(args_val) ? args - 2 : rest - 1) < CHI_DIM(_chiOverAppTable)) { \
            *SP++ = chiFromCont(_chiOverAppTable[rest - 1]);            \
        } else {                                                        \
            *SP++ = _chiFromUnboxed(rest + 2);                          \
            *SP++ = chiFromCont(&_chiOverAppN);                         \
        }                                                               \
    })

#define KNOWN_OVERAPP(f, args, arity) ({ _CHI_OVERAPP(CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, (args), (arity)); KNOWN_JUMP(f); })
#define OVERAPP(args, arity)          ({ _CHI_OVERAPP(CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, (args), (arity)); JUMP_FN0; })

#define _CHI_PARTIAL(idx, cont, clos, args, arity, args_val, arity_val) \
    ({                                                                  \
        const size_t args = (args_val), arity = (arity_val);            \
        CHI_ASSERT(args < arity);                                       \
        CHI_ASSUME(args <= CHI_AMAX);                                   \
        ChiCont cont;                                                   \
        switch (__builtin_constant_p(args_val) ? args : 0) {            \
        case 1: cont = _chiPartialTable1[arity - 2]; break;             \
        case 2: cont = _chiPartialTable2[arity - 3]; break;             \
        case 3: cont = _chiPartialTable3[arity - 4]; break;             \
        default: cont = &_chiPartialNofM; break;                        \
        }                                                               \
        NEW(clos, CHI_FN(arity - args), args + 2);                      \
        NEW_INIT(clos, 0, chiFromCont(cont));                           \
        for (size_t idx = 0; idx <= args; ++idx)                        \
            NEW_INIT(clos, idx + 1, A(idx));                            \
        RET(clos);                                                      \
    })
#define PARTIAL(args, arity) _CHI_PARTIAL(CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, (args), (arity))

#ifdef CHI_DYNLIB

#define _CHI_MODULE_EXPORT_NAME(n, ns, i) ns,
#define _CHI_MODULE_EXPORT_IDX(n, ns, i)  i,
#define MODULE_EXPORTS(e)                                              \
    static const char* const _chiModuleExportName[] = { e(_CHI_MODULE_EXPORT_NAME) }; \
    static const int32_t _chiModuleExportIdx[] = { e(_CHI_MODULE_EXPORT_IDX) };

#define _CHI_MODULE_IMPORT_NAME(n, ns) ns,
#define MODULE_IMPORTS(i)                                           \
    static const char* const _chiModuleImportName[] = { i(_CHI_MODULE_IMPORT_NAME) };

#define MODULE_INIT(n, ns, i)                                           \
    static const ChiModuleDesc n##_desc =                               \
        { .name        = ns,                                            \
          .init        = &(i),                                          \
          .importCount = CHI_DIM(_chiModuleImportName),                 \
          .importName  = _chiModuleImportName,                          \
          .exportCount = CHI_DIM(_chiModuleExportName),                 \
          .exportName  = _chiModuleExportName,                          \
          .exportIdx   = _chiModuleExportIdx,                           \
        };                                                              \
    extern const ChiModuleDesc* n;                                      \
    __attribute__ ((section("chi_module_registry")))                    \
    const ChiModuleDesc* n = &n##_desc;

#else

#define _CHI_MODULE_EXPORT_VAR(n, ns, i) extern const int32_t n; const int32_t n = i;
#define MODULE_EXPORTS(e) e(_CHI_MODULE_EXPORT_VAR)

#define _CHI_MODULE_IMPORT_DECL(n, ns) CHI_EXTERN_CONT_DECL(n)
#define _CHI_MODULE_IMPORT_REF(n, ns) &n,
#define MODULE_IMPORTS(i) \
    i(_CHI_MODULE_IMPORT_DECL) \
    static ChiCont _chiModuleImport[] = { i(_CHI_MODULE_IMPORT_REF) };

#define MODULE_INIT(n, ns, i)                                   \
    CHI_EXTERN_CONT_DECL(n)                                     \
    CONT(n) {                                                   \
        PROLOGUE(n);                                            \
        A(0) = chiFromCont(&i);                                 \
        A(1) = chiFromUInt32(CHI_DIM(_chiModuleImport));        \
        A(2) = chiFromPtr(_chiModuleImport);                    \
        KNOWN_JUMP(chiInitModule);                              \
    }

#endif

#include <chili.h>
