# GPX

GPX was created by Dr. Henry Thomas (aka Wingcommander) in April 2013

GPX is a post processing utility for converting gcode output from 3D slicing software like
Cura, KISSlicer, S3DCreator and Slic3r to x3g files for standalone 3D printing on Makerbot
Cupcake, ThingOMatic, and Replicator 1/2/2x printers - with support for both stock and
sailfish firmwares. My hope is that is little utility will open up Makerbot 3D printers to
a range of new and exciting sources and utilities for 3D printing input.

# Installation

## Linux

```
sudo apt-get install gpx
```

## Windows

* Download a release .zip file from [GPX
releases](https://github.com/markwal/GPX/releases)
* Copy gpx.exe from the .zip file to somewhere on your path.

*or*

If you'd prefer an installer, you may want to install GPX via
[GpxUi](https://markwal.github.io/GpxUi)

## Mac

* Download a release .dmg file from [GPX releases](https://github.com/markwal/GPX/releases)
* Open up the .dmg file and drag the gpx application to /usr/local/bin or somewhere on your PATH

*or* via *homebrew*:

`brew install gpx`

*or* with a GUI:

[GpxUi](https://markwal.github.io/GpxUi)

# Installing from source

You need to have the GNU tools already installed and configured on your machine.
For Windows, mingw or cygwin.  For Linux, `sudo apt-get install
build-essential`.  For Mac, perhaps: Xcode menu > Preferences > Downloads >
Command Line Tools.

```
git clone https://github.com/markwal/GPX
cd GPX
mkdir build
cd build
../configure
make
sudo make install
```

# Copyright

Copyright (c) 2013 WHPThomas, All rights reserved.
Additional changes Copyright (c) 2014, 2015 DNewman, MWalker
All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

# Usage
```
gpx [-CFdgilpqrtvw] [-b BAUDRATE] [-c CONFIG] [-e EEPROM] [-f DIAMETER] [-m MACHINE] [-N h|t|ht] [-n SCALE] [-x X] [-y Y] [-z Z] IN [OUT]

Options:
	-C	Create temporary file with a copy of the machine configuration
	-F	write X3G on-wire framing data to output file
	-N	Disable writing of the X3G header (start build notice),
	  	tail (end build notice), or both
	-d	simulated ditto printing
	-g	Makerbot/ReplicatorG GCODE flavor
	-i	enable stdin and stdout support for command line pipes
	-l	log to file
	-p	override build percentage
	-q	quiet mode
	-r	Reprap GCODE flavor
	-t	truncate filename (DOS 8.3 format)
	-v	verbose mode
	-w	rewrite 5d extrusion values
CONFIG: the filename of a custom machine definition (ini file)
EEPROM: the filename of an eeprom settings definition (ini file)
DIAMETER: the actual filament diameter in the printer

MACHINE: the predefined machine type
	some machine definitions have been updated with corrected steps per mm
	the original can be selected by prefixing o to the machine id
	(or1, or1d, or2, or2h, orx, ot7, ot7d)
	c3  = Cupcake Gen3 XYZ, Mk5/6 + Gen4 Extruder
	c4  = Cupcake Gen4 XYZ, Mk5/6 + Gen4 Extruder
	cp4 = Cupcake Pololu XYZ, Mk5/6 + Gen4 Extruder
	cpp = Cupcake Pololu XYZ, Mk5/6 + Pololu Extruder
	cxy = Core-XY with HBP - single extruder
	cxysz = Core-XY with HBP - single extruder, slow Z
	cr1 = Clone R1 Single with HBP
	cr1d = Clone R1 Dual with HBP
	r1  = Replicator 1 - single extruder
	r1d = Replicator 1 - dual extruder
	r2  = Replicator 2 (default)
	r2h = Replicator 2 with HBP
	r2x = Replicator 2X
	t6  = TOM Mk6 - single extruder
	t7  = TOM Mk7 - single extruder
	t7d = TOM Mk7 - dual extruder
	z   = ZYYX - single extruder
	zd  = ZYYX - dual extruder
	fcp = FlashForge Creator Pro

SCALE: the coordinate system scale for the conversion (ABS = 1.0035)
X,Y & Z: the coordinate system offsets for the conversion
	X = the x axis offset
	Y = the y axis offset
	Z = the z axis offset

IN: the name of the sliced gcode input filename
OUT: the name of the X3G output filename
       specify '--' to write to stdout

Examples:
	gpx -p -m r2 my-sliced-model.gcode
	gpx -c custom-tom.ini example.gcode /volumes/things/example.x3g
	gpx -x 3 -y -3 offset-model.gcode
```
