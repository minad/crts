/**
 * Shortcuts for chiFromBool(x)
 */
#define CHI_TRUE  ((Chili){true})
#define CHI_FALSE ((Chili){false})

enum {
    _CHI_COLOR_USED   = 1,
    _CHI_COLOR_SHADED = 2,
    _CHI_COLOR_EPOCH  = 4
};

CHI_NEWTYPE(Color, uint8_t)

typedef enum {
    _CHI_BARRIER_NONE,
    _CHI_BARRIER_SYNC,
    _CHI_BARRIER_ASYNC,
} ChiBarrier;

typedef struct {
    ChiColor   white, gray, black;
    ChiBarrier barrier : 8;
} ChiMarkState;

/**
 * Data word
 */
typedef uint64_t ChiWord;
typedef _Atomic(ChiWord) ChiAtomicWord;

enum {
    CHI_WORDSIZE = 8,
    CHI_WORDBITS = 64
};

/**
 * Poisoned value
 */
#define _CHILI_POISON    (Chili){ _CHILI_REFTAG | CHI_BF_INIT(_CHILI_BF_TYPE, CHI_POISON_TAG) }

#define _CHILI_NAN       (UINT64_C(0xFFF) << 51)
#define _CHILI_LARGE     CHI_BF_MAX(_CHILI_BF_SIZE)
#define _CHILI_REFTAG    CHI_BF_SHIFTED_MASK(_CHILI_BF_REFTAG)

/**
 * Pointer shifting. Since all the addresses are 8 byte
 * aligned, we can shift by 3 bits. However on 32 bit architectures
 * the address space is small enough, such that shifting is not necessary
 * and not beneficial.
 */
#if CHI_ARCH_32BIT
#  define _CHILI_PTR_SHIFT 0
#else
#  define _CHILI_PTR_SHIFT 3
#endif

#if CHI_NANBOXING_ENABLED
#  define _CHILI_BF_TYPE   (0,   9, 0, ChiType)
#  define _CHILI_BF_SIZE   (9,   6, 0, size_t)
#  define _CHILI_BF_GEN    (15,  3, 0, ChiGen)
#  define _CHILI_BF_PTR    (18, 34, _CHILI_PTR_SHIFT, uintptr_t)
#  define _CHILI_BF_REFTAG (52, 12, 0, uint32_t)

_Static_assert(_CHILI_NAN < _CHILI_REFTAG, "NaN value must be less than reftag, such that it is detected as unboxed");
_Static_assert(!CHI_FLOAT63_ENABLED, "63 bit floating point compression must not be enabled together with NaN boxing");

#  if !CHI_ARCH_32BIT
#    define _CHILI_BF_PTR_START (18, 35, _CHILI_PTR_SHIFT, uintptr_t)
_Static_assert(CHI_CHUNK_ARENA_FIXED, "NaN boxing: Chunk arena must be fixed on 64 bit platforms.");
_Static_assert(CHI_BF_GET(_CHILI_BF_PTR_START, _CHILI_REFTAG) == CHI_CHUNK_START,
               "NaN boxing: Chunk start should fall within reference tag");
#  endif

#else
#  define _CHILI_BF_GEN    (0,   3, 0, ChiGen)
#  define _CHILI_BF_PTR    (3,  45, _CHILI_PTR_SHIFT, uintptr_t)
#  define _CHILI_BF_TYPE   (48,  9, 0, ChiType)
#  define _CHILI_BF_SIZE   (57,  6, 0, size_t)
#  define _CHILI_BF_REFTAG (63,  1, 0, bool)
#endif

#if !defined(NDEBUG) && CHI_NANBOXING_ENABLED
#  define _CHILI_EMPTY_REFTAG (UINT64_C(0xE0E) << CHI_BF_SHIFT(_CHILI_BF_REFTAG))
#else
#  define _CHILI_EMPTY_REFTAG 0
#endif

_Static_assert(CHI_MAX_UNPINNED == _CHILI_LARGE - 1, "CHI_MAX_UNPINNED is wrong");

/**
 * Heap generation
 * We use an enum for a bit more type safety,
 * since the compiler warns if enums are cast to int.
 */
typedef enum {
    CHI_GEN_NURSERY = 0,
    CHI_GEN_MAJOR   = CHI_BF_MAX(_CHILI_BF_GEN),
} ChiGen;

enum { _CHI_OBJECT_HEADER_SIZE = 1 };

typedef struct ChiObject_ ChiObject;

typedef struct CHI_PACKED {
    bool dirty : 1;
    bool CHI_CHOICE(CHI_SYSTEM_HAS_TASK, shared, _unused) : 1;
    uint8_t _pad : 6;
} ChiObjectFlags;

/**
 * An object allocated on the major heap consists
 * of a header and the payload. Unused objects are kept in singly-linked lists, where
 * the header word is reused as pointer to the next unused object.
 */
struct CHI_WORD_ALIGNED ChiObject_ {
    union {
        ChiAtomicWord header;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        struct {
            _Atomic(uint8_t)  color, owner; // lowest address: must be lowest bits because of next pointer
            _Atomic(bool)     lock;
            ChiObjectFlags    flags;
            uint32_t          size;
        };
        struct {
            ChiObject* next;
            CHI_IF(CHI_ARCH_32BIT, uint32_t _pad;)
        } free;
#else
        struct {
            uint32_t          size;
            ChiObjectFlags    flags;
            _Atomic(bool)     lock;
            _Atomic(uint8_t)  owner, color; // biggest address: must be lowest bits because of next pointer
        };
        struct {
            CHI_IF(CHI_ARCH_32BIT, uint32_t _pad;)
            ChiObject* next;
        } free;
#endif
    };
    ChiWord payload[];
};

/**
 * Chilis are either unboxed values or contain
 * a pointer to a heap object. On 64 bit, many
 * pointer bits are unused. We use those bits
 * to store meta data, in particular the type of the heap object.
 *
 * Chilis use a NaN boxing format:
 *
 *                          ┌──────────┬─────────────────┬───────────┬────────────┬────────────┐
 *     Bits                 │   63..52 │      51..18     │   17..15  │    14..9   │    8..0    │
 *                          ├──────────┼─────────────────┼───────────┼────────────┼────────────┤
 *     Small object         │    FFF   │ 34 bit heap ptr │ 3 bit gen │  size > 0  │ 9 bit type │
 *                          ├──────────┼─────────────────┼───────────┼────────────┼────────────┤
 *     Large object         │    FFF   │ 34 bit heap ptr │ GEN_MAJOR │ LARGE = 63 │ 9 bit type │
 *                          ├──────────┼─────────────────┴───────────┴────────────┼────────────┤
 *     Unboxed empty object │    E0E   │                    0                     │ 9 bit type │
 *                          ├──────────┼──────────────────────────────────────────┴────────────┤
 *     Unboxed Float64 NaN  │    7FF   │                    0                                  │
 *                          ├──────────┼───────────────────────────────────────────────────────┤
 *     Unboxed Float64 -Inf │    FFF   │                    0                                  │
 *                          ├──────────┴───────────────────────────────────────────────────────┤
 *     Other unboxed value  │                Payload, value < (0xFFF << 52)                    │
 *                          └──────────────────────────────────────────────────────────────────┘
 *
 * Only values larger than (0xFFF << 52) are garbage collected.
 * The heap can have a size of 128G=8*2^34 since the heap pointer field is limited to 34 bits.
 * Large objects are allocated on the major heap and pinned as indicated by
 * the value ::_CHILI_LARGE, which is stored in the size field.
 * Empty objects are used for ADT constructors which
 * consist of only a tag without payload. This only applies to constructors
 * of ADTs, which are not enumerations. Enumerations are ADTs
 * where all constructors do not have payload. They are encoded as unboxed UInt32.
 *
 * Note: If larger heaps than 128G are needed, NaN boxing can be disabled at the cost of introducing
 *       boxed Float64 values. If that is not desired, there are multiple possibilities to
 *       allow larger heaps by using a larger pointer field:
 *       1. Use a smaller size or type field
 *       2. Remove the generation field and compute the generation based on the heap pointer.
 *          Then 37 bit references are possible, allowing a 1T heap.
 *       3. Since NaN is represented by 2^53-2 values, we could rotate the float sign bit
 *          to the end. Then only 11 bits would be used as reference tag (0x7FF vs 0xFFF).
 *          References would be larger than 0x7FF...1, -Inf would be represented as 0x7FF...1.
 *
 * Chilis without NaN boxing:
 *
 *                          ┌─┬────────────┬────────────┬─────────────────┬───────────┐
 *     Bits                 │X│    62..57  │    56..48  │      47..3      │    2..0   │
 *                          ├─┼────────────┼────────────┼─────────────────┼───────────┤
 *     Small object         │1│ 6 bit size │ 9 bit type │ 45 bit heap ptr │ 3 bit gen │
 *                          ├─┼────────────┼────────────┼─────────────────┼───────────┤
 *     Large object         │1│ LARGE = 63 │ 9 bit type │ 45 bit heap ptr │ GEN_MAJOR │
 *                          ├─┼────────────┼────────────┼─────────────────┴───────────┤
 *     Unboxed empty object │0│            │ 9 bit type │              0              │
 *                          ├─┼────────────┴────────────┴─────────────────────────────┤
 *     Other unboxed value  │0│                    63 bit payload                     │
 *                          └─┴───────────────────────────────────────────────────────┘
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
 *     Float63        │0│                  63 bit compressed float data             │
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
CHI_NEWTYPE(Field, ChiAtomicWord)
#define CHILI_UN(x) CHI_UN(li,x)

typedef struct {
    Chili THREAD;
    Chili PROC;
    Chili PLOCAL;
    Chili YIELD;
    CHI_IF(CHI_TRACEPOINTS_CONT_ENABLED, _Atomic(int32_t) TRACEPOINT;)
    CHI_IF(CHI_FNLOG_ENABLED, bool FNLOG;)
    uint8_t VAARGS;
} ChiAuxRegs;

#include CHI_STRINGIZE(callconv/CHI_CALLCONV.h)
typedef CHI_CONT_PROTO((ChiContFn));

/*
 * The frame format depends on the frame type.
 * It is necessary to know the precise format of the stack frames for stack walking.
 *
 *                           ┌───────────────┬───────────────┬─────┐
 *     Generic               │ ChiCont       │ Payload       │ ... │
 *                           └───────────────┴───────────────┴─────┘
 *
 *                           ┌───────────────┬───────────────┐
 *     chiExceptionCatchCont │ ChiCont       │ Old Handler   │
 *                           └───────────────┴───────────────┘
 *
 *                           ┌───────────────┬───────────────┐
 *     chiThunkUpdateCont    │ ChiCont       │ Thunk/Blackh. │
 *                           └───────────────┴───────────────┘
 *
 *                           ┌───────────────┬───────────────┬───────────────┬─────┐
 *     Interpreter/VASIZE    │ ChiCont       │ Frame Size    │ Payload       │ ... │
 *                           └───────────────┴───────────────┴───────────────┴─────┘
 *
 *                           ┌───────────────┬───────────────┬───────────────┬─────┬───────────────┐
 *     chiRestoreCont        │ ChiCont       │ Frame Size    │ Arguments     │ ... │ ChiCont       │
 *                           └───────────────┴───────────────┴───────────────┴─────┴───────────────┘
 *
 * @note ::CHI_VAARGS and ::CHI_VASIZE are not stored in the
 *       type field of ::ChiContInfo but in the respective na and size field.
 *       Notable ::CHI_VASIZE continuations are ::chiRestoreCont and ::overAppN.
 */

/**
 * Frame with variable number of arguments.
 * If ChiContInfo::na == CHI_VAARGS, the number of arguments
 * must be stored in the AUX.VAARGS register.
 */
enum { CHI_VAARGS = 0x1F };

/**
 * Frame with variable size.
 * If ChiContInfo::size == CHI_VASIZE, the frame size
 * is stored in the first field of the frame.
 */
enum { CHI_VASIZE = 0xFFFFFF };

/**
 * Continuation information.
 * In particular, this includes the number of arguments and the size of the frame.

 *
 * @warning The size can be CHI_VASIZE and na can be
 *          CHI_VAARGS.
 *
 * @note This structure is stored next to the code
 *       of the continuation at an alignment of CHI_CONT_PREFIX_ALIGN bytes.
 *       This is achieved by using a custom linker script
 *       which works only with the BFD linker as of now.
 */
#if CHI_CONT_PREFIX
typedef const struct {
    const char* file;
    uint32_t    line : 24;
    char        name[];
} _ChiContInfoLoc;
typedef const struct CHI_PACKED CHI_ALIGNED(CHI_CONT_PREFIX_ALIGN) {
    uint32_t    trap; // Trap instruction, signals that invalid instructions follow
    uint32_t    size    : 24;
    uint8_t     na      : 7;
    CHI_IF(CHI_CBY_SUPPORT_ENABLED, bool interp : 1;)
    CHI_IF(CHI_LOC_ENABLED, _ChiContInfoLoc* loc;)
} ChiContInfo;
typedef ChiContFn* ChiCont;
#define CHI_STATIC_CONT_DECL(n)  static ChiContFn n;
#define CHI_INTERN_CONT_DECL(n)  CHI_INTERN ChiContFn n;
#define CHI_EXPORT_CONT_DECL(n)     extern CHI_EXPORT ChiContFn n;
#define CHI_EXTERN_CONT_DECL(n)  extern ChiContFn n;
#define CHI_CONT_FN(n)           n
#else
typedef const struct CHI_PACKED {
    ChiContFn*  fn;
    uint32_t    size : 24;
    uint8_t     na   : 7;
    CHI_IF(CHI_CBY_SUPPORT_ENABLED, bool interp : 1;)
    CHI_IF(CHI_LOC_ENABLED, uint32_t line : 24; const char* file; char name[];)
} ChiContInfo;
typedef ChiContInfo* ChiCont;
#define _CHI_CONT_DECL(a, b, n) a ChiContFn CHI_CONT_FN(n); b const ChiContInfo n;
#define CHI_STATIC_CONT_DECL(n) _CHI_CONT_DECL(static, static, n)
#define CHI_INTERN_CONT_DECL(n) _CHI_CONT_DECL(CHI_INTERN, CHI_EXTERN, n)
#define CHI_EXTERN_CONT_DECL(n) _CHI_CONT_DECL(extern, extern, n)
#define CHI_EXPORT_CONT_DECL(n) _CHI_CONT_DECL(extern CHI_EXPORT, extern CHI_EXPORT, n)
#define CHI_CONT_FN(n)          CHI_CAT(n, _fn)
#endif

/**
 * Descriptor used for modules provided by dynamic libraries
 */
typedef const struct {
    ChiCont            init;
    int32_t            mainIdx;
    uint32_t           importCount;
    const char* const* importName;
    char               name[];
}  ChiModuleDesc;

typedef struct {
    const ChiModuleDesc*  main;
    const ChiModuleDesc** modules;
    size_t                size;
} ChiModuleRegistry;

#ifdef __clang__
#  define CHI_CC_VERSION_TAG    CHI_CAT4(clang_, __clang_major__, _, __clang_minor__)
#  define CHI_CC_VERSION_STRING "clang " CHI_STRINGIZE(__clang_major__) "." CHI_STRINGIZE(__clang_minor__)
#elif defined (__GNUC__)
#  define CHI_CC_VERSION_TAG    CHI_CAT4(gcc_, __GNUC__, _, __GNUC_MINOR__)
#  define CHI_CC_VERSION_STRING "gcc " CHI_STRINGIZE(__GNUC__) "." CHI_STRINGIZE(__GNUC_MINOR__)
#else
#  error Unsupported Compiler
#endif

// Mangle module functions with compilation mode and compiler version.
// This allows to detect a mismatch early at linking time.
#define chiInitModule     CHI_CAT4(chiInitModule_,     CHI_MODE, _, CHI_CC_VERSION_TAG)
#define chiModuleRegistry CHI_CAT4(chiModuleRegistry_, CHI_MODE, _, CHI_CC_VERSION_TAG)

/**
 * Heap object type.
 *
 * In particular values between CHI_FIRST_TAG and CHI_LAST_TAG
 * are used for the tag of algebraic datatypes and the arity of closures.
 */
typedef enum {
    CHI_FIRST_TAG = 0,               // START: first adt tag, first closure arity
    CHI_LAST_TAG = 487 - CHI_AMAX,   // END: last adt tag, last closure arity
    CHI_FIRST_FN,
    CHI_LAST_FN = CHI_FIRST_FN + CHI_AMAX - 1,
    CBY_INTERP_MODULE,                 // used by interpreter
    CBY_NATIVE_MODULE,                 // used by interpreter
    CHI_LAST_IMMUTABLE = CBY_NATIVE_MODULE, // END: Immutable objects, which do not need atomic access
    CHI_FIRST_MUTABLE,               // START: mutable, special treatment by gc
    CHI_STRINGBUILDER = CHI_FIRST_MUTABLE,
    CHI_ARRAY_SMALL,
    CHI_ARRAY_LARGE,
    CHI_THUNK,
    CHI_LAST_MUTABLE = CHI_THUNK,    // END: mutable
    CHI_THREAD,                      // thread is mutable, but needs special treatment by gc
    CHI_STACK,                       // stack is mutable, but needs special treatment by gc
    CHI_POISON_TAG,
    CHI_FIRST_RAW,                   // START: raw objects, no scanning necessary
    CHI_FAIL = CHI_FIRST_RAW,
    CHI_PRESTRING,
    CHI_BIGINT_POS,
    CHI_BIGINT_NEG,
    CBY_FFI,                         // used by interpreter
    CHI_STRING,
    CHI_BOX,
    CHI_BUFFER0,                     // must be last to allow type>=CHI_BUFFER0 for buffer check
    CHI_BUFFER7 = CHI_BUFFER0 + 7,
    CHI_LAST_TYPE = CHI_BUFFER7,
} ChiType;

_Static_assert(CHI_LAST_TYPE == CHI_BF_MAX(_CHILI_BF_TYPE), "CHI_LAST_TYPE is wrong");

CHI_EXPORT_CONT_DECL(chiPromoteAll)
CHI_EXPORT_CONT_DECL(chiStackTrace)
CHI_EXPORT_CONT_DECL(chiInitModule)
CHI_EXPORT_CONT_DECL(chiExceptionThrow)
CHI_EXPORT_CONT_DECL(chiExceptionCatch)
CHI_EXPORT_CONT_DECL(chiHeapOverflow)
CHI_EXPORT_CONT_DECL(CHI_PRIVATE(chiApp1))
CHI_EXPORT_CONT_DECL(CHI_PRIVATE(chiApp2))
CHI_EXPORT_CONT_DECL(CHI_PRIVATE(chiApp3))
CHI_EXPORT_CONT_DECL(CHI_PRIVATE(chiApp4))
CHI_EXPORT_CONT_DECL(CHI_PRIVATE(chiAppN))
CHI_EXPORT_CONT_DECL(CHI_PRIVATE(chiYieldClos))
CHI_EXPORT_CONT_DECL(CHI_PRIVATE(chiYieldCont))
CHI_EXPORT_CONT_DECL(CHI_PRIVATE(chiProtectEnd))

CHI_LOCAL ChiContFn* const _chiAppTable[] =
    { CHI_CONT_FN(_chiApp1), CHI_CONT_FN(_chiApp2), CHI_CONT_FN(_chiApp3), CHI_CONT_FN(_chiApp4) };

CHI_FLAGTYPE(ChiNewFlags,
             CHI_NEW_DEFAULT       = 0,
             CHI_NEW_TRY           = 1,
             CHI_NEW_UNINITIALIZED = 2,
             CHI_NEW_LOCAL         = 4, // incompatible with CHI_NEW_SHARED
             CHI_NEW_SHARED        = 8, // requires CHI_NEW_CLEAN
             CHI_NEW_CLEAN         = 16,
             CHI_NEW_CARDS         = 32) // requires CHI_NEW_LOCAL

_Noreturn void chiMain(int, char**);
CHI_EXPORT void chiExit(uint8_t);
CHI_EXPORT CHI_WU Chili chiNew(ChiType, size_t);
CHI_EXPORT CHI_WU Chili chiNewFlags(ChiType, size_t, ChiNewFlags);
CHI_EXPORT void chiFnLog(ChiCont);
CHI_EXPORT CHI_WU uint64_t chiCurrentTimeMillis(void);
CHI_EXPORT CHI_WU uint64_t chiCurrentTimeNanos(void);
CHI_EXPORT void chiSortModuleRegistry(ChiModuleRegistry*);
CHI_EXPORT CHI_WU CHI_RET_NONNULL ChiWord* CHI_PRIVATE(chiGrowHeap)(ChiWord*, const ChiWord*);
CHI_EXPORT void CHI_PRIVATE(chiTraceContTime)(ChiCont, Chili*);
CHI_EXPORT void CHI_PRIVATE(chiTraceContTimeName)(ChiCont, Chili*, const char*);
CHI_EXPORT void CHI_PRIVATE(chiTraceContAlloc)(ChiCont, Chili*, size_t);
CHI_EXPORT void CHI_PRIVATE(chiProtectBegin)(Chili*);

#if CHI_PROF_ENABLED
CHI_EXPORT void chiProfilerEnable(bool);
#else
CHI_EXPORT_INL void chiProfilerEnable(bool CHI_UNUSED(x)) {}
#endif

CHI_INL bool CHI_PRIVATE(chiBitGet)(const uintptr_t* bits, size_t n) {
    size_t w = 8 * sizeof (uintptr_t), s = n & (w - 1);
    return !!((bits[n / w] >> s) & 1);
}

CHI_INL bool CHI_PRIVATE(chiBitSet)(uintptr_t* bits, size_t n, bool x) {
    size_t w = 8 * sizeof (uintptr_t), s = n & (w - 1);
    uintptr_t old = bits[n / w];
    bits[n / w] = (old & ~(1UL << s)) | ((uintptr_t)x << s);
    return !!((old >> s) & 1);
}

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

// We manually unpack the double to get a sane conversion to integers
CHI_INL CHI_WU int64_t CHI_PRIVATE(chiFloat64ToInt64)(double x) {
    uint64_t bits = _chiFloat64ToBits(x);
    int32_t exp = (int32_t)((bits >> 52) & 0x7FF);
    if (exp == 0x7FF)
        return 0; // convention: return 0 for +-inf, NaN
    exp -= 1023 + 52;
    int64_t frac = (int64_t)(bits & (-UINT64_C(1) >> 12)) | (INT64_C(1) << 52);
    int64_t sign = bits >> 63 ? -1 : 1;
    if (exp < -63 || exp > 63)
        return 0;
    return sign * (exp < 0 ? frac >> -exp : frac << exp);
}

/**
 * Initialize atomic field. Does not involve a memory barrier.
 */
CHI_INL void CHI_PRIVATE(chiFieldInit)(ChiField* c, Chili x) {
    atomic_store_explicit(CHI_UNP(Field, c), CHILI_UN(x), memory_order_relaxed);
}

/**
 * Atomic load with acquire semantics.
 * No reads or writes in the current thread can be reordered before this operation.
 * All writes in other threads releasing this variable will be visible in this thread.
 *
 * TODO: It would be possible to adopt the OCaml memory model,
 * where reads are relaxed but acquire fences are introduced before
 * atomic writes and compare-and-set.
 */
CHI_INL CHI_WU Chili CHI_PRIVATE(chiFieldRead)(ChiField* c) {
    return (Chili){ atomic_load_explicit(CHI_UNP(Field, c), memory_order_acquire) };
}

CHI_INL CHI_WU bool _chiFitsUnboxed63(ChiWord v) {
    return !(v >> 63);
}

CHI_INL CHI_WU bool CHI_PRIVATE(chiUnboxed63)(Chili c) {
    return _chiFitsUnboxed63(CHILI_UN(c));
}

/**
 * Returns true if the Chili is a reference.
 */
CHI_INL CHI_WU bool CHI_PRIVATE(chiRef)(Chili c) {
    bool ref = CHI_NANBOXING_ENABLED ? CHILI_UN(c) > _CHILI_REFTAG : CHI_BF_GET(_CHILI_BF_REFTAG, CHILI_UN(c));
    CHI_ASSERT(!CHI_POISON_OBJECT_ENABLED || !ref || CHI_BF_GET(_CHILI_BF_TYPE, CHILI_UN(c)) != CHI_POISON_TAG);
    return ref;
}

/**
 * Pack unboxed value
 */
CHI_INL CHI_WU Chili CHI_PRIVATE(chiFromUnboxed)(ChiWord v) {
    Chili c = (Chili){ v };
    CHI_ASSERT(!_chiRef(c));
    return c;
}

/**
 * Unpack unboxed value
 */
CHI_INL CHI_WU ChiWord CHI_PRIVATE(chiToUnboxed)(Chili c) {
    CHI_ASSERT(!_chiRef(c));
    return CHILI_UN(c);
}

CHI_INL CHI_WU ChiType CHI_PRIVATE(chiType)(Chili c) {
    CHI_ASSERT_TAGGED(c);
    return CHI_BF_GET(_CHILI_BF_TYPE, CHILI_UN(c));
}

CHI_INL CHI_WU size_t _chiSizeField(Chili c) {
    CHI_ASSERT_TAGGED(c);
    return CHI_BF_GET(_CHILI_BF_SIZE, CHILI_UN(c));
}

CHI_INL CHI_WU uintptr_t CHI_PRIVATE(chiAddress)(Chili c) {
    CHI_ASSERT_TAGGED(c);
    return CHI_BF_GET(_CHILI_BF_PTR, CHILI_UN(c));
}

CHI_INL CHI_WU CHI_ASSUME_WORD_ALIGNED void* _chiPtrField(Chili c) {
    CHI_ASSERT_TAGGED(c);
#ifdef _CHILI_BF_PTR_START
    return (void*)CHI_BF_GET(_CHILI_BF_PTR_START, CHILI_UN(c));
#else
    return (void*)(CHI_BF_GET(_CHILI_BF_PTR, CHILI_UN(c)) CHI_IF(CHI_NANBOXING_ENABLED, | CHI_CHUNK_START));
#endif
}

/**
 * Returns true for reference of given type. This branchless function should be used
 * instead of the combination `chiRef(c) && chiType(c) == t`.
 */
CHI_INL CHI_WU bool CHI_PRIVATE(chiRefType)(Chili c, ChiType t) {
    CHI_ASSERT(t != 0);
    return (CHILI_UN(c) & (_CHILI_REFTAG | CHI_BF_SHIFTED_MASK(_CHILI_BF_TYPE))) ==
        (_CHILI_REFTAG | ((ChiWord)t << CHI_BF_SHIFT(_CHILI_BF_TYPE)));
}

/**
 * Returns true for reference to major generation. This brancless function should be used
 * instead of the combination `chiRef(c) && chiGen(c) == CHI_GEN_MAJOR`.
 */
CHI_INL CHI_WU bool CHI_PRIVATE(chiRefMajor)(Chili c) {
    return (CHILI_UN(c) & (_CHILI_REFTAG | CHI_BF_SHIFTED_MASK(_CHILI_BF_GEN))) ==
        (_CHILI_REFTAG | ((ChiWord)CHI_GEN_MAJOR << CHI_BF_SHIFT(_CHILI_BF_GEN)));
}

/**
 * Returns heap generation of object
 */
CHI_INL ChiGen CHI_PRIVATE(chiGen)(Chili c) {
    CHI_ASSERT(_chiRef(c));
    return CHI_BF_GET(_CHILI_BF_GEN, CHILI_UN(c));
}

CHI_INL CHI_WU Chili CHI_PRIVATE(chiWrap)(void* p, size_t s, ChiType t, ChiGen g) {
    CHI_ASSERT(s > 0);
    CHI_ASSERT(s <= _CHILI_LARGE);
    CHI_ASSERT(t <= CHI_LAST_TYPE);
    CHI_ASSERT(g >= CHI_GEN_NURSERY && g <= CHI_GEN_MAJOR);

#if CHI_CHUNK_ARENA_FIXED
    CHI_ASSERT((uintptr_t)p >= CHI_CHUNK_START);
    CHI_ASSERT((uintptr_t)p < CHI_CHUNK_START + CHI_CHUNK_MAX_SIZE);
#endif

    return (Chili){ _CHILI_REFTAG
#ifdef _CHILI_BF_PTR_START
            | CHI_BF_INIT(_CHILI_BF_PTR_START, (uintptr_t)p)
#else
            | CHI_BF_INIT(_CHILI_BF_PTR, (uintptr_t)p CHI_IF(CHI_NANBOXING_ENABLED, & ~CHI_CHUNK_START))
#endif
            | CHI_BF_INIT(_CHILI_BF_SIZE, s)
            | CHI_BF_INIT(_CHILI_BF_TYPE, t)
            | CHI_BF_INIT(_CHILI_BF_GEN, g)
            };
}

CHI_INL CHI_WU Chili CHI_PRIVATE(chiWrapLarge)(void* p, size_t s, ChiType t) {
    return _chiWrap(p, CHI_MIN(s, _CHILI_LARGE), t, CHI_GEN_MAJOR);
}

CHI_INL CHI_WU Chili chiNewEmpty(ChiType t) {
    return (Chili){ _CHILI_EMPTY_REFTAG | CHI_BF_INIT(_CHILI_BF_TYPE, t) };
}

CHI_INL CHI_WU bool CHI_PRIVATE(chiEmpty)(Chili c) {
    return !_chiSizeField(c);
}

/**
 * Returns true if the ChiType is a raw type.
 * Raw objects do not contain references to other objects.
 */
CHI_INL CHI_WU bool CHI_PRIVATE(chiRaw)(ChiType t) {
    return t >= CHI_FIRST_RAW;
}

CHI_INL CHI_WU ChiObject* CHI_PRIVATE(chiObjectUnchecked)(Chili c) {
    CHI_ASSERT(_chiGen(c) == CHI_GEN_MAJOR);
    return (ChiObject*)_chiPtrField(c) - 1;
}

CHI_INL CHI_WU bool CHI_PRIVATE(chiObjectShared)(const ChiObject* o) {
    CHI_NOWARN_UNUSED(o);
    return CHI_AND(CHI_SYSTEM_HAS_TASK, o->flags.shared);
}

CHI_INL CHI_WU ChiColor CHI_PRIVATE(chiObjectColor)(const ChiObject* o) {
    return CHI_WRAP(Color, atomic_load_explicit(&o->color, memory_order_relaxed));
}

CHI_INL bool CHI_PRIVATE(chiObjectGarbage)(ChiMarkState s, ChiObject* obj) {
    uint32_t c = CHI_UN(Color, _chiObjectColor(obj));
    return (c & ~(uint32_t)_CHI_COLOR_SHADED) != CHI_UN(Color, s.white) && c != CHI_UN(Color, s.black);
}

#if CHI_POISON_ENABLED
struct _ChiDebugData {
    uintptr_t ownerMinor, ownerMinorMask;
    uint32_t wid;
    ChiMarkState markState;
    bool protect;
};
extern CHI_EXPORT _Thread_local struct _ChiDebugData _chiDebugData;

CHI_INL CHI_WU uint8_t CHI_PRIVATE(chiObjectOwner)(const ChiObject* o) {
    return atomic_load_explicit(&o->owner, memory_order_relaxed);
}

CHI_INL CHI_WU uint8_t CHI_PRIVATE(chiExpectedObjectOwner)(void) {
    // use wid+1 such that first processor does not get 0 (uninitialized)
    // only approximate, but sufficient for debugging
    return (uint8_t)(_chiDebugData.wid + 1);
}

CHI_INL void _chiDebugCheckObjectAlive(ChiMarkState s, Chili c) {
    if (_chiObjectGarbage(s, _chiObjectUnchecked(c)))
        CHI_BUG("Found garbage %C white=%u gray=%u black=%u",
                c,
                CHI_UN(Color, s.white),
                CHI_UN(Color, s.gray),
                CHI_UN(Color, s.black));
}

CHI_INL void _chiDebugCheckObject(Chili c) {
    if (_chiGen(c) == CHI_GEN_MAJOR) {
        ChiObject* obj = _chiObjectUnchecked(c);
        if (!_chiDebugData.protect)
            _chiDebugCheckObjectAlive(_chiDebugData.markState, c);
        if (_chiObjectShared(obj) || _chiObjectOwner(obj) == _chiExpectedObjectOwner())
           return;
    } else if (*(uintptr_t*)((uintptr_t)_chiPtrField(c) & _chiDebugData.ownerMinorMask) == _chiDebugData.ownerMinor) {
        return;
    }
    CHI_BUG("Processor %u: Illegal access to %C", _chiDebugData.wid, c);
}

CHI_INL void _chiDebugCheckRef(Chili c, size_t i, Chili r) {
    if (!_chiRef(r) || _chiGen(c) != CHI_GEN_MAJOR)
        return;
    ChiObject* obj = _chiObjectUnchecked(c);
    if (_chiObjectShared(obj)) {
        if (_chiGen(r) != CHI_GEN_MAJOR || !_chiObjectShared(_chiObjectUnchecked(r)))
            CHI_BUG("Processor %u: Illegal shared to local reference %C@%zu -> %C", _chiDebugData.wid, c, i, r);
    } else {
        if (_chiGen(r) != CHI_GEN_MAJOR && !obj->flags.dirty)
            CHI_BUG("Processor %u: Illegal non-dirty reference %C@%zu -> %C", _chiDebugData.wid, c, i, r);
    }
}
#define CHI_CHECK_OBJECT_ALIVE(s, c) _chiDebugCheckObjectAlive(s, c)
#define CHI_CHECK_OBJECT(c)          _chiDebugCheckObject(c)
#define CHI_CHECK_REF(c, i, r)       _chiDebugCheckRef((c), (i), (r))
#else
#define CHI_CHECK_OBJECT_ALIVE(s, c) ({})
#define CHI_CHECK_OBJECT(c)          ({})
#define CHI_CHECK_REF(c, i, r)       CHI_NOWARN_UNUSED(c)
#endif

CHI_INL CHI_WU ChiObject* CHI_PRIVATE(chiObject)(Chili c) {
    CHI_CHECK_OBJECT(c);
    return _chiObjectUnchecked(c);
}

CHI_INL CHI_WU CHI_ASSUME_WORD_ALIGNED CHI_RET_NONNULL void* CHI_PRIVATE(chiRawPayload)(Chili c) {
    CHI_CHECK_OBJECT(c);
    return _chiPtrField(c);
}

CHI_INL CHI_WU CHI_ASSUME_WORD_ALIGNED Chili* CHI_PRIVATE(chiPayload)(Chili c) {
    CHI_ASSERT(!_chiRaw(_chiType(c)));
    CHI_ASSERT(_chiType(c) != CHI_STACK);
    return (Chili*)_chiRawPayload(c);
}

CHI_INL CHI_WU size_t CHI_PRIVATE(chiSizeSmall)(Chili c) {
    CHI_ASSERT(_chiGen(c) != CHI_GEN_MAJOR);
    size_t s = _chiSizeField(c);
    CHI_ASSERT(s != _CHILI_LARGE);
    return s;
}

CHI_INL CHI_WU size_t CHI_PRIVATE(chiSize)(Chili c) {
    size_t s = _chiSizeField(c);
    return s != _CHILI_LARGE ? s : _chiObject(c)->size;
}

CHI_INL CHI_WU uint32_t chiToUInt32(Chili c) {
    ChiWord v = _chiToUnboxed(c);
    CHI_ASSERT(v <= UINT32_MAX);
    return (uint32_t)v;
}

CHI_INL CHI_WU Chili chiFromUInt32(uint32_t v) {
    return _chiFromUnboxed(v);
}

CHI_INL CHI_WU Chili chiFromInt32(int32_t v) {
    return chiFromUInt32((uint32_t)v);
}

CHI_INL CHI_WU int32_t chiToInt32(Chili c) {
    return (int32_t)chiToUInt32(c);
}

CHI_INL ChiType CHI_FN(size_t n) {
    CHI_ASSERT(n);
    CHI_ASSERT(n <= CHI_AMAX);
    return (ChiType)(CHI_FIRST_FN + n - 1);
}

CHI_INL ChiType CHI_TAG(size_t n) {
    CHI_ASSERT(n <= CHI_LAST_TAG);
    return (ChiType)n;
}

CHI_INL uint32_t chiTag(Chili c) {
    ChiType t = _chiType(c);
    CHI_ASSERT(t <= CHI_LAST_TAG);
    return (uint32_t)t;
}

CHI_INL CHI_WU Chili chiFromFloat32(float v) {
    return chiFromUInt32(_chiFloat32ToBits(v));
}

CHI_INL CHI_WU float chiToFloat32(Chili c) {
    return _chiBitsToFloat32(chiToUInt32(c));
}

/**
 * Do nothing, chiTouch is just used to keep value alive
 */
CHI_EXPORT_INL void chiTouch(Chili CHI_UNUSED(c)) {
}

/**
 * Returns true if two Chis are identical.
 */
CHI_INL CHI_WU bool chiIdentical(Chili a, Chili b) {
    return CHILI_UN(a) == CHILI_UN(b);
}

/**
 * Immutable access
 */
CHI_INL CHI_WU Chili chiIdx(Chili c, size_t i) {
    CHI_ASSERT(_chiType(c) <= CHI_LAST_IMMUTABLE || (_chiType(c) == CHI_THUNK && i != 1));
    CHI_ASSERT(i < _chiSize(c));
    Chili v = _chiPayload(c)[i];
    CHI_CHECK_REF(c, i, v);
    return v;
}

/**
 * Immutable/thunk init
 */
CHI_INL void CHI_PRIVATE(chiInit)(Chili c, Chili* p, Chili v) {
    CHI_ASSERT(_chiType(c) <= CHI_LAST_IMMUTABLE || _chiType(c) == CHI_THUNK);
    CHI_ASSERT(p >= _chiPayload(c));
    CHI_ASSERT(p < _chiPayload(c) + _chiSize(c));
    CHI_CHECK_REF(c, (size_t)(p - _chiPayload(c)), v);
    *p = v;
}

/**
 * Pack boolean value
 */
CHI_INL CHI_WU Chili chiFromBool(bool b) {
    ChiWord w = b;
    CHI_ASSERT(w == 0 || w == 1);
    return _chiFromUnboxed(w);
}

/**
 * Unpack boolean value
 */
CHI_INL CHI_WU bool chiToBool(Chili c) {
    ChiWord b = CHILI_UN(c);
    CHI_ASSERT(b == 0 || b == 1);
    return b;
}

/**
 * True for boxed values and nonzero unboxed values.
 * This function is not the same as chiToBool, which can only be applied to
 * unboxed boolean values (0 or 1).
 */
CHI_INL CHI_WU bool chiTrue(Chili c) {
    return CHILI_UN(c) != 0;
}

/**
 * True in case of *Try* function succeeded.
 */
CHI_EXPORT_INL CHI_WU bool chiSuccess(Chili c) {
    return CHI_LIKELY(!_chiRefType(c, CHI_FAIL));
}

/**
 * We assume that valid pointers can be packed as unboxed values
 */
CHI_INL CHI_WU Chili chiFromPtr(const void* p) {
    return _chiFromUnboxed((uintptr_t)p);
}

/**
 * We assume that valid pointers can be packed as unboxed values
 */
CHI_INL CHI_WU void* chiToPtr(Chili c) {
    return (void*)(uintptr_t)_chiToUnboxed(c);
}

/**
 * Pack continuation pointer
 */
CHI_INL CHI_WU Chili chiFromCont(ChiCont c) {
#if !CHI_CONT_PREFIX && CHI_ARCH_32BIT
    return _chiFromUnboxed(((ChiWord)(uintptr_t)c << 32) | (uintptr_t)c->fn);
#else
    return chiFromPtr(c);
#endif
}

/**
 * Unpack continuation pointer
 */
CHI_INL CHI_WU ChiCont chiToCont(Chili c) {
#if !CHI_CONT_PREFIX && CHI_ARCH_32BIT
    return (ChiCont)(uintptr_t)(_chiToUnboxed(c) >> 32);
#else
    return (ChiCont)chiToPtr(c);
#endif
}

CHI_INL CHI_WU ChiContFn* CHI_PRIVATE(chiContFn)(ChiCont c) {
    return CHI_CHOICE(CHI_CONT_PREFIX, c, c->fn);
}

CHI_INL CHI_WU ChiContFn* CHI_PRIVATE(chiToContFn)(Chili c) {
#if !CHI_CONT_PREFIX && CHI_ARCH_32BIT
    ChiContFn* fn = (ChiContFn*)chiToPtr(c);
    CHI_ASSERT(fn == chiToCont(c)->fn);
    return fn;
#else
    return _chiContFn(chiToCont(c));
#endif
}

CHI_INL CHI_WU ChiContInfo* CHI_PRIVATE(chiContInfo)(ChiCont cont) {
    return CHI_CHOICE(CHI_CONT_PREFIX, CHI_ALIGN_CAST((uint8_t*)cont - CHI_CONT_PREFIX_ALIGN, ChiContInfo*), cont);
}

/**
 * Return true if function closure
 */
CHI_INL CHI_WU bool CHI_PRIVATE(chiFn)(ChiType t) {
    return t >= CHI_FIRST_FN && t <= CHI_LAST_FN;
}

/**
 * Arity of function type
 */
CHI_INL CHI_WU uint32_t CHI_PRIVATE(chiFnTypeArity)(ChiType t) {
    CHI_ASSERT(_chiFn(t));
    return (uint32_t)(t - CHI_FIRST_FN + 1);
}

/**
 * Arity of function
 */
CHI_INL CHI_WU uint32_t CHI_PRIVATE(chiFnArity)(Chili c) {
    uint32_t a = _chiFnTypeArity(_chiType(c));
    CHI_ASSERT(({ uint32_t n = _chiContInfo(chiToCont(chiIdx(c, 0)))->na; n == CHI_VAARGS || n == a + 1; }));
    return a;
}
