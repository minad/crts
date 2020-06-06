#include <chili/cont.h>
#include "location.h"
#include "runtime.h"

extern int32_t main_z_Main;
CHI_WEAK int32_t main_z_Main = -2;

CHI_EXTERN_CONT_DECL(z_System__EntryPoints)
ATTR_CONT(z_System__EntryPoints, CHI_WEAK,, .na = 1) {
    PROLOGUE(z_System__EntryPoints);
    UNDEF_ARGS(0);
    ASET(0, CHI_CURRENT_RUNTIME->fail);
    RET;
}

void chiLocResolve(ChiLocResolve* resolve, ChiLoc loc, bool mangled) {
    chiLocResolveDefault(resolve, loc, mangled);
}

ChiLoc chiLocateFrame(const Chili* c) {
    return chiLocateFrameDefault(c);
}
