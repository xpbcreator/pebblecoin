# to be run in Jenkins' shell environment on windows
# Run this from root directory
# Expects MSBUILD and VCREDIST_PATH to be set

# helper variables
PLATFORM_NAME=win32-x64
BUILDCMD="$MSBUILD /property:Configuration=Release /property:Platform=x64"
GENERATOR="Visual Studio 14 Win64"

# quit on any sub-command error
set -e 

# clear prev build
rm -rf "build/$PLATFORM_NAME"

# set up build
mkdir -p "build/$PLATFORM_NAME"
cd "build/$PLATFORM_NAME"
cmake -D STATIC=1 -D CMAKE_BUILD_TYPE=Release -G "$GENERATOR" -Wno-dev ../..

# make sure everything builds
$BUILDCMD src\\print_version.vcxproj

$BUILDCMD src\\qtwallet.vcxproj
$BUILDCMD src\\simplewallet.vcxproj
$BUILDCMD src\\daemon.vcxproj

$BUILDCMD tests\\tests.vcxproj

# run tests
$BUILDCMD tests\\RUN_TESTS.vcxproj
tests\\Release\\unit_tests.exe
tests\\Release\\coretests.exe

# Create zip file
VERSION=`src\\\\Release\\\\print_version`
REPO_BRANCH=`git branch -r --contains HEAD | tail -n 1`
REPO=`echo $REPO_BRANCH | cut -d/ -f1`
BRANCH=`echo $REPO_BRANCH | cut -d/ -f2`
ARCHIVE=pebblecoin-all-$PLATFORM_NAME-v$VERSION-$REPO-$BRANCH-$BUILD_NUMBER.zip

rm -rf dist
mkdir -p dist
cd dist

cp ..\\src\\Release\\pebblecoind.exe .
cp ..\\src\\Release\\pebblecoin-qt.exe .
cp ..\\src\\Release\\simplewallet.exe .
# Copy redistributable
cd "C:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC\\redist\\x64\\Microsoft.VC140.CRT"
cp * "$WORKSPACE\\build\\$PLATFORM_NAME\\dist"

# create package
cd "$WORKSPACE"
# store archive name
echo build\\$PLATFORM_NAME\\$ARCHIVE > archive_name
# actually create archive
powershell -ExecutionPolicy ByPass -File build_system\\win32-zipdist.ps1
