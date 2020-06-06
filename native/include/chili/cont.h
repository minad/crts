#pragma once

#define RET                   JUMP_FN(SP[-1])
#define JUMP_FN(c)            JUMP(_chiToContFn(c))
#define KNOWN_JUMP(n)         JUMP(CHI_CONT_FN(n))

// read only registers, since we want to track writes in the code explicitly
#define HLRW                (_chiReg->hl)
#define AUX                 (_chiReg->aux)
#define SL                  ((Chili* const)SLRW)
#define HL                  ((ChiWord* const)HLRW)

#define _CHI_POISON_ARGS(idx, n) ({ for (uint32_t idx = (n); idx < CHI_AMAX; ++idx) ASET(idx, _CHILI_POISON); })
#define CHI_POISON_ARGS(n)       _CHI_POISON_ARGS(CHI_GENSYM, (n))
#define UNDEF_ARGS(n)            CHI_CHOICE(CHI_POISON_ENABLED, CHI_POISON_ARGS(n), _UNDEF_ARGS(n))

#define _CHI_AGET(idx, i)    ({ size_t idx = (i); CHI_ASSERT(idx < CHI_AMAX); *_A(idx); })
#define AGET(i)              _CHI_AGET(CHI_GENSYM, (i))

#define _CHI_ASET(idx, i, x) ({ size_t idx = (i); CHI_ASSERT(idx < CHI_AMAX); *_A(idx) = (x); ({}); })
#define ASET(i, x)           _CHI_ASET(CHI_GENSYM, (i), (x))

#define _CHI_AUNDEF(idx, i)  ({ size_t idx = (i); CHI_ASSERT(idx < CHI_AMAX); if (idx + CHI_DIM(_chiReg->a) < CHI_AMAX) CHI_UNDEF(*_A(idx)); })
#define AUNDEF(i)            _CHI_AUNDEF(CHI_GENSYM, (i))

#define _CHI_TRACE_TRIGGER  CHI_AND(CHI_TRACEPOINTS_CONT_ENABLED, atomic_load_explicit(&AUX.TRACEPOINT, memory_order_relaxed))
#define TRACE_TIME          ({ if (CHI_UNLIKELY(_CHI_TRACE_TRIGGER > 0)) _chiTraceContTime(_chiCurrentCont, SP); })
#define TRACE_TIME_NAME(n)  ({ if (CHI_UNLIKELY(_CHI_TRACE_TRIGGER > 0)) _chiTraceContTimeName(_chiCurrentCont, SP, (n)); })
#define TRACE_ALLOC(n)      ({ if (CHI_UNLIKELY(_CHI_TRACE_TRIGGER < 0)) _chiTraceContAlloc(_chiCurrentCont, SP, (n)); })
#define TRACE_FFI(n)        TRACE_TIME_NAME("ffi " #n)

#define PROTECT(a)          ({ _chiProtectBegin(SP); ASET(0, (a)); KNOWN_JUMP(_chiProtectEnd); })
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

#define CONT(n, ...)        ATTR_CONT(n,,, __VA_ARGS__)
#define STATIC_CONT(n, ...) ATTR_CONT(n, static,, __VA_ARGS__)
#define INTERN_CONT(n, ...) ATTR_CONT(n, CHI_INTERN,, __VA_ARGS__)
#define ATTR_CONT(...)      CHI_CAT(_CHI_CONT, CHI_CONT_PREFIX)(__VA_ARGS__)

#define PROLOGUE(c)                                                     \
    CHI_ASSERT(!strcmp(CHI_STRINGIZE(CHI_CONT_FN(c)), __func__));       \
    CHI_ASSERT(CHI_CAT(_CHI_CONT_PREFIX_VALID, CHI_CONT_PREFIX)(c));    \
    _PROLOGUE;                                                          \
    const ChiCont _chiCurrentCont = &c;                                 \
    CHI_IF(CHI_POISON_ENABLED, CHI_POISON_ARGS(_chiContInfo(_chiCurrentCont)->na)); \
    CHI_IF(CHI_FNLOG_ENABLED, if (AUX.FNLOG) chiFnLog(_chiCurrentCont);); \
    TRACE_TIME

#define NEW_PAYLOAD(type, var) ((type*)_chiNewPayload##var)

#define _CHI_NEW_LARGE(s, var, type, size)              \
    const size_t s = (size);                            \
    Chili var = chiNew((type), s);                      \
    void *_chiNewPayload##var = _chiPayload(var);       \
    ({})
#define NEW_LARGE(var, type, size) _CHI_NEW_LARGE(CHI_GENSYM, var, (type), (size))

#define _CHI_NEW(s, var, type, size)                                    \
    const size_t s = (size);                                            \
    CHI_ASSERT(s <= CHI_MAX_UNPINNED);                                  \
    void *_chiNewPayload##var = HP;                                     \
    Chili var = _chiWrap(HP, s, (type), CHI_GEN_NURSERY);               \
    HP += s;                                                            \
    ({})
#define NEW(var, type, size) _CHI_NEW(CHI_GENSYM, var, (type), (size))
#define NEW_INIT(var, idx, x)  _chiInit(var, NEW_PAYLOAD(Chili, var) + (idx), (x))

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
#define LIMITS_SAVE(...)    _CHI_LIMITS_SAVE(CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, __VA_ARGS__)
#define LIMITS_PROC(...)    _CHI_LIMITS_SAVE(CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, ({ AUX.YIELD = chiFromCont(_chiCurrentCont); KNOWN_JUMP(chiYield); }), __VA_ARGS__)
#define LIMITS_CLOS(...)    _CHI_LIMITS_SAVE(CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, KNOWN_JUMP(_chiYieldClos), __VA_ARGS__)
#define LIMITS_CONT(...)    _CHI_LIMITS_SAVE(CHI_GENSYM, CHI_GENSYM, CHI_GENSYM, KNOWN_JUMP(_chiYieldCont), __VA_ARGS__)

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

// Update frame needs two stack slots!
#define _CHI_PUSH_UPDATE(thk, val)                                      \
    ({                                                                  \
        Chili thk = (val);                                              \
        _chiFieldInit(&_chiToThunk(thk)->cont, chiFromCont(&_chiThunkBlackhole)); \
        SP[0] = thk;                                                    \
        SP[1] = chiFromCont(&_chiThunkUpdateCont);                      \
        SP += 2;                                                        \
    })
#define PUSH_UPDATE(t) _CHI_PUSH_UPDATE(CHI_GENSYM, (t))

#define _CHI_FORCE(thk, val, ret)                                       \
    ({                                                                  \
        Chili thk = (val);                                              \
        ASET(0, thk);                                                   \
        if ((CHI_MARK_COLLAPSE_ENABLED ||                               \
             CHI_SCAV_COLLAPSE_ENABLED) &&                              \
            !_chiRefType(thk, CHI_THUNK))                               \
            ret;                                                        \
        else                                                            \
            JUMP_FN(_chiFieldRead(&_chiToThunk(thk)->cont));            \
    })
#define FORCE(t, ret) _CHI_FORCE(CHI_GENSYM, (t), ret)

#define _CHI_APP(args, args_val)                \
    ({                                          \
        const size_t args = (args_val);         \
        CHI_ASSERT(args < CHI_AMAX);            \
        if (args == _chiFnArity(AGET(0)))       \
            JUMP_FN(chiIdx(AGET(0), 0));        \
        if (args - 1 < CHI_DIM(_chiAppTable))   \
            JUMP(_chiAppTable[args - 1]);       \
        AUX.VAARGS = (uint8_t)(args + 1);       \
        KNOWN_JUMP(_chiAppN);                   \
    })
#define APP(args) _CHI_APP(CHI_GENSYM, (args))

#ifdef CHI_DYNLIB

#define _CHI_MODULE_IMPORT_NAME(n, ns) ns,
#define MODULE_IMPORTS(i)                                           \
    static const char* const _chiModuleImportName[] = { i(_CHI_MODULE_IMPORT_NAME) };

#define MODULE_MAIN(n, i)

#define MODULE_INIT(n, ns, i, m)                                        \
    static const ChiModuleDesc n##_desc =                               \
        { .name        = ns,                                            \
          .init        = &(i),                                          \
          .mainIdx     = (m),                                           \
          .importCount = CHI_DIM(_chiModuleImportName),                 \
          .importName  = _chiModuleImportName,                          \
        };                                                              \
    extern const ChiModuleDesc* n;                                      \
    __attribute__ ((section("chi_module_registry")))                    \
    const ChiModuleDesc* n = &n##_desc;

#else

#define _CHI_MODULE_IMPORT_DECL(n, ns) CHI_EXTERN_CONT_DECL(n)
#define _CHI_MODULE_IMPORT_REF(n, ns) &n,
#define MODULE_IMPORTS(i) \
    i(_CHI_MODULE_IMPORT_DECL) \
    static ChiCont _chiModuleImport[] = { i(_CHI_MODULE_IMPORT_REF) };

#define MODULE_MAIN(n, i)                       \
    extern const int32_t main_##n;              \
    const int32_t main_##n = (i);

#define MODULE_INIT(n, ns, i, m)                                     \
    CHI_EXTERN_CONT_DECL(n)                                          \
    CONT(n) {                                                        \
        PROLOGUE(n);                                                 \
        ASET(0, chiFromCont(&i));                                    \
        ASET(1, chiFromUInt32(CHI_DIM(_chiModuleImport)));           \
        ASET(2, chiFromPtr(_chiModuleImport));                       \
        KNOWN_JUMP(chiInitModule);                                   \
    }

#endif

#include <chili.h>
