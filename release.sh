#!/bin/bash

# This is the script to build most of the release archives that we publish to
# github releases.  It is intended to run on a linux system. It uses mingw to
# cross compile for Windows, so the only build that has to be done elsewhere
# is the Mac

# In order to use this script you need to install MingW64 for command line use
# Under WSL (Windows Subsystem for Linux) Ubuntu (Can be installed from the MS Store)
# this command installs MingW64: sudo apt install mingw-w64

# Also, gpx needs to be installed on the machine you're running this script on
# ie after building the linux version: sudo make install

echo "------------------------------------------------------------------"
echo "Linux build first"
mkdir -p build
cd build
mkdir -p linux
cd linux
../../configure --enable-maintainer-mode $args
make 
make test
make install
make bdist
archive=`ls gpx*.tar.gz`
if [[ "${#archive[@]}" == "1" ]]; then
    mv $archive ..
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
            args="-host=i686-w64-mingw32"
            ;;
        win64)
            args="-host=x86_64-w64-mingw32"
            ;;
    esac
    echo "Calling configure"
    ../../configure --enable-maintainer-mode $args
    make
    make bdist
    archive=`ls gpx*.zip`
    if [[ "${#archive[@]}" == "1" ]]; then
        mv $archive ..
    fi
    cd ..
done
