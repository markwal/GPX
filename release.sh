#!/bin/bash

# This is the script to build most of the release archives that we publish to
# github releases.  It is intended to run on a linux system though a Mac might be
# better since the idea is to cross-compile as many as possible and the Mac can
# do all of the builds we currently release: mac, linux, mingw32, mingw64, but
# for now, I don't have a Mac, so this script does the other three

while getopts ":c" opt; do
    case $opt in
        c)
            clean=yes
            ;;
        \?)
            echo "Usage:" >&2
            echo "Paul put content here." >&2
            exit 1
            ;;
    esac
done

if [ -n "$clean" ]; then
    echo "Doing the cleaning..."
fi

echo "Doing the building..."
mkdir -p build
cd build
for plat in "linux" "win32" "win64"; do
    echo "------------------------------------------------------------------"
    echo "Making dir $plat"
    mkdir -p $plat
    cd $plat
    pwd
    case $plat in
        linux)
            args=""
            test=yes
            ;;
        win32)
            args="-host i686-w64-mingw32"
            unset test
            ;;
        win64)
            args="-host x86_64-w64-mingw32"
            unset test
            ;;
    esac
    echo "Calling configure"
    ../../configure $args
    make
    if [ -n "$test" ]; then
        make test
    fi
    make bdist
    archive=`ls gpx*.tar.gz`
    if [[ "${#archive[@]}" == "1" ]]; then
        mv $archive ../${archive/.tar.gz/-$plat.tar.gz}
    fi
    cd ..
done
