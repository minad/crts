#pragma once

// Disable keywords which are only needed in the presence of threading.
#define _Thread_local
#define _Atomic(x) x

#define NULL ((void*)0)
#define offsetof(s, m) __builtin_offsetof(s, m)

typedef __SIZE_TYPE__ size_t;
