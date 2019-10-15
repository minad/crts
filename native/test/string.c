#include "../strutil.h"
#include "test.h"

static bool isSmall(Chili c) {
    return CHI_STRING_UNBOXING ? chiUnboxed(c) : chiType(c) == CHI_STRING;
}

TEST(chiCharToString) {
    ASSERTEQ(chiStringSize(chiCharToString(chiChr('a'))), 1);
    Chili a = chiCharToString(chiChr('a'));
    ASSERT(isSmall(a));
    ASSERTEQ(chiStringSize(a), 1);
    ASSERTEQ(chiStringBytes(&a)[0], 'a');
    Chili b = chiCharToString(chiChr(9999));
    ASSERT(isSmall(b));
    ASSERTEQ(chiStringSize(b), 3);
    ASSERT(memeqstr(chiStringBytes(&b), chiStringSize(b), "\xe2\x9c\x8f"));
}

TEST(chiStringPin_small1) {
    Chili a = chiStringFromRef(chiStringRef("abc"));
    ASSERT(isSmall(a));
    Chili b = chiStringPin(a);
    ASSERT(chiGen(b) == CHI_GEN_MAJOR);
    ASSERT(chiType(b) == CHI_STRING);
    ASSERTEQ(chiStringSize(b), 3);
    ASSERT(memeqstr(chiStringBytes(&b), chiStringSize(b), "abc"));
    ASSERT(!chiStringBytes(&b)[3]);
}

TEST(chiStringPin_small2) {
    Chili a = chiStringFromRef(chiStringRef("1234567"));
    ASSERT(isSmall(a));
    Chili b = chiStringPin(a);
    ASSERT(chiGen(b) == CHI_GEN_MAJOR);
    ASSERT(chiType(b) == CHI_STRING);
    ASSERTEQ(chiStringSize(b), 7);
    ASSERT(memeqstr(chiStringBytes(&b), chiStringSize(b), "1234567"));
    ASSERT(!chiStringBytes(&b)[7]);
}

TEST(chiStringPin_large) {
    Chili a = chiStringFromRef(chiStringRef("0123456789"));
    ASSERT(!isSmall(a));
    Chili b = chiStringPin(a);
    ASSERT(chiGen(b) == CHI_GEN_MAJOR);
    ASSERT(chiType(b) == CHI_STRING);
    ASSERTEQ(chiStringSize(b), 10);
    ASSERT(memeqstr(chiStringBytes(&b), chiStringSize(b), "0123456789"));
    ASSERT(!chiStringBytes(&b)[10]);
}
