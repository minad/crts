#include "zip.h"
#include "inflate.h"
#include "mem.h"
#include "system.h"

#define NOHASH
#define HASH        ZipFileHash
#define ENTRY       ZipFile
#define PREFIX      fileHash
#define KEY(e)      chiStringRef(e->name)
#define EXISTS(e)   e->name
#define KEYEQ(a, b) chiStringRefEq(a, b)
#define HASHFN      chiHashStringRef
#include "hashtable.h"

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
CHI_INL uint16_t cby_letoh16(uint16_t x) { return x; }
CHI_INL uint32_t cby_letoh32(uint32_t x) { return x; }
#else
CHI_INL uint16_t cby_letoh16(uint16_t x) { return __builtin_bswap16(x); }
CHI_INL uint32_t cby_letoh32(uint32_t x) { return __builtin_bswap32(x); }
#endif

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
    FILE_HEADER_SIG = 0x04034b50,
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

typedef struct CHI_PACKED {
    uint32_t sig;
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
} FileHeader;

static ZipResult findEndRecord(FILE* fp, EndRecord* eocd) {
    int64_t isize = fseeko(fp, 0, SEEK_END) ? -1 : ftello(fp);
    if (isize < (int64_t)sizeof (EndRecord) || isize > UINT32_MAX)
        return ZIP_ERROR_SEEK;
    size_t size = (size_t)isize;

    uint8_t block[2 * sizeof (EndRecord)];
    size_t blockSize = CHI_MIN(size, sizeof (block));
    if (fseeko(fp, (off_t)(size - blockSize), SEEK_SET))
        return ZIP_ERROR_SEEK;

    if (fread(block, blockSize, 1, fp) != 1)
        return ZIP_ERROR_READ;

    // find end of central directory
    size_t pos = blockSize - sizeof (EndRecord);
    for (;;) {
        if (cby_letoh32(chiPeekUInt32(block + pos)) == END_RECORD_SIG)
            break;
        if (pos == 0)
            return ZIP_ERROR_SIGNATURE;
        --pos;
    }

    memcpy(eocd, block + pos, sizeof (EndRecord));

    if (cby_letoh32(eocd->dirStart) >= size || fseeko(fp, (off_t)cby_letoh32(eocd->dirStart), SEEK_SET))
        return ZIP_ERROR_SEEK;

    return ZIP_OK;
}

static ZipResult readFileRecord(Zip* z) {
    FileRecord r;
    if (fread(&r, sizeof (r), 1, z->fp) != 1)
        return ZIP_ERROR_READ;

    if (cby_letoh32(r.sig) != FILE_RECORD_SIG)
        return ZIP_ERROR_SIGNATURE;

    if (cby_letoh16(r.generalPurpose) & GP_UNSUPPORTED)
        return ZIP_ERROR_FLAGS;

    uint16_t nameSize = cby_letoh16(r.nameSize);
    if (!nameSize || nameSize > NAME_LIMIT)
        return ZIP_ERROR_NAME;

    if (cby_letoh32(r.externalAttrs) & ATTR_DIR)
        return fseeko(z->fp, nameSize
                        + cby_letoh16(r.extraSize)
                        + cby_letoh16(r.commentSize), SEEK_CUR)
            ? ZIP_ERROR_SEEK : ZIP_OK;

    uint32_t size = cby_letoh32(r.uncompressedSize);
    if (size > SIZE_LIMIT)
        return ZIP_ERROR_SIZE;

    char* name = chiCStringAlloc(nameSize);
    if (fread(name, nameSize, 1, z->fp) != 1) {
        chiFree(name);
        return ZIP_ERROR_READ;
    }

    if (fseeko(z->fp, cby_letoh16(r.extraSize) + cby_letoh16(r.commentSize), SEEK_CUR)) {
        chiFree(name);
        return ZIP_ERROR_SEEK;
    }

    ZipFile* f;
    if (!fileHashCreate(&z->fileHash, chiStringRef(name), &f)) {
        chiFree(name);
        return ZIP_ERROR_DUPLICATE;
    }
    f->name = name;
    f->size = size;
    f->start = cby_letoh32(r.start);

    return ZIP_OK;
}

Zip* zipOpen(const char* file, ZipResult* res) {
    CHI_ASSERT(res);

    FILE* fp = chiFileOpenRead(file);
    if (!fp) {
        *res = ZIP_ERROR_OPEN;
        return 0;
    }

    EndRecord eocd;
    *res = findEndRecord(fp, &eocd);
    if (*res != ZIP_OK) {
        fclose(fp);
        return 0;
    }

    uint32_t fileCount = cby_letoh16(eocd.numRecords);
    Zip* z = (Zip*)chiZalloc(sizeof (Zip) + sizeof (ZipFile) * fileCount);
    z->fp = fp;

    for (uint32_t i = 0; i < fileCount; ++i) {
        *res = readFileRecord(z);
        if (*res != ZIP_OK) {
            zipClose(z);
            return 0;
        }
    }

    return z;
}

void zipClose(Zip* z) {
    fclose(z->fp);
    HASH_FOREACH(fileHash, f, &z->fileHash)
        chiFree(f->name);
    fileHashDestroy(&z->fileHash);
    chiFree(z);
}

ZipFile* zipFind(Zip* z, ChiStringRef name) {
    return fileHashFind(&z->fileHash, name);
}

ZipResult zipRead(Zip* z, ZipFile* f, void* buf) {
    if (fseeko(z->fp, (off_t)f->start, SEEK_SET))
        return ZIP_ERROR_SEEK;

    FileHeader h;
    if (fread(&h, sizeof (FileHeader), 1, z->fp) != 1)
        return ZIP_ERROR_READ;

    if (cby_letoh32(h.sig) != FILE_HEADER_SIG)
        return ZIP_ERROR_SIGNATURE;

    uint16_t comp = cby_letoh16(h.compression);
    if (comp != COMP_DEFLATE && comp != COMP_STORED)
        return ZIP_ERROR_COMPRESSION;

    if (cby_letoh16(h.generalPurpose) & GP_UNSUPPORTED)
        return ZIP_ERROR_FLAGS;

    if (fseeko(z->fp, cby_letoh16(h.nameSize) + cby_letoh16(h.extraSize), SEEK_CUR))
        return ZIP_ERROR_SEEK;

    if (comp == COMP_STORED)
        return fread(buf, f->size, 1, z->fp) == 1
            ? ZIP_OK : ZIP_ERROR_READ;

    uint32_t compressedSize = cby_letoh32(h.compressedSize);
    if (compressedSize > SIZE_LIMIT)
        return ZIP_ERROR_SIZE;

    CHI_AUTO_ALLOC(uint8_t, tmp, compressedSize);
    if (fread(tmp, compressedSize, 1, z->fp) != 1)
        return ZIP_ERROR_READ;

    if (!inflate(buf, f->size, tmp, compressedSize))
        return ZIP_ERROR_INFLATE;

    return ZIP_OK;
}

#define _ZIP_RESULT(N, n) n,
const char* const zipResultName[] = { ZIP_FOREACH_RESULT(_ZIP_RESULT) };
#undef _ZIP_RESULT
