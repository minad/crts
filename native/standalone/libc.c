#include <libc.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define TRAP(x, ...) _Noreturn void x(__VA_ARGS__) { __libc_trap(#x); }
TRAP(__ubsan_handle_alignment_assumption, void* data, long ptr, long align, long off)
TRAP(__ubsan_handle_builtin_unreachable, void* data)
TRAP(__ubsan_handle_divrem_overflow, void* data, long lhs, long rhs)
TRAP(__ubsan_handle_float_cast_overflow, void* data, long val)
TRAP(__ubsan_handle_invalid_builtin, void* data)
TRAP(__ubsan_handle_load_invalid_value, void* data, long val)
TRAP(__ubsan_handle_nonnull_arg, void* data)
TRAP(__ubsan_handle_nonnull_return_v1, void* data, long val)
TRAP(__ubsan_handle_out_of_bounds, void* data, long idx)
TRAP(__ubsan_handle_pointer_overflow, void)
TRAP(__ubsan_handle_shift_out_of_bounds, void* data, long lhs, long rhs)
TRAP(__ubsan_handle_type_mismatch_v1, void* data, long ptr)
TRAP(__stack_chk_fail, void)
#undef TRAP
#pragma GCC diagnostic pop
