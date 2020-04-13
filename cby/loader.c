#include <stdlib.h>
#include "bytecode/decode.h"
#include "cby.h"
#include "native/error.h"
#include "native/ffidefs.h"
#include "native/new.h"
#include "native/sink.h"
#include "native/strutil.h"
#include "zip.h"

#if CBY_CRC32_ENABLED
#  include <zlib.h>
#endif

#define VEC_TYPE   char*
#define VEC_PREFIX stringVec
#define VEC        StringVec
#include "native/generic/vec.h"

#ifdef _WIN32
#  include "dyn/win.h"
#else
#  include "dyn/posix.h"
#endif

typedef char*        CString;
#define S_ELEM       CString
#define S_LESS(a, b) (strcmp(*a, *b) < 0)
#include "native/generic/sort.h"

static bool isQualified(const char* file) {
    return strchr(file, ':') != 0;
}

static bool isLibrary(const char* file) {
    return strends(file, CBY_DYNLIB_EXT)
        || (CBY_ARCHIVE_ENABLED && strends(file, CBY_ARCHIVE_EXT));
}

bool cbyLibrary(const char* file) {
    return isLibrary(file) || isQualified(file);
}

CHI_FMT(1,2) static Chili newErr(ChiProcessor* proc, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    Chili r = chiFmtStringv(proc, fmt, ap);
    va_end(ap);
    return chiNewLeft(proc, r);
}

static Chili newFFI(ChiProcessor* proc, const CbyCode** ipp, ChiStringRef modName) {
    const CbyCode* IP = *ipp + 4;

    ChiStringRef nameStr = FETCH_STRING;
    CHI_AUTO_FREE(name, chiCStringUnsafeDup(nameStr));

    const uint8_t rtype = FETCH8;
    const uint8_t nargs = FETCH8;
    if (nargs > CBY_FFI_AMAX)
        chiErr("FFI function '%s' has too many arguments", name);

#if CBY_HAS_FFI_BACKEND(libffi)
    // Must be pinned because of ffi_type argument array
    const Chili c = chiNewInl(proc, CBY_FFI, CHI_BYTES_TO_WORDS(sizeof (CbyFFI) + nargs * sizeof (ffi_type*)),
                              CHI_NEW_SHARED | CHI_NEW_CLEAN);
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
    const Chili c = chiNewInl(proc, CBY_FFI, CHI_SIZEOF_WORDS(CbyFFI), CHI_NEW_SHARED | CHI_NEW_CLEAN);
    CbyFFI* ffi = chiToCbyFFI(c);
    IP += nargs;
#endif

    *ipp = IP;
    ffi->handle = 0;
    ffi->fn = 0;

#if !CBY_HAS_FFI_BACKEND(none)
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

    chiEvent(proc, FFI_LOAD,
             .module = modName,
             .name = chiStringRef(name));
#else
    CHI_NOWARN_UNUSED(modName);
#endif

    return c;
}

static Chili moduleLoaded(ChiProcessor* proc, Chili mod, ChiStringRef file) {
    const char *path = cbyInterpreter(proc)->ml.path;
    Chili name = chiToCbyModule(mod)->name;
    chiEvent(proc, MODULE_LOAD,
              .module = chiStringRef(&name),
              .file = file,
              .path = chiStringRef(path ? path : ""));
    return chiNewRight(proc, mod);
}

CHI_DEFINE_AUTO(CbyDyn*, cbyDynClose)

static void pushName(const char* name, StringVec* vec) {
    stringVecAppend(vec, chiCStringDup(name));
}

static void pushNameExt(const char* name, StringVec* vec) {
    if (strends(name, CBY_MODULE_EXT))
        pushName(name, vec);
}

static void listDirModules(ChiSink* sink, const char* path, StringVec* vec) {
    CHI_AUTO(dir, chiDirOpen(path), chiDirClose);
    if (chiDirNull(dir)) {
        chiSinkWarn(sink, "Could not open directory '%s': %m", path);
        return;
    }
    const char* entry;
    while ((entry = chiDirEntry(dir)) != 0)
        pushNameExt(entry, vec);
}

static void freeArchive(CbyArchiveCache* cache) {
    if (cache->handle) {
        zipClose((Zip*)cache->handle);
        chiFree(cache->name);
        cache->handle = cache->name = 0;
    }
}

static Zip* loadArchive(CbyArchiveCache* cache, const char* file, ZipResult* res) {
    if (cache->handle && streq(cache->name, file)) {
        *res = ZIP_OK;
        return (Zip*)cache->handle;
    }

    Zip* z = zipOpen(file, res);
    if (z) {
        freeArchive(cache);
        cache->handle = z;
        cache->name = chiCStringDup(file);
    }

    return z;
}

static char* findMainModule(Zip* z) {
    ZipFile* f = zipFind(z, CHI_STRINGREF(CBY_ARCHIVE_MAIN));
    if (f) {
        char* name = chiCStringAlloc(f->uncompressedSize);
        if (zipRead(z, f, name) == ZIP_OK)
            return name;
        chiFree(name);
    }
    return 0;
}

static Chili parseModule(ChiProcessor*, ChiStringRef, Chili);

static Chili loadArchiveModule(ChiProcessor* proc, const char* file, ChiStringRef mainMod) {
    ZipResult res;
    Zip* z = loadArchive(CHI_AND(CBY_ARCHIVE_ENABLED, &cbyInterpreter(proc)->ml.cache), file, &res);
    if (!z)
        return newErr(proc, "Could not open archive '%s': %s", file, zipResultName[res]);

    CHI_AUTO_FREE(mainName, mainMod.size ? 0 : findMainModule(z));
    if (!mainMod.size) {
        if (!mainName)
            return newErr(proc, "No main module found in archive '%s'", file);
        mainMod = chiStringRef(mainName);
    }

    CHI_STRING_SINK(nameSink);
    chiSinkFmt(nameSink, "%S"CBY_MODULE_EXT, mainMod);
    ChiStringRef name = chiSinkString(nameSink);

    ZipFile* f = zipFind(z, name);
    if (!f)
        return newErr(proc, "Could not find module '%S' in archive '%s'", name, file);

    Chili code = chiBufferNewUninitialized(proc, f->uncompressedSize, CHI_NEW_DEFAULT);
    res = zipRead(z, f, cbyCode(code));
    if (res != ZIP_OK)
        return newErr(proc, "Could not read module '%S' from archive '%s': %s",
                      name, file, zipResultName[res]);

    CHI_STRING_SINK(qualFileSink);
    chiSinkFmt(qualFileSink, "%s:%S", file, name);
    return parseModule(proc, chiSinkString(qualFileSink), code);
}

static void listArchiveModules(ChiSink* sink, const char* file, StringVec* vec) {
    ZipResult res;
    CHI_AUTO(z, zipOpen(file, &res), zipClose);
    if (!z) {
        chiSinkWarn(sink, "Could not open archive '%s': %s", file, zipResultName[res]);
        return;
    }
    for (size_t i = 0; i < z->hash.capacity; ++i) {
        const char* name = z->hash.entry[i].name;
        if (name)
            pushNameExt(name, vec);
    }
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
static Chili parseModule(ChiProcessor* proc, ChiStringRef file, Chili code) {
    size_t size = chiBufferSize(code);
    if (size < CBY_MAGIC_SIZE + 4)
        return newErr(proc, "Module '%S' is truncated", file);

    const CbyCode* IP = cbyCode(code);
    if (!memeq(IP, CBY_BYTECODE_MAGIC, CBY_MAGIC_SIZE))
        return newErr(proc, "Module '%S' has invalid file type", file);

#if CBY_CRC32_ENABLED
    IP += size - 4;
    uint32_t checksum = FETCH32;
    IP -= size;
    if (checksum != crc32(crc32(0, 0, 0), IP, (uint32_t)size - 4))
        return newErr(proc, "Invalid checksum in '%S'", file);
#endif

    IP += CBY_MAGIC_SIZE;
    uint32_t constSize = FETCH32;
    IP += constSize;

    const ChiStringRef name = FETCH_STRING;

    const uint32_t ffiCount = FETCH32;
    const Chili mod = chiNewInl(proc, CBY_INTERP_MODULE,
                                   CHI_SIZEOF_WORDS(CbyInterpModule) + ffiCount, CHI_NEW_DEFAULT);
    CbyInterpModule* m = chiToCbyInterpModule(mod);
    m->base.name = chiStringNew(proc, name);
    m->code = code;

    for (uint32_t i = 0; i < ffiCount; ++i)
        m->ffi[i] = newFFI(proc, &IP, name);

    const uint32_t exportCount = FETCH32;
    m->base.exports = chiArrayNewUninitialized(proc, exportCount, CHI_NEW_DEFAULT);
    for (uint32_t i = 0; i < exportCount; ++i) {
        Chili exportName = chiStringNew(proc, FETCH_STRING);
        int32_t exportIdx = (int32_t)FETCH32;
        chiArrayInit(m->base.exports, i, chiNewTuple(proc, exportName, chiFromInt32(exportIdx)));
    }

    const uint32_t importCount = FETCH32;
    m->base.imports = chiArrayNewUninitialized(proc, importCount, CHI_NEW_DEFAULT);
    for (uint32_t i = 0; i < importCount; ++i) {
        Chili importName = chiStringNew(proc, FETCH_STRING);
        chiArrayInit(m->base.imports, i, importName);
    }

    m->base.init = chiFromIP(IP);
    return moduleLoaded(proc, mod, file);
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

static Chili initNativeModule(ChiProcessor* proc, const char* file, const ChiModuleDesc* desc, CbyDyn* h) {
    const Chili mod = chiNewInl(proc, CBY_NATIVE_MODULE, CHI_SIZEOF_WORDS(CbyNativeModule), CHI_NEW_DEFAULT);
    CbyNativeModule* m = chiToCbyNativeModule(mod);
    m->base.name = chiStringNew(proc, desc->name);
    m->base.init = chiFromCont(desc->init);
    m->handle = chiFromPtr(h);

    m->base.exports = chiArrayNewUninitialized(proc, desc->exportCount, CHI_NEW_DEFAULT);
    for (uint32_t i = 0; i < desc->exportCount; ++i)
        chiArrayInit(m->base.exports, i, chiNewTuple(proc,
                                                     chiStringNew(proc, desc->exportName[i]),
                                                     chiFromInt32(desc->exportIdx[i])));

    m->base.imports = chiArrayNewUninitialized(proc, desc->importCount, CHI_NEW_DEFAULT);
    for (uint32_t i = 0; i < desc->importCount; ++i)
        chiArrayInit(m->base.imports, i, chiStringNew(proc, desc->importName[i]));

    return moduleLoaded(proc, mod, chiStringRef(file));
}

typedef const ChiModuleDesc* ModuleDesc;

#define S_ELEM       ModuleDesc
#define S_LESS(a, b) (strcmp((*a)->name, (*b)->name) < 0)
#include "native/generic/sort.h"

void chiSortModuleRegistry(ChiModuleRegistry* registry) {
    sortModuleDesc(registry->modules, registry->size);
}

static Chili loadNativeModule(ChiProcessor* proc, const char* file, ChiStringRef name) {
    CbyDyn* h = cbyDynOpenFile(file);
    if (!h)
        return newErr(proc, "Could not open dynamic library '%s': %s", file, cbyDynError());

    const ChiModuleRegistry* registry = (const ChiModuleRegistry*)cbyDynSym(h, CHI_STRINGIZE(chiModuleRegistry));
    if (!registry) {
        cbyDynClose(h);
        return newErr(proc, CHI_STRINGIZE(chiNativeModuleHash)
                      " not found in dynamic library '%s': %s", file, cbyDynError());
    }

    if (!name.size) {
        if (!registry->main) {
            cbyDynClose(h);
            return newErr(proc, "No main module found in dynamic library '%s'", file);
        }
        return initNativeModule(proc, file, registry->main, h);
    }

    size_t first = 0, last = registry->size - 1U;
    for (;;) {
        const size_t mid = first + (last - first) / 2;
        int32_t cmp = chiStringRefCmp(name, chiStringRef(registry->modules[mid]->name));
        if (!cmp)
            return initNativeModule(proc, file, registry->modules[mid], h);
        if (cmp < 0 && first < mid) {
            last = mid - 1;
            continue;
        }
        if (cmp > 0 && mid < last) {
            first = mid + 1;
            continue;
        }
        cbyDynClose(h);
        return newErr(proc, "Module '%S' not found in dynamic library '%s'", name, file);
    }
}

static Chili loadLibraryModule(ChiProcessor* proc, const char* file, ChiStringRef name) {
    return CBY_ARCHIVE_ENABLED && strends(file, CBY_ARCHIVE_EXT) ?
        loadArchiveModule(proc, file, name) : loadNativeModule(proc, file, name);
}

static Chili loadFileModule(ChiProcessor* proc, const char* file) {
    CHI_AUTO(handle, chiFileOpenRead(file), chiFileClose);
    if (chiFileNull(handle))
        return newErr(proc, "Could not open module '%s': %m", file);

    uint64_t size = chiFileSize(handle);
    if (!size || size > UINT32_MAX)
        return newErr(proc, "Invalid file size of '%s': %m", file);

    Chili code = chiBufferNewUninitialized(proc, (uint32_t)size, CHI_NEW_DEFAULT);
    size_t read;
    if (!chiFileRead(handle, cbyCode(code), (size_t)size, &read) || size != read)
        return newErr(proc, "Could not read module '%s'", file);

    return parseModule(proc, chiStringRef(file), code);
}

void cbyAddPath(CbyModuleLoader* ml, ChiStringRef elem) {
    size_t n = ml->path ? strlen(ml->path) + 1 : 0;
    if (ml->path)
        ml->path[n - 1] = ':';
    ml->path = (char*)chiRealloc(ml->path, n + elem.size + 1);
    memcpy(ml->path + n, elem.bytes, elem.size);
    ml->path[n + elem.size] = 0;
}

static Chili loadFromFile(ChiProcessor* proc, const char* file) {
    if (isQualified(file)) {
        CHI_AUTO_FREE(filePart, chiCStringDup(file));
        char* modPart = strchr(filePart, ':');
        *modPart++ = 0;
        if (strends(modPart, CBY_MODULE_EXT))
            modPart[strlen(modPart) - CHI_STRSIZE(CBY_MODULE_EXT)] = 0;
        return loadLibraryModule(proc, filePart, chiStringRef(modPart));
    }
    if (isLibrary(file))
        return loadLibraryModule(proc, file, CHI_EMPTY_STRINGREF);
    return loadFileModule(proc, file);
}

Chili cbyModuleLoadByName(ChiProcessor* proc, ChiStringRef name) {
    const char* pathPtr = cbyInterpreter(proc)->ml.path;
    CHI_AUTO_FREE(path, chiCStringDup(pathPtr ? pathPtr : "."));
    CHI_STRING_SINK(fileExtSink);
    char* file, *rest = path;
    while ((file = strsplitmut(&rest, ':'))) {
        if (isLibrary(file)) {
            Chili ret = loadLibraryModule(proc, file, name);
            if (chiRight(ret))
                return ret;
        }
        chiSinkFmt(fileExtSink, "%s/%S"CBY_MODULE_EXT, file, name);
        const char* fileExt = chiSinkCString(fileExtSink);
        if (chiFilePerm(fileExt, CHI_FILE_READABLE))
            return loadFromFile(proc, fileExt);
    }
    return newErr(proc, "Module '%S' not found", name);
}

void cbyModuleLoaderDestroy(CbyModuleLoader* ml) {
    CHI_IF(CBY_ARCHIVE_ENABLED, freeArchive(&ml->cache));
    chiFree(ml->path);
}

Chili cbyModuleLoadByNameOrFile(ChiProcessor* proc, const char* name) {
    return chiFilePerm(name, CHI_FILE_READABLE) || cbyLibrary(name) ?
        loadFromFile(proc, name) :
        cbyModuleLoadByName(proc, chiStringRef(name));
}

void cbyModuleListing(CbyModuleLoader* ml, ChiSink* sink) {
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
        if (strends(file, CBY_DYNLIB_EXT)) {
            type = "Native library";
            listNativeModules(sink, file, &vec);
        } else if (CBY_ARCHIVE_ENABLED && strends(file, CBY_ARCHIVE_EXT)) {
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
    const uint32_t exportCount = (uint32_t)chiSize(m->exports);
    for (uint32_t i = 0; i < exportCount; ++i) {
        Chili p = chiArrayRead(m->exports, i), n = chiIdx(p, 0);
        if (chiStringRefEq(chiStringRef(&n), nameRef))
            return chiToInt32(chiIdx(p, 1));
    }
    return -2;
}

Chili cbyModuleParse(Chili name, Chili code) {
    return parseModule(CHI_CURRENT_PROCESSOR, chiStringRef(&name), code);
}
