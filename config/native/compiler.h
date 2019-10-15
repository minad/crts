/*
 * Configuration file for the native Chili runtime.
 * This file is included by both C and Haskell
 * and should therefore only contain traditional preprocessor syntax
 * and no C++-style comments.
 */

#ifdef __GLASGOW_HASKELL__
#  define __has_include(x) 0
#endif

/**
 * Minimum block limit in words
 */
#define CHI_BLOCK_MINLIMIT         497

/**
 * Maximum size in words of a unpinned object
 * allocated via bump allocation
 * This value must fit in the size field of Chili.
 */
#define CHI_MAX_UNPINNED           62

/**
 * Maximum number of arguments of a continuation.
 * Continuations with more arguments are split into
 * curried functions as of now. An alternative
 * would be to put the additional arguments on the stack.
 */
#define CHI_AMAX                   16

/**
 * Size of the context stored in
 * the interpreter stack frame.
 * Don't change this.
 */
#define CBY_CONTEXT_SIZE             4

/**
 * Bytecode magic and version
 */
#define CBY_BYTECODE_MAGIC           "cby1"
