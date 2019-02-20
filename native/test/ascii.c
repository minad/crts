#include "../ascii.h"
#include "test.h"

TEST(chiToHexDigit) {
    char test[] = "0123456789abcdef";
    for (int i = 0; i < (int)CHI_DIM(test) - 1; ++i) {
        ASSERTEQ(i, chiToHexDigit(test[i]));
        ASSERTEQ(i, chiToHexDigit(chiToUpper(test[i])));
    }
}
