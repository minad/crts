#!/usr/bin/perl -w
use strict;
use File::Copy;
use File::Slurper qw(read_text write_text);

my $path = "../../../../libtommath";

my @fns = qw(
    mp_add
    mp_add_d
    mp_and
    mp_clamp
    mp_clear
    mp_cmp
    mp_cmp_mag
    mp_complement
    mp_copy
    mp_count_bits
    mp_div
    mp_div_2d
    mp_div_d
    mp_exch
    mp_get_double
    mp_get_i64
    mp_get_mag64
    mp_grow
    mp_init
    mp_init_copy
    mp_init_size
    mp_lshd
    mp_mod
    mp_mod_2d
    mp_mul
    mp_mul_2d
    mp_mul_d
    mp_neg
    mp_or
    mp_radix_size
    mp_radix_smap
    mp_read_unsigned_bin
    mp_rshd
    mp_set_double
    mp_set_i64
    mp_set_u64
    mp_signed_rsh
    mp_sub
    mp_sub_d
    mp_toradix
    mp_xor
    mp_zero
    s_mp_add
    s_mp_karatsuba_mul
    s_mp_mul_digs
    s_mp_mul_digs_fast
    s_mp_reverse
    s_mp_sub
  );

copy("$path/LICENSE", "LICENSE");

my $ltm_impl = '';
my $ltm_class = '';
foreach my $fn (@fns) {
    $ltm_impl .= read_text "$path/$fn.c";
    $ltm_class .= "#define @{[uc $fn]}_C\n";
}

my $ltm_headers = read_text("$path/tommath.h") . read_text("$path/tommath_cutoffs.h") . read_text("$path/tommath_private.h");
$ltm_headers =~ s/\s*\/\*[^\*]*\*\/\s*#ifndef MP_MALLOC.*?#endif//gs;
$ltm_headers =~ s/#include <limits.h>//g;

while ($ltm_headers =~ /(MP_PRIVATE )?((mp_err|mp_ord|int|void|double|unsigned long long) ((s_mp|mp)_[\w_]+)(?=\())/gm) {
    my $name = $2;
    my $all = $&;
    if ($ltm_impl =~ /^$name(?=\()/m) {
        $ltm_headers =~ s/^$all(?=\()/static $&/gm;
    }
}
$ltm_headers =~ s/(mp_err|void|uint64_t|int64_t) (mp_set_i64|mp_get_i64|mp_set_u64|mp_get_mag64)/static $1 $2/g;

my $ltm_code = "$ltm_class$ltm_headers$ltm_impl";

# cleanup some things
$ltm_code =~ s/#\s*include "[^"]+"\n//g;
$ltm_code =~ s/END://g;
$ltm_code =~ s/#if.*__STDC_IEC_559__.*/#if 1/g;
$ltm_code =~ s/CHAR_BIT/8/g;
$ltm_code =~ s/(\/\* LibTomMath.*?Unlicense \*\/)\n+//gs;
my $ltm_header = $1;
$ltm_code =~ s/\n\n+/\n\n/gs;

write_text 'tommath_amalgam.h', "$ltm_header\n$ltm_code";
