#include <chili.h>

extern const ChiModuleDesc *__start_chi_module_registry[], *__stop_chi_module_registry[];

extern const ChiModuleDesc* z_Main;
CHI_WEAK const ChiModuleDesc* z_Main = 0;

extern ChiModuleRegistry chiModuleRegistry;
ChiModuleRegistry chiModuleRegistry;

CHI_CONSTRUCTOR {
    chiModuleRegistry.main = z_Main;
    chiModuleRegistry.modules = __start_chi_module_registry;
    chiModuleRegistry.size = (size_t)(__stop_chi_module_registry - __start_chi_module_registry);
    chiSortModuleRegistry(&chiModuleRegistry);
}
