/*
 * Sort .text$* sections by name to support data next to code.
 * This mirrors the behaviour of grouped sections in
 * the windows pe format.
 *
 * TODO: We should get it working with lld-7 instead of bfd, since
 * it supports INSERT BEFORE/AFTER.
 */
SECTIONS { .text : { KEEP(*(SORT(.text$*))) } } INSERT BEFORE .fini;
