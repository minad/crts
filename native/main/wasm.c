#include <chili/cont.h>
// TODO: Wasm linker does not yet support --defsym
// see also runtime.c
CHI_EXTERN_CONT_DECL(CHI_WASM_MAIN)
CHI_EXTERN_CONT_DECL(z_Main)
CONT(z_Main) {
    PROLOGUE(z_Main);
    KNOWN_JUMP(CHI_WASM_MAIN);
}
extern int32_t CHI_CAT(CHI_WASM_MAIN, _z_main);
extern int32_t wasmMainIdx(void);
int32_t wasmMainIdx(void) { return CHI_CAT(CHI_WASM_MAIN, _z_main); }
