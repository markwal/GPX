;
;  ling.gcode (unit test for gpx)
;
;  Created by WHPThomas on 1/04/13.
;
;  Copyright (c) 2013 WHPThomas.
;
;
;  This program is free software; you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation; either version 2 of the License, or
;  (at your option) any later version.
;
;  This program is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;  GNU General Public License for more details.
;
;  You should have received a copy of the GNU General Public License
;  along with this program; if not, write to the Free Software Foundation,
;  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
; PREFIX
M70 (M103 - extruder off)
M103 (turn extruder off)

M70 (M73 - start build)
M73 P0 (start build progress)

G21 (units to mm)
G90 (set positioning to absolute)

; G0 - Rapid Positioning
M70 (G0 - rapid move)
G0 X10 Y10 Z10 E1
M70 (G0 - rapid move-f)
G0 X-10 Y-10 Z-10 E2 F1000

; G1 - Coordinated Motion
M70 (G1 - coord move)
G1 X10 Y10 Z10 E3
M70 (G1 - coord move-f)
G1 X10 Y10 Z10 E4 F2000

; G2 - Clockwise Arc
; G3 - Counter Clockwise Arc

; G4 - Dwell
M70 (G4 - dwell)
G4 P1000

; G10 - Create Coordinate System Offset from the Absolute one
M70 (G10 - set offsets)
G10 P1 X10 Y10 Z10
G10 P2 X20 Y20 Z20
G10 P3 X30 Y30 Z30
G10 P4 X40 Y40 Z40
G10 P5 X50 Y50 Z50
G10 P6 X60 Y60 Z60

; G20 - Use Inches as Units
; G70 - Use Inches as Units
M70 (G20 - imperial units)
G20
G1 X1 Y1 Z1 E0.197

; G21 - Use Milimeters as Units
; G71 - Use Milimeters as Units
M70 (G21 - metric units)
G21

; G28 - Home given axes to maximum
M70 (G28 - home to max)
G28 X Y

; G54 - Use coordinate system from G10 P1
M70 (G54 - use G10 P1)
G54
G1 X0 Y0 Z0 E6

; G55 - Use coordinate system from G10 P2
M70 (G55 - use G10 P2)
G55
G1 X0 Y0 Z0 E7

; G56 - Use coordinate system from G10 P3
M70 (G56 - use G10 P3)
G56
G1 X0 Y0 Z0 E8

; G57 - Use coordinate system from G10 P4
M70 (G57 - use G10 P4)
G57
G1 X0 Y0 Z0 E9

; G58 - Use coordinate system from G10 P5
M70 (G58 - use G10 P5)
G58
G1 X0 Y0 Z0 E10

; G59 - Use coordinate system from G10 P6
M70 (G59 - use G10 P6)
G59
G1 X0 Y0 Z0 E11

; G53 - Set absolute coordinate system
M70 (G53 - machine zero)
G53
G1 X0 Y0 Z0 E12

; G91 - Relative Positioning
M70 (G91 - relative)
G90
G1 X10 Y10 Z10 E1

; G90 - Absolute Positioning
M70 (G90 - absolute)
G90
G1 X0 Y0 Z0 E14

; G92 - Define current position on axes
M70 (G92 - define pos)
G92 X30 Y30 Z30 E0
G1 X0 Y0 Z0 E0

; G97 - Spindle speed rate
M70 (G97 - spindle speed)
G97 S50

; G130 - Set given axes potentiometer Value
M70 (G130 - set pots)
G130 X20 Y20 Z20 A20 B20

; G161 - Home given axes to minimum
M70 (G130 - home xy max)
G162 X Y F2500 (home XY axes maximum)

; G162 - Home given axes to maximum
M70 (G130 - home xy min)
G161 Z F1100 (home Z axis minimum)

; M0 - Unconditional Halt, not supported on SD?
; M1 - Optional Halt, not supported on SD?
; M2 - "End program

M70 (M104 - set temp)
M104 S230 T0
M104 S230 T1

; M3 - Spindle On - Clockwise
M70 (M3 - spindle on)
M3

; M4 - Spindle On - Counter Clockwise
M70 (M4 - spindle on)
M4

; M5 - Spindle Off
M70 (M5 - spindle off)
M5

; M6 - Wait for toolhead to come up to reach (or exceed) temperature
M70 (M6 - wait for tool)
M6 T0
M6 T1
M6

; M7 - Coolant A on (flood coolant)
;M70 (M7 - flood coolant)
;M7

; M8 - Coolant B on (mist coolant)
;M70 (M8 - mist coolant)
;M8

; M9 - All Coolant Off
;M70 (M9 - coolant off)
;M9

; M10 - Close Clamp
;M70 (M10 - close clamp)
;M10

; M11 - Open Clamp
;M70 (M11 - open clamp)
;M11

M70 (T1 - tool change)
T1

; M13 - Spindle CW and Coolant A On
;M70 (M13 - spindle +cool)
;M13

; M14 - Spindle CCW and Coolant A On
;M70 (M14 - spindle +cool)
;M14

; M17 - Enable Motor(s)
;M70 (M17 - enable motor)
;M17

; M18 - Disable Motor(s)
;M70 (M18 - disable motor)
;M18

; M21 - Open Collet
;M70 (M21 - open collet)
;M21

; M22 - Close Collet
;M70 (M22 - close collet)
;M22

; M30 - Program Rewind

; M70 - Display Message On Machine
M70 P20 (This is a really large message that will take up quite a few rows to display on the makerbot LCD screen)


; M71 - Display Message, Wait For User Button Press
M70 (M71 - wait for input)
M71 (Press the M Button)

; M72 - Play a Tone or Song
M70 (M72 - play song)
M72 P0
M72 P1

; M73 - Manual Set Build %
M70 (M73 - progress)
M73 P10 (build progress)

; M101 - Turn Extruder On, Forward
; M102 - Turn Extruder On, Reverse
; M103 - Turn Extruder Off
; M104 - Set Temperature
; M105 - Get Temperature
; M106 - Turn Automated Build Platform (or the Fan, on older models) On
; M107 - Turn Automated Build Platform (or the Fan, on older models) Off
; M108 - Set Extruder's Max Speed (R = RPM, P = PWM)
; M109 - Set Build Platform Temperature
; M110 - Set Build Chamber Temperature
; M126 - Valve Open
; M127 - Valve Close
; M128 - "Get Position
; M131 - Store Current Position to EEPROM
; M132 - Load Current Position from EEPROM
; M140 - Set Build Platform Temperature
; M141 - Set Chamber Temperature (Ignored)
; M142 - Set Chamber Holding Pressure (Ignored)
; M200 - Reset driver
; M300 - Set Servo 1 Position
; M301 - Set Servo 2 Position
; M310 - Start data capture
; M311 - Stop data capture
; M312 - Log a note to the data capture store
; M320 - Acceleration on for subsequent instructions
; M321 - Acceleration off for subsequent instructions

; T0 - Set Current Tool 0
; T1 - Set Current Tool 1

;POSTFIX
M70 (M73 - end build)
M73 P100 (end build progress)
M70 (after the end)
