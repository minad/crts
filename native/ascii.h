#pragma once

#include "private.h"

/*
 * Do not use ctype.h functions which are locale dependent.
 * These functions should be considered legacy since they
 * only function with legacy single byte encodings.
 * In the Chili runtime, we only operate on the ASCII subset of UTF-8.
 */

CHI_INL bool chiLower(int c)  { return c >= 'a' && c <= 'z'; }
CHI_INL bool chiUpper(int c)  { return c >= 'A' && c <= 'Z'; }
CHI_INL bool chiAlpha(int c)  { return chiLower(c) || chiUpper(c); }
CHI_INL bool chiDigit(int c)  { return c >= '0' && c <= '9'; }
CHI_INL bool chiAlnum(int c)  { return chiAlpha(c) || chiDigit(c); }
CHI_INL int chiToUpper(int c) { return chiLower(c) ? c - 'a' + 'A' : c; }
CHI_INL int chiToHexDigit(int c) { return (c & 0xF) + 9 * (c >> 6); }
