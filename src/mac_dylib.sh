#!/bin/sh

echo Packaging dynamic libraries...
# list all the local libs
DYLIBS=`otool -L pebblecoin-qt | grep /usr/local | grep -v :$ | awk -F' ' '{ print $1 }'`
# go recursively to handle symlinks
for dylib in $DYLIBS; do
DYLIBS="$DYLIBS `otool -L $dylib | grep /usr/local | grep -v :$ | awk -F' ' '{ print $1 }'`";
done;

# copy them in
mkdir -p lib
for dylib in $DYLIBS; do
cp $dylib lib; chmod 644 lib/`basename $dylib`;
done;

# replace pebblecoin-qt links and library links
for dylib in $DYLIBS; do
echo Rewriting `basename $dylib`...
install_name_tool -change $dylib @executable_path/lib/`basename $dylib` pebblecoin-qt;
# change it in all the other libraries
for dylib2 in $DYLIBS; do
install_name_tool -change $dylib @executable_path/lib/`basename $dylib` lib/`basename $dylib2`;
done;
done;
