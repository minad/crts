#!/usr/bin/perl
use strict;
use File::Find;
use File::Slurper qw(read_text write_text);
use File::Basename qw(dirname);

my @files;
sub found {
    $_ = $File::Find::name;
    push @files, $_ if /\.[ch]$/;
}
find(\&found, 'runtime');

my %accesses_global = ();
my %callgraph = ();
my %file = ();
my %includes = ();

$callgraph{"CHI_STATIC_BUFFER"} = ["_chiStaticBuffer"];
$callgraph{"CHI_STATIC_BIGINT"} = ["_chiStaticBigInt"];
$callgraph{"CHI_STATIC_STRING"} = ["_chiStaticString"];
$callgraph{"chiStringNew"} = ["_chiStringNew"];

sub uniq {
    my %found;
    foreach (@_) {
        $found{$_} = 1;
    }
    return sort(keys(%found));
}

sub find_calls {
    my($body) = @_;
    my @calls = ();
    # See https://stackoverflow.com/questions/16477483/regex-match-text-from-nested-parenthesis
    while ($body =~ /(\w+)\(((?:[^()]++|\((?2)\))*+)\)/g) {
        push @calls, $1;
        push @calls, find_calls($2);
    }
    return @calls;
}

foreach my $file (sort(@files)) {
    my $code = read_text $file;
    while ($code =~ /\s+(\w+)\(((?:[^()]++|\((?2)\))*+)\)\s*\{((?:[^{}]++|\{(?3)\})*+)\}/msg) {
        my($fn, $args, $body) = ($1, $2, $3);
        $fn = "CONT($&)" if ($fn =~ /CONT$/ && $args =~ /^\w+/);

        my @accesses = ($body =~ /CHI_CURRENT_/g);
        if ($#accesses >= 0) {
            print "DUPLICATE GLOBAL ACCESS: $fn in $file\n" if $#accesses >= 1;
            $accesses_global{$fn} = 1;
        } else {
            $accesses_global{$fn} = 0;
        }

        foreach my $field (qw(rt option worker gc heap cls segment page desc secondary)) {
            my %found;
            while ($body =~ /((\w+)(?:->|\.)$field)(?:->|\.)/g) {
                next if $2 eq "opt";
                if ($found{$&}) {
                    print "TOO MANY DEREFS: $1 in $fn $file\n" if $found{$&} == 2;
                    $found{$&}++;
                } else {
                    $found{$&} = 1;
                }
            }
        }

        my @interp = ($body =~ /cbyInterpreter\(/g);
        print "MULTIPLE INTERPRETER DEREFS: $fn in $file\n" if $#interp >= 1;

        print "MUTABLE OPTION ACCESS: $fn in $file\n" if ($body =~ /((const\s+)?)ChiRuntimeOption\s*\*/ && $1 eq "");

        my @calls = uniq(find_calls($body));
        $callgraph{$fn} = \@calls;
        if (exists($file{$fn})) {
            $file{$fn} = [$file, @{$file{$fn}}];
        } else {
            $file{$fn} = [$file];
        }
    }

    my $dir = dirname($file);
    $includes{$file} = {} unless exists($includes{$file});
    while ($code =~ /# *include +"([^"]+)"/g) {
        my $name = $1;
        next if $name =~ /generic\/|cont\.h/;
        if (-e "$dir/$name") {
            ${$includes{$file}}{"$dir/$name"} = "\"$name\"";
        } elsif (-e "runtime/$name") {
            ${$includes{$file}}{"runtime/$name"} = "\"$name\"";
        } else {
            print "INCLUDE NOT FOUND: $name in $file\n";
        }
    }
    while ($code =~ /# *include +<([^>]+)[>]/g) {
        my $name = $1;
        next if $name =~ /generic\/|cont\.h/;
        if (-e "runtime/native/include/$name") {
            ${$includes{$file}}{"runtime/native/include/$name"} = "<$name>";
        } else {
            #print "SYSTEM INCLUDE NOT FOUND: $name in $file\n";
        }
    }
}

sub compute_accesses_global {
    my($fn) = @_;
    return [$fn] if exists($accesses_global{$fn}) && $accesses_global{$fn};
    if (exists($callgraph{$fn})) {
        foreach my $called (@{$callgraph{$fn}}) {
            my $acc = accesses_global($called);
            return [$fn, @{$acc}] if $acc;
        }
    }
    return 0;
}

my %visited = ();
sub accesses_global {
    my($fn) = @_;
    if (!exists($visited{$fn})) {
        $visited{$fn} = 0;
        $visited{$fn} = compute_accesses_global($fn);
    }
    return $visited{$fn};
}

sub includes_indirectly {
    my($file, $other) = @_;
    foreach my $o (keys(%{$includes{$file}})) {
        return 1 if includes($o, $other);
    }
    return 0;
}

sub includes {
    my($file, $other) = @_;
    return 1 if ${$includes{$file}}{$other};
    return includes_indirectly($file, $other);
}

foreach my $fn (sort(keys(%accesses_global))) {
    if ($accesses_global{$fn}) {
        foreach my $called (@{$callgraph{$fn}}) {
            %visited = ();
            my $acc = accesses_global($called);
            if ($acc) {
                my $acc = join '->', ($fn, @{$acc});
                print "INVALID GLOBAL ACCESS: $acc in @{[join ',', @{$file{$fn}}]}\n";
            }
        }
    }
}

sub sort_includes {
    my($code) = @_;
    return $code if $code =~ /lint.*no-sort/;
    my @lines = split(/\n/, $code);
    my @filtered = ();
    my $cont;
    foreach (@lines) {
        if (/cont\.h/) {
            $cont = $_;
        } else {
            push @filtered, $_;
        }
    }
    @filtered = uniq(@filtered);
    unshift @filtered, $cont if $cont;
    my @sorted = ();
    foreach (@filtered) {
        push(@sorted, $_) if /</;
    }
    foreach (@filtered) {
        push(@sorted, $_) if /"/;
    }
    foreach (@filtered) {
        push(@sorted, $_) unless /"|</;
    }
    return join("\n", @sorted) . "\n";
}

foreach my $file (sort(keys(%includes))) {
    my @redundant = ();
    foreach my $i (keys(%{$includes{$file}})) {
        if (includes_indirectly($file, $i)) {
            push(@redundant, $includes{$file}{$i});
            $includes{$file}{$i} = 0;
            #print "REDUNDANT INCLUDE: $i in $file\n" ;
        }
    }
    my $code = read_text $file;
    my $newcode = $code;
    foreach my $r (@redundant) {
        $newcode =~ s/# *include\s+$r([^\n]*?lint[^\n]+keep[^\n]*)?\n/$1 ? $& : ''/msge;
    }
    $newcode =~ s/(lint[^\n]+no-sort[^\n]*\n)?(# *include[^\n]+\n)+/sort_includes($&)/msge;
    write_text $file, $newcode unless $code eq $newcode;
}
