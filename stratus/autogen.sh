#!/bin/sh

libtoolize --force
aclocal 
autoheader
automake --add-missing --foreign
autoconf
./configure $*
