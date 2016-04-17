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
import getopt

showOffset = False
byteOffset = 0

crc8_table = [
  0,  94, 188, 226,  97,  63, 221, 131, 194, 156, 126,  32, 163, 253,  31,  65,
157, 195,  33, 127, 252, 162,  64,  30,  95,   1, 227, 189,  62,  96, 130, 220,
 35, 125, 159, 193,  66,  28, 254, 160, 225, 191,  93,   3, 128, 222,  60,  98,
190, 224,   2,  92, 223, 129,  99,  61, 124,  34, 192, 158,  29,  67, 161, 255,
 70,  24, 250, 164,  39, 121, 155, 197, 132, 218,  56, 102, 229, 187,  89,   7,
219, 133, 103,  57, 186, 228,   6,  88,  25,  71, 165, 251, 120,  38, 196, 154,
101,  59, 217, 135,   4,  90, 184, 230, 167, 249,  27,  69, 198, 152, 122,  36,
248, 166,  68,  26, 153, 199,  37, 123,  58, 100, 134, 216,  91,   5, 231, 185,
140, 210,  48, 110, 237, 179,  81,  15,  78,  16, 242, 172,  47, 113, 147, 205,
 17,  79, 173, 243, 112,  46, 204, 146, 211, 141, 111,  49, 178, 236,  14,  80,
175, 241,  19,  77, 206, 144, 114,  44, 109,  51, 209, 143,  12,  82, 176, 238,
 50, 108, 142, 208,  83,  13, 239, 177, 240, 174,  76,  18, 145, 207,  45, 115,
202, 148, 118,  40, 171, 245,  23,  73,   8,  86, 180, 234, 105,  55, 213, 139,
 87,   9, 235, 181,  54, 104, 138, 212, 149, 203,  41, 119, 244, 170,  72,  22,
233, 183,  85,  11, 136, 214,  52, 106,  43, 117, 151, 201,  74,  20, 246, 168,
116,  42, 200, 150,  21,  75, 169, 247, 182, 232,  10,  84, 215, 137, 107,  53]

# CRC of payload should match CRC byte immediately after payload
# Equivalently, CRC of payload and CRC byte should be 0x00

def crc8(bytes):
    val = 0;
    for b in bytearray(bytes):
        val = crc8_table[val ^ b]
    return val

def axes_str(bitmask, high_bit=None):
    str = ''
    first = True
    if not high_bit is None:
        if bitmask & 0x80:
            str = high_bit[1] + ' '
        else:
            str = high_bit[0] + ' '
    for i in range(0,5):
        if bitmask & (1 << i):
            if not first:
                str += ', '
            first = False
            str += 'XYZAB'[i]
    return str

def Format(fmt, args):
    fmt_list = fmt.replace('%%', '~~').split('%')
    fmt_new = ''
    args_new = list(args)
    iargs = 0
    if len(fmt_list) > 0:
        fmt_new = fmt_list[0]
        for l in fmt_list[1:]:
            if l[0] == 'a':
                args_new[iargs] = axes_str(args_new[iargs])
                fmt_new += '%s' + l[1:]
            elif l[0] == 'A':
                args_new[iargs] = axes_str(args_new[iargs], ('Disable', 'Enable'))
                fmt_new += '%s' + l[1:]
            elif l[0] == 'b':
                args_new[iargs] = 'XYZAB'[args_new[iargs]]
                fmt_new += '%s' + l[1:]
            elif l[0] == 'O':
                if args_new[iargs] == 0:
                    args_new[iargs] = "off"
                else:
                    args_new[iargs] = "on"
                fmt_new += '%s' + l[1:]
            else:
                fmt_new += '%' + l
            iargs += 1
    return fmt_new.replace('~~', '%%'), tuple(args_new)

toolQueryTable = {
    0:  ("<H",  "(0) Get version, Host Version %i"),
    2:  ("",    "(2) Get toolhead temperature"),
    20: ("",    "(20) Unknown tool query"),
    22: ("",    "(22) Is tool ready?"),
    25: ("<HB", "(25) Read from EEPROM offset %i, %i bytes"),
    30: ("",    "(30) Get build platform temperature"),
    32: ("",    "(32) Get toolhead target temperature"),
    33: ("",    "(33) Get build platform target temperature"),
    35: ("",    "(35) Is build platform ready?"),
    36: ("",    "(36) Get tool status"),
    37: ("",    "(37) Get PID state"),
}

toolCommandTable = {
    1: ("", "(1) Initialize firmware to boot state"),
    3: ("<H", "(3) Set target temperature to %i C"),
    4: ("<B", "(4) Set Motor 1 speed (PWM) to %i"),
    5: ("<B", "(5) Set Motor 2 speed (PWM) to %i"),
    6: ("<I", "(6) Set Motor 1 set speed (RPM) to %i"),
    7: ("<I", "(7) Set Motor 2 speed (RPM) to %i"),
    8: ("<I", "(8) Set Motor 1 direction to %i"),
    9: ("<I", "(9) Set Motor 2 direction to %i"),
    10: ("B", "(10) Toggle Motor 1 to %d"),
    11: ("B", "(11) Toggle Motor 2 to %d"),
    12: ("B", "(12) Toggle cooling fan %d"),
    13: ("B", "(13) Toggle blower fan %d"),
    14: ("B", "(14) Set Servo 1 angle to %d"),
    15: ("B", "(15) Set Servo 2 angle to %d"),
    27: ("B", "(27) Automated build platform: toggle %d"),
    31: ("<H", "(31) Set build platform temperature to %i C"),
    129: ("<iiiI", "(129) Absolute move to (%i, %i, %i) with DDA %i"),
}

def parseToolAction():
    global s3gFile
    global byteOffset
    packetStr = s3gFile.read(3)
    if len(packetStr) != 3:
        raise "Error: file appears to be truncated; cannot parse"
    byteOffset += 3
    (index,command,payload) = struct.unpack("<BBB",packetStr)
    contents = s3gFile.read(payload)
    if len(contents) != payload:
        raise "Error: file appears to be truncated; cannot parse"
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
            raise "Error: file appears to be truncated; cannot parse"
        parsed = struct.unpack(parse,tuple[2])
    else:
        parsed = parse()
    if type(disp) == type(""):
        print disp % parsed

def parseToolQuery():
    global s3gFile
    global byteOffset
    packetStr = s3gFile.read(2)
    if len(packetStr) != 2:
        raise "Error: file appears to be truncated; cannot parse"
    byteOffset += 2
    (index,command) = struct.unpack("<BB",packetStr)
    try:
        (parse, disp) = toolQueryTable[command]
    except KeyError:
        sys.stdout.write("Tool query not recognized %d\n" % command)
        return (index,command,"")
    if type(parse) == type(""):
        payloadLen = struct.calcsize(parse)
        if payloadLen > 0:
            contents = s3gFile.read(payloadLen)
            if len(contents) != payload:
                print "Error: file appears to be truncated; cannot parse"
            byteOffset += payload
        else:
            contents = ""
    return (index,command,contents)

def printToolQuery(tuple):
    print "(10) Tool %i:" % (tuple[0]),
    # command - tuple[1]
    # data - tuple[2]
    try:
        (parse, disp) = toolQueryTable[tuple[1]]
    except KeyError:
        sys.stdout.write("Tool query not recognized %d\n" % tuple[1])
        return True
    if type(parse) == type(""):
        packetLen = struct.calcsize(parse)
        if len(tuple[2]) != packetLen:
            sys.stdout.write("Malformed packet: packetLen %d, tuple len %d\n" % (packetLen, len(tuple[2])))
            parsed = None
        else:
            parsed = struct.unpack(parse,tuple[2])
    else:
        parsed = parse()
    if parsed is None:
        print disp
    elif type(disp) == type(""):
        print disp % parsed

def parseDisplayMessageAction():
    global s3gFile
    global byteOffset
    packetStr = s3gFile.read(4)
    if len(packetStr) < 4:
        raise "Error: file appears to be truncated; cannot parse"
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
        raise "Error: file appears to be truncated; cannot parse"
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

def parseFramedData():
    global s3gFile
    global byteOffset

    #  Read payload length
    packetStr = s3gFile.read(1)
    byteOffset += 1

    # Read the payload + CRC byte
    (payloadLen) = struct.unpack("<B",packetStr)
    payloadStr = s3gFile.read(payloadLen[0] + 1)

    # Compute CRC over payload + CRC byte
    # Result should be 0x00
    crc = crc8(payloadStr)

    # Move back to the start of the payload
    s3gFile.seek(-(payloadLen[0]+1),1)

    # Now parse the payload
    if parseNextCommand(False):
        # Eat the CRC at the end of the frame
        s3gFile.read(1)
        byteOffset += 1

    # Flag a bad CRC
    # I suppose we could actually return this as a string
    #   and do something to get this printed out on the same line
    if crc != 0:
        print "*** The CRC fails to match the data in the previous command ***"

    return None

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
    0:   ("<H", "(0) Get version, Host Version %i"),
    1:   ("", "(1) Unknown"),
    3:   ("", "(3) Clear buffer"),
	7:   ("", "(7) Abort immediately"),
	8:   ("", "(8) Pause"),
    12:  ("<HB", "(12) Read from EEPROM, offset %i, count %i"),
    10:  (parseToolQuery, printToolQuery),
    11:  ("", "(11) Is finished?"),
    20:  ("", "(20) Get build name"),
    21:  ("", "(21) Get extended position"),
    22:  ("<B", "(22) Extended stop, bitfield is 0x%02x"),
	24:  ("<BBBII", "(24) Get build statistics"),
    27:  ("<H", "(27) Get advanced version number, Host Version %i"),
    18:  ("<B", "(18) Get next filename, restart %i"),
    129: ("<iiiI", "(129) Absolute move to (%i, %i, %i) with DDA %i"),
    130: ("<iii", "(130) Define position as (%i, %i, %i)"),
    131: ("<BIH", "(131) Home minimum on %a, feedrate %i us/step, timeout %i s"),
    132: ("<BIH", "(132) Home maximum on %a, feedrate %i us/step, timeout %i s"),
    133: ("<I", "(133) Dwell for %i milliseconds"),
    134: ("<B", "(134) Switch to Tool %i"),
    135: ("<BHH", "(135) Wait until Tool %i is ready, %i ms between polls, %i s timeout"),
    136: (parseToolAction, printToolAction),
    137: ("<B", "(137) %A stepper motors"),
    138: ("<H", "(138) Wait on user response, option %i"),
    139: ("<iiiiiI", "(139) Absolute move to (%i, %i, %i, %i, %i) with DDA %i"),
    140: ("<iiiii", "(140) Define position as (%i, %i, %i, %i, %i)"),
    141: ("<BHH", "(141) Wait until platform %i is ready, %i ms between polls, %i s timeout"),
    142: ("<iiiiiIB", "(142) Move to (%i, %i, %i, %i, %i) in %i us, %a relative"),
    143: ("<b", "(143) Store home position for %a"),
    144: ("<b", "(144) Recall home position for %a"),
    145: ("<BB", "(145) Set %b axis digipot to %i"),
    146: ("<BBBBB", "(146) Set RGB LED (0x%02x, 0x%02x, 0x%02x), blink rate %i, effect %i"),
    147: ("<HHB", "(147) Set buzzer frequency %i, duration %i ms, effect %i"),
    148: ("<BHB", "(148) Pause for button 0x%02x, timeout %i s, timeout behavior %i"),
    149: (parseDisplayMessageAction, "(149) Display message, options 0x%02x, position (%i, %i), timeout %i s, message \"%s\""),
    150: ("<BB", "(150) Set build percentage %i%%, reserved %i"),
    151: ("<B", "(151) Queue song %i"),
    152: ("<B", "(152) Restore factory defaults, options 0x%02x"),
    153: (parseBuildStartNotificationAction, "(153) Start build notification, steps %i, name \"%s\""),
    154: ("<B", "(154) End build notification, options 0x%02x"),
    155: ("<iiiiiIBfh", "(155) Move to (%i, %i, %i, %i, %i), DDA rate %i, %a relative, distance %f mm, feedrate*64 %i steps/s"),
    156: ("<B", "(156) Set segment acceleration %O"),
    157: ("<BBBIHHIIB", "(157) Stream version %i.%i, %i, %i, %i, %i, %i, %i, %i"),
    158: ("<f", "(158) Pause @ Z position %f"),
    213: (parseFramedData, None)
}

def parseNextCommand(showStart):
    """Parse and handle the next command.  Returns
    True for success, False on EOF, and raises an
    exception on corrupt data."""
    global s3gFile
    global byteOffset
    commandStr = s3gFile.read(1)
    if len(commandStr) == 0:
        print "EOF"
        return False
    if showStart:
        if showOffset:
            sys.stdout.write(str(lineNumber) + ' [' + str(byteOffset) + ']: ')
        else:
            sys.stdout.write(str(lineNumber) + ': ')
    byteOffset += 1
    (command) = struct.unpack("<B",commandStr)
    try:
        (parse, disp) = commandTable[command[0]]
    except KeyError:
        sys.stdout.write("Command not recognized %d\n" % command[0])
        return True
    if type(parse) == type(""):
        packetLen = struct.calcsize(parse)
        if packetLen > 0:
            packetData = s3gFile.read(packetLen)
            if len(packetData) != packetLen:
                raise "Error: file appears to be truncated; cannot parse"
            byteOffset += packetLen
            parsed = struct.unpack(parse,packetData)
        else:
            parsed = ""
    else:
        parsed = parse()
    if type(disp) == type(""):
        fmt, args = Format(disp, parsed)
        print fmt % args
    elif disp is not None:
        disp(parsed)
    return True

def usage(prog, exit_stat=0):
    str = 'Usage: %s [-o] input-x3g-file\n' % prog
    str += \
'  -o, --offsets\n' + \
'    Display the byte offset into the file for each command\n'
    if exit_stat != 0:
        sys.stderr.write(str)
    else:
        sys.stdout.write(str)
    sys.exit(exit_stat)

try:
    opts, args = getopt.getopt(sys.argv[1:], 'ho', ['help', 'offsets'])
except:
    usage(sys.argv[0], 1)

if len(args) == 0:
    usage(sys.argv[0], 1)

for opt, val in opts:
    if opt in ( '-h', '--help'):
        usage( sys.argv[0], 0 )
    elif opt in ('-o', '--offsets'):
        showOffset = True

s3gFile = open(args[0],'rb')
lineNumber = 1
sys.stdout.write('Command count')
if showOffset:
    sys.stdout.write(' [File byte offset]')
sys.stdout.write(': (Command ID) Command description\n')
while parseNextCommand(True):
    lineNumber  = lineNumber + 1
s3gFile.close()
