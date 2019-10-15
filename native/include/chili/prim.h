#pragma once

#include <chili.h>
#include <math.h>

CHI_INL Chili     chi_Prim_charToString(ChiChar x)                { return chiCharToString(x); }
CHI_INL bool      chi_Prim_charEq(ChiChar x, ChiChar y)           { return chiOrd(x) == chiOrd(y); }
CHI_INL bool      chi_Prim_charLe(ChiChar x, ChiChar y)           { return chiOrd(x) <= chiOrd(y); }
CHI_INL bool      chi_Prim_charLt(ChiChar x, ChiChar y)           { return chiOrd(x) < chiOrd(y); }
CHI_INL bool      chi_Prim_charNe(ChiChar x, ChiChar y)           { return chiOrd(x) != chiOrd(y); }
CHI_INL uint32_t  chi_Prim_charToUInt32(ChiChar x)                { return chiOrd(x); }
CHI_INL Chili     chi_Prim_float32ToInt(float x)                  { return chiFloat64ToBigInt((double)x); }
CHI_INL bool      chi_Prim_float32Eq(float x, float y)            { return x == y; }
CHI_INL bool      chi_Prim_float32Le(float x, float y)            { return x <= y; }
CHI_INL bool      chi_Prim_float32Lt(float x, float y)            { return x < y; }
CHI_INL bool      chi_Prim_float32Ne(float x, float y)            { return x != y; }
CHI_INL double    chi_Prim_float32ToFloat64(float x)              { return (double)x; }
CHI_INL float     chi_Prim_float32Abs(float x)                    { return fabsf(x); }
CHI_INL float     chi_Prim_float32Acos(float x)                   { return acosf(x); }
CHI_INL float     chi_Prim_float32Add(float x, float y)           { return x + y; }
CHI_INL float     chi_Prim_float32Asin(float x)                   { return asinf(x); }
CHI_INL float     chi_Prim_float32Atan(float x)                   { return atanf(x); }
CHI_INL float     chi_Prim_float32Ceil(float x)                   { return ceilf(x); }
CHI_INL float     chi_Prim_float32Cos(float x)                    { return cosf(x); }
CHI_INL float     chi_Prim_float32Cosh(float x)                   { return coshf(x); }
CHI_INL float     chi_Prim_float32Div(float x, float y)           { return x / y; }
CHI_INL float     chi_Prim_float32Exp(float x)                    { return expf(x); }
CHI_INL float     chi_Prim_float32Expm1(float x)                  { return expm1f(x); }
CHI_INL float     chi_Prim_float32Floor(float x)                  { return floorf(x); }
CHI_INL float     chi_Prim_float32Log(float x)                    { return logf(x); }
CHI_INL float     chi_Prim_float32Log1p(float x)                  { return log1pf(x); }
CHI_INL float     chi_Prim_float32Max(float x, float y)           { return x != x || y != y ? 0.0f/0.0f : (x > y ? x : y); }
CHI_INL float     chi_Prim_float32Min(float x, float y)           { return x != x || y != y ? 0.0f/0.0f : (x < y ? x : y); }
CHI_INL float     chi_Prim_float32Mul(float x, float y)           { return x * y; }
CHI_INL float     chi_Prim_float32Neg(float x)                    { return -x; }
CHI_INL float     chi_Prim_float32Pow(float x, float y)           { return powf(x, y); }
CHI_INL float     chi_Prim_float32Round(float x)                  { return roundf(x); }
CHI_INL float     chi_Prim_float32Sin(float x)                    { return sinf(x); }
CHI_INL float     chi_Prim_float32Sinh(float x)                   { return sinhf(x); }
CHI_INL float     chi_Prim_float32Sqrt(float x)                   { return sqrtf(x); }
CHI_INL float     chi_Prim_float32Sub(float x, float y)           { return x - y; }
CHI_INL float     chi_Prim_float32Tan(float x)                    { return tanf(x); }
CHI_INL float     chi_Prim_float32Tanh(float x)                   { return tanhf(x); }
CHI_INL int32_t   chi_Prim_float32ToInt32(float x)                { return (int32_t)_chiFloat64ToInt64((double)x); }
CHI_INL float     chi_Prim_float32Trunc(float x)                  { return truncf(x); }
CHI_INL int64_t   chi_Prim_float32ToInt64(float x)                { return _chiFloat64ToInt64((double)x); }
CHI_INL uint32_t  chi_Prim_float32ToUInt32(float x)               { return (uint32_t)(uint64_t)_chiFloat64ToInt64((double)x); }
CHI_INL uint64_t  chi_Prim_float32ToUInt64(float x)               { return (uint64_t)_chiFloat64ToInt64((double)x); }
CHI_INL Chili     chi_Prim_float64ToInt(double x)                 { return chiFloat64ToBigInt(x); }
CHI_INL bool      chi_Prim_float64Eq(double x, double y)          { return x == y; }
CHI_INL bool      chi_Prim_float64Le(double x, double y)          { return x <= y; }
CHI_INL bool      chi_Prim_float64Ne(double x, double y)          { return x != y; }
CHI_INL bool      chi_Prim_float64Lt(double x, double y)          { return x < y; }
CHI_INL double    chi_Prim_float64Abs(double x)                   { return fabs(x); }
CHI_INL double    chi_Prim_float64Acos(double x)                  { return acos(x); }
CHI_INL double    chi_Prim_float64Add(double x, double y)         { return x + y; }
CHI_INL double    chi_Prim_float64Asin(double x)                  { return asin(x); }
CHI_INL double    chi_Prim_float64Atan(double x)                  { return atan(x); }
CHI_INL double    chi_Prim_float64Ceil(double x)                  { return ceil(x); }
CHI_INL double    chi_Prim_float64Cos(double x)                   { return cos(x); }
CHI_INL double    chi_Prim_float64Cosh(double x)                  { return cosh(x); }
CHI_INL double    chi_Prim_float64Div(double x, double y)         { return x / y; }
CHI_INL double    chi_Prim_float64Exp(double x)                   { return exp(x); }
CHI_INL double    chi_Prim_float64Expm1(double x)                 { return expm1(x); }
CHI_INL double    chi_Prim_float64Floor(double x)                 { return floor(x); }
CHI_INL double    chi_Prim_float64Log(double x)                   { return log(x); }
CHI_INL double    chi_Prim_float64Log1p(double x)                 { return log1p(x); }
CHI_INL double    chi_Prim_float64Max(double x, double y)         { return x != x || y != y ? 0.0/0.0 : (x > y ? x : y); }
CHI_INL double    chi_Prim_float64Min(double x, double y)         { return x != x || y != y ? 0.0/0.0 : (x < y ? x : y); }
CHI_INL double    chi_Prim_float64Mul(double x, double y)         { return x * y; }
CHI_INL double    chi_Prim_float64Neg(double x)                   { return -x; }
CHI_INL double    chi_Prim_float64Pow(double x, double y)         { return pow(x, y); }
CHI_INL double    chi_Prim_float64Round(double x)                 { return round(x); }
CHI_INL double    chi_Prim_float64Sin(double x)                   { return sin(x); }
CHI_INL double    chi_Prim_float64Sinh(double x)                  { return sinh(x); }
CHI_INL double    chi_Prim_float64Sqrt(double x)                  { return sqrt(x); }
CHI_INL double    chi_Prim_float64Sub(double x, double y)         { return x - y; }
CHI_INL double    chi_Prim_float64Tan(double x)                   { return tan(x); }
CHI_INL double    chi_Prim_float64Tanh(double x)                  { return tanh(x); }
CHI_INL double    chi_Prim_float64Trunc(double x)                 { return trunc(x); }
CHI_INL float     chi_Prim_float64ToFloat32(double x)             { return (float)x; }
CHI_INL int32_t   chi_Prim_float64ToInt32(double x)               { return (int32_t)_chiFloat64ToInt64(x); }
CHI_INL int64_t   chi_Prim_float64ToInt64(double x)               { return _chiFloat64ToInt64(x); }
CHI_INL uint32_t  chi_Prim_float64ToUInt32(double x)              { return (uint32_t)(uint64_t)_chiFloat64ToInt64(x); }
CHI_INL uint64_t  chi_Prim_float64ToUInt64(double x)              { return (uint64_t)_chiFloat64ToInt64(x); }
CHI_INL int32_t   chi_Prim_int8ToInt32(int8_t x)                  { return x; }
CHI_INL int32_t   chi_Prim_int16ToInt32(int16_t x)                { return x; }
CHI_INL Chili     chi_Prim_int32ToInt(int32_t x)                  { return chiInt64ToBigInt(x); }
CHI_INL bool      chi_Prim_int32Eq(int32_t x, int32_t y)          { return x == y; }
CHI_INL bool      chi_Prim_int32Le(int32_t x, int32_t y)          { return x <= y; }
CHI_INL bool      chi_Prim_int32Lt(int32_t x, int32_t y)          { return x < y; }
CHI_INL bool      chi_Prim_int32Ne(int32_t x, int32_t y)          { return x != y; }
CHI_INL double    chi_Prim_int32ToFloat64(int32_t x)              { return (double)x; }
CHI_INL float     chi_Prim_int32ToFloat32(int32_t x)              { return (float)x; }
CHI_INL int16_t   chi_Prim_int32ToInt16(int32_t x)                { return (int16_t)x; }
CHI_INL int32_t   chi_Prim_int32Add(int32_t x, int32_t y)         { return x + y; }
CHI_INL int32_t   chi_Prim_int32And(int32_t x, int32_t y)         { return x & y; }
CHI_INL int32_t   chi_Prim_int32Div(int32_t x, int32_t y)         { return CHI_DIV(x, y); }
CHI_INL int32_t   chi_Prim_int32Mod(int32_t x, int32_t y)         { return CHI_MOD(x, y); }
CHI_INL int32_t   chi_Prim_int32Mul(int32_t x, int32_t y)         { return x * y; }
CHI_INL int32_t   chi_Prim_int32Neg(int32_t x)                    { return -x; }
CHI_INL int32_t   chi_Prim_int32Not(int32_t x)                    { return ~x; }
CHI_INL int32_t   chi_Prim_int32Or(int32_t x, int32_t y)          { return x | y; }
CHI_INL int32_t   chi_Prim_int32Quo(int32_t x, int32_t y)        { return x / y; }
CHI_INL int32_t   chi_Prim_int32Rem(int32_t x, int32_t y)         { return CHI_REM(x, y); }
CHI_INL int32_t   chi_Prim_int32Shl(int32_t x, uint8_t y)         { return x << y; }
CHI_INL int32_t   chi_Prim_int32Shr(int32_t x, uint8_t y)         { return x >> y; }
CHI_INL int32_t   chi_Prim_int32Sub(int32_t x, int32_t y)         { return x - y; }
CHI_INL int32_t   chi_Prim_int32Xor(int32_t x, int32_t y)         { return x ^ y; }
CHI_INL int64_t   chi_Prim_int32ToInt64(int32_t x)                { return x; }
CHI_INL int8_t    chi_Prim_int32ToInt8(int32_t x)                 { return (int8_t)x; }
CHI_INL uint32_t  chi_Prim_int32ToUInt32(int32_t x)               { return (uint32_t)x; }
CHI_INL uint64_t  chi_Prim_int32ToUInt64(int32_t x)               { return (uint64_t)x; }
CHI_INL Chili     chi_Prim_int64ToInt(int64_t x)                  { return chiInt64ToBigInt(x); }
CHI_INL bool      chi_Prim_int64Eq(int64_t x, int64_t y)          { return x == y; }
CHI_INL bool      chi_Prim_int64Le(int64_t x, int64_t y)          { return x <= y; }
CHI_INL bool      chi_Prim_int64Lt(int64_t x, int64_t y)          { return x < y; }
CHI_INL bool      chi_Prim_int64Ne(int64_t x, int64_t y)          { return x != y; }
CHI_INL double    chi_Prim_int64ToFloat64(int64_t x)              { return (double)x; }
CHI_INL float     chi_Prim_int64ToFloat32(int64_t x)              { return (float)x; }
CHI_INL int32_t   chi_Prim_int64ToInt32(int64_t x)                { return (int32_t)x; }
CHI_INL int64_t   chi_Prim_int64Add(int64_t x, int64_t y)         { return x + y; }
CHI_INL int64_t   chi_Prim_int64And(int64_t x, int64_t y)         { return x & y; }
CHI_INL int64_t   chi_Prim_int64Div(int64_t x, int64_t y)         { return CHI_DIV(x, y); }
CHI_INL int64_t   chi_Prim_int64Mod(int64_t x, int64_t y)         { return CHI_MOD(x, y); }
CHI_INL int64_t   chi_Prim_int64Mul(int64_t x, int64_t y)         { return x * y; }
CHI_INL int64_t   chi_Prim_int64Neg(int64_t x)                    { return -x; }
CHI_INL int64_t   chi_Prim_int64Not(int64_t x)                    { return ~x; }
CHI_INL int64_t   chi_Prim_int64Or(int64_t x, int64_t y)          { return x | y; }
CHI_INL int64_t   chi_Prim_int64Quo(int64_t x, int64_t y)        { return x / y; }
CHI_INL int64_t   chi_Prim_int64Rem(int64_t x, int64_t y)         { return CHI_REM(x, y); }
CHI_INL int64_t   chi_Prim_int64Shl(int64_t x, uint8_t y)         { return x << y; }
CHI_INL int64_t   chi_Prim_int64Shr(int64_t x, uint8_t y)         { return x >> y; }
CHI_INL int64_t   chi_Prim_int64Sub(int64_t x, int64_t y)         { return x - y; }
CHI_INL int64_t   chi_Prim_int64Xor(int64_t x, int64_t y)         { return x ^ y; }
CHI_INL uint32_t  chi_Prim_int64ToUInt32(int64_t x)               { return (uint32_t)x; }
CHI_INL uint64_t  chi_Prim_int64ToUInt64(int64_t x)               { return (uint64_t)x; }
CHI_INL int32_t   chi_Prim_intCmp(Chili x, Chili y)               { return chiBigIntCmp(x, y); }
CHI_INL bool      chi_Prim_intEq(Chili x, Chili y)                { return chiBigIntEq(x, y); }
CHI_INL bool      chi_Prim_intLe(Chili x, Chili y)                { return chiBigIntLe(x, y); }
CHI_INL bool      chi_Prim_intLt(Chili x, Chili y)                { return chiBigIntLt(x, y); }
CHI_INL bool      chi_Prim_intNe(Chili x, Chili y)                { return chiBigIntNe(x, y); }
CHI_INL double    chi_Prim_intToFloat64(Chili x)                  { return chiBigIntToFloat64(x); }
CHI_INL float     chi_Prim_intToFloat32(Chili x)                  { return (float)chiBigIntToFloat64(x); }
CHI_INL int32_t   chi_Prim_intToInt32(Chili x)                    { return (int32_t)chiBigIntToUInt64(x); }
CHI_INL int64_t   chi_Prim_intToInt64(Chili x)                    { return (int64_t)chiBigIntToUInt64(x); }
CHI_INL uint32_t  chi_Prim_intToUInt32(Chili x)                   { return (uint32_t)chiBigIntToUInt64(x); }
CHI_INL uint64_t  chi_Prim_intToUInt64(Chili x)                   { return chiBigIntToUInt64(x); }
CHI_INL int32_t   chi_Prim_stringCmp(Chili x, Chili y)            { return chiStringCmp(x, y); }
CHI_INL bool      chi_Prim_stringEq(Chili x, Chili y)             { return chiStringEq(x, y); }
CHI_INL bool      chi_Prim_stringLe(Chili x, Chili y)             { return chiStringCmp(x, y) <= 0; }
CHI_INL bool      chi_Prim_stringLt(Chili x, Chili y)             { return chiStringCmp(x, y) < 0; }
CHI_INL bool      chi_Prim_stringNe(Chili x, Chili y)             { return !chiStringEq(x, y); }
CHI_INL bool      chi_Prim_stringNull(Chili x)                    { return !chiTrue(x); }
CHI_INL uint32_t  chi_Prim_uint16ToUInt32(uint16_t x)             { return x; }
CHI_INL uint8_t   chi_Prim_uint32ToUInt8(uint32_t x)              { return (uint8_t)x; }
CHI_INL uint32_t  chi_Prim_uint8ToUInt32(uint8_t x)               { return x; }
CHI_INL uint16_t  chi_Prim_uint32ToUInt16(uint32_t x)             { return (uint16_t)x; }
CHI_INL Chili     chi_Prim_uint32ToInt(uint32_t x)                { return chiUInt64ToBigInt(x); }
CHI_INL ChiChar   chi_Prim_uint32ToChar(uint32_t x)               { return chiChr(x); }
CHI_INL bool      chi_Prim_uint32Eq(uint32_t x, uint32_t y)       { return x == y; }
CHI_INL bool      chi_Prim_uint32Le(uint32_t x, uint32_t y)       { return x <= y; }
CHI_INL bool      chi_Prim_uint32Lt(uint32_t x, uint32_t y)       { return x < y; }
CHI_INL bool      chi_Prim_uint32Ne(uint32_t x, uint32_t y)       { return x != y; }
CHI_INL double    chi_Prim_uint32ToFloat64(uint32_t x)            { return (double)x; }
CHI_INL float     chi_Prim_uint32ToFloat32(uint32_t x)            { return (float)x; }
CHI_INL int32_t   chi_Prim_uint32ToInt32(uint32_t x)              { return (int32_t)x; }
CHI_INL int64_t   chi_Prim_uint32ToInt64(uint32_t x)              { return x; }
CHI_INL uint32_t  chi_Prim_uint32Add(uint32_t x, uint32_t y)      { return x + y; }
CHI_INL uint32_t  chi_Prim_uint32And(uint32_t x, uint32_t y)      { return x & y; }
CHI_INL uint32_t  chi_Prim_uint32Div(uint32_t x, uint32_t y)      { return x / y; }
CHI_INL uint32_t  chi_Prim_uint32Mod(uint32_t x, uint32_t y)      { return x % y; }
CHI_INL uint32_t  chi_Prim_uint32Mul(uint32_t x, uint32_t y)      { return x * y; }
CHI_INL uint32_t  chi_Prim_uint32Neg(uint32_t x)                  { return -x; }
CHI_INL uint32_t  chi_Prim_uint32Not(uint32_t x)                  { return ~x; }
CHI_INL uint32_t  chi_Prim_uint32Or(uint32_t x, uint32_t y)       { return x | y; }
CHI_INL uint32_t  chi_Prim_uint32Shl(uint32_t x, uint8_t y)       { return x << y; }
CHI_INL uint32_t  chi_Prim_uint32Shr(uint32_t x, uint8_t y)       { return x >> y; }
CHI_INL uint32_t  chi_Prim_uint32Sub(uint32_t x, uint32_t y)      { return x - y; }
CHI_INL uint32_t  chi_Prim_uint32Xor(uint32_t x, uint32_t y)      { return x ^ y; }
CHI_INL uint64_t  chi_Prim_uint32ToUInt64(uint32_t x)             { return x; }
CHI_INL Chili     chi_Prim_uint64ToInt(uint64_t x)                { return chiUInt64ToBigInt(x); }
CHI_INL bool      chi_Prim_uint64Eq(uint64_t x, uint64_t y)       { return x == y; }
CHI_INL bool      chi_Prim_uint64Le(uint64_t x, uint64_t y)       { return x <= y; }
CHI_INL bool      chi_Prim_uint64Lt(uint64_t x, uint64_t y)       { return x < y;  }
CHI_INL bool      chi_Prim_uint64Ne(uint64_t x, uint64_t y)       { return x != y; }
CHI_INL double    chi_Prim_uint64ToFloat64(uint64_t x)            { return (double)x; }
CHI_INL float     chi_Prim_uint64ToFloat32(uint64_t x)            { return (float)x; }
CHI_INL int32_t   chi_Prim_uint64ToInt32(uint64_t x)              { return (int32_t)x; }
CHI_INL int64_t   chi_Prim_uint64ToInt64(uint64_t x)              { return (int64_t)x; }
CHI_INL uint32_t  chi_Prim_uint64ToUInt32(uint64_t x)             { return (uint32_t)x; }
CHI_INL uint64_t  chi_Prim_uint64Add(uint64_t x, uint64_t y)      { return x + y; }
CHI_INL uint64_t  chi_Prim_uint64And(uint64_t x, uint64_t y)      { return x & y; }
CHI_INL uint64_t  chi_Prim_uint64Div(uint64_t x, uint64_t y)      { return x / y; }
CHI_INL uint64_t  chi_Prim_uint64Mod(uint64_t x, uint64_t y)      { return x % y; }
CHI_INL uint64_t  chi_Prim_uint64Mul(uint64_t x, uint64_t y)      { return x * y; }
CHI_INL uint64_t  chi_Prim_uint64Neg(uint64_t x)                  { return -x; }
CHI_INL uint64_t  chi_Prim_uint64Not(uint64_t x)                  { return ~x; }
CHI_INL uint64_t  chi_Prim_uint64Or(uint64_t x, uint64_t y)       { return x | y; }
CHI_INL uint64_t  chi_Prim_uint64Shl(uint64_t x, uint8_t y)       { return x << y; }
CHI_INL uint64_t  chi_Prim_uint64Shr(uint64_t x, uint8_t y)       { return x >> y; }
CHI_INL uint64_t  chi_Prim_uint64Sub(uint64_t x, uint64_t y)      { return x - y; }
CHI_INL uint64_t  chi_Prim_uint64Xor(uint64_t x, uint64_t y)      { return x ^ y; }
CHI_INL float     chi_Prim_bitsToFloat32(uint32_t x)              { return _chiBitsToFloat32(x); }
CHI_INL double    chi_Prim_bitsToFloat64(uint64_t x)              { return _chiBitsToFloat64(x); }
CHI_INL uint32_t  chi_Prim_float32ToBits(float x)                 { return _chiFloat32ToBits(x); }
CHI_INL uint64_t  chi_Prim_float64ToBits(double x)                { return _chiFloat64ToBits(x); }

CHI_INL bool      chi_Prim_arrayCas(Chili c, uint32_t i, Chili o, Chili x)                    { return chiArrayCas(c, i, o, x); }
CHI_INL void      chi_Prim_arrayCopy(Chili s, uint32_t si, Chili d, uint32_t di, uint32_t n)  { chiArrayCopy(s, si, d, di, n); }
CHI_INL Chili     chi_Prim_arrayRead(Chili c, uint32_t i)                                     { return chiArrayRead(c, i); }
CHI_INL uint32_t  chi_Prim_arraySize(Chili c)                                                 { return (uint32_t)_chiSize(c); }
CHI_INL void      chi_Prim_arrayWrite(Chili c, uint32_t i, Chili x)                           { chiArrayWrite(c, i, x); }
CHI_INL int32_t   chi_Prim_bufferCmp(Chili a, uint32_t ai, Chili b, uint32_t bi, uint32_t n)  { return chiBufferCmp(a, ai, b, bi, n); }
CHI_INL void      chi_Prim_bufferCopy(Chili s, uint32_t si, Chili d, uint32_t di, uint32_t n) { chiBufferCopy(s, si, d, di, n); }
CHI_INL void      chi_Prim_bufferFill(Chili c, uint8_t b, uint32_t i, uint32_t n)             { return chiBufferFill(c, b, i, n); }
CHI_INL float     chi_Prim_bufferReadFloat32(Chili c, uint32_t i)                             { return chiBufferReadFloat32(c, i); }
CHI_INL double    chi_Prim_bufferReadFloat64(Chili c, uint32_t i)                             { return chiBufferReadFloat64(c, i); }
CHI_INL uint16_t  chi_Prim_bufferReadUInt16(Chili c, uint32_t i)                              { return chiBufferReadUInt16(c, i); }
CHI_INL uint32_t  chi_Prim_bufferReadUInt32(Chili c, uint32_t i)                              { return chiBufferReadUInt32(c, i); }
CHI_INL uint64_t  chi_Prim_bufferReadUInt64(Chili c, uint32_t i)                              { return chiBufferReadUInt64(c, i); }
CHI_INL uint8_t   chi_Prim_bufferReadUInt8(Chili c, uint32_t i)                               { return chiBufferReadUInt8(c, i); }
CHI_INL uint32_t  chi_Prim_bufferSize(Chili c)                                                { return chiBufferSize(c); }
CHI_INL void      chi_Prim_bufferWriteFloat32(Chili c, uint32_t i, float x)                   { chiBufferWriteFloat32(c, i, x); }
CHI_INL void      chi_Prim_bufferWriteFloat64(Chili c, uint32_t i, double x)                  { chiBufferWriteFloat64(c, i, x); }
CHI_INL void      chi_Prim_bufferWriteUInt16(Chili c, uint32_t i, uint16_t x)                 { chiBufferWriteUInt16(c, i, x); }
CHI_INL void      chi_Prim_bufferWriteUInt32(Chili c, uint32_t i, uint32_t x)                 { chiBufferWriteUInt32(c, i, x); }
CHI_INL void      chi_Prim_bufferWriteUInt64(Chili c, uint32_t i, uint64_t x)                 { chiBufferWriteUInt64(c, i, x); }
CHI_INL void      chi_Prim_bufferWriteUInt8(Chili c, uint32_t i, uint8_t x)                   { chiBufferWriteUInt8(c, i, x); }
CHI_INL uint32_t  chi_Prim_stringCursorBegin(Chili CHI_UNUSED(c))                             { return 0; }
CHI_INL bool      chi_Prim_stringCursorEq(uint32_t i, uint32_t j)                             { return i == j; }
CHI_INL bool      chi_Prim_stringCursorNe(uint32_t i, uint32_t j)                             { return i != j; }
CHI_INL bool      chi_Prim_stringCursorLt(uint32_t i, uint32_t j)                             { return i < j; }
CHI_INL bool      chi_Prim_stringCursorLe(uint32_t i, uint32_t j)                             { return i <= j; }
CHI_INL uint32_t  chi_Prim_stringCursorEnd(Chili c)                                           { return chiStringSize(c); }
CHI_INL ChiChar   chi_Prim_stringCursorGet(Chili c, uint32_t i)                               { return chiStringGet(c, i); }
CHI_INL uint32_t  chi_Prim_stringCursorNext(Chili c, uint32_t i)                              { return chiStringNext(c, i); }
CHI_INL uint32_t  chi_Prim_stringCursorPrev(Chili c, uint32_t i)                              { return chiStringPrev(c, i); }
CHI_INL Chili     chi_Prim_stringBuilderBuild(Chili b)                                        { return chiStringBuilderBuild(b); }
CHI_INL bool      chi_Prim_identical(Chili a, Chili b)                                        { return chiIdentical(a, b); }
CHI_INL uint32_t  chi_Prim_tag(Chili c)                                                       { return chiTag(c); }

#define _CHI_PRIM_TRY2(r, p) ({ Chili r = (p); if (!chiSuccess(r)) KNOWN_JUMP(chiHeapOverflow); r; })
#define _CHI_PRIM_TRY(p)     _CHI_PRIM_TRY2(CHI_GENSYM, (p))

#define _CHI_PRIM_STRINGBUILDER_CHAR(x, b, ch, sb) ({ Chili b = (sb); ChiChar x = (ch); if (CHI_UNLIKELY(!chiStringBuilderEnsure(b, 4))) KNOWN_JUMP(chiHeapOverflow); chiStringBuilderChar(b, x); b; })
#define _CHI_PRIM_STRINGBUILDER_STRING(x, b, str, sb) ({ Chili b = (sb); ChiStringRef x = (str); if (CHI_UNLIKELY(!chiStringBuilderEnsure(b, x.size))) KNOWN_JUMP(chiHeapOverflow); chiStringBuilderString(b, x); b; })

#define chi_Prim_lazyForce                 FORCE
#define chi_Prim_arrayNew(s, x)            _CHI_PRIM_TRY(chiArrayNewFlags((s), (x), CHI_NEW_TRY))
#define chi_Prim_bufferNew(s)              _CHI_PRIM_TRY(chiBufferNewFlags((s), 0, CHI_NEW_TRY))
#define chi_Prim_arrayClone(c, i, s)       _CHI_PRIM_TRY(chiArrayTryClone((c), (i), (s)))
#define chi_Prim_bufferClone(c, i, s)      _CHI_PRIM_TRY(chiBufferTryClone((c), (i), (s)))
#define chi_Prim_stringSlice(c, i, j)      _CHI_PRIM_TRY(chiStringTrySlice((c), (i), (j)))
#define chi_Prim_stringBuilderNew(n)       _CHI_PRIM_TRY(chiStringBuilderTryNew(n))
#define chi_Prim_stringBuilderChar(c, b)   _CHI_PRIM_STRINGBUILDER_CHAR(CHI_GENSYM, CHI_GENSYM, (c), (b))
#define chi_Prim_stringBuilderString(s, b) _CHI_PRIM_STRINGBUILDER_STRING(CHI_GENSYM, CHI_GENSYM, (s), (b))
#define chi_Prim_intAdd(x, y)              _CHI_PRIM_TRY(chiBigIntTryAdd((x), (y)))
#define chi_Prim_intAnd(x, y)              _CHI_PRIM_TRY(chiBigIntTryAnd((x), (y)))
#define chi_Prim_intDiv(x, y)              _CHI_PRIM_TRY(chiBigIntTryDiv((x), (y)))
#define chi_Prim_intMod(x, y)              _CHI_PRIM_TRY(chiBigIntTryMod((x), (y)))
#define chi_Prim_intMul(x, y)              _CHI_PRIM_TRY(chiBigIntTryMul((x), (y)))
#define chi_Prim_intQuo(x, y)              _CHI_PRIM_TRY(chiBigIntTryQuo((x), (y)))
#define chi_Prim_intRem(x, y)              _CHI_PRIM_TRY(chiBigIntTryRem((x), (y)))
#define chi_Prim_intShl(x, y)              _CHI_PRIM_TRY(chiBigIntTryShl((x), (y)))
#define chi_Prim_intShr(x, y)              _CHI_PRIM_TRY(chiBigIntTryShr((x), (y)))
#define chi_Prim_intSub(x, y)              _CHI_PRIM_TRY(chiBigIntTrySub((x), (y)))
#define chi_Prim_intOr(x, y)               _CHI_PRIM_TRY(chiBigIntTryOr((x), (y)))
#define chi_Prim_intXor(x, y)              _CHI_PRIM_TRY(chiBigIntTryXor((x), (y)))
#define chi_Prim_intNeg(x)                 _CHI_PRIM_TRY(chiBigIntTryNeg(x))
#define chi_Prim_intNot(x)                 _CHI_PRIM_TRY(chiBigIntTryNot(x))
