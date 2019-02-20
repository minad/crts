/*
 * Inflate library derived from tinflate by Joergen Ibsen.
 *
 * https://github.com/jibsen/tinf
 *
 * The following license applies to tinflate and this derivative.
 *
 * Copyright (c) 2003-2019 Joergen Ibsen
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must
 *      not claim that you wrote the original software. If you use this
 *      software in a product, an acknowledgment in the product
 *      documentation would be appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must
 *      not be misrepresented as being the original software.
 *
 *   3. This notice may not be removed or altered from any source
 *      distribution.
 */

#include "inflate.h"
#include "private.h"

typedef struct {
    uint16_t counts[16];   // Number of codes with a given length
    uint16_t symbols[288]; // Symbols sorted by code
    int32_t maxSym;
} InflateTree;

typedef struct {
    InflateTree ltree; // Literal/length tree
    InflateTree dtree; // Distance tree
    const uint8_t *src, *srcEnd;
    uint8_t *dstStart, *dst, *dstEnd;
    uint32_t tag, bitcount, overflow;
} InflateData;

CHI_INL uint16_t inflateReadLE16(const uint8_t *p) {
    return (uint16_t)(((uint32_t) p[0]) | ((uint32_t) p[1] << 8));
}

// Build fixed Huffman trees
static void inflateBuildFixedTrees(InflateTree *lt, InflateTree *dt) {
    // Build fixed literal/length tree
    for (uint32_t i = 0; i < 16; ++i)
        lt->counts[i] = 0;

    lt->maxSym = 285;
    lt->counts[7] = 24;
    lt->counts[8] = 152;
    lt->counts[9] = 112;

    for (uint16_t i = 0; i < 24; ++i)
        lt->symbols[i] = (uint16_t)(256 + i);
    for (uint16_t i = 0; i < 144; ++i)
        lt->symbols[24 + i] = i;
    for (uint16_t i = 0; i < 8; ++i)
        lt->symbols[24 + 144 + i] = (uint16_t)(280 + i);
    for (uint16_t i = 0; i < 112; ++i)
        lt->symbols[24 + 144 + 8 + i] = (uint16_t)(144 + i);

    // Build fixed distance tree
    for (uint32_t i = 0; i < 16; ++i)
        dt->counts[i] = 0;

    dt->maxSym = 29;
    dt->counts[5] = 32;

    for (uint16_t i = 0; i < 32; ++i)
        dt->symbols[i] = i;
}

// Given an array of code lengths, build a tree
CHI_INL bool inflateBuildTree(InflateTree *t, const uint8_t *lengths, const uint32_t num) {
  CHI_ASSERT(num <= 288);

    for (uint32_t i = 0; i < 16; ++i)
        t->counts[i] = 0;

    // Count number of codes for each non-zero length
    t->maxSym = -1;
    for (uint32_t i = 0; i < num; ++i) {
        CHI_ASSERT(lengths[i] <= 15);
        if (lengths[i]) {
            t->maxSym = (int32_t)i;
            t->counts[lengths[i]]++;
        }
    }

    // Compute offset table for distribution sort
    uint32_t numCodes = 0, available = 1;
    uint16_t offs[16];
    for (uint32_t i = 0; i < 16; ++i) {
        const uint16_t used = t->counts[i];
        // Check length contains no more codes than available
        if (used > available)
            return false;
        available = 2 * (available - used);
        offs[i] = (uint16_t)numCodes;
        numCodes += used;
    }

    // Check all codes were used, or for the special case of only one
    // code that it has length 1
    if ((numCodes > 1 && available > 0) || (numCodes == 1 && t->counts[1] != 1))
        return false;

    // Fill in symbols sorted by code
    for (uint16_t i = 0; i < num; ++i) {
        if (lengths[i])
            t->symbols[offs[lengths[i]]++] = i;
    }

    // For the special case of only one code (which will be 0) add a
    // code 1 which results in a symbol that is too large
    if (numCodes == 1) {
        t->counts[1] = 2;
        t->symbols[1] = (uint16_t)(t->maxSym + 1);
    }

    return true;
}

CHI_INL void inflateRefill(InflateData *d, const uint32_t num) {
    CHI_ASSERT(num <= 32);

    // Read bytes until at least num bits available
    while (d->bitcount < num) {
        if (d->src != d->srcEnd)
            d->tag |= (uint32_t) *d->src++ << d->bitcount;
        else
            d->overflow = 1;
        d->bitcount += 8;
    }

    CHI_ASSERT(d->bitcount <= 32);
}

CHI_INL uint32_t inflateGetBitsNoRefill(InflateData *d, const uint32_t num) {
    CHI_ASSERT(num <= d->bitcount);

    // Get bits from tag
    const uint32_t bits = d->tag & ((1U << num) - 1);

    // Remove bits from tag
    d->tag >>= num;
    d->bitcount -= num;

    return bits;
}

// Get num bits from src stream
CHI_INL uint32_t inflateGetBits(InflateData *d, const uint32_t num) {
    inflateRefill(d, num);
    return inflateGetBitsNoRefill(d, num);
}

// Read a num bit value from stream and add base
CHI_INL uint32_t inflateGetBitsBase(InflateData *d, const uint32_t num, uint32_t base) {
    return base + (num ? inflateGetBits(d, num) : 0);
}

// Given a data stream and a tree, decode a symbol
CHI_INL uint16_t inflateDecodeSymbol(InflateData *d, const InflateTree *t) {
    /* Get more bits while code index is above number of codes
     *
     * Rather than the actual code, we are computing the position of the
     * code in the sorted order of codes, which is the index of the
     * corresponding symbol.
     *
     * Conceptually, for each code length (level in the tree), there are
     * counts[len] leaves on the left and internal nodes on the right.
     * The index we have decoded so far is base + offs, and if that
     * falls within the leaves we are done. Otherwise we adjust the range
     * of offs and add one more bit to it.
     */
    uint32_t base = 0, offs = 0;
    for (uint32_t len = 1; ; ++len) {
        offs = 2 * offs + inflateGetBits(d, 1);

        CHI_ASSERT(len <= 15);

        if (offs < t->counts[len])
            break;

        base += t->counts[len];
        offs -= t->counts[len];
    }

    CHI_ASSERT(base + offs < 288);
    return t->symbols[base + offs];
}

// Given a data stream, decode dynamic trees from it
static bool inflateDecodeTrees(InflateData *d, InflateTree *lt, InflateTree *dt) {
    // Special ordering of code length codes
    static const uint8_t clcidx[19] = {
        16, 17, 18, 0,  8, 7,  9, 6, 10, 5,
        11,  4, 12, 3, 13, 2, 14, 1, 15
    };

    // Get 5 bits HLIT (257-286)
    const uint32_t hlit = inflateGetBitsBase(d, 5, 257);

    // Get 5 bits HDIST (1-32)
    const uint32_t hdist = inflateGetBitsBase(d, 5, 1);

    // Get 4 bits HCLEN (4-19)
    const uint32_t hclen = inflateGetBitsBase(d, 4, 4);

    /* The RFC limits the range of HLIT to 286, but lists HDIST as range
     * 1-32, even though distance codes 30 and 31 have no meaning. While
     * we could allow the full range of HLIT and HDIST to make it possible
     * to decode the fixed trees with this function, we consider it an
     * error here.
     *
     * See also: https://github.com/madler/zlib/issues/82
     */
    if (hlit > 286 || hdist > 30)
        return false;

    uint8_t lengths[288 + 32];
    for (uint32_t i = 0; i < 19; ++i)
        lengths[i] = 0;

    // Read code lengths for code length alphabet
    for (uint32_t i = 0; i < hclen; ++i) {
        // Get 3 bits code length (0-7)
        lengths[clcidx[i]] = (uint8_t)inflateGetBits(d, 3);
    }

    // Build code length tree (in literal/length tree to save space)
    if (!inflateBuildTree(lt, lengths, 19))
        return false;

    // Check code length tree is not empty
    if (lt->maxSym == -1)
        return false;

    // Decode code lengths for the dynamic trees
    for (uint32_t num = 0; num < hlit + hdist; ) {
        uint16_t sym = inflateDecodeSymbol(d, lt);

        if (sym > lt->maxSym)
            return false;

        uint32_t length;
        switch (sym) {
        case 16:
            // Copy previous code length 3-6 times (read 2 bits)
            if (num == 0)
                return false;
            sym = lengths[num - 1];
            length = inflateGetBitsBase(d, 2, 3);
            break;
        case 17:
            // Repeat code length 0 for 3-10 times (read 3 bits)
            sym = 0;
            length = inflateGetBitsBase(d, 3, 3);
            break;
        case 18:
            // Repeat code length 0 for 11-138 times (read 7 bits)
            sym = 0;
            length = inflateGetBitsBase(d, 7, 11);
            break;
        default:
            // Values 0-15 represent the actual code lengths
            length = 1;
            break;
        }

        if (length > hlit + hdist - num)
            return false;

        while (length--)
            lengths[num++] = (uint8_t)sym;
    }

    // Check EOB symbol is present
    if (lengths[256] == 0)
        return false;

    // Build dynamic trees
    if (!inflateBuildTree(lt, lengths, hlit))
        return false;

    if (!inflateBuildTree(dt, lengths + hlit, hdist))
        return false;

    return true;
}

// Given a stream and two trees, inflate a block of data
CHI_INL bool inflateBlockData(InflateData *d, InflateTree *lt, InflateTree *dt) {
    // Extra bits and base tables for length codes
    static const uint8_t lengthBits[30] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
        1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
        4, 4, 4, 4, 5, 5, 5, 5, 0, 127
    };

    static const uint16_t lengthBase[30] = {
        3,  4,  5,   6,   7,   8,   9,  10,  11,  13,
        15, 17, 19,  23,  27,  31,  35,  43,  51,  59,
        67, 83, 99, 115, 131, 163, 195, 227, 258,   0
    };

    // Extra bits and base tables for distance codes
    static const uint8_t distBits[30] = {
        0, 0,  0,  0,  1,  1,  2,  2,  3,  3,
        4, 4,  5,  5,  6,  6,  7,  7,  8,  8,
        9, 9, 10, 10, 11, 11, 12, 12, 13, 13
    };

    static const uint16_t distBase[30] = {
        1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
        33,   49,   65,   97,  129,  193,  257,   385,   513,   769,
        1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
    };

    for (;;) {
        uint16_t sym = inflateDecodeSymbol(d, lt);

        // Check for overflow in bit reader
        if (d->overflow)
            return false;

        if (sym < 256) {
            if (d->dst == d->dstEnd)
                return false;
            *d->dst++ = (uint8_t)sym;
        } else {
            // Check for end of block
            if (sym == 256)
                return true;

            // Check sym is within range and distance tree is not empty
            if (sym > lt->maxSym || sym - 257 > 28 || dt->maxSym == -1)
                return false;

            sym = (uint16_t)(sym - 257);

            // Possibly get more bits from length code
            const uint32_t length = inflateGetBitsBase(d, lengthBits[sym], lengthBase[sym]);

            const uint16_t dist = inflateDecodeSymbol(d, dt);

            // Check dist is within range
            if (dist > dt->maxSym || dist > 29)
                return false;

            // Possibly get more bits from distance code
            const uint32_t offs = inflateGetBitsBase(d, distBits[dist], distBase[dist]);

            if (offs > (size_t)(d->dst - d->dstStart))
                return false;

            if ((size_t)(d->dstEnd - d->dst) < length)
                return false;

            // Copy match
            for (uint32_t i = 0; i < length; ++i)
                d->dst[i] = i[d->dst - offs];

            d->dst += length;
        }
    }
}

// Inflate an uncompressed block of data
static bool inflateStoredBlock(InflateData *d) {
    if (d->srcEnd - d->src < 4)
        return false;

    // Get length
    const uint32_t length = inflateReadLE16(d->src);

    // Get one's complement of length
    const uint32_t invlength = inflateReadLE16(d->src + 2);

    // Check length
    if (length != (~invlength & 0x0000FFFF))
        return false;

    d->src += 4;

    if ((size_t)(d->srcEnd - d->src) < length ||
        (size_t)(d->dstEnd - d->dst) < length)
        return false;

    memcpy(d->dst, d->src, length);
    d->dst += length;
    d->src += length;

    // Make sure we start next block on a byte boundary
    d->tag = 0;
    d->bitcount = 0;

    return true;
}

// Inflate a block of data compressed with fixed Huffman trees
static bool inflateFixedBlock(InflateData *d) {
    inflateBuildFixedTrees(&d->ltree, &d->dtree);
    return inflateBlockData(d, &d->ltree, &d->dtree);
}

// Inflate a block of data compressed with dynamic Huffman trees
static bool inflateDynamicBlock(InflateData *d) {
    return inflateDecodeTrees(d, &d->ltree, &d->dtree) && inflateBlockData(d, &d->ltree, &d->dtree);
}

// Inflate stream from src to dst
CHI_WARN_OFF(frame-larger-than=)
bool inflate(void *dst, size_t dstLen, const void *src, size_t srcLen) {
    InflateData d = {
        .src = (const uint8_t*)src,
        .srcEnd = (const uint8_t*)src + srcLen,
        .dst = (uint8_t*)dst,
        .dstStart = (uint8_t*)dst,
        .dstEnd = (uint8_t*)dst + dstLen,
    };

    uint32_t bfinal;
    do {
        // Read final block flag
        bfinal = inflateGetBits(&d, 1);

        // Read block type (2 bits)
        const uint32_t btype = inflateGetBits(&d, 2);

        bool res = false;
        switch (btype) {
        case 0:  res = inflateStoredBlock(&d); break;
        case 1:  res = inflateFixedBlock(&d); break;
        case 2:  res = inflateDynamicBlock(&d); break;
        }
        if (!res)
            return false;
    } while (!bfinal);

    if (d.overflow)
        return false;

    return dstLen == (size_t)(d.dst - d.dstStart);
}
CHI_WARN_ON
