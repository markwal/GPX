#!/bin/bash

# This is the script to build most of the release archives that we publish to
# github releases.  It is intended to run on a linux system though a Mac might be
# better since the idea is to cross-compile as many as possible and the Mac can
# do all of the builds we currently release: mac, linux, mingw32, mingw64, but
# for now, I don't have a Mac, so this script does the other three

echo "------------------------------------------------------------------"
echo "Linux build first"
mkdir -p build
cd build
mkdir -p linux
cd linux
make 
make test
make bdist
archive=`ls gpx*.tar.gz`
if [[ "${#archive[@]}" == "1" ]]; then
    mv $archive ../${archive/.tar.gz/-linux.tar.gz}
fi
cd ..

for plat in "win32" "win64"; do
    echo "------------------------------------------------------------------"
    echo "Building $plat"
    mkdir -p $plat
    cd $plat
    pwd
    case $plat in
        win32)
            args="-host i686-w64-mingw32"
            ;;
        win64)
            args="-host x86_64-w64-mingw32"
            ;;
    esac
    echo "Calling configure"
    ../../configure $args
    make
    make bdist DIST_TARGETS=dist-zip
    archive=`ls gpx*.zip`
    if [[ "${#archive[@]}" == "1" ]]; then
        mv $archive ../${archive/.zip/-$plat.zip}
    fi
    cd ..
done
