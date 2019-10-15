#include <chili.h>

extern const ChiModuleDesc *__start_chi_module_registry[], *__stop_chi_module_registry[];

extern const ChiModuleDesc* CHI_DYNLIB_MAIN;
CHI_WEAK const ChiModuleDesc* CHI_DYNLIB_MAIN = 0;

extern ChiModuleRegistry chiModuleRegistry;
ChiModuleRegistry chiModuleRegistry;

CHI_CONSTRUCTOR(dynlib) {
    chiModuleRegistry.main = CHI_DYNLIB_MAIN;
    chiModuleRegistry.modules = __start_chi_module_registry;
    chiModuleRegistry.size = (size_t)(__stop_chi_module_registry - __start_chi_module_registry);
    chiSortModuleRegistry(&chiModuleRegistry);
}
