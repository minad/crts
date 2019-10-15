#include <stdlib.h>
#include "../runtime.h"

CHI_WARN_OFF(frame-larger-than=)
_Noreturn void chiMain(int argc, char** argv) {
    ChiRuntime rt;
    chiSystemSetup();
    chiRuntimeMain(&rt, argc, argv, exit);
}
CHI_WARN_ON
