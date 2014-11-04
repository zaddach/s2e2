#!/bin/sh

HELPER_FUNCTIONS=$( cat helper.h | grep DEF_HELPER | sed -E -e 's/^.*\(([^,]*),.*$/\1/' )
INCLUDE_DIRECTIVES=""
for HELPER in ${HELPER_FUNCTIONS}
do 
    SOURCE_FILES=$( grep "helper_${HELPER}(" *.c | grep -v gen_ | cut -d: -f1 )
    for SOURCE_FILE in ${SOURCE_FILES}
    do
        INCLUDE_DIRECTIVE=$( echo ${SOURCE_FILE} | sed -E -e 's/^[[:space:]]*/#include "/;s/$/"/')
        INCLUDE_DIRECTIVES="${INCLUDE_DIRECTIVES}\n${INCLUDE_DIRECTIVE}"
    done
done

echo "${INCLUDE_DIRECTIVES}" | sort | uniq

