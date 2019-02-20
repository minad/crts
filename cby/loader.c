#include "adt.h"
#include "cby.h"
#include "event.h"
#include "ffidefs.h"
#include "strutil.h"
#include "sink.h"
#include "error.h"
#include "zip.h"
#include "bytecode/decode.h"
#include <stdlib.h>

#define VEC_TYPE   char*
#define VEC_PREFIX stringVec
#define VEC        StringVec
#include "vector.h"

#ifdef _WIN32
#  include "dyn_win.h"
#else
#  include "dyn_posix.h"
#endif

typedef char*        CString;
#define S_ELEM       CString
#define S_LESS(a, b) (strcmp(*a, *b) < 0)
#include "sort.h"

static bool isQualified(const char* file) {
    return strchr(file, ':') != 0;
}

static bool isLibrary(const char* file) {
    return strends(file, CBY_NATIVE_LIBRARY_EXT)
        || (CBY_ARCHIVE_ENABLED && strends(file, CBY_MODULE_ARCHIVE_EXT));
}

bool cbyLibrary(const char* file) {
    return isLibrary(file) || isQualified(file);
}

CHI_COLD CHI_FMT(1,2) static Chili newErr(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    Chili r = chiFmtStringv(fmt, ap);
    va_end(ap);
    return chiNewLeft(r);
}

static Chili newFFI(const CbyCode** ipp, ChiStringRef modName) {
    const CbyCode* IP = *ipp + 4;

    ChiStringRef nameStr = FETCH_STRING;
    CHI_AUTO_FREE(name, chiCStringUnsafeDup(nameStr));

    const uint8_t rtype = FETCH8;
    const uint8_t nargs = FETCH8;
    if (nargs > CBY_FFI_AMAX)
        chiErr("FFI function '%s' has too many arguments", name);

#if CBY_HAS_FFI_BACKEND(libffi)
    // Must be pinned because of ffi_type argument array
    const Chili c = chiNewPin(CBY_FFI, CHI_BYTES_TO_WORDS(sizeof (CbyFFI) + nargs * sizeof (ffi_type*)));
    CbyFFI* ffi = chiToCbyFFI(c);

#   define _FFI_TYPE_PTR(name, libffi, dcarg, dccall, type) &ffi_type_##libffi,
    static ffi_type* const ffiType[] = { CHI_FOREACH_FFI_TYPE(,_FFI_TYPE_PTR) };
    for (uint32_t i = 0; i < nargs; ++i) {
        uint32_t n = FETCH8;
        if (n >= CHI_DIM(ffiType))
            chiErr("Invalid FFI type '%u' for function '%s'", n, name);
        ffi->atype[i] = ffiType[n];
    }

    if (ffi_prep_cif(&ffi->cif, FFI_DEFAULT_ABI, nargs, ffiType[rtype], ffi->atype) != FFI_OK)
        chiErr("Preparing FFI CIF failed for function '%s' failed", name);
#else
    CHI_NOWARN_UNUSED(rtype);
    const Chili c = chiNew(CBY_FFI, CHI_SIZEOF_WORDS(CbyFFI));
    CbyFFI* ffi = chiToCbyFFI(c);
    IP += nargs;
#endif

    *ipp = IP;
    ffi->handle = 0;

    char* fnName = name, *hasLibPrefix = strchr(fnName, ':');
    if (hasLibPrefix) {
        const char* libName = fnName;
        *hasLibPrefix = 0;
        fnName = hasLibPrefix + 1;
        ffi->handle = cbyDynOpen(libName);
        if (!ffi->handle) {
            chiWarn("FFI library '%s' not found: %s", libName, cbyDynError());
            return c;
        }
    }

    ffi->fn = cbyDynSym(ffi->handle ? ffi->handle : CBY_DYN_DEFAULT_HANDLE, fnName);
    if (!ffi->fn) {
        if (ffi->handle) {
            cbyDynClose(ffi->handle);
            ffi->handle = 0;
        }
        chiWarn("FFI function '%s' not found: %s", fnName, cbyDynError());
        return c;
    }

    CHI_EVENT(CHI_CURRENT_PROCESSOR, FFI_LOAD,
              .module = modName,
              .name = chiStringRef(name));

    return c;
}

static Chili moduleLoaded(CbyModuleLoader* ml, Chili mod, ChiStringRef file) {
    Chili name = chiToCbyModule(mod)->name;
    CHI_EVENT(CHI_CURRENT_PROCESSOR, MODULE_LOAD,
              .module = chiStringRef(&name),
              .file = file,
              .path = chiStringRef(ml->path ? ml->path : ""));
    return chiNewRight(mod);
}

static Chili newCode(uint32_t size) {
    // Code objects are pinned for better cpu caching. Furthermore we can
    // use pointers from ffi to the ffi names located in the code object.
    return chiBufferNewFlags(size, 0, CHI_NEW_PIN | CHI_NEW_UNINITIALIZED);
}

CHI_DEFINE_AUTO(CbyDyn*, cbyDynClose)

static void pushName(const char* name, StringVec* vec) {
    stringVecPush(vec, chiCStringDup(name));
}

static void pushNameExt(const char* name, StringVec* vec) {
    if (strends(name, CBY_MODULE_EXT))
        pushName(name, vec);
}

#ifdef _WIN32
static void listDirModules(ChiSink* sink, const char* path, StringVec* vec) {
    CHI_AUTO_FREE(pathW, chiAllocUtf8To16(path));
    WIN32_FIND_DATAW file;
    HANDLE h = FindFirstFileW(pathW, &file);
    if (h == INVALID_HANDLE_VALUE) {
        chiWinSetErr();
        chiSinkWarn(sink, "Could not open directory '%s': %m", path);
        return;
    }

    do {
        CHI_AUTO_FREE(name, chiAllocUtf16To8(file.cFileName));
        pushNameExt(name, vec);
    } while (FindNextFile(h, &file) != 0);

    FindClose(h);
}
#else
#include <dirent.h>
CHI_DEFINE_AUTO(DIR*, closedir)
static void listDirModules(ChiSink* sink, const char* path, StringVec* vec) {
    CHI_AUTO(dir, opendir(path), closedir);
    if (!dir) {
        chiSinkWarn(sink, "Could not open directory '%s': %m", path);
        return;
    }
    struct dirent* d;
    while ((d = readdir(dir)) != 0)
        pushNameExt(d->d_name, vec);
}
#endif

static Chili parseModule(CbyModuleLoader*, ChiStringRef, Chili);

static void freeArchive(CbyModuleLoader* ml) {
    if (ml->archive) {
        zipClose((Zip*)ml->archive);
        chiFree(ml->archiveName);
        ml->archive = ml->archiveName = 0;
    }
}

static Zip* loadArchive(CbyModuleLoader* ml, const char* file, ZipResult* res) {
    if (ml->archive && streq(ml->archiveName, file)) {
        *res = ZIP_OK;
        return (Zip*)ml->archive;
    }

    Zip* z = zipOpen(file, res);
    if (z) {
        freeArchive(ml);
        ml->archive = z;
        ml->archiveName = chiCStringDup(file);
    }

    return z;
}

static char* findMainModule(Zip* z) {
    ZipFile* f = zipFind(z, CHI_STRINGREF(CBY_ARCHIVE_MAIN));
    if (f) {
        char* name = chiCStringAlloc(f->size);
        if (zipRead(z, f, name) == ZIP_OK)
            return name;
        chiFree(name);
    }
    return 0;
}

static Chili loadArchiveModule(CbyModuleLoader* ml, const char* file, ChiStringRef mainMod) {
    ZipResult res;
    Zip* z = loadArchive(ml, file, &res);
    if (!z)
        return newErr("Could not open archive '%s': %s", file, zipResultName[res]);

    CHI_AUTO_FREE(mainName, mainMod.size ? 0 : findMainModule(z));
    if (!mainMod.size) {
        if (!mainName)
            return newErr("No main module found in archive '%s'", file);
        mainMod = chiStringRef(mainName);
    }

    CHI_STRING_SINK(nameSink);
    chiSinkFmt(nameSink, "%S"CBY_MODULE_EXT, mainMod);
    ChiStringRef name = chiSinkString(nameSink);

    ZipFile* f = zipFind(z, name);
    if (!f)
        return newErr("Could not find module '%S' in archive '%s'", name, file);

    Chili code = newCode(f->size);
    res = zipRead(z, f, cbyCode(code));
    if (res != ZIP_OK)
        return newErr("Could not read module '%S' from archive '%s': %s",
                      name, file, zipResultName[res]);

    CHI_STRING_SINK(qualFileSink);
    chiSinkFmt(qualFileSink, "%s:%S", file, name);
    return parseModule(ml, chiSinkString(qualFileSink), code);
}

static void listArchiveModules(ChiSink* sink, const char* file, StringVec* vec) {
    ZipResult res;
    CHI_AUTO(z, zipOpen(file, &res), zipClose);
    if (!z) {
        chiSinkWarn(sink, "Could not open archive '%s': %s", file, zipResultName[res]);
        return;
    }
    for (size_t i = 0; i < z->fileHash.capacity; ++i) {
        const char* name = z->fileHash.entry[i].name;
        if (name)
            pushNameExt(name, vec);
    }
}

// Karl Malbrain's compact CRC-32. See "A compact CCITT crc16 and crc32 C implementation
// that balances processor cache usage against speed".
static uint32_t crc32(uint32_t crc, const void* p, const size_t n) {
    static const uint32_t crc32Table[] = {
        0, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
        0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
    };
    crc = ~crc;
    const uint8_t* b = (const uint8_t*)p;
    for (size_t i = 0; i < n; ++i) {
        crc = (crc >> 4) ^ crc32Table[(crc & 0xF) ^ (b[i] & 0xF)];
        crc = (crc >> 4) ^ crc32Table[(crc & 0xF) ^ (b[i] >> 4)];
    }
    return ~crc;
}

/**
 * Module file format
 *
 *     ┌───────────────────────────────────────────┐
 *     │ CBY_BYTECODE_MAGIC                        │
 *     ├────────────────┬──────────────────────────┤ ─┐
 *     │ constSize      │ Constants...             │  │
 *     ├────────────────┴──────────────────────────┤  │
 *     │ nameOffset                                │  │
 *     ├────────────────┬──────────────────────────┤  │
 *     │ ffiCount       │ FFI entries...           │  │
 *     ├────────────────┼──────────────────────────┤  │ Header
 *     │ exportCount    │ Export entries...        │  │
 *     ├────────────────┼──────────────────────────┤  │
 *     │ importCount    │ Import entries...        │  │
 *     ├────────────────┴──────────────────────────┤  │
 *     │ initOffset                                │  │
 *     ├────────────────┬──────────────────────────┤ ─┘
 *     │ codeSize       │ Functions...             │
 *     ├────────────────┴──────────────────────────┤
 *     │ crc32 checksum                            │
 *     └───────────────────────────────────────────┘
 *
 *
 * Function format
 *
 *     ┌────────────────┬───────────┬─────────────┐
 *     │ line           │ fileOff   │ nameOff     │
 *     ├────────────────┼───────────┴─────────────┤
 *     │ size           │ Instructions...         │
 *     └────────────────┴─────────────────────────┘
 *
 */
static Chili parseModule(CbyModuleLoader* ml, ChiStringRef file, Chili code) {
    size_t size = chiBufferSize(code);
    if (size < CBY_MAGIC_SIZE + 4)
        return newErr("Module '%S' is truncated", file);

    const CbyCode* IP = cbyCode(code);
    if (!memeq(IP, CBY_BYTECODE_MAGIC, CBY_MAGIC_SIZE))
        return newErr("Module '%S' has invalid file type", file);

    IP += size - 4;
    uint32_t checksum = FETCH32;
    IP -= size;
    if (checksum != crc32(0, IP, size - 4))
        return newErr("Invalid checksum in '%S'", file);

    IP += CBY_MAGIC_SIZE;
    uint32_t constSize = FETCH32;
    IP += constSize;

    const ChiStringRef name = FETCH_STRING;

    const uint32_t ffiCount = FETCH32;
    const Chili mod = chiNew(CBY_INTERP_MODULE, CHI_SIZEOF_WORDS(CbyInterpModule) + ffiCount);
    CbyInterpModule* m = chiToCbyInterpModule(mod);
    m->name = chiStringNew(name);
    m->code = code;

    for (uint32_t i = 0; i < ffiCount; ++i)
        m->ffi[i] = newFFI(&IP, name);

    const uint32_t exportCount = FETCH32;
    m->exports = exportCount > 0 ? chiArrayNewFlags(exportCount, CHI_FALSE, CHI_NEW_UNINITIALIZED) : CHI_FALSE;
    for (uint32_t i = 0; i < exportCount; ++i) {
        Chili exportName = chiStringNew(FETCH_STRING);
        int32_t exportIdx = (int32_t)FETCH32;
        chiAtomicInit(chiArrayField(m->exports, i), chiNewTuple(exportName, chiFromInt32(exportIdx)));
    }

    const uint32_t importCount = FETCH32;
    m->imports = importCount > 0 ? chiArrayNewFlags(importCount, CHI_FALSE, CHI_NEW_UNINITIALIZED) : CHI_FALSE;
    for (uint32_t i = 0; i < importCount; ++i) {
        Chili importName = chiStringNew(FETCH_STRING);
        chiAtomicInit(chiArrayField(m->imports, i), importName);
    }

    m->init = chiFromIP(IP);
    return moduleLoaded(ml, mod, file);
}

static void listNativeModules(ChiSink* sink, const char* file, StringVec* vec) {
    CHI_AUTO(h, cbyDynOpenFile(file), cbyDynClose);
    if (!h) {
        chiSinkWarn(sink, "Could not open dynamic library '%s': %s", file, cbyDynError());
        return;
    }

    const ChiModuleRegistry* registry = (const ChiModuleRegistry*)cbyDynSym(h, CHI_STRINGIZE(chiModuleRegistry));
    if (!registry) {
        chiSinkWarn(sink, CHI_STRINGIZE(chiModuleRegistry)
                    " not found in dynamic library '%s': %s", file, cbyDynError());
        return;
    }

    for (size_t i = 0; i < registry->size; ++i)
        pushName(registry->modules[i]->name, vec);
}

static Chili initNativeModule(CbyModuleLoader* ml, const char* file, const ChiModuleDesc* desc, CbyDyn* h) {
    const Chili mod = chiNew(CBY_NATIVE_MODULE, CHI_SIZEOF_WORDS(CbyNativeModule));
    CbyNativeModule* m = chiToCbyNativeModule(mod);
    m->name = chiStringNew(desc->name);
    m->init = chiFromCont(desc->init);
    m->handle = chiFromPtr(h);

    m->exports = desc->exportCount > 0 ? chiArrayNewFlags(desc->exportCount, CHI_FALSE, CHI_NEW_UNINITIALIZED) : CHI_FALSE;
    for (uint32_t i = 0; i < desc->exportCount; ++i)
        chiAtomicInit(chiArrayField(m->exports, i), chiNewTuple(chiStringNew(desc->exportName[i]),
                                                                          chiFromInt32(desc->exportIdx[i])));

    m->imports = desc->importCount > 0 ? chiArrayNewFlags(desc->importCount, CHI_FALSE, CHI_NEW_UNINITIALIZED) : CHI_FALSE;
    for (uint32_t i = 0; i < desc->importCount; ++i)
        chiAtomicInit(chiArrayField(m->imports, i), chiStringNew(desc->importName[i]));

    return moduleLoaded(ml, mod, chiStringRef(file));
}

typedef const ChiModuleDesc* ModuleDesc;

#define S_ELEM       ModuleDesc
#define S_LESS(a, b) (strcmp((*a)->name, (*b)->name) < 0)
#include "sort.h"

void chiSortModuleRegistry(ChiModuleRegistry* registry) {
    sortModuleDesc(registry->modules, registry->size);
}

static Chili loadNativeModule(CbyModuleLoader* ml, const char* file, ChiStringRef name) {
    CbyDyn* h = cbyDynOpenFile(file);
    if (!h)
        return newErr("Could not open dynamic library '%s': %s", file, cbyDynError());

    const ChiModuleRegistry* registry = (const ChiModuleRegistry*)cbyDynSym(h, CHI_STRINGIZE(chiModuleRegistry));
    if (!registry) {
        cbyDynClose(h);
        return newErr(CHI_STRINGIZE(chiNativeModuleHash)
                      " not found in dynamic library '%s': %s", file, cbyDynError());
    }

    if (!name.size) {
        if (!registry->main) {
            cbyDynClose(h);
            return newErr("No main module found in dynamic library '%s'", file);
        }
        return initNativeModule(ml, file, registry->main, h);
    }

    size_t first = 0, last = registry->size - 1U;
    for (;;) {
        const size_t mid = first + (last - first) / 2;
        int32_t cmp = chiStringRefCmp(name, chiStringRef(registry->modules[mid]->name));
        if (!cmp)
            return initNativeModule(ml, file, registry->modules[mid], h);
        if (cmp < 0 && first < mid) {
            last = mid - 1;
            continue;
        }
        if (cmp > 0 && mid < last) {
            first = mid + 1;
            continue;
        }
        cbyDynClose(h);
        return newErr("Module '%S' not found in dynamic library '%s'", name, file);
    }
}

static Chili loadLibraryModule(CbyModuleLoader* ml, const char* file, ChiStringRef name) {
    return CBY_ARCHIVE_ENABLED && strends(file, CBY_MODULE_ARCHIVE_EXT) ?
        loadArchiveModule(ml, file, name) : loadNativeModule(ml, file, name);
}

CHI_DEFINE_AUTO(FILE*, fclose)

static uint64_t fileSize(FILE* fp) {
    int64_t size = fseeko(fp, 0, SEEK_END) ? -1 : ftello(fp);
    return size < 0 || fseeko(fp, 0, SEEK_SET) ? 0 : (uint64_t)size;
}

static Chili loadFileModule(CbyModuleLoader* ml, const char* file) {
    CHI_AUTO(fp, chiFileOpenRead(file), fclose);
    if (!fp)
        return newErr("Could not open module '%s': %m", file);

    uint64_t size = fileSize(fp);
    if (!size || size > UINT32_MAX)
        return newErr("Invalid file size of '%s': %m", file);

    Chili code = newCode((uint32_t)size);
    if (fread(cbyCode(code), (uint32_t)size, 1, fp) != 1)
        return newErr("Could not read module '%s'", file);

    return parseModule(ml, chiStringRef(file), code);
}

Chili cbyLoadFromStdin(CbyModuleLoader* ml, uint32_t size) {
    Chili code = newCode(size);
    if (fread(cbyCode(code), size, 1, stdin) != 1)
        return newErr("Could not read module from stdin");
    return parseModule(ml, CHI_STRINGREF("stdin"), code);
}

void cbyAddPath(CbyModuleLoader* ml, ChiStringRef elem) {
    size_t n = ml->path ? strlen(ml->path) + 1 : 0;
    if (ml->path)
        ml->path[n - 1] = ':';
    ml->path = (char*)chiRealloc(ml->path, n + elem.size + 1);
    memcpy(ml->path + n, elem.bytes, elem.size);
    ml->path[n + elem.size] = 0;
}

Chili cbyLoadByName(CbyModuleLoader* ml, ChiStringRef name) {
    CHI_AUTO_FREE(path, chiCStringDup(ml->path ? ml->path : "."));
    CHI_STRING_SINK(fileExtSink);
    char* file, *rest = path;
    while ((file = strsplitmut(&rest, ':'))) {
        if (isLibrary(file)) {
            Chili ret = loadLibraryModule(ml, file, name);
            if (chiRight(ret))
                return ret;
        }
        chiSinkFmt(fileExtSink, "%s/%S" CBY_MODULE_EXT, file, name);
        const char* fileExt = chiSinkCString(fileExtSink);
        if (chiFilePerm(fileExt, CHI_FILE_READABLE))
            return cbyLoadFromFile(ml, fileExt);
    }
    return newErr("Module '%S' not found", name);
}

Chili cbyLoadFromFile(CbyModuleLoader* ml, const char* file) {
    if (isQualified(file)) {
        CHI_AUTO_FREE(filePart, chiCStringDup(file));
        char* modPart = strchr(filePart, ':');
        *modPart = 0;
        return loadLibraryModule(ml, filePart, chiStringRef(modPart + 1));
    }
    if (isLibrary(file))
        return loadLibraryModule(ml, file, CHI_EMPTY_STRINGREF);
    return loadFileModule(ml, file);
}

void cbyModuleLoaderDestroy(CbyModuleLoader* ml) {
    freeArchive(ml);
    chiFree(ml->path);
}

Chili cbyLoadByNameOrFile(CbyModuleLoader* ml, const char* name) {
    return chiFilePerm(name, CHI_FILE_READABLE) || cbyLibrary(name) ?
        cbyLoadFromFile(ml, name) :
        cbyLoadByName(ml, chiStringRef(name));
}

void cbyListModules(CbyModuleLoader* ml, ChiSink* sink) {
    if (!ml->path) {
        chiSinkPuts(sink, "Path is empty\n");
        return;
    }
    chiSinkFmt(sink, "Path %s\n", ml->path);
    StringVec vec = {};
    CHI_AUTO_FREE(path, chiCStringDup(ml->path));
    char* file, *rest = path;
    while ((file = strsplitmut(&rest, ':'))) {
        vec.used = 0;
        const char* type;
        if (strends(file, CBY_NATIVE_LIBRARY_EXT)) {
            type = "Native library";
            listNativeModules(sink, file, &vec);
        } else if (CBY_ARCHIVE_ENABLED && strends(file, CBY_MODULE_ARCHIVE_EXT)) {
            type = "Archive";
            listArchiveModules(sink, file, &vec);
        } else {
            type = "Directory";
            listDirModules(sink, file, &vec);
        }
        chiSinkFmt(sink, "%s %s\n", type, file);
        sortCString(vec.elem, vec.used);
        for (size_t i = 0; i < vec.used; ++i) {
            char* name = CHI_VEC_AT(&vec, i);
            chiSinkFmt(sink, "  %s\n", name);
            chiFree(name);
        }
    }
    stringVecFree(&vec);
}

int32_t cbyModuleExport(const Chili c, const char* name) {
    const CbyModule* m = chiToCbyModule(c);
    ChiStringRef nameRef = chiStringRef(name);
    if (chiTrue(m->exports)) {
        const uint32_t exportCount = (uint32_t)chiSize(m->exports);
        for (uint32_t i = 0; i < exportCount; ++i) {
            Chili p = chiArrayRead(m->exports, i);
            if (chiStringRefEq(chiStringRef(&CHI_IDX(p, 0)), nameRef))
                return chiToInt32(CHI_IDX(p, 1));
        }
    }
    return -2;
}
