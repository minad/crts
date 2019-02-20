#pragma once

#include "blockvec.h"

typedef struct ChiTimeout_ ChiTimeout;

void chiScan(ChiBlockVec*, ChiTimeout*, ChiScanStats*, uint32_t, bool);
