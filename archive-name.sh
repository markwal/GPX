#!/usr/bin/env bash

# configure helper to make the platform bit of the binary archive name more
# user friendly
#
# invoked from configure.ac AC_SUBST macro
# $1 == $host_os   $2 == $host_cpu

platform=$1

if [[ "$platform" =~ darwin.* ]];
then
    platform="osx"
elif [[ "$platform" == "mingw32" ]];
then
    if [[ "$2" == "i686" ]];
    then
        platform="win32"
    else
        platform="win64"
    fi
elif [[ "$platform" =~ .*linux.* ]];
then
    platform="linux"
fi

echo "$platform"
