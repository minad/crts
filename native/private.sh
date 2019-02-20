#!/bin/bash
(
    echo "// Generated by private.sh"
    echo "#include <chili.h>"
    for i in $(grep -hoPr '(?<=CHI_PRIVATE\()\w\w+(?=\))' $(dirname "$0")/include | sort | uniq); do
        echo "#define $i _$i"
    done
) > $(dirname "$0")/private.h