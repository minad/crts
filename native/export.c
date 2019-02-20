#include <chili/cont.h>
#include "export.h"

CHI_WEAK int32_t
    z_System_2fEntryPoints_z_exit = -2,
    z_System_2fEntryPoints_z_par = -2,
    z_System_2fEntryPoints_z_scheduler = -2,
    z_System_2fEntryPoints_z_unhandled = -2,
    z_System_2fEntryPoints_z_interrupt = -2,
    z_Main_z_main = -2;

CHI_CONT_DECL(extern, chiEntryPointsDefault)
CONT(chiEntryPointsDefault) {
    PROLOGUE(chiEntryPointsDefault);
    RET(CHI_FAIL);
}

CHI_CONT_ALIAS(chiEntryPointsDefault, z_System_2fEntryPoints)
