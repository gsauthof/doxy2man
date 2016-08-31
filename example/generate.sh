#!/bin/bash

doxy2man="$1"
: ${doxy2man:=../doxy2man}

set -eux

[ -f omg.h ] || { echo "Couldn't find omg.h"; exit 1; }

rm -f Doxyfile

doxygen -g

sed -e 's/^\(GENERATE_XML\) *= *NO/\1 = YES/i' \
    -e 's/^\(XML_PROGRAMLISTING\) *= *YES/\1 = NO/i' \
    -i Doxyfile

doxygen

"$doxy2man" --novalidate xml/omg_8h.xml
