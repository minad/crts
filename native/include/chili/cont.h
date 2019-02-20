#pragma once

#define IDX(c, i)             CHI_IDX(c, i)
#define STATIC_BIGINT(s)      CHI_STATIC_BIGINT(s)
#define STATIC_BUFFER(s)      CHI_STATIC_BUFFER(s)
#define STATIC_STRING(s)      CHI_STATIC_STRING(s)
#define RET(r)                ({ A(0) = (r); JUMP_FN(SP[-1]); })
#define THROW(e)              ({ A(0) = (e); KNOWN_JUMP(_chiThrow); })
#define PAR(t)                ({ A(0) = (t); KNOWN_JUMP(_chiPar); })
#define CATCH(f, h)           ({ A(0) = (f); A(1) = (h); KNOWN_JUMP(_chiCatch); })
#define JUMP_FN(c)            JUMP(chiToCont(c))
#define JUMP_FN0              JUMP_FN(IDX(A(0), 0))
#define FIRST_JUMP(c)         _CALLCONV_FIRST_JUMP(CHI_IFELSE(CHI_CONTINFO_PREFIX, (c), (c)->fn))
#define KNOWN_JUMP_NOTRACE(f) _CALLCONV_JUMP(CHI_IFELSE(CHI_CONTINFO_PREFIX, f, CHI_CAT(f, _fn)))
#define JUMP_NOTRACE(c)       _CALLCONV_JUMP(CHI_IFELSE(CHI_CONTINFO_PREFIX, (c), (c)->fn))
#define JUMP(c)               ({ TRACE_TIME; JUMP_NOTRACE(c); })
#define KNOWN_JUMP(f)         ({ TRACE_TIME; KNOWN_JUMP_NOTRACE(f); })

// read only registers, since we want to track writes in the code explicitly
#define NA                 ((const uint8_t)NARW)
#define SL                 ((Chili* const)SLRW)
#define HL                 ((ChiWord* const)HLRW)

#define TRACE_TIME          ({ if (CHI_TRACEPOINTS_ENABLED && CHI_UNLIKELY(AUX.TP > 0)) _chiTraceTime(_chiCurrentCont, SP); })
#define TRACE_TIME_NAME(n)  ({ if (CHI_TRACEPOINTS_ENABLED && CHI_UNLIKELY(AUX.TP > 0)) _chiTraceTimeName(_chiCurrentCont, SP, (n)); })
#define TRACE_ALLOC(n)      ({ if (CHI_TRACEPOINTS_ENABLED && CHI_UNLIKELY(AUX.TP < 0)) _chiTraceAlloc(_chiCurrentCont, SP, (n)); })
#define TRACE_FFI(n)        TRACE_TIME_NAME("ffi " #n)

#define PROTECT_BEGIN       _chiProtectBegin(SP, HP)
#define PROTECT_END(ret)    ({ A(0) = (ret); KNOWN_JUMP_NOTRACE(_chiProtectEnd); })

#ifndef CHI_GUID
#  error CHI_GUID must be defined
#endif

#define _CHI_CONT_SECTION(n, i) __attribute__ ((aligned(CHI_CONTINFO_ALIGN), section(".text$" CHI_STRINGIZE(CHI_GUID) "." #n "." #i)))

#ifdef NDEBUG
#define _CHI_CONT_CHECK(n)
#else
#define _CHI_CONT_CHECK(n)                                              \
    static const _CHI_CONT_SECTION(n, 0)                                \
    __attribute__ ((used)) void* CHI_CAT(n, _linkcheck) = &CHI_CAT(n, _linkcheck);
#endif

#define _CHI_CONT1(static_cont, attr_fn, n, f, ...)                     \
    _CHI_CONT_CHECK(n)                                                  \
    CHI_IF(CHI_LOC_ENABLED,                                             \
    static const _ChiContInfoLoc CHI_CAT(n, _loc) = {                   \
        .file = __FILE__ "\0" f, .name = #n,                            \
    };)                                                                 \
    _CHI_CONT_SECTION(n, 1) __attribute__ ((used))                      \
    static const ChiContInfo CHI_CAT(n, _info) = {                      \
        CHI_IF(CHI_LOC_ENABLED, .loc = &CHI_CAT(n, _loc),)              \
        .lineMangled = CHI_LOC_ENABLED ? __LINE__ : 0,                  \
        ##__VA_ARGS__                                                   \
    };                                                                  \
    static_cont attr_fn _CHI_CONT_SECTION(n, 2) CHI_CONT_FN(n)

#define _CHI_CONT0(static_cont, attr_fn, n, f, ...)                     \
    static_cont attr_fn CHI_CONT_FN(CHI_CAT(n, _fn));                   \
    __attribute__ ((used))                                              \
    static_cont const ChiContInfo n = {                                 \
        CHI_IF(CHI_LOC_ENABLED, .file = __FILE__ "\0" f, .name = #n,)   \
        .fn = CHI_CAT(n, _fn),                                          \
        .lineMangled = CHI_LOC_ENABLED ? __LINE__ : 0,                  \
        ##__VA_ARGS__                                                   \
    };                                                                  \
    static_cont attr_fn CHI_CONT_FN(CHI_CAT(n, _fn))

#define _CHI_CONT(x, ...) x(__VA_ARGS__)
#define _CONT(...)        _CHI_CONT(CHI_CAT(_CHI_CONT, CHI_CONTINFO_PREFIX), __VA_ARGS__)

#define CONT(n, ...)        _CONT(,, n, ""__VA_ARGS__)
#define STATIC_CONT(n, ...) _CONT(static,, n, ""__VA_ARGS__)

#define PROLOGUE(c)                                                 \
    CHI_ASSERT(!strcmp(CHI_STRINGIZE(c)                             \
                       CHI_IFELSE(CHI_CONTINFO_PREFIX, "", "_fn"),  \
                       __func__));                                  \
    _PROLOGUE;                                                      \
    const ChiCont _chiCurrentCont = &c;                             \
    if (CHI_DIM(AUX.CH))                                            \
        AUX.CH[AUX.CN++ & (CHI_DIM(AUX.CH) - 1)] = _chiCurrentCont; \
    TRACE_TIME

#define NEW(t, s)                               \
    ({                                          \
        const size_t _s = (s);                  \
        CHI_ASSERT(_s <= CHI_MAX_UNPINNED);     \
        Chili _c = _chiWrap(HP, _s, (t));       \
        HP += _s;                               \
        _c;                                     \
    })

#define _LIMITS_SAVE(save, limit, ...)                                  \
    ({                                                                  \
        const struct { size_t stack, heap; } _limits = { limit, ##__VA_ARGS__ }; \
        CHI_ASSERT(_limits.heap <= CHI_BLOCK_MINLIMIT);                 \
        if (_limits.heap) TRACE_ALLOC(_limits.heap);                    \
        Chili* _newSP = SP + _limits.stack;                             \
        ChiWord* _newHP = HP + _limits.heap;                            \
        if (CHI_UNLIKELY((CHI_SYSTEM_HAS_INTERRUPT ? (!_limits.heap && !HL) : !AUX.EN--) \
                         || (_limits.stack && _newSP > SL)              \
                         || (_limits.heap && _newHP > HL))) {           \
            if (_limits.stack) SLRW = _newSP;                           \
            if (_limits.heap) HLRW = _newHP;                            \
            save;                                                       \
        }                                                               \
    })                                                                  \

#define LIMITS(...)                                                     \
    _LIMITS_SAVE(({ *SP = chiFromCont(_chiCurrentCont);                 \
                     KNOWN_JUMP(_chiInterrupt); }),                     \
                 __VA_ARGS__)

/* Grow heap. Does update HL to ensure that we do not have have HP > HL afterwards.
 * It might be that HL=0 after GROW_HEAP when an interrupt is pending,
 * but it will be guaranteed that enough memory is available.
 */
#define GROW_HEAP(n)                            \
    ({                                          \
        ChiWord* _hl = HP + (n);                \
        if (CHI_UNLIKELY(_hl > HL))             \
            HP = _chiGrowHeap(HP, _hl);         \
    })

#define FORCE(t)                                \
    ({                                          \
        A(0) = (t);                             \
        Chili _val;                             \
        if (_chiThunkForced(A(0), &_val))       \
            RET(_val);                       \
        A(1) = _val;                            \
        KNOWN_JUMP(_chiForce);                       \
    })

#define APP(args)                                                       \
    ({                                                                  \
        const size_t _app_args = (args);                                \
        CHI_ASSERT(_app_args < CHI_AMAX);                               \
        if (_app_args == _chiFnArity(A(0)))                             \
            JUMP_FN0;                                                   \
        if (!__builtin_constant_p(args)) {                              \
            NARW = (uint8_t)_app_args;                                  \
            KNOWN_JUMP(_chiAppN);                                            \
        }                                                               \
        switch (_app_args) {                                            \
        case 1: KNOWN_JUMP(_chiApp1);                                        \
        case 2: KNOWN_JUMP(_chiApp2);                                        \
        case 3: KNOWN_JUMP(_chiApp3);                                        \
        case 4: KNOWN_JUMP(_chiApp4);                                        \
        default: NARW = (uint8_t)_app_args; KNOWN_JUMP(_chiAppN);            \
        }                                                               \
    })

#define _PUSH_OVERAPP(...)                                              \
    ({                                                                  \
        const struct { size_t args, arity; } _overapp = { __VA_ARGS__ }; \
        const size_t _overapp_rest = _overapp.args - _overapp.arity;    \
        CHI_ASSERT(_overapp.args > _overapp.arity);                     \
        CHI_BOUNDED(_overapp_rest, CHI_AMAX);                           \
        for (size_t i = 0; i < _overapp_rest; ++i)                      \
            SP[i] = A(1 + _overapp.arity + i);                          \
        SP[_overapp_rest] = _chiFromUnboxed(_overapp_rest + 2);         \
        SP[_overapp_rest + 1] = chiFromCont(&_chiOverApp);              \
        SP += _overapp_rest + 2;                                        \
    })

#define KNOWN_OVERAPP(f, ...) ({ _PUSH_OVERAPP(__VA_ARGS__); KNOWN_JUMP(f); })
#define OVERAPP(...)          ({ _PUSH_OVERAPP(__VA_ARGS__); JUMP_FN0; })

#define PARTIAL(...)                                                    \
    ({                                                                  \
        const struct { size_t args, arity; } _partial = { __VA_ARGS__ }; \
        const size_t _partial_args = _partial.args;                     \
        CHI_ASSERT(_partial_args < _partial.arity);                   \
        CHI_BOUNDED(_partial_args, CHI_AMAX);                       \
        Chili _partial_clos = NEW(CHI_FN(_partial.arity - _partial_args), _partial_args + 2); \
        IDX(_partial_clos, 0) =                                         \
            _partial_args == 1 && _partial.arity == 2 ? chiFromCont(&_chiPartial1of2) : \
            _partial_args == 1 && _partial.arity == 3 ? chiFromCont(&_chiPartial1of3) : \
            _partial_args == 2 && _partial.arity == 3 ? chiFromCont(&_chiPartial2of3) : \
            _partial_args == 1 && _partial.arity == 4 ? chiFromCont(&_chiPartial1of4) : \
            _partial_args == 2 && _partial.arity == 4 ? chiFromCont(&_chiPartial2of4) : \
            _partial_args == 3 && _partial.arity == 4 ? chiFromCont(&_chiPartial3of4) : \
            chiFromCont(&_chiPartialNofM);                              \
        for (size_t i = 0; i <= _partial_args; ++i)                     \
            IDX(_partial_clos, i + 1) = A(i);                           \
        RET(_partial_clos);                                             \
    })

#ifdef CHI_DYNLIB

#define _EXPORT_NAME(n, ns, i) ns,
#define _EXPORT_IDX(n, ns, i)  i,
#define MODULE_EXPORTS(e)                                              \
    static const char* const _chiModuleExportName[] = { e(_EXPORT_NAME) }; \
    static const int32_t _chiModuleExportIdx[] = { e(_EXPORT_IDX) };

#define _IMPORT_NAME(n, ns) ns,
#define MODULE_IMPORTS(i)                                           \
    static const char* const _chiModuleImportName[] = { i(_IMPORT_NAME) };

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

#define _EXPORT_VAR(n, ns, i) extern const int32_t n; const int32_t n = i;
#define MODULE_EXPORTS(e) e(_EXPORT_VAR)

#define _IMPORT_DECL(n, ns) CHI_CONT_DECL(extern, n)
#define _IMPORT_REF(n, ns) &n,
#define MODULE_IMPORTS(i) \
    i(_IMPORT_DECL) \
    static ChiCont _chiModuleImport[] = { i(_IMPORT_REF) };

#define MODULE_INIT(n, ns, i)                                   \
    CHI_CONT_DECL(extern, n)                                    \
    CONT(n) {                                                   \
        PROLOGUE(n);                                            \
        A(0) = chiFromCont(&i);                                 \
        A(1) = chiFromUInt32(CHI_DIM(_chiModuleImport));        \
        A(2) = chiFromPtr(_chiModuleImport);                    \
        KNOWN_JUMP(chiInitModule);                              \
    }

#endif

#include "../chili.h"
