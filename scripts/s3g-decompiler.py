#!/usr/bin/python
#
#  s3g-decompiler.py
#
#  Created by Adam Mayer on Jan 25 2011
#  Updated by Jetty, Dan Newman, and Henry Thomas 2011 - 2015
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

byteOffset = 0

toolCommandTable = {
    1: ("", "(1) init: Initialize firmware to boot state"),
    3: ("<H", "(3) Set target temperature %i"),
    4: ("<B", "(4) Motor 1: set speed (PWM) %i"),
    5: ("<B", "(5) Motor 2: set speed (PWM) %i"),
    6: ("<I", "(6) Motor 1: set speed (RPM) %i"),
    7: ("<I", "(7) Motor 2: set speed (RPM) %i"),
    8: ("<I", "(8) Motor 1: set direction %i"),
    9: ("<I", "(9) Motor 2: set direction %i"),
    10: ("B", "(10) Motor 1: toggle %d"),
    11: ("B", "(11) Motor 2: toggle %d"),
    12: ("B", "(12) Toggle cooling fan %d"),
    13: ("B", "(13) Toggle blower fan %d"),
    14: ("B", "(14) Servo 1: angle %d"),
    15: ("B", "(15) Servo 2: angle %d"),
    27: ("B", "(27) Automated build platform: toggle %d"),
    31: ("<H", "(31) Set build platform temperature %i"),
    129: ("<iiiI", "(129) Absolute move: (%i,%i,%i) at DDA %i"),
}

def parseToolAction():
    global s3gFile
    global byteOffset
    packetStr = s3gFile.read(3)
    if len(packetStr) != 3:
        raise "Incomplete s3g file during tool command parse"
    byteOffset += 3
    (index,command,payload) = struct.unpack("<BBB",packetStr)
    contents = s3gFile.read(payload)
    if len(contents) != payload:
        raise "Incomplete s3g file: tool packet truncated"
    byteOffset += payload
    return (index,command,contents)

def printToolAction(tuple):
    print "(136) Tool %i:" % (tuple[0]),
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
    global byteOffset
    packetStr = s3gFile.read(4)
    if len(packetStr) < 4:
        raise "Incomplete s3g file during tool command parse"
    byteOffset += 4
    (options,offsetX,offsetY,timeout) = struct.unpack("<BBBB",packetStr)
    message = "";
    while True:
       c = s3gFile.read(1);
       byteOffset += 1
       if c == '\0':
          break;
       else:
          message += c;

    return (options,offsetX,offsetY,timeout,message)

def parseBuildStartNotificationAction():
    global s3gFile
    global byteOffset
    packetStr = s3gFile.read(4)
    if len(packetStr) < 4:
        raise "Incomplete s3g file during tool command parse"
    byteOffset += 4
    (steps) = struct.unpack("<i",packetStr)
    buildName = "";
    while True:
       c = s3gFile.read(1);
       byteOffset += 1
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
    129: ("<iiiI", "(129) Absolute move to (%i,%i,%i) at DDA %i"),
    130: ("<iii", "(130) Define machine position as (%i,%i,%i)"),
    131: ("<BIH", "(131) Home minimum on %X, feedrate %i, timeout %i s"),
    132: ("<BIH", "(132) Home maximum on %X, feedrate %i, timeout %i s"),
    133: ("<I", "(133) Dwell for %i microseconds"),
    134: ("<B", "(134) Change to Tool %i"),
    135: ("<BHH", "(135) Wait until Tool %i is ready, %i ms between polls, %i s timeout"),
    136: (parseToolAction, printToolAction),
    137: ("<B", "(137) Enable/disable stepper motors, axes bitmask %X"),
    138: ("<H", "(138) Wait on user response, option %i"),
    139: ("<iiiiiI", "(139) Absolute move to (%i,%i,%i,%i,%i) at DDA %i"),
    140: ("<iiiii", "(140) Define position as (%i,%i,%i,%i,%i)"),
    141: ("<BHH", "(141) Wait until platform %i is ready, %i ms between polls, %i s timeout"),
    142: ("<iiiiiIB", "(142) Move to (%i,%i,%i,%i,%i) in %i us, relative mask %X)"),
    143: ("<b", "(143) Store home position, axes bitmask %d"),
    144: ("<b", "(144) Recall home position, axes bitmask %d"),
    145: ("<BB", "(145) Set digipots for axes bitmask %i to %i"),
    146: ("<BBBBB", "(146) Set RGB LED red %i, green %i, blue %i, blink rate %i, effect %i"),
    147: ("<HHB", "(147) Set buzzer, frequency %i, length %i, effect %i"),
    148: ("<BHB", "(148) Pause for button 0x%X, timeout %i s, timeout_bevavior %i"),
    149: (parseDisplayMessageAction, "(149) Display message, options 0x%X at %i,%i timeout %i s, message \"%s\""),
    150: ("<BB", "(150) Set build percentage %i%%, ignore %i"),
    151: ("<B", "(151) Queue song %i"),
    152: ("<B", "(152) Reset to factory defaults, options 0x%X"),
    153: (parseBuildStartNotificationAction, "[153] Start build notification, steps %i, name \"%s\""),
    154: ("<B", "(154) End build notification, flags 0x%X"),
    155: ("<iiiiiIBfh", "(155) Move to (%i,%i,%i,%i,%i), dda_rate %i, relative mask %X, distance %f, feedrateX64 %i"),
    156: ("<B", "(156) Set acceleration state to %i"),
    157: ("<BBBIHHIIB", "(157) Stream version: %i.%i, %i, %i, %i, %i, %i, %i, %i"),
    158: ("<f", "(158) Pause @ Z position %f"),
}

def parseNextCommand():
    """Parse and handle the next command.  Returns
    True for success, False on EOF, and raises an
    exception on corrupt data."""
    global s3gFile
    global byteOffset
    commandStr = s3gFile.read(1)
    if len(commandStr) == 0:
        print "EOF"
        return False
    sys.stdout.write(str(lineNumber) + ' [' + str(byteOffset) + ']: ')
    byteOffset += 1
    (command) = struct.unpack("B",commandStr)
    (parse, disp) = commandTable[command[0]]
    if type(parse) == type(""):
        packetLen = struct.calcsize(parse)
        packetData = s3gFile.read(packetLen)
        if len(packetData) != packetLen:
            raise "Packet incomplete"
        byteOffset += packetLen
        parsed = struct.unpack(parse,packetData)
    else:
        parsed = parse()
    if type(disp) == type(""):
        print disp % parsed
    else:
        disp(parsed)
    return True

s3gFile = open(sys.argv[1],'rb')
lineNumber = 1
sys.stdout.write('Command count [File byte offset]: (Command ID) Command description\n')
while parseNextCommand():
    lineNumber  = lineNumber + 1
