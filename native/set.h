#pragma once

#include "mem.h"

#define HASH            Set
#define ENTRY           Chili
#define KEY(e)          *e
#define KEYEQ           chiIdentical
#define HASHFN          chiHashChili
#define EXISTS(e)       chiTrue(*e)
#define PREFIX          set
#include "hashtable.h"

CHI_INL bool setInserted(Set* set, Chili c) {
    Chili* p;
    if (setCreate(set, c, &p)) {
        *p = c;
        return true;
    }
    return false;
}
