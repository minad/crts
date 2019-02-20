#define _CHI_OBJECT(NS_NAME, NsName, Name, ...)                         \
    typedef struct CHI_PACKED CHI_WORD_ALIGNED { __VA_ARGS__; } NsName; \
    CHI_API_INL CHI_WU NsName* chiTo##Name(Chili c) {                       \
        CHI_ASSERT(_chiType(c) == NS_NAME);                              \
        return (NsName*)_chiRawPayload(c);                              \
    }

/**
 * Helper macro to define heap object types
 */
#define CHI_OBJECT(NAME, Name, ...)  _CHI_OBJECT(CHI##_##NAME, Chi##Name, Name, __VA_ARGS__)

/**
 * Heap object field accessor macro
 */
#define CHI_IDX(c, i) (*_chiIdx(c, i))

/**
 * Shortcuts for chiFromBool(x)
 */
#define CHI_TRUE  ((Chili){true})
#define CHI_FALSE ((Chili){false})

/**
 * Returned by *Try* functions in case of failure
 */
#define CHI_FAIL  chiNewEmpty(CHI_FAIL_OBJECT)

#define _CHILI_NAN       (0xFFF8ULL << 48)
#define _CHILI_PINNED    CHI_BF_MASK_0(_CHILI_BF_SIZE)
#define _CHILI_REFTAG    CHI_BF_MASK_S(_CHILI_BF_REFTAG)

#if CHI_NANBOXING_ENABLED
#  define _CHILI_BF_TYPE   (0,   9, ChiType)
#  define _CHILI_BF_SIZE   (9,   6, size_t)
#  define _CHILI_BF_PTR    (15, 35, uintptr_t)
#  define _CHILI_BF_REFTAG (50, 14, uint32_t)
#  define _CHILI_PTR_SHIFT_LOW         3
#  define _CHILI_CHUNK_START_IN_REFTAG 1
_Static_assert(CHI_ARCH_BITS == 32 || CHI_CHUNK_ARENA_FIXED, "NaN boxing: Chunk arena must be fixed on 64 bit platforms.");
_Static_assert(_CHILI_REFTAG == (0xFFFCULL << 48), "NaN boxing: Reference tag is fixed by IEEE 754 NaN format");
_Static_assert(CHI_BF_SHIFT(_CHILI_BF_PTR) >= _CHILI_PTR_SHIFT_LOW, "NaN boxing: Pointer bit field should be aligned");
_Static_assert(((CHI_BF_MASK_S(_CHILI_BF_REFTAG) >> (CHI_BF_SHIFT(_CHILI_BF_PTR) - _CHILI_PTR_SHIFT_LOW)) & CHI_CHUNK_START) == CHI_CHUNK_START || (CHI_CHUNK_START >> _CHILI_PTR_SHIFT_LOW) < CHI_BF_OVF(_CHILI_BF_PTR),
              "NaN boxing: Chunk start bit should fall within reference tag");
#else
#  define _CHILI_BF_PTR    (0,  48, uintptr_t)
#  define _CHILI_BF_TYPE   (48,  9, ChiType)
#  define _CHILI_BF_SIZE   (57,  6, size_t)
#  define _CHILI_BF_REFTAG (63,  1, bool)
#  define _CHILI_PTR_SHIFT_LOW 0
#  define _CHILI_CHUNK_START_IN_REFTAG 0
#endif

_Static_assert(CHI_MAX_UNPINNED < _CHILI_PINNED, "CHI_MAX_UNPINNED must be less than _CHILI_PINNED");

// object header
#define _CHI_OBJECT_BF_COLOR  (0,  2, ChiColor)
#define _CHI_OBJECT_BF_ACTIVE (2,  1, bool)
#define _CHI_OBJECT_BF_DIRTY  (3,  1, bool)
#define _CHI_OBJECT_BF_SIZE   (4, 60, size_t)

/**
 * Data word
 */
typedef uint64_t ChiWord;
typedef _Atomic(ChiWord) ChiAtomicWord;
#define CHI_WORDSIZE 8
#define CHI_WORDBITS 64
#define _CHI_INT64_SHIFT (1LL << (CHI_WORDBITS - 2))

/**
 * Chis are either unboxed values or contain
 * a pointer to a heap object. On 64 bit, many
 * pointer bits are unused. We use those bits
 * to store meta data, in particular the type of the heap object.
 *
 * Chis use a NaN boxing format:
 *
 *                    ┌──────────┬───────────────────────┬────────────┬────────────┐
 *     Bits           │   63..50 │         49..15        │    14..9   │    8..0    │
 *                    ├──────────┼───────────────────────┼────────────┼────────────┤
 *     Small object   │  FFFC..  │ 35 bit heap reference │ 6 bit size │ 9 bit type │
 *                    ├──────────┼───────────────────────┼────────────┼────────────┤
 *     Large object   │  FFFC..  │ 35 bit heap reference │ PINNED = 63│ 9 bit type │
 *                    ├──────────┼───────────────────────┼────────────┼────────────┤
 *     Empty object   │  FFFC..  │           0           │      0     │ 9 bit type │
 *                    ├──────────┼───────────────────────┴────────────┴────────────┤
 *     Float64        │ !=FFFC.. │            50 bit float payload                 │
 *                    ├──────────┴─────────────────────────────────────────────────┤
 *     Unboxed        │           Payload, value less than FFFC...                 │
 *                    └────────────────────────────────────────────────────────────┘
 *
 * Only objects with upper FFFC... are garbage collected.
 * The heap can have a size of 256G.
 * Large objects are allocated on the major heap and pinned as indicated by
 * the value ::_CHILI_PINNED, which is stored in the size field.
 * Empty objects are used for ADT constructors which
 * consist of only a tag without payload. This only applies to constructors
 * of ADTs, which are not enumerations. Enumerations are ADTs
 * where all constructors do not have payload. They are encoded as unboxed UInt32.
 *
 * Various small values are stored in an unboxed format:
 *
 *                    ┌─┬────────┬───────────────────────┬────────┬────────┬────────┐
 *     Bits           │0│ 62..56 │         55..32        │ 31..16 │ 15..8  │  7..0  │
 *                    ├─┼────────┴───────────────────────┴────────┴────────┼────────┤
 *     (U)Int8        │0│         55 unused bits                           │  8 bit │
 *                    ├─┼─────────────────────────────────────────┬────────┴────────┤
 *     (U)Int16       │0│         47 unused bits                  │   16 bit data   │
 *                    ├─┼────────────────────────────────┬────────┴─────────────────┤
 *     (U)Int32       │0│         31 unused bits         │       32 bit data        │
 *                    ├─┼────────────────────────────────┼──────────────────────────┤
 *     Float32        │0│         31 unused bits         │       32 bit data        │
 *                    ├─┼────────┬───────────────────────┴──────────────────────────┤
 *     Small String   │0│  Size  │             7 UTF-8 encoded bytes                │
 *                    ├─┼────────┴──────────────────────────────────────────────────┤
 *     Small (U)Int64 │0│                  63 bit integer data                      │
 *                    ├─┼───────────────────────────────────────────────────────────┤
 *     Small BigInt   │0│                  63 bit integer data                      │
 *                    ├─┼───────────────────────────────────────────────────────────┤
 *     Native pointer │0│                  63 bit pointer data                      │
 *                    ├─┴───────────────────────────────────────────────────────────┤
 *     True           │                           != 0                              │
 *                    ├─────────────────────────────────────────────────────────────┤
 *     False          │                             0                               │
 *                    └─────────────────────────────────────────────────────────────┘
 */
CHI_NEWTYPE(li, ChiWord)
CHI_NEWTYPE(Atomic, ChiAtomicWord)
#define CHILI_UN(x) CHI_UN(li,x)

typedef struct {
    volatile int32_t TP; ///< Trace point
    uint32_t         EN; ///< Evaluation counter
    uint32_t         CN; ///< Continuation count
    const void*      CH[CHI_CONT_HISTORY_SIZE];
} _ChiAux;

_Static_assert(!(CHI_CONT_HISTORY_SIZE & (CHI_CONT_HISTORY_SIZE - 1)),
              "history size must be power of two");

#include CHI_INCLUDE(callconv/CHI_CALLCONV)
typedef CHI_CONT_FN((ChiContFn));

#define CHI_FOREACH_FRAME(FRAME) \
    FRAME(DEFAULT)                 \
    FRAME(CATCH)                   \
    FRAME(UPDATE)                  \
    FRAME(INTERPFN)                \
    FRAME(INTERP)                  \
    FRAME(APPN)                    \
    FRAME(ENTER)

/**
 * Each continuation has a frame type corresponding to the
 * format of the stack frame. For generic frames the type is ::CHI_FRAME_DEFAULT.
 * However there are continuations which use a different frame format.
 * It is necessary to know the precise format of the stack frames for stack walking.
 *
 *                    ┌───────────────┬───────────────┬─────┐
 *     DEFAULT        │ ChiCont       │ Payload       │ ... │
 *                    └───────────────┴───────────────┴─────┘
 *
 *                    ┌───────────────┬───────────────┐
 *     CATCH          │ ChiCont       │ Old Handler   │
 *                    └───────────────┴───────────────┘
 *
 *                    ┌───────────────┬───────────────┐
 *     UPDATE         │ ChiCont       │ Thunk/Blackh. │
 *                    └───────────────┴───────────────┘
 *
 *                    ┌───────────────┬───────────────┬───────────────┬─────┐
 *     INTERP/VASIZE  │ ChiCont       │ Frame Size    │ Payload       │ ... │
 *                    └───────────────┴───────────────┴───────────────┴─────┘
 *
 *                    ┌───────────────┬───────────────┬───────────────┬─────┬───────────────┐
 *     ENTER          │ ChiCont       │ Frame Size    │ Arguments     │ ... │ ChiCont       │
 *                    └───────────────┴───────────────┴───────────────┴─────┴───────────────┘
 *
 *                    ┌───────────────┐
 *     APPN/VAARGS    │ ChiCont       │
 *                    └───────────────┘
 *
 * @note ::CHI_VAARGS and ::CHI_VASIZE are not stored in the
 *       type field of ::ChiContInfo but in the respective na and size field.
 *       Notable ::CHI_VASIZE continuations are ::chiEnterCont and ::chiOverApp.
 */
#define _CHI_FRAME(n) CHI_FRAME_##n,
typedef enum { CHI_FOREACH_FRAME(_CHI_FRAME) } ChiFrame;
#undef _CHI_FRAME

/**
 * Frame with variable number of arguments.
 * If ChiContInfo::na == CHI_VAARGS, the number of arguments
 * must be taken from the NA register.
 */
#define CHI_VAARGS 0x1F

/**
 * Frame with variable size.
 * If ChiContInfo::size == CHI_VASIZE, the frame size
 * is stored in the first field of the frame.
 */
#define CHI_VASIZE 0x3FFFF

/**
 * Continuation information.
 * In particular, this includes the ChiFrameType
 * and the size of the frame.
 *
 * @warning The size can be CHI_VASIZE and na can be
 *          CHI_VAARGS.
 *
 * @note This structure is stored next to the code
 *       of the continuation at an alignment of CHI_CONTINFO_ALIGN bytes.
 *       This is achieved by using a custom linker script
 *       which works only with the BFD linker as of now.
 */
#if CHI_CONTINFO_PREFIX
typedef struct {
    const char* file;
    char name[];
} _ChiContInfoLoc;
typedef const struct CHI_PACKED CHI_ALIGNED(CHI_CONTINFO_ALIGN) {
    CHI_IF(CHI_LOC_ENABLED, const _ChiContInfoLoc* loc;)
    uint32_t    line        : 18;
    uint32_t    lineMangled : 18;
    size_t      size        : 18;
    ChiFrame    type        : 5;
    uint8_t     na          : 5;
} ChiContInfo;
typedef ChiContFn* ChiCont;
#define CHI_CONT_DECL(a, n)  a ChiContFn n;
#define CHI_CONT_ALIAS(a, n) CHI_ALIAS(a) extern ChiContFn n;
#else
/*
 * Disable because of static redeclarations.
 * We still get the additional type safety in CHI_CONTINFO_PREFIX builds.
 */
#pragma GCC diagnostic ignored "-Wc++-compat"
#pragma GCC diagnostic ignored "-Wredundant-decls"
typedef const struct CHI_PACKED {
    ChiContFn*  fn;
    uint32_t    line        : 18;
    uint32_t    lineMangled : 18;
    size_t      size        : 18;
    ChiFrame    type        : 5;
    uint8_t     na          : 5;
    CHI_IF(CHI_LOC_ENABLED, const char* file; char name[];)
} ChiContInfo;
typedef const ChiContInfo* ChiCont;
#define CHI_CONT_DECL(a, n)      \
    a ChiContFn CHI_CAT(n, _fn); \
    a const ChiContInfo n;
#define CHI_CONT_ALIAS(a, n)                                            \
    CHI_ALIAS(CHI_CAT(a, _fn)) extern ChiContFn CHI_CAT(n, _fn);        \
    CHI_ALIAS(a) extern const ChiContInfo n;
#endif

/**
 * Descriptor used for modules
 * provided by shared objects.
 */
typedef const struct {
    const char*        name;
    ChiCont            init;
    const char* const* exportName;
    const int32_t*     exportIdx;
    const char* const* importName;
    uint32_t           exportCount;
    uint32_t           importCount;
}  ChiModuleDesc;

typedef struct {
    const ChiModuleDesc*  main;
    const ChiModuleDesc** modules;
    size_t                size;
} ChiModuleRegistry;

#ifdef __clang__
#  define _CHI_COMPILER CHI_CAT(_clang_, CHI_CAT(__clang_major__, CHI_CAT(_, __clang_minor__)))
#else
#  define _CHI_COMPILER CHI_CAT(_gcc_, CHI_CAT(__GNUC__, CHI_CAT(_, __GNUC_MINOR__)))
#endif

// Mangle module functions with compilation mode and compiler version.
// This allows to detect a mismatch early at linking time.
#define chiInitModule     CHI_CAT(chiInitModule_,     CHI_CAT(CHI_MODE, _CHI_COMPILER))
#define chiModuleRegistry CHI_CAT(chiModuleRegistry_, CHI_CAT(CHI_MODE, _CHI_COMPILER))

/**
 * Heap object type.
 *
 * In particular values between CHI_FIRST_TAG and CHI_LAST_TAG
 * are used for the tag of algebraic datatypes and the arity of closures.
 */
typedef enum {
    CHI_FIRST_TAG = 0,               // START: first adt tag, first closure arity
    CHI_LAST_TAG = 495 - CHI_AMAX,   // END: last adt tag, last closure arity
    CHI_POISON_OBJECT,
    CHI_FAIL_OBJECT,
    CHI_FIRST_FN,
    CHI_LAST_FN = CHI_FIRST_FN + CHI_AMAX - 1,
    CHI_THUNKFN,
    CBY_INTERP_MODULE,                 // used by interpreter
    CBY_NATIVE_MODULE,                 // used by interpreter
    CHI_LAST_IMMUTABLE = CBY_NATIVE_MODULE, // END: Immutable objects, which do not need atomic access
    CHI_FIRST_MUTABLE,               // START: mutable, special treatment by gc
    CHI_STRINGBUILDER = CHI_FIRST_MUTABLE,
    CHI_ARRAY,
    CHI_THREAD,
    CHI_THUNK,
    CHI_STACK,                       // stack needs special treatment by gc
    CHI_LAST_MUTABLE = CHI_STACK,    // END: mutable
    CHI_RAW,                         // START: raw objects, no scanning necessary
    CHI_BIGINT,
    CBY_FFI,                         // used by interpreter
    CHI_BUFFER,
    CHI_STRING,
    CHI_BOX64,
    CHI_LAST_TYPE = CHI_BOX64,
} ChiType;

_Static_assert(CHI_LAST_TYPE == CHI_BF_OVF(_CHILI_BF_TYPE) - 1, "CHI_LAST_TYPE is wrong");

CHI_CONT_DECL(extern CHI_API, chiExit)
CHI_CONT_DECL(extern CHI_API, chiGCRun)
CHI_CONT_DECL(extern CHI_API, chiStackTrace)
CHI_CONT_DECL(extern CHI_API, chiInitModule)
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiApp1))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiApp2))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiApp3))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiApp4))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiAppN))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiCatch))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiForce))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiHeapOverflow))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiInterrupt))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiOverApp))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiPartial1of2))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiPartial1of3))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiPartial1of4))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiPartial2of3))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiPartial2of4))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiPartial3of4))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiPartialNofM))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiProtectEnd))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiPar))
CHI_CONT_DECL(extern CHI_API, CHI_PRIVATE(chiThrow))

enum {
    CHI_NEW_DEFAULT       = 0,
    CHI_NEW_PIN           = 1,
    CHI_NEW_TRY           = 2,
    CHI_NEW_UNINITIALIZED = 4,
};

CHI_API void chiEventInstant(Chili);
CHI_API void chiEventBegin(void);
CHI_API void chiEventEnd(Chili);
CHI_API CHI_WU bool chiEventEnabled(void);
CHI_API CHI_WU bool chiEventFilter(Chili);
CHI_API CHI_WU Chili chiNew(ChiType, size_t);
CHI_API CHI_WU Chili chiNewPin(ChiType, size_t);
CHI_API CHI_WU Chili chiNewFlags(ChiType, size_t, uint32_t);
CHI_API CHI_WU Chili chiTryPin(Chili);
CHI_API CHI_WU Chili chiPin(Chili);
CHI_API CHI_WU uint64_t chiCurrentTimeMillis(void);
CHI_API CHI_WU uint64_t chiCurrentTimeNanos(void);
CHI_API CHI_WU bool chiBlackholeEvaporated(Chili);
CHI_API void chiGCBlock(bool);
CHI_API void chiProfilerEnable(bool);
CHI_API _Noreturn void chiMain(int, char**);
CHI_API CHI_WU CHI_RET_NONNULL ChiWord* CHI_PRIVATE(chiGrowHeap)(ChiWord*, const ChiWord*);
CHI_API void CHI_PRIVATE(chiTraceTime)(ChiCont, Chili*);
CHI_API void CHI_PRIVATE(chiTraceTimeName)(ChiCont, Chili*, const char*);
CHI_API void CHI_PRIVATE(chiTraceAlloc)(ChiCont, Chili*, size_t);
CHI_API CHI_WU Chili CHI_PRIVATE(chiBox64)(uint64_t);
CHI_API void CHI_PRIVATE(chiProtectBegin)(Chili*, ChiWord*);
CHI_API void chiSortModuleRegistry(ChiModuleRegistry*);
CHI_API CHI_WU int64_t CHI_PRIVATE(chiFloat64ToInt64)(double);

CHI_INL CHI_WU float CHI_PRIVATE(chiBitsToFloat32)(uint32_t x) {
    return CHI_CAST(x, float);
}

CHI_INL CHI_WU double CHI_PRIVATE(chiBitsToFloat64)(uint64_t x) {
    return CHI_CAST(x, double);
}

CHI_INL CHI_WU uint32_t CHI_PRIVATE(chiFloat32ToBits)(float x) {
    return CHI_CAST(x, uint32_t);
}

CHI_INL CHI_WU uint64_t CHI_PRIVATE(chiFloat64ToBits)(double x) {
    return CHI_CAST(x, uint64_t);
}

/**
 * Initialize atomic field. This operation itself is *not atomic*.
 */
CHI_INL void CHI_PRIVATE(chiAtomicInit)(ChiAtomic* c, Chili x) {
    atomic_init(CHI_UNP(Atomic, c), CHILI_UN(x));
}

/**
 * Atomic load *without* memory ordering constraints.
 * Only the atomicity of the operation is guaranteed.
 */
CHI_INL CHI_WU Chili CHI_PRIVATE(chiAtomicLoad)(ChiAtomic* c) {
    return (Chili){ atomic_load_explicit(CHI_UNP(Atomic, c), memory_order_relaxed) };
}

/**
 * Atomic store *without* memory ordering constraints.
 * Only the atomicity of the operation is guaranteed.
 */
CHI_INL void CHI_PRIVATE(chiAtomicStore)(ChiAtomic* c, Chili x) {
    atomic_store_explicit(CHI_UNP(Atomic, c),
                          CHILI_UN(x),
                          memory_order_relaxed);
}

/**
 * Compare and swap operation. Implies full memory barrier (seq cst).
 */
CHI_INL CHI_WU bool CHI_PRIVATE(chiAtomicCas)(ChiAtomic* c, Chili expected, Chili desired) {
    return atomic_compare_exchange_strong(CHI_UNP(Atomic, c),
                                          &CHILI_UN(expected),
                                          CHILI_UN(desired));
}


CHI_INL CHI_WU bool _chiFitsUnboxed(ChiWord v) {
    return CHI_NANBOXING_ENABLED ? v < _CHILI_REFTAG : !(v & _CHILI_REFTAG);
}

/**
 * Returns true if the Chili contains an unboxed value.
 */
CHI_INL CHI_WU bool CHI_PRIVATE(chiUnboxed)(Chili c) {
    bool unboxed = _chiFitsUnboxed(CHILI_UN(c));
    CHI_ASSERT(!CHI_POISON_OBJECT_ENABLED || unboxed || CHI_BF_GET(_CHILI_BF_TYPE, CHILI_UN(c)) != CHI_POISON_OBJECT);
    return unboxed;
}

/**
 * Pack unboxed value
 */
CHI_INL CHI_WU Chili CHI_PRIVATE(chiFromUnboxed)(ChiWord v) {
    CHI_ASSERT(_chiFitsUnboxed(v));
    return (Chili){ v };
}

/**
 * Unpack unboxed value
 */
CHI_INL CHI_WU ChiWord CHI_PRIVATE(chiToUnboxed)(Chili c) {
    CHI_ASSERT(_chiUnboxed(c));
    return CHILI_UN(c);
}

CHI_INL CHI_WU ChiType CHI_PRIVATE(chiType)(Chili c) {
    CHI_ASSERT(!_chiUnboxed(c));
    return CHI_BF_GET(_CHILI_BF_TYPE, CHILI_UN(c));
}

CHI_INL CHI_WU size_t CHI_PRIVATE(chiSizeField)(Chili c) {
    CHI_ASSERT(!_chiUnboxed(c));
    return CHI_BF_GET(_CHILI_BF_SIZE, CHILI_UN(c));
}

CHI_INL CHI_WU CHI_ASSUMEALIGNED(CHI_WORDSIZE) void* _chiPtrField(Chili c) {
    CHI_ASSERT(!_chiUnboxed(c));
    if (_CHILI_CHUNK_START_IN_REFTAG)
        return (void*)(uintptr_t)((CHILI_UN(c) >> (CHI_BF_SHIFT(_CHILI_BF_PTR) - _CHILI_PTR_SHIFT_LOW))
                                  & ((CHI_BF_MASK_0(_CHILI_BF_PTR) << _CHILI_PTR_SHIFT_LOW) | CHI_CHUNK_START));
    return (void*)((CHI_BF_GET(_CHILI_BF_PTR, CHILI_UN(c)) << _CHILI_PTR_SHIFT_LOW) | CHI_CHUNK_START);
}

CHI_INL CHI_WU Chili _chiWrapUnchecked(void* p, size_t s, ChiType t) {
    CHI_ASSERT(t <= CHI_LAST_TYPE);
    CHI_ASSERT(s <= _CHILI_PINNED);
    CHI_ASSERT((uintptr_t)p % CHI_WORDSIZE == 0);

#if CHI_CHUNK_ARENA_FIXED
    CHI_ASSERT((uintptr_t)p >= CHI_CHUNK_START);
    CHI_ASSERT((uintptr_t)p < CHI_CHUNK_START + CHI_CHUNK_MAX_SIZE);
#endif

    if (_CHILI_CHUNK_START_IN_REFTAG)
        return (Chili){ _CHILI_REFTAG
                | ((ChiWord)(uintptr_t)p << (CHI_BF_SHIFT(_CHILI_BF_PTR) - _CHILI_PTR_SHIFT_LOW))
                | CHI_BF_INIT(_CHILI_BF_SIZE, s)
                | CHI_BF_INIT(_CHILI_BF_TYPE, t)
                };

    return (Chili){ _CHILI_REFTAG
            | CHI_BF_INIT(_CHILI_BF_PTR, ((uintptr_t)p & ~CHI_CHUNK_START) >> _CHILI_PTR_SHIFT_LOW)
            | CHI_BF_INIT(_CHILI_BF_SIZE, s)
            | CHI_BF_INIT(_CHILI_BF_TYPE, t)
            };
}

CHI_INL CHI_WU Chili CHI_PRIVATE(chiWrap)(void* p, size_t s, ChiType t) {
    Chili c = _chiWrapUnchecked(p, s, t);
    CHI_ASSERT(_chiType(c) == t);
    CHI_ASSERT(_chiPtrField(c) == p);
    return c;
}

CHI_API_INL CHI_WU Chili chiNewEmpty(uint32_t tag) {
    return _chiWrap((void*)CHI_CHUNK_START, 0, (ChiType)tag);
}

CHI_INL CHI_WU bool CHI_PRIVATE(chiEmpty)(Chili c) {
    return !_chiSizeField(c);
}

CHI_INL CHI_WU bool CHI_PRIVATE(chiReference)(Chili c) {
    return !_chiUnboxed(c) && !_chiEmpty(c);
}

CHI_API_INL CHI_WU bool chiPinned(Chili c) {
    CHI_ASSERT(_chiReference(c));
    return _chiSizeField(c) == _CHILI_PINNED;
}

/**
 * Returns true if the ChiType is a raw type.
 * Raw objects do not contain references to other objects.
 */
CHI_INL CHI_WU bool CHI_PRIVATE(chiRaw)(ChiType t) {
    return t >= CHI_RAW;
}

CHI_INL CHI_WU CHI_ASSUMEALIGNED(CHI_WORDSIZE) void* CHI_PRIVATE(chiRawPayload)(Chili c) {
    CHI_ASSERT(!_chiEmpty(c));
    return _chiPtrField(c);
}

CHI_INL CHI_WU CHI_ASSUMEALIGNED(CHI_WORDSIZE) Chili* CHI_PRIVATE(chiPayload)(Chili c) {
    CHI_ASSERT(!_chiRaw(_chiType(c)));
    return (Chili*)_chiRawPayload(c);
}

CHI_API_INL CHI_WU size_t chiSize(Chili c) {
    size_t s = _chiSizeField(c);
    if (s != _CHILI_PINNED)
        return s;
    return CHI_BF_GET(_CHI_OBJECT_BF_SIZE, *((ChiAtomicWord*)_chiRawPayload(c) - 1));
}

CHI_INL CHI_WU ChiWord _chiInt64Pack(int64_t i) {
    return (ChiWord)(i + _CHI_INT64_SHIFT);
}

CHI_INL CHI_WU int64_t _chiInt64Unpack(ChiWord i) {
    return (int64_t)i - _CHI_INT64_SHIFT;
}

CHI_INL CHI_WU bool _chiFitsUnboxed64(ChiWord v) {
    return !(v >> 63);
}

CHI_INL CHI_WU bool CHI_PRIVATE(chiUnboxed64)(Chili c) {
    return _chiFitsUnboxed64(CHILI_UN(c));
}

CHI_API_INL CHI_WU Chili chiFromUInt64(uint64_t i) {
    return CHI_LIKELY(_chiFitsUnboxed64(i)) ? _chiFromUnboxed(i) : _chiBox64(i);
}

CHI_INL CHI_WU uint64_t CHI_PRIVATE(chiUnbox64)(Chili c) {
    CHI_ASSERT(_chiType(c) == CHI_BOX64);
    return *(uint64_t*)_chiRawPayload(c);
}

CHI_API_INL CHI_WU uint64_t chiToUInt64(Chili c) {
    return CHI_LIKELY(_chiUnboxed64(c)) ? _chiToUnboxed(c) : _chiUnbox64(c);
}

CHI_API_INL CHI_WU Chili chiFromInt64(int64_t i) {
    return chiFromUInt64(_chiInt64Pack(i));
}

CHI_API_INL CHI_WU int64_t chiToInt64(Chili c) {
    return _chiInt64Unpack(chiToUInt64(c));
}

CHI_API_INL CHI_WU uint32_t chiToUInt32(Chili c) {
    ChiWord v = _chiToUnboxed(c);
    CHI_ASSERT(v <= UINT32_MAX);
    return (uint32_t)v;
}

CHI_API_INL CHI_WU Chili chiFromUInt32(uint32_t v) {
    return _chiFromUnboxed(v);
}

CHI_API_INL CHI_WU Chili chiFromInt32(int32_t v) {
    return chiFromUInt32((uint32_t)v);
}

CHI_API_INL CHI_WU int32_t chiToInt32(Chili c) {
    return (int32_t)chiToUInt32(c);
}

CHI_INL ChiType CHI_FN(size_t n) {
    CHI_ASSERT(n < CHI_AMAX);
    return (ChiType)(CHI_FIRST_FN + n);
}

CHI_INL ChiType CHI_TAG(size_t n) {
    CHI_ASSERT(n <= CHI_LAST_TAG);
    return (ChiType)n;
}

CHI_API_INL uint32_t chiTag(Chili c) {
    ChiType t = _chiType(c);
    CHI_ASSERT(t <= CHI_LAST_TAG);
    return (uint32_t)t;
}

CHI_API_INL CHI_WU Chili chiFromFloat64(double f) {
    if (CHI_NANBOXING_ENABLED) {
        Chili c = { f == f ? _chiFloat64ToBits(f) : _CHILI_NAN };
        CHI_ASSERT(_chiUnboxed(c));
        return c;
    }
    return _chiBox64(_chiFloat64ToBits(f));
}

CHI_API_INL CHI_WU double chiToFloat64(Chili c) {
    if (CHI_NANBOXING_ENABLED) {
        CHI_ASSERT(_chiUnboxed(c));
        return _chiBitsToFloat64(CHILI_UN(c));
    }
    return _chiBitsToFloat64(_chiUnbox64(c));
}

CHI_API_INL CHI_WU Chili chiFromFloat32(float v) {
    return chiFromUInt32(_chiFloat32ToBits(v));
}

CHI_API_INL CHI_WU float chiToFloat32(Chili c) {
    return _chiBitsToFloat32(chiToUInt32(c));
}

/**
 * Return true if function closure
 */
CHI_INL CHI_WU bool CHI_PRIVATE(chiFn)(ChiType t) {
    return t >= CHI_FIRST_FN && t <= CHI_LAST_FN;
}

/**
 * Arity of function
 */
CHI_INL CHI_WU uint32_t CHI_PRIVATE(chiFnArity)(Chili c) {
    ChiType t = _chiType(c);
    CHI_ASSERT(_chiFn(t));
    return (uint32_t)(t - CHI_FIRST_FN);
}

CHI_INL uint32_t CHI_PRIVATE(chiFnOrThunkArity)(Chili c) {
    const ChiType t = _chiType(c);
    CHI_ASSERT(_chiFn(t) || t == CHI_THUNKFN);
    return t == CHI_THUNKFN ? 0 : t - CHI_FIRST_FN;
}

/**
 * Do nothing, chiTouch is just used to keep value alive
 */
CHI_API_INL void chiTouch(Chili CHI_UNUSED(c)) {
}

/**
 * Returns true if two Chis are identical.
 */
CHI_API_INL CHI_WU bool chiIdentical(Chili a, Chili b) {
    return CHILI_UN(a) == CHILI_UN(b);
}

CHI_INL CHI_WU Chili* _chiIdx(Chili c, size_t i) {
    CHI_ASSERT(_chiType(c) <= CHI_LAST_IMMUTABLE);
    CHI_ASSERT(i < chiSize(c));
    return _chiPayload(c) + i;
}

/**
 * Pack boolean value
 */
CHI_API_INL CHI_WU Chili chiFromBool(bool b) {
    ChiWord w = b;
    CHI_ASSERT(w == 0 || w == 1);
    return _chiFromUnboxed(w);
}

/**
 * Unpack boolean value
 */
CHI_API_INL CHI_WU bool chiToBool(Chili c) {
    ChiWord b = CHILI_UN(c);
    CHI_ASSERT(b == 0 || b == 1);
    return b;
}

/**
 * True for boxed values and nonzero unboxed values.
 * This function is not the same as chiToBool, which can only be applied to
 * unboxed boolean values (0 or 1).
 */
CHI_API_INL CHI_WU bool chiTrue(Chili c) {
    return CHILI_UN(c) != 0;
}

/**
 * True in case of *Try* function succeeded.
 */
CHI_API_INL CHI_WU bool chiSuccess(Chili c) {
    return CHI_LIKELY(!chiIdentical(c, CHI_FAIL));
}

/**
 * We assume that valid pointers can be packed as unboxed values
 */
CHI_API_INL CHI_WU Chili chiFromPtr(const void* p) {
    return _chiFromUnboxed((uintptr_t)p);
}

/**
 * We assume that valid pointers can be packed as unboxed values
 */
CHI_API_INL CHI_WU void* chiToPtr(Chili c) {
    return (void*)(uintptr_t)_chiToUnboxed(c);
}

/**
 * Pack continuation pointer
 */
CHI_API_INL CHI_WU Chili chiFromCont(ChiCont c) {
    return chiFromPtr(c);
}

/**
 * Unpack continuation pointer
 */
CHI_API_INL CHI_WU ChiCont chiToCont(Chili c) {
    return (ChiCont)chiToPtr(c);
}
