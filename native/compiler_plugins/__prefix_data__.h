#ifdef __clang__
#  define ___prefix_data___(n) __attribute__ ((annotate("__prefix_data__ " #n)))
#  define __prefix_data__(n)   ___prefix_data___(n)
#else
#  error Prefix data is not supported
#endif
