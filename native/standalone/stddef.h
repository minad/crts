#pragma once

/*
 * Disable keywords which are only need in the presence of threading.
 *
 * NOTE: The volatile macro is dangerous, which
 * disables volatile racing access to variables.
 * Racing is only possible if the system supports threads.
 * Use __volatile__ if you need it under any circumstances,
 * e.g., for __asm__.
 */
#define _Thread_local
#define _Atomic(x) x
#define volatile

#define NULL 0
#define offsetof(s, m) __builtin_offsetof(s, m)

typedef __SIZE_TYPE__ size_t;
