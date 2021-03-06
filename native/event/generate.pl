#!/usr/bin/perl -w
use strict;
use File::Slurper qw(read_text write_text);

my $generated = 'Generated by generate.pl from defs.in';
my $defs = read_text "runtime/native/event/defs.in";
my $types = "";
my %type = ();
my $enum_names = "";

while ($defs =~ /CHI_NEWTYPE\((\w+),\s*(\w+)\)/gs) {
    $types .= "typedef $2 Chi$1;\n\n";
    $type{"Chi$1"} = ['newtype', $1, $2];
}

while ($defs =~ /typedef\s+enum\s+.*?\{(.*?)\}\s*(\w+);/gs) {
    my $lines = $1;
    my $name = $2;
    $lines =~ s/\s//g;
    my @enums = split ',', $lines;
    $type{$name} = ['enum', $name];
    $types .= "typedef uint32_t $name;\n\n";
    $enum_names .= "\n" if $enum_names ne "";
    $enum_names .= "static const char* const enum${name}[] = {\n";
    foreach (@enums) {
        if ($_ !~ /^_/) {
            s/^CHI_//;
            $enum_names .= "    \"$_\",\n";
        }
    }
    $enum_names .= "};";
    $name =~ s/^Chi//;
}

while ($defs =~ /typedef\s+struct\s+.*?\{(.*?)\}\s*(\w+);/gs) {
    my $lines = $1;
    my $name = $2;
    $types .= "$&\n\n";
    $lines =~ s/\A\s+|\s+\Z//g;
    my @fields = ();
    foreach my $s (split /\n/, $lines) {
        $s =~ s/\s+/ /g;
        $s =~ s/;//g;
        my @type = split ' ', $s;
        die ("Invalid field type $s\n") if ($#type != 1);
        my ($type, $field) = @type;
        next if ($field =~ /^_/);
        if ($type =~ /ChiBytesRef|ChiStringRef|ChiWid|size_t|uintptr_t|u?int\d+_t|bool/) {
            push @fields, [$field, [$type]];
        } elsif ($type{$type}) {
            push @fields, [$field, $type{$type}];
        } elsif ($type =~ /^(\w+)\*$/ && $type{$1}) {
            push @fields, [$field, ['ptr', $type{$1}]];
        } else {
            die "Invalid type $type\n";
        }
    }
    $type{$name} = ['struct', \@fields];
}

while ($defs =~ /typedef\s+(\w+)\s+(\w+);/gs) {
    $type{$2} = $type{$1};
}

sub writeField {
    my($indent, $type, $ptr) = @_;
    my $t = $$type[0];
    my $ind = ' ' x $indent;
    if ($t eq 'struct') {
        my @fields = @{$$type[1]};
        my $ret = "";
        my $i = 0;
        foreach my $f (@fields) {
            ++$i;
            if ($$f[1][0] eq 'ptr' && $$f[1][1][0] eq 'struct') {
                $ret .= "${ind}SBLOCK_BEGIN($$f[0]);\n";
                $ret .= writeField($indent + 4, $$f[1], "$ptr$$f[0]");
                $ret .= "${ind}SBLOCK_END($$f[0]);\n";
            } elsif ($$f[1][0] eq 'struct') {
                my @subfields = @{$$f[1][1]};
                if ($#subfields > 0) {
                    $ret .= "${ind}SBLOCK_BEGIN($$f[0]);\n";
                    $ret .= writeField($indent + 4, $$f[1], "$ptr$$f[0].");
                    $ret .= "${ind}SBLOCK_END($$f[0]);\n";
                } else {
                    $ret .= "${ind}SFIELD($$f[0], " . writeField(0, $subfields[0][1], "$ptr$$f[0].$subfields[0][0]") . ");\n";
                }
            } else {
                $ret .= "${ind}SFIELD($$f[0], " . writeField(0, $$f[1], "$ptr$$f[0]") . ");\n";
            }
        }
        return $ret;
    }
    return "${ind}SDEC(CHI_UN($$type[1], $ptr))" if ($t eq "newtype");
    return "${ind}SDEC($ptr)" if ($t eq "ChiWid");
    return "${ind}SENUM($$type[1], $ptr)" if ($t eq "enum");
    return "${ind}SBOOL($ptr)" if ($t =~ /^bool$/);
    return "${ind}SHEX($ptr)" if ($t =~ /^uintptr_t$/);
    return "${ind}SDEC($ptr)" if ($t =~ /^(size_t|uint\d+_t)$/);
    return "${ind}SSTR($ptr)" if ($t eq 'ChiStringRef');
    return "${ind}SBYTES($ptr)" if ($t eq 'ChiBytesRef');
    die "Invalid type $t";
}

sub lttngField {
    my($type, $name, $ptr) = @_;
    my $t = $$type[0];
    return lttngField($$type[1], $name, $ptr) if ($t eq 'ptr');
    if ($t eq 'struct') {
        my @fields = @{$$type[1]};
        if ($#fields == 0 && ${$fields[0][1]}[0] eq 'ptr') {
            return lttngField($fields[0][1], $name, "$ptr$fields[0][0]->");
        }
        if ($#fields == 0 && ${$fields[0][1]}[0] eq 'struct') {
            return lttngField($fields[0][1], $name, "$ptr$fields[0][0].");
        }
        if ($#fields == 0 && $name ne "" && $ptr ne "") {
            return lttngField($fields[0][1], $name, "$ptr$fields[0][0]") . "\n";
        }
        my $ret = "";
        my $i = 0;
        foreach my $f (@fields) {
            ++$i;
            my $n = $name eq "" ? $$f[0] : "${name}_$$f[0]";
            if ($$f[1][0] eq 'ptr' && $$f[1][1][0] eq 'struct') {
                $ret .= lttngField($$f[1], $n, "$ptr$$f[0]->");
            } elsif ($$f[1][0] eq 'struct') {
                $ret .= lttngField($$f[1], $n, "$ptr$$f[0].");
            } else {
                $ret .= lttngField($$f[1], $n, "$ptr$$f[0]") . "\n";
            }
        }
        return $ret;
    }
    return "        ctf_integer($$type[2], $name, CHI_UN($$type[1], $ptr))" if ($t eq "newtype");
    return "        ctf_integer(uint32_t, $name, $ptr)" if ($t eq "ChiWid");
    return "        ctf_integer_hex($t, $name, $ptr)" if ($t eq "uintptr_t");
    return "        ctf_integer($t, $name, $ptr)" if ($t =~ /^(size_t|u?int\d+_t|bool)$/);
    return "        ctf_integer(uint32_t, $name, $ptr)" if ($t =~ /^(enum)$/);
    return "        ctf_sequence_text(uint8_t, $name, $ptr.bytes, uint32_t, $ptr.size)" if ($t eq 'ChiStringRef');
    return "        ctf_sequence_hex(uint8_t, $name, $ptr.bytes, uint32_t, $ptr.size)" if ($t eq 'ChiBytesRef');
    die "Invalid type $t";
}

my $fns = "";
foreach my $name (sort(keys %type)) {
    if ($name =~ /^ChiEvent(\w+)$/) {
        my $field = writeField 4, $type{$name}, "d->";
        $fns .= "SPAYLOAD_BEGIN($1, $name* d)
${field}SPAYLOAD_END

";
    }
}

my $mainFn = "";
while ($defs =~ /(DURATION|INSTANT)\s+(\w+)\s+(\w+)\s+(\w+)\s*/g) {
    my $cls = $1;
    my $name = $2;
    my $payload = $4;
    if ($cls eq "DURATION") {
        $mainFn .= "   case CHI_EVENT_${name}_BEGIN: break;\n";
        $mainFn .= "   case CHI_EVENT_${name}_END:";
        $mainFn .= " SPAYLOAD($payload, &e->payload->${name}_END);" if $payload ne "0";
        $mainFn .= " break;\n";
    } else {
        $mainFn .= "   case CHI_EVENT_$name:";
        $mainFn .= " SPAYLOAD($payload, &e->payload->$name);" if $payload ne "0";
        $mainFn .= " break;\n";
    }
}

write_text "runtime/native/event/serialize.h",
        qq(// $generated

${fns}SEVENT_BEGIN
   SFIELD(ts, SDEC(CHI_UN(Nanos, e->ts)));
   if (eventDesc[e->type].cls == CLASS_END)
       SFIELD(dur, SDEC(CHI_UN(Nanos, e->dur)));
   if (eventDesc[e->type].ctx != CTX_RUNTIME)
       SFIELD(wid, SDEC(e->wid));
   if (eventDesc[e->type].ctx == CTX_THREAD)
       SFIELD(tid, SDEC(e->tid));
   switch (e->type) {
$mainFn   default: break;
   }
SEVENT_END

#undef SBLOCK_BEGIN
#undef SBLOCK_END
#undef SBOOL
#undef SBYTES
#undef SENUM
#undef SEVENT_BEGIN
#undef SEVENT_END
#undef SFIELD
#undef SDEC
#undef SHEX
#undef SPAYLOAD
#undef SPAYLOAD_BEGIN
#undef SPAYLOAD_END
#undef SSTR
);

my $lttng = "";
while ($defs =~ /(DURATION|INSTANT)\s+(\w+)\s+(\w+)\s+(\w*)\s*/g) {
    my $cls = $1;
    my $name = $2;
    my $ctx = $3;
    my $payload = $4;

    my $end = "";
    if ($cls eq "DURATION") {
        $end = "_END";
        $lttng .= "TRACEPOINT_EVENT(
    chili,
    ${name}_BEGIN,
";
        $lttng .= "    TP_ARGS(";
        if ($ctx eq "LIBRARY" || $ctx eq "THREAD" || $ctx eq "PROCESSOR") {
            $lttng .= "const ChiProcessor*, proc";
        } elsif ($ctx eq "WORKER") {
            $lttng .= "const ChiWorker*, worker";
        } else {
            $lttng .= "const ChiRuntime*, rt";
        }
        $lttng .= "),\n    TP_FIELDS(\n";
        if ($ctx eq "LIBRARY" || $ctx eq "THREAD") {
            $lttng .= "        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)\n";
            $lttng .= "        ctf_integer(uint32_t, wid, proc->worker->wid)\n";
            $lttng .= "        ctf_integer(uint32_t, tid, chiThreadId(proc->thread))\n";
        } elsif ($ctx eq "PROCESSOR") {
            $lttng .= "        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)\n";
            $lttng .= "        ctf_integer(uint32_t, wid, proc->worker->wid)\n";
        } elsif ($ctx eq "WORKER") {
            $lttng .= "        ctf_integer_hex(uintptr_t, rt, (uintptr_t)worker->rt)\n";
            $lttng .= "        ctf_integer(uint32_t, wid, worker->wid)\n";
        } else {
            $lttng .= "        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)\n";
        }
        $lttng .= "    )\n)\n\n";
    }

    $lttng .= "TRACEPOINT_EVENT(
    chili,
    $name$end,
";

    $lttng .= "    TP_ARGS(";
    if ($ctx eq "LIBRARY" || $ctx eq "THREAD" || $ctx eq "PROCESSOR") {
        $lttng .= "const ChiProcessor*, proc";
    } elsif ($ctx eq "WORKER") {
        $lttng .= "const ChiWorker*, worker";
    } else {
        $lttng .= "const ChiRuntime*, rt";
    }
    if ($payload ne "0") {
        $lttng .= ", ";
        $lttng .= "const ChiEvent$payload*, d";
    }

    $lttng .= "),\n    TP_FIELDS(\n";
    if ($ctx eq "LIBRARY" || $ctx eq "THREAD") {
        $lttng .= "        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)\n";
        $lttng .= "        ctf_integer(uint32_t, tid, chiThreadId(proc->thread))\n";
    } elsif ($ctx eq "PROCESSOR") {
        $lttng .= "        ctf_integer_hex(uintptr_t, rt, (uintptr_t)proc->rt)\n";
        $lttng .= "        ctf_integer(uint32_t, wid, proc->worker->wid)\n";
    } elsif ($ctx eq "WORKER") {
        $lttng .= "        ctf_integer_hex(uintptr_t, rt, (uintptr_t)worker->rt)\n";
        $lttng .= "        ctf_integer(uint32_t, wid, worker->wid)\n";
    } else {
        $lttng .= "        ctf_integer_hex(uintptr_t, rt, (uintptr_t)rt)\n";
    }
    $lttng .= lttngField($type{"ChiEvent$payload"}, "", "d->")if ($payload ne "0");

    $lttng .= "    )\n)\n\n";
}

write_text "runtime/native/event/lttng.h",
        qq(// $generated

#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER chili

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "../event/lttng.h"

#if !defined(_CHI_EVENT_LTTNG_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _CHI_EVENT_LTTNG_H

#include <lttng/tracepoint.h>
#include "../processor.h"

${lttng}#endif

#include <lttng/tracepoint-event.h>
);

my $dtrace = "";
while ($defs =~ /(DURATION|INSTANT)\s+(\w+)\s+(\w+)\s+(\w*)\s*/g) {
    my $cls = $1;
    my $name = lc $2;
    my $ctx = $3;
    my $payload = $4;
    $name =~ s/_/__/g;
    if ($ctx eq "LIBRARY" || $ctx eq "THREAD" || $ctx eq "PROCESSOR") {
        $ctx = "ChiProcessor*";
    } elsif ($ctx eq "WORKER") {
        $ctx = "ChiWorker*";
    } else {
        $ctx = "ChiRuntime*";
    }
    $payload = $payload eq "0" ? "" : ", ChiEvent$payload*";
    if ($cls eq "DURATION") {
        $dtrace .= "  probe ${name}__begin($ctx);\n  probe ${name}__end($ctx$payload);\n";
    } else {
        $dtrace .= "  probe $name($ctx$payload);\n";
    }
}

write_text "runtime/native/event/dtrace.d",
        "/* $generated */

typedef struct {
    uint32_t size;
    uint8_t* bytes;
} ChiBytesRef;

typedef struct {
    uint32_t size;
    uint8_t* bytes;
} ChiStringRef;

typedef struct ChiRuntime_   ChiRuntime;
typedef struct ChiWorker_    ChiWorker;
typedef struct ChiProcessor_ ChiProcessor;

${types}provider chili {
$dtrace};
";

my $table = "";
while ($defs =~ /(DURATION|INSTANT)\s+(\w+)\s+(\w+)\s+(\w*)\s*([^\s]*)\s*"(.*?)"/g) {
    my $class = $1;
    my $ev = $2;
    my $ctx = $3;
    my $payload = $4;
    my $desc = $6;
    if ($class eq "DURATION") {
        $class = "Duration";
    } else {
        $class = "Instant";
    }
    if ($payload ne "0") {
        $payload = "ChiEvent$payload";
    }
    $ctx = ucfirst (lc $ctx);
    $payload = "" if ($payload eq "0");
    $table .= "|$class|$ev|$ctx|$payload|$desc\n";
}

write_text "runtime/native/event/table.adoc",
        qq(// $generated

|===
|Class|Event|Context|Payload|Description

$table|===
);

sub stubField {
    my($indent, $type, $args) = @_;
    my $t = $$type[0];
    my $ind = ' ' x $indent;
    if ($t eq 'struct') {
        my @fields = @{$$type[1]};
        my $ret = "";
        my $i = 0;
        foreach my $f (@fields) {
            ++$i;
            if ($$f[1][0] eq 'ptr' && $$f[1][1][0] eq 'struct') {
                $ret .= "${ind}.$$f[0] = {\n";
                $ret .= stubField($indent + 4, $$f[1], $args);
                $ret .= "${ind}},\n";
            } elsif ($$f[1][0] eq 'struct') {
                my @subfields = @{$$f[1][1]};
                if ($#subfields > 0) {
                    $ret .= "${ind}.$$f[0] = {\n";
                    $ret .= stubField($indent + 4, $$f[1], $args);
                    $ret .= "${ind}},\n";
                } else {
                    $ret .= "${ind}.$$f[0] = " . stubField(0, $subfields[0][1], $args) . ",\n";
                }
            } else {
                $ret .= "${ind}.$$f[0] = " . stubField(0, $$f[1], $args) . ",\n";
            }
        }
        return $ret;
    }

    my $arg = "a@{[$#{$args} + 1]}";
    if ($t eq "newtype" && $$type[2] eq "uint64_t") {
        push(@{$args}, [$arg, "uint64_t", "UInt64"]);
        return $arg;
    } elsif ($t eq "bool") {
        push(@{$args}, [$arg, $t, "Bool"]);
        return $arg;
    } elsif ($t eq "uintptr_t") {
        push(@{$args}, [$arg, $t, "CUIntPtr"]);
        return $arg;
    } elsif ($t eq "size_t") {
        push(@{$args}, [$arg, $t, "CSize"]);
        return $arg;
    } elsif ($t =~ /^uint(\d+)_t$/) {
        push(@{$args}, [$arg, $t, "UInt$1"]);
        return $arg;
    } elsif ($t eq "ChiWid") {
        push(@{$args}, [$arg, "Chili", "ProcessorHandle"]);
        return "((ChiProcessor*)chiToPtr($arg))->worker->wid";
    } elsif ($t =~ /^int(\d+)_t$/) {
        push(@{$args}, [$arg, $t, "Int$1"]);
        return $arg;
    } elsif ($t eq "ChiStringRef") {
        push(@{$args}, [$arg, "Chili", "String"]);
        return "chiStringRef(&$arg)";
    } elsif ($t eq "ChiBytesRef") {
        push(@{$args}, [$arg, "Chili", "Buffer"]);
        return "{ .size = chiBufferSize($arg), .bytes = chiBufferBytes($arg) }";
    } elsif ($t eq "enum") {
        push(@{$args}, [$arg, "uint32_t", "UInt32"]);
        return "($$type[1])$arg";
    } else {
        die "Invalid type $t";
    }
}

my $enum = "";
my $typesafe_wrappers = "";
my $union_val = "";
my $names = "";
my $maxlen = 0;
my $desc = "";
my $dur_stats_enum = "";
my $instant_stats_enum = "";
my $count = 0;
my $dur_begin_worker = 0;
my $dur_begin_rt = 0;
my $c_stub_header_enabled = "";
my $c_stub_header_disabled = "";
my $c_stub_impl = "";
my $chi_stub_types = "";
my $chi_stub_impl_native = "";
my $chi_stub_impl_nop = "";

while ($defs =~ /(DURATION|INSTANT)\s+(\w+)\s+(\w+)\s+(\w+)\s+([^\s]+)/g) {
    my $cls = $1;
    my $name = $2;
    my $ctx = $3;
    my $payload = $4;
    my $stats = $5;

    $maxlen = length($name) if (length($name) > $maxlen);
    $desc .= ",\n    " if ($desc);
    $names .= ",\n    " if ($names);
    if ($cls eq "DURATION") {
        my $dur_begin;
        if ($ctx eq "RUNTIME") {
            $dur_begin = $dur_begin_rt;
            ++$dur_begin_rt;
        } else {
            $dur_begin = $dur_begin_worker;
            ++$dur_begin_worker;
        }
        $desc .= "{ .ctx = CTX_$ctx, .cls = CLASS_BEGIN, .begin = $dur_begin },\n    ";
        $desc .= "{ .ctx = CTX_$ctx, .cls = CLASS_END, .begin = $dur_begin@{[$stats ne \"0\" ? \", .stats = 1+DSTATS_$name\" : '']} }";
        $names .= "\"${name}\",\n    \"${name}\"";
        $enum .= "    CHI_EVENT_${name}_BEGIN,\n    CHI_EVENT_${name}_END,\n";
        $count += 2;

        if ($stats ne "0") {
            $dur_stats_enum .= ",\n    " if $dur_stats_enum;
            $dur_stats_enum .= "DSTATS_$name";
            if ($stats ne "1") {
                $dur_stats_enum .= ", _END_STATS_$name = DSTATS_$name + $stats - 1";
            }
        }
    } else {
        $desc .= "{ .ctx = CTX_$ctx, .cls = CLASS_INSTANT@{[$stats ne \"0\" ? \", .stats = 1+ISTATS_$name\" : '']} }";
        $names .= "\"$name\"";
        $enum .= "    CHI_EVENT_$name,\n";
        ++$count;

        if ($stats ne "0") {
            $instant_stats_enum .= ",\n    " if $instant_stats_enum;
            $instant_stats_enum .= "ISTATS_$name";
            if ($stats ne "1") {
                $instant_stats_enum .= ", _END_ISTATS_$name = ISTATS_$name + $stats - 1";
            }
        }
    }

    if ($payload ne "0") {
        $union_val .= $cls eq "DURATION"
            ? "    ChiEvent$payload ${name}_END;\n"
            : "    ChiEvent$payload ${name};\n";
    }

    my $payloadArg = "";
    my $payloadName = "0";
    if ($payload ne "0") {
        $payloadArg = ", const ChiEvent$payload* d";
        $payloadName = "(const ChiEventPayload*)d";
    }

    my $ctxType;
    if ($ctx eq "RUNTIME") {
        $ctxType = "ChiRuntime";
    } elsif ($ctx eq "WORKER") {
        $ctxType = "ChiWorker";
    } else {
        $ctxType = "ChiProcessor";
    }

    if ($cls eq "DURATION") {
        $typesafe_wrappers .= "CHI_INL void _CHI_EVENT_${name}_BEGIN($ctxType* x) { chiEventWrite(x, CHI_EVENT_${name}_BEGIN, 0); }\n";
        $typesafe_wrappers .= "CHI_INL void _CHI_EVENT_${name}_END($ctxType* x$payloadArg) { chiEventWrite(x, CHI_EVENT_${name}_END, $payloadName); }\n";
    } else {
        $typesafe_wrappers .= "CHI_INL void _CHI_EVENT_${name}($ctxType* x$payloadArg) { chiEventWrite(x, CHI_EVENT_${name}, $payloadName); }\n";
    }

    my @header_args_enabled = ();
    my @header_args_disabled = ();
    my @impl_args = ();
    my @chi_argtypes = ();
    my @chi_args = ();
    if ($payload ne "0") {
        my @args = ();
        $payload = ",\n" . stubField(8, $type{"ChiEvent$payload"}, \@args) . "    ";
        foreach my $arg (@args) {
            push(@header_args_enabled, $arg->[1]);
            push(@header_args_disabled, "$arg->[1] CHI_UNUSED($arg->[0])");
            push(@impl_args, "$arg->[1] $arg->[0]");
            push(@chi_argtypes, $arg->[2]);
            push(@chi_args, $arg->[0]);
        }
    } else {
        push @header_args_enabled, "void";
        push @header_args_disabled, "void";
        push @impl_args, "void";
        $payload = "";
    }
    push @chi_argtypes, "IO Unit";

    my $nop_args = "";
    foreach (@chi_args) {
        $nop_args .= " _$_";
    }

    if ($ctx eq "LIBRARY") {
        if ($cls eq "DURATION") {
            $c_stub_header_enabled .= "CHI_EXPORT void chiEvent_${name}_BEGIN(void);\n";
            $c_stub_header_enabled .= "CHI_EXPORT void chiEvent_${name}_END(@{[join ', ', @header_args_enabled]});\n";
            $c_stub_header_disabled .= "CHI_EXPORT_INL void chiEvent_${name}_BEGIN(void) {}\n";
            $c_stub_header_disabled .= "CHI_EXPORT_INL void chiEvent_${name}_END(@{[join ', ', @header_args_disabled]}) {}\n";
            $c_stub_impl .= "void chiEvent_${name}_BEGIN(void) {\n    chiEvent0(CHI_CURRENT_PROCESSOR, ${name}_BEGIN);\n}\n\n";
            $c_stub_impl .= "void chiEvent_${name}_END(@{[join ', ', @impl_args]}) {\n    chiEvent@{[$payload eq '' ? '0' : '']}(CHI_CURRENT_PROCESSOR, ${name}_END$payload);\n}\n\n";

            my $n = $count - 2;
            $chi_stub_types .= "${name}_BEGIN : IO Unit\n";
            $chi_stub_types .= "${name}_BEGIN_ENABLED : IO Bool\n";
            $chi_stub_impl_native .= "${name}_BEGIN = whenEnabled $n (foreign (implicit IO Unit) \"chiEvent_${name}_BEGIN\")\n";
            $chi_stub_impl_native .= "${name}_BEGIN_ENABLED = enabled $n\n";
            $chi_stub_impl_nop .= "${name}_BEGIN = skip applicativeIO\n";
            $chi_stub_impl_nop .= "${name}_BEGIN_ENABLED = applicativeIO.pure False\n";

            $chi_stub_types .= "${name}_END : @{[join ' -> ', @chi_argtypes]}\n";
            $chi_stub_types .= "${name}_END_ENABLED : IO Bool\n";
            $chi_stub_impl_native .= "${name}_END @{[join ' ', @chi_args]} = whenEnabled $n (foreign (implicit (@{[join ' -> ', @chi_argtypes]})) \"chiEvent_${name}_END\" @{[join ' ', @chi_args]})\n";
            $chi_stub_impl_native .= "${name}_END_ENABLED = enabled $n\n";
            $chi_stub_impl_nop .= "${name}_END$nop_args = skip applicativeIO\n";
            $chi_stub_impl_nop .= "${name}_END_ENABLED = applicativeIO.pure False\n";
        } else {
            $c_stub_header_enabled .= "CHI_EXPORT void chiEvent_${name}(@{[join ', ', @header_args_enabled]});\n";
            $c_stub_header_disabled .= "CHI_EXPORT_INL void chiEvent_${name}(@{[join ', ', @header_args_disabled]}) {}\n";
            $c_stub_impl .= "void chiEvent_${name}(@{[join ', ', @impl_args]}) {\n    chiEvent@{[$payload eq '' ? '0' : '']}(CHI_CURRENT_PROCESSOR, ${name}$payload);\n}\n\n";

            my $n = $count - 1;
            $chi_stub_types .= "${name} : @{[join ' -> ', @chi_argtypes]}\n";
            $chi_stub_types .= "${name}_ENABLED : IO Bool\n";
            $chi_stub_impl_native .= "${name} @{[join ' ', @chi_args]} = whenEnabled $n (foreign (implicit (@{[join ' -> ', @chi_argtypes]})) \"chiEvent_${name}\" @{[join ' ', @chi_args]})\n";
            $chi_stub_impl_native .= "${name}_ENABLED = enabled $n\n";
            $chi_stub_impl_nop .= "${name}$nop_args = skip applicativeIO\n";
            $chi_stub_impl_nop .= "${name}_ENABLED = applicativeIO.pure False\n";
        }
    }
}

write_text "runtime/native/event/desc.h",
    qq(// $generated
enum {
    $dur_stats_enum,
    DSTATS_COUNT
};

enum {
    $instant_stats_enum,
    ISTATS_COUNT,
};

CHI_INTERN const char* const chiEventName[] = {
    $names
};

static const struct CHI_PACKED {
    EventClass   cls        : 2;
    EventContext ctx        : 2;
    uint8_t      stats      : 6;
    uint8_t      begin      : 6;
} eventDesc[] = {
    $desc
};

$enum_names
);

write_text "runtime/native/event/stubs.h",
        qq(// $generated
$c_stub_impl);

write_text "runtime/native/include/chili/event.h",
        qq(// $generated
CHI_INL CHI_WU bool chiEventFilterEnabled(const void* bits, uint32_t n) {
    return CHI_EVENT_ENABLED && CHI_UNLIKELY(_chiBitGet((const uintptr_t*)bits, n));
}

#if CHI_EVENT_ENABLED

CHI_EXPORT CHI_WU const void* chiEventFilter(void);
$c_stub_header_enabled
#else

CHI_EXPORT_INL CHI_WU const void* chiEventFilter(void) { return 0; }
$c_stub_header_disabled
#endif);

write_text "lib/System/Runtime/Event.chi",
        qq(-- $generated
use Base/Applicative open
use Base/Bool open
use Base/Int open
use Base/String open
use Base/UInt32 open
use Base/Unit open
use Buffer/IO open
use IO/Types open
use unsafe Foreign/Runtime open
use unsafe Foreign/Types open
use unsafe IO/Unsafe as UnsafeIO

trusted module System/Runtime/Event where

$chi_stub_types
#if target.native

private
filter : CPtr (CConst CVoid)
filter = UnsafeIO.unsafePerform (foreign (implicit IO (CPtr (CConst CVoid))) "chiEventFilter")

private
filterEnabled : CPtr (CConst CVoid) -> UInt32 -> IO Bool
filterEnabled = foreign "chiEventFilterEnabled"

private
enabled : Int -> IO Bool
enabled n = filterEnabled filter (downsizeIntUInt32.downsize' n)

private
whenEnabled : Int -> IO Unit -> IO Unit
whenEnabled n m = do
  x <- enabled n
  when applicativeIO x m

$chi_stub_impl_native
#else

$chi_stub_impl_nop
#endif
);

$defs =~ s/\s*#.*//sg;
$defs =~ s/struct /struct CHI_ALIGNED(8) /g;
write_text "runtime/native/event/defs.h",
    qq(// $generated
#pragma once

#include "../private.h"

typedef struct ChiRuntime_   ChiRuntime;
typedef struct ChiWorker_    ChiWorker;
typedef struct ChiProcessor_ ChiProcessor;

$defs

enum {
    _CHI_EVENT_DURATION_WORKER = $dur_begin_worker,
    _CHI_EVENT_DURATION_RUNTIME = $dur_begin_rt,
    _CHI_EVENT_COUNT = $count,
    _CHI_EVENT_MAXLEN = $maxlen,
};

#define _CHI_EVENT_FILTER_SIZE CHI_DIV_CEIL(_CHI_EVENT_COUNT, 8 * sizeof (uintptr_t))

typedef enum {
$enum} ChiEvent;

typedef union {
$union_val} ChiEventPayload;

#if CHI_EVENT_ENABLED
CHI_INTERN void chiEventWrite(void*, ChiEvent, const ChiEventPayload*);
#else
CHI_INL void chiEventWrite(void* CHI_UNUSED(x), ChiEvent CHI_UNUSED(e), const ChiEventPayload* CHI_UNUSED(d)) {}
#endif

$typesafe_wrappers);
