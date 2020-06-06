#include "zip.h"

#if CBY_ARCHIVE_ENABLED

#include <zlib.h>
#include "native/endian.h"
#include "native/mem.h"
#include "native/strutil.h"

#define HT_NOSTRUCT
#define HT_HASH    ZipFileHash
#define HT_ENTRY   ZipFile
#define HT_PREFIX  fileHash
#define HT_KEY(e)  e->name
#define HT_KEYEQ   streq
#define HT_KEYTYPE const char*
#define HT_HASHFN  chiHashCString
#include "native/generic/hashtable.h"

/* General purpose bits:
 *
 * 0: encrypted
 * 1-2: compression level
 * 3: data descriptor used
 * 4: reserved
 * 5: patched data
 * 6: strong encryption
 * 7-10: unused
 * 11: encoding flag, utf8
 * 12: reserved
 * 13: encrypted central dir
 * 14-15: reserved
 */
enum {
    GP_ENCRYPTION  = (1 << 0) | (1 << 6) | (1 << 13),
    GP_DATADESC    = 1 << 3,
    GP_PATCHED     = 1 << 5,
    GP_UNSUPPORTED = GP_ENCRYPTION | GP_DATADESC | GP_PATCHED,
    END_RECORD_SIG = 0x06054b50,
    FILE_RECORD_SIG = 0x02014b50,
    FILE_HEADER_SIZE = 30,
    COMP_STORED = 0,
    COMP_DEFLATE = 8,
    ATTR_DIR = 0x10,
    SIZE_LIMIT = CHI_MiB(64),
    NAME_LIMIT = 4096,
};

typedef struct CHI_PACKED {
    uint32_t sig;
    uint16_t disk;
    uint16_t dirDisk;
    uint16_t numRecordsDisk;
    uint16_t numRecords;
    uint32_t dirSize;
    uint32_t dirStart;
    uint16_t commentSize;
} EndRecord;

typedef struct CHI_PACKED {
    uint32_t sig;
    uint16_t versionMadeBy;
    uint16_t versionNeeded;
    uint16_t generalPurpose;
    uint16_t compression;
    uint16_t lastModTime;
    uint16_t lastModDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t nameSize;
    uint16_t extraSize;
    uint16_t commentSize;
    uint16_t diskNumberStart;
    uint16_t internalAttrs;
    uint32_t externalAttrs;
    uint32_t start;
} FileRecord;

static bool inflateBuffer(void *dst, size_t dstLen, const void *src, size_t srcLen) {
    z_stream s  =
        { .total_in  = (uInt)srcLen,
          .avail_in  = (uInt)srcLen,
          .total_out = (uInt)dstLen,
          .avail_out = (uInt)dstLen,
          .next_in   = (Bytef*)CHI_CONST_CAST(src, void*),
          .next_out  = (Bytef*)dst
        };
    if (inflateInit2(&s, -MAX_WBITS) != Z_OK)
        return false;
    bool ok = inflate(&s, Z_FINISH) == Z_STREAM_END;
    inflateEnd(&s);
    return ok;
}

static ZipResult findEndRecord(ChiFile handle, EndRecord* eocd) {
    uint64_t size = chiFileSize(handle);
    if (size < sizeof (EndRecord) || size > UINT32_MAX)
        return ZIP_ERROR_SIZE;

    uint8_t block[2 * sizeof (EndRecord)];
    size_t blockSize = CHI_MIN((size_t)size, sizeof (block));
    if (!chiFileReadOff(handle, block, blockSize, size - blockSize))
        return ZIP_ERROR_READ;

    // find end of central directory
    size_t pos = blockSize - sizeof (EndRecord);
    for (;;) {
        if (chiLE32(chiPeekUInt32(block + pos)) == END_RECORD_SIG)
            break;
        if (pos == 0)
            return ZIP_ERROR_SIGNATURE;
        --pos;
    }

    memcpy(eocd, block + pos, sizeof (EndRecord));
    return ZIP_OK;
}

static ZipResult parseFileRecord(Zip* z, const uint8_t** pp, const uint8_t* end) {
    const uint8_t* p = *pp;

    FileRecord r;
    if (p + sizeof (r) > end)
        return ZIP_ERROR_OFFSET;
    memcpy(&r, p, sizeof (r));
    p += sizeof (r);

    if (chiLE32(r.sig) != FILE_RECORD_SIG)
        return ZIP_ERROR_SIGNATURE;

    if (chiLE16(r.generalPurpose) & GP_UNSUPPORTED)
        return ZIP_ERROR_FLAGS;

    uint16_t nameSize = chiLE16(r.nameSize);
    if (!nameSize || nameSize > NAME_LIMIT)
        return ZIP_ERROR_NAME;

    uint32_t uncompressedSize = chiLE32(r.uncompressedSize);
    if (uncompressedSize > SIZE_LIMIT)
        return ZIP_ERROR_SIZE;

    *pp = p + nameSize + chiLE16(r.extraSize) + chiLE16(r.commentSize);
    if (*pp > end)
        return ZIP_ERROR_OFFSET;

    if (chiLE32(r.externalAttrs) & ATTR_DIR)
        return ZIP_OK;

    char* name = chiCStringAlloc(nameSize);
    memcpy(name, p, nameSize);

    ZipFile* f;
    if (!fileHashCreate(&z->hash, name, &f)) {
        chiFree(name);
        return ZIP_ERROR_DUPLICATE;
    }
    f->name = name;
    f->compressedSize = chiLE32(r.compressedSize);
    f->uncompressedSize = uncompressedSize;
    f->start = chiLE32(r.start) + FILE_HEADER_SIZE + chiLE16(r.nameSize) + chiLE16(r.extraSize);
    return ZIP_OK;
}

Zip* zipOpen(const char* file, ZipResult* res) {
    CHI_ASSERT(res);

    ChiFile handle = chiFileOpenRead(file);
    if (chiFileNull(handle)) {
        *res = ZIP_ERROR_OPEN;
        return 0;
    }

    EndRecord eocd;
    *res = findEndRecord(handle, &eocd);
    if (*res != ZIP_OK) {
        chiFileClose(handle);
        return 0;
    }

    uint32_t dirSize = chiLE32(eocd.dirSize);
    CHI_AUTO_ALLOC(uint8_t, dir, dirSize);
    if (!chiFileReadOff(handle, dir, dirSize, chiLE32(eocd.dirStart))) {
        *res = ZIP_ERROR_READ;
        chiFileClose(handle);
        return 0;
    }

    uint32_t fileCount = chiLE16(eocd.numRecords);
    Zip* z = (Zip*)chiZalloc(sizeof (Zip) + sizeof (ZipFile) * fileCount);
    z->handle = handle;
    const uint8_t* p = dir;
    for (uint32_t i = 0; i < fileCount; ++i) {
        *res = parseFileRecord(z, &p, dir + dirSize);
        if (*res != ZIP_OK) {
            zipClose(z);
            return 0;
        }
    }

    return z;
}

void zipClose(Zip* z) {
    chiFileClose(z->handle);
    CHI_HT_FOREACH(ZipFileHash, f, &z->hash)
        chiFree(f->name);
    fileHashFree(&z->hash);
    chiFree(z);
}

ZipFile* zipFind(Zip* z, const char* name) {
    return fileHashFind(&z->hash, name);
}

ZipResult zipRead(Zip* z, ZipFile* f, void* buf) {
    if (f->compressedSize == f->uncompressedSize)
        return chiFileReadOff(z->handle, buf, f->uncompressedSize, f->start) ? ZIP_OK : ZIP_ERROR_READ;

    CHI_AUTO_ALLOC(uint8_t, tmp, f->compressedSize);
    if (chiFileReadOff(z->handle, tmp, f->compressedSize, f->start) != 1)
        return ZIP_ERROR_READ;

    if (!inflateBuffer(buf, f->uncompressedSize, tmp, f->compressedSize))
        return ZIP_ERROR_INFLATE;

    return ZIP_OK;
}

#define _ZIP_RESULT(N, n) n,
const char* const zipResultName[] = { ZIP_FOREACH_RESULT(_ZIP_RESULT) };
#undef _ZIP_RESULT

#endif
