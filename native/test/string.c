#include <chili/object/string.h>
#include "../strutil.h"
#include "test.h"

static bool isSmall(Chili c) {
    return CHI_STRING_SMALLOPT ? _chiUnboxed(c) : _chiType(c) == CHI_STRING;
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

TEST(chiTryPinString) {
    Chili a = CHI_STATIC_STRING("abc");
    ASSERT(isSmall(a));
    Chili b = chiTryPinString(a);
    ASSERT(chiPinned(b));
    ASSERT(_chiType(b) == CHI_STRING);
    ASSERTEQ(chiStringSize(b), 3);
    ASSERT(memeqstr(chiStringBytes(&b), chiStringSize(b), "abc"));
    ASSERT(!chiStringBytes(&b)[3]);
}
