#!/bin/bash
# to be run in Jenkins' shell environment on linux
# Run this from root directory

# helper variables
PLATFORM_NAME=linux-x64
BUILDCMD="ninja -j 2"
GENERATOR=Ninja

# quit on any sub-command error
set -e

# clear prev build
rm -rf "build/$PLATFORM_NAME"

# set up build
mkdir -p "build/$PLATFORM_NAME"
cd "build/$PLATFORM_NAME"
cmake -D STATIC=1 -D CMAKE_BUILD_TYPE=Release -G "$GENERATOR" -Wno-dev ../..

# make sure everything builds
$BUILDCMD print_version

$BUILDCMD qtwallet
$BUILDCMD simplewallet
$BUILDCMD daemon

$BUILDCMD tests

# run tests
$BUILDCMD test
tests/unit_tests
tests/coretests

# Create zip file
VERSION=`src/print_version`
REPO_BRANCH=`git branch -r --contains HEAD | tail -n 1`
REPO=`echo $REPO_BRANCH | cut -d/ -f1`
BRANCH=`echo $REPO_BRANCH | cut -d/ -f2`
ARCHIVE=pebblecoin-all-$PLATFORM_NAME-v$VERSION-$REPO-$BRANCH-$BUILD_NUMBER.tar.gz

cd src
# strip
strip -s pebblecoind simplewallet pebblecoin-qt
# create package
tar -cvzf $ARCHIVE pebblecoind simplewallet pebblecoin-qt
cd ../../..
# store archive name
echo build/$PLATFORM_NAME/src/$ARCHIVE > archive_name
