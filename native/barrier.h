#pragma once

#include "private.h"

typedef struct ChiProcessor_ ChiProcessor;

bool chiThunkCas(ChiProcessor*, Chili, Chili, Chili);
void chiWriteField(ChiProcessor*, Chili, ChiAtomic*, Chili);
CHI_WU bool chiCasField(ChiProcessor*, Chili, ChiAtomic*, Chili, Chili);
void chiWriteBarrier(ChiProcessor*, Chili);
