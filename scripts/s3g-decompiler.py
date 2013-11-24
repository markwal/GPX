#!/usr/bin/python
#
#  s3g-decompiler.py
#
#  Created by Adam Mayer on Jan 25 2011.
#
#  Originally from ReplicatorG sources /src/replicatorg/scripts
#  which are part of the ReplicatorG project - http://www.replicat.org
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import struct
import sys

toolCommandTable = {
    1: ("", "[1] init: Initialize firmware to boot state"),
    3: ("<H", "[3] set target temperature = %i"),
    4: ("<B", "[4] Motor 1: set speed (PWM) = %i"),
    5: ("<B", "[5] Motor 2: set speed (PWM) = %i"),
    6: ("<I", "[6] Motor 1: set speed (RPM) = %i"),
    7: ("<I", "[7] Motor 2: set speed (RPM) = %i"),
    8: ("<I", "[8] Motor 1: set direction = %i"),
    9: ("<I", "[9] Motor 2: set direction = %i"),
    10: ("B", "[10] Motor 1: toggle = %d"),
    11: ("B", "[11] Motor 2: toggle = %d"),
    12: ("B", "[12] toggle cooling fan = %d"),
    13: ("B", "[13] toggle blower fan = %d"),
    14: ("B", "[14] Servo 1: angle = %d"),
    15: ("B", "[15] Servo 2: angle = %d"),
    27: ("B", "[27] Automated build platform: toggle = %d"),
    31: ("<H", "'[31] set build platform temperature = %i"),
}

def parseToolAction():
    global s3gFile
    packetStr = s3gFile.read(3)
    if len(packetStr) != 3:
        raise "Incomplete s3g file during tool command parse"
    (index,command,payload) = struct.unpack("<BBB",packetStr)
    contents = s3gFile.read(payload)
    if len(contents) != payload:
        raise "Incomplete s3g file: tool packet truncated"
    return (index,command,contents)

def printToolAction(tuple):
    print "\t[136] Extruder(%i)" % (tuple[0]),
    # command - tuple[1]
    # data - tuple[2]
    (parse, disp) = toolCommandTable[tuple[1]]
    if type(parse) == type(""):
        packetLen = struct.calcsize(parse)
        if len(tuple[2]) != packetLen:
            raise "Packet incomplete"
        parsed = struct.unpack(parse,tuple[2])
    else:
        parsed = parse()
    if type(disp) == type(""):
        print disp % parsed

def parseDisplayMessageAction():
    global s3gFile
    packetStr = s3gFile.read(4)
    if len(packetStr) < 4:
        raise "Incomplete s3g file during tool command parse"
    (options,offsetX,offsetY,timeout) = struct.unpack("<BBBB",packetStr)
    message = "";
    while True:
       c = s3gFile.read(1);
       if c == '\0':
          break;
       else:
          message += c;

    return (options,offsetX,offsetY,timeout,message)

def parseBuildStartNotificationAction():
    global s3gFile
    packetStr = s3gFile.read(4)
    if len(packetStr) < 4:
        raise "Incomplete s3g file during tool command parse"
    (steps) = struct.unpack("<i",packetStr)
    buildName = "";
    while True:
       c = s3gFile.read(1);
       if c == '\0':
          break;
       else:
          buildName += c;

    return (steps[0],buildName)

# Command table entries consist of:
# * The key: the integer command code
# * A tuple:
#   * idx 0: the python struct description of the rest of the data,
#            of a function that unpacks the remaining data from the
#            stream
#   * idx 1: either a format string that will take the tuple of unpacked
#            data, or a function that takes the tuple as input and returns
#            a string
# REMINDER: all values are little-endian. Struct strings with multibyte
# types should begin with "<".
# For a refresher on Python struct syntax, see here:
# http://docs.python.org/library/struct.html

commandTable = {    
    129: ("<iiiI","\t[129] Absolute move to (%i,%i,%i) at DDA %i"),
    130: ("<iii","\t[130] Machine position set as (%i,%i,%i)"),
    131: ("<BIH","\t[131] Home minimum on %X, feedrate %i, timeout %i s"),
    132: ("<BIH","\t[132] Home maximum on %X, feedrate %i, timeout %i s"),
    133: ("<I","\t[133] Delay of %i us"),
    134: ("<B","\t[134] Change extruder %i"),
    135: ("<BHH","\t[135] Wait until extruder %i ready (%i ms between polls, %i s timeout"),
    136: (parseToolAction, printToolAction),
    137: ("<B", "\t[137] Enable/disable steppers %X"),
    138: ("<H", "\t[138] User block on ID %i"),
    139: ("<iiiiiI","\t[139] Absolute move to (%i,%i,%i,%i,%i) at DDA %i"),
    140: ("<iiiii","\t[140] Set extended position as (%i,%i,%i,%i,%i)"),
    141: ("<BHH","\t[141] Wait for platform %i (%i ms between polls, %i s timeout)"),
    142: ("<iiiiiIB","\t[142] Move to (%i,%i,%i,%i,%i) in %i us (relative: %X)"),
    143: ("<b","\t[143] Store home position for axes %d"),
    144: ("<b","\t[144] Recall home position for axes %d"),
    145: ("<BB","\t[145] Set pot axis %i to %i"),
    146: ("<BBBBB","\t[146] Set RGB led red %i, green %i, blue %i, blink rate %i, effect %i"),
    147: ("<HHB","\t[147] Set beep, frequency %i, length %i, effect %i"),
    148: ("<BHB","\t[148] Pause for button 0x%X, timeout %i s, timeout_bevavior %i"),
    149: (parseDisplayMessageAction, "\t[149] Display message, options 0x%X at %i,%i timeout %i\n '%s'"),
    150: ("<BB","\t[150] Set build percent %i%%, ignore %i"),
    151: ("<B","\t[151] Queue song %i"),
    152: ("<B","\t[152] Reset to factory, options 0x%X"),
    153: (parseBuildStartNotificationAction, "\t[153] Start build, steps %i: %s"),
    154: ("<B","\t[154] End build, flags 0x%X"),
    155: ("<iiiiiIBfh","\t[155] Move to (%i,%i,%i,%i,%i) dda_rate: %i (relative: %X) distance: %f feedrateX64: %i"),
    156: ("<B","\t[156] Set acceleration to %i"),
    157: ("<BBBIHHIIB","\t[157] Stream version %i.%i (%i %i %i %i %i %i %i)"),
    158: ("<f","\t[158] Pause @ zPos %f"),
}

def parseNextCommand():
    """Parse and handle the next command.  Returns
    True for success, False on EOF, and raises an
    exception on corrupt data."""
    global s3gFile
    commandStr = s3gFile.read(1)
    if len(commandStr) == 0:
        print "EOF"
        return False
    sys.stdout.write(str(lineNumber) + ': ')
    (command) = struct.unpack("B",commandStr)
    (parse, disp) = commandTable[command[0]]
    if type(parse) == type(""):
        packetLen = struct.calcsize(parse)
        packetData = s3gFile.read(packetLen)
        if len(packetData) != packetLen:
            raise "Packet incomplete"
        parsed = struct.unpack(parse,packetData)
    else:
        parsed = parse()
    if type(disp) == type(""):
        print disp % parsed
    else:
        disp(parsed)
    return True

s3gFile = open(sys.argv[1],'rb')
lineNumber = 0
while parseNextCommand():
    lineNumber  = lineNumber + 1
