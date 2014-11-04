#!/bin/sh

if [ x"$1" = x"" ] || [ x"$2" = x"" ]
then
    exit 1
fi

( 
    for HELPER in $( cat "$1" | grep -v -E '^(#.*)?$' )
    do 
        grep "helper_${HELPER}(" *.c | grep -v gen_ | cut -d: -f1
    done 
) | sort | uniq | sed -E -e 's/^/#include "/;s/$/"/' > "$2"

