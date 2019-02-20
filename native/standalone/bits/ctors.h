#pragma once

void __call_ctors(void);
int __cxa_atexit(void(*)(void*), void*, void*);
void __cxa_finalize(void*);
