#include <chili/cont.h>
#include "export.h"
#include "location.h"
#include "runtime.h"

CHI_WEAK int32_t
    z_System__EntryPoints_z_start = -2,
    z_System__EntryPoints_z_userInterrupt = -2,
    z_System__EntryPoints_z_timerInterrupt = -2,
    z_System__EntryPoints_z_unhandled = -2,
    z_System__EntryPoints_z_blackhole = -2,
    z_Main_z_main = -2;

ATTR_CONT(z_System__EntryPoints, CHI_WEAK,,) {
    PROLOGUE(z_System__EntryPoints);
    RET(CHI_FAIL);
}

void chiLocResolve(ChiLocResolve* resolve, ChiLoc loc, bool mangled) {
    chiLocResolveDefault(resolve, loc, mangled);
}

ChiLoc chiLocateFrame(const Chili* c) {
    return chiLocateFrameDefault(c);
}
