//
//  gpx.c
//
//  Created by WHPThomas on 1/04/13.
//
//  Copyright (c) 2013 WHPThomas, All rights reserved.
//
//  gpx references ReplicatorG sources from /src/replicatorg/drivers
//  which are part of the ReplicatorG project - http://www.replicat.org
//  Copyright (c) 2008 Zach Smith
//  and Makerbot4GSailfish.java Copyright (C) 2012 Jetty / Dan Newman
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "gpx.h"

// Machine definitions

//  Axis - maxfeedrate, stepspermm, endstop
//  Extruder - maxfeedrate, stepspermm, motorsteps

static Machine replicator_1 = {
    {18000, 2500, 94.139704, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 94.139704, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    1,  // extruder count
    20, // timeout
};

static Machine replicator_1D = {
    {18000, 2500, 94.139704, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 94.139704, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    2,  // extruder count
    20, // timeout
};

static Machine replicator_2 = {
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 0}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    1,  // extruder count
    20, // timeout
};

static Machine replicator_2X = {
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    2,  // extruder count
    20, // timeout
};

// The default machine definition is the Replicator 2

Machine machine = {
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // y axis
    {1170, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 0}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    1,  // extruder count
    20, // timeout
};

// PRIVATE FUNCTION PROTOTYPES

static double get_home_feedrate(int flag);

// GLOBAL VARIABLES

Command command;            // the gcode command line
Point5d currentPosition;    // the extruders current position in 5D space
Point5d machineTarget;      // machine target point
Point5d workTarget;         // work target point (including G10 offsets)
Point2d excess;             // the accumulated rounding error in mm to step conversion
int currentExtruder;        // the currently selectd extruder using Tn
double currentFeedrate;     // the current feed rate
int currentOffset;          // current G10 offset
Point3d offset[7];          // G10 offsets
Tool tool[2];               // tool state
int isRelative;             // signals relitive or absolute coordinates
int positionKnown;          // is the current extruder position known
int programState;           // gcode program state used to trigger start and end code sequences
unsigned line_number;       // the current line number in the gcode file
static char buffer[256];    // the statically allocated parse-in-place buffer

FILE *in;                   // the gcode input file stream
FILE *out;                  // the x3g output file stream

// cleanup code in case we encounter an error that causes the program to exit

static void on_exit(void)
{
    // close open files
    if(in != stdin) {
        fclose(in);
        if(out != stdout) {
            if(ferror(out)) {
                perror("while writing to output file");
            }
            fclose(out);
        }
    }
}

// initialization of global variables

static void initialize_globals(void)
{
    int i;
    
    // we default to using pipes
    in = stdin;
    out = stdout;
    
    // register cleanup function
    atexit(on_exit);
    
    command.flag = 0;
    
    // initialize current position to zero
    
    currentPosition.x = 0.0;
    currentPosition.y = 0.0;
    currentPosition.z = 0.0;
    
    currentPosition.a = 0.0;
    currentPosition.b = 0.0;
    
    command.e = 0.0;
    command.f = 0.0;
    command.l = 0.0;
    command.p = 0.0;
    command.q = 0.0;
    command.r = 0.0;
    command.s = 0.0;
    
    command.comment = "";
    
    excess.a = 0.0;
    excess.b = 0.0;
    
    currentFeedrate = get_home_feedrate(XYZ_BIT_MASK);
    
    currentOffset = 0;
    
    for(i = 0; i < 7; i++) {
        offset[i].x = 0.0;
        offset[i].y = 0.0;
        offset[i].z = 0.0;
    }
    
    currentExtruder = 0;
    
    for(i = 0; i < 2; i++) {
        tool[i].motor_enabled = 0;
        tool[i].rpm = 0;
        tool[i].nozzle_temperature = 0;
        tool[i].build_platform_temperature = 0;
    }
    
    isRelative = 0;
    positionKnown = 0;
    programState = 0;
    
    line_number = 1;
}

// STATE

#define start_program() programState = RUNNING_STATE
#define end_program() programState = ENDED_STATE

#define program_is_ready() programState < RUNNING_STATE
#define program_is_running() programState < ENDED_STATE

// IO FUNCTIONS

#define write_8(VALUE) fputc(VALUE, out)

static int write_16(unsigned short value)
{
    union {
        unsigned short s;
        unsigned char b[2];
    } u;
    u.s = value;
    
    if(fputc(u.b[0], out) == EOF) return EOF;
    if(fputc(u.b[1], out) == EOF) return EOF;
    return 0;
}

static int write_32(unsigned int value)
{
    union {
        unsigned int i;
        unsigned char b[4];
    } u;
    u.i = value;
    
    if(fputc(u.b[0], out) == EOF) return EOF;
    if(fputc(u.b[1], out) == EOF) return EOF;
    if(fputc(u.b[2], out) == EOF) return EOF;
    if(fputc(u.b[3], out) == EOF) return EOF;
    
    return 0;
}

static int write_float(float value) {
    union {
        float f;
        unsigned char b[4];
    } u;
    u.f = value;
    
    if(fputc(u.b[0], out) == EOF) return EOF;
    if(fputc(u.b[1], out) == EOF) return EOF;
    if(fputc(u.b[2], out) == EOF) return EOF;
    if(fputc(u.b[3], out) == EOF) return EOF;

    return 0;
}

// return the magnitude (length) of the 5D vector

static double magnitude(int flag, Ptr5d vector)
{
    double acc = 0.0;
    if(flag & X_IS_SET) {
        acc = vector->x * vector->x;
    }
    if(flag & Y_IS_SET) {
        acc += vector->y * vector->y;
    }
    if(flag & Z_IS_SET) {
        acc += vector->z * vector->z;
    }
    if(flag & A_IS_SET) {
        acc += vector->a * vector->a;
    }
    if(flag & B_IS_SET) {
        acc += vector->b * vector->b;
    }
    return sqrt(acc);
}

// return the largest axis in the vector

static double largest_axis(int flag, Ptr5d vector)
{
    double length, rval = 0.0;
    if(flag & X_IS_SET) {
        rval = fabs(vector->x);
    }
    if(flag & Y_IS_SET) {
        length = fabs(vector->y);
        if(rval < length) rval = length;
    }
    if(flag & Z_IS_SET) {
        length = fabs(vector->z);
        if(rval < length) rval = length;
    }
    if(flag & A_IS_SET) {
        length = fabs(vector->a);
        if(rval < length) rval = length;
    }
    if(flag & B_IS_SET) {
        length = fabs(vector->b);
        if(rval < length) rval = length;
    }
    return rval;
}

// calculate the dda for the longest axis for the current machine definition

static int get_longest_dda()
{
    // calculate once
    static int longestDDA = 0;
    if(longestDDA == 0) {
        longestDDA = (int)(60 * 1000 * 1000 / (machine.x.max_feedrate * machine.x.steps_per_mm));
    
        int axisDDA = (int)(60 * 1000 * 1000 / (machine.y.max_feedrate * machine.y.steps_per_mm));
        if(longestDDA < axisDDA) longestDDA = axisDDA;
    
        axisDDA = (int)(60 * 1000 * 1000 / (machine.z.max_feedrate * machine.z.steps_per_mm));
        if(longestDDA < axisDDA) longestDDA = axisDDA;
    }
    return longestDDA;
}

// return the maximum home feedrate

static double get_home_feedrate(int flag) {
    double feedrate = 0.0;
    if(flag & X_IS_SET) {
        feedrate = machine.x.home_feedrate;
    }
    if(flag & Y_IS_SET && feedrate < machine.y.home_feedrate) {
        feedrate = machine.y.home_feedrate;
    }
    if(flag & Z_IS_SET && feedrate < machine.z.home_feedrate) {
        feedrate = machine.z.home_feedrate;
    }
    return feedrate;
}

// return the maximum safe feedrate

double get_safe_feedrate(int flag, Ptr5d delta) {
    
    double feedrate = currentFeedrate;
    if(feedrate == 0.0) {
        feedrate = machine.x.max_feedrate;
        if(feedrate < machine.y.max_feedrate) {
            feedrate = machine.y.max_feedrate;
        }
        if(feedrate < machine.z.max_feedrate) {
            feedrate = machine.z.max_feedrate;
        }
        if(feedrate < machine.a.max_feedrate) {
            feedrate = machine.a.max_feedrate;
        }
        if(feedrate < machine.b.max_feedrate) {
            feedrate = machine.b.max_feedrate;
        }
    }

    double distance = magnitude(flag & XYZ_BIT_MASK, delta);
    if(flag & X_IS_SET && (feedrate * delta->x / distance) > machine.x.max_feedrate) {
        feedrate = machine.x.max_feedrate * distance / delta->x;
    }
    if(flag & Y_IS_SET && (feedrate * delta->y / distance) > machine.y.max_feedrate) {
        feedrate = machine.y.max_feedrate * distance / delta->y;
    }
    if(flag & Z_IS_SET && (feedrate * delta->z / distance) > machine.z.max_feedrate) {
        feedrate = machine.z.max_feedrate * distance / delta->z;
    }

    if(distance == 0) {
        if(flag & A_IS_SET && feedrate > machine.a.max_feedrate) {
            feedrate = machine.a.max_feedrate;
        }        
        if(flag & B_IS_SET && feedrate > machine.b.max_feedrate) {
            feedrate = machine.b.max_feedrate;
        }
    }
    else {
        if(flag & A_IS_SET && (feedrate * delta->a / distance) > machine.a.max_feedrate) {
            feedrate = machine.a.max_feedrate * distance / delta->a;
        }
        if(flag & B_IS_SET && (feedrate * delta->b / distance) > machine.b.max_feedrate) {
            feedrate = machine.b.max_feedrate * distance / delta->b;
        }
    }
    return feedrate;
}

// convert mm to steps using the current machine definition and accumulate the rounding error 

static Point5d mm_to_steps(Ptr5d mm, Ptr2d excess)
{
    double value;
    Point5d result;
    result.x = round(mm->x * machine.x.steps_per_mm);
    result.y = round(mm->y * machine.y.steps_per_mm);
    result.z = round(mm->z * machine.z.steps_per_mm);
    if(excess) {
        value = (mm->a * machine.a.steps_per_mm) + excess->a;
        result.a = round(value);
        excess->a = value - result.a;
        
        value = (mm->b * machine.b.steps_per_mm) + excess->b;
        result.b = round(value);
        excess->b = value - result.b;
    }
    else {
        result.a = round(mm->a * machine.a.steps_per_mm);
        result.b = round(mm->b * machine.b.steps_per_mm);        
    }
    return result;
}

// X3G COMMANDS

// 131 - Find axes minimums
// 132 - Find axes maximums

static void home_axes(unsigned direction)
{
    Point5d unitVector;
    int xyz_flag = command.flag & XYZ_BIT_MASK;
    double feedrate = command.flag & F_IS_SET ? currentFeedrate : get_home_feedrate(command.flag);
    double longestAxis = 0.0;
    assert(direction <= 1);

    // compute the slowest feedrate
    if(xyz_flag & X_IS_SET) {
        if(machine.x.home_feedrate < feedrate) {
            feedrate = machine.x.home_feedrate;
        }
        unitVector.x = 1;
        longestAxis = machine.x.steps_per_mm;
        // confirm machine compatibility
        if(direction != machine.x.endstop) {
            fprintf(stderr, "(line %u) Semantic Warning: X axis homing to %s endstop" EOL, line_number, direction ? "maximum" : "minimum");
        }
    }
    if(xyz_flag & Y_IS_SET) {
        if(machine.y.home_feedrate < feedrate) {
            feedrate = machine.y.home_feedrate;
        }
        unitVector.y = 1;
        if(longestAxis < machine.y.steps_per_mm) {
            longestAxis = machine.y.steps_per_mm;
        }
        if(direction != machine.y.endstop) {
            fprintf(stderr, "(line %u) Semantic Warning: Y axis homing to %s endstop" EOL, line_number, direction ? "maximum" : "minimum");
        }
        
    }
    if(xyz_flag & Z_IS_SET) {
        if(machine.z.home_feedrate < feedrate) {
            feedrate = machine.z.home_feedrate;
        }
        unitVector.z = 1;
        if(longestAxis < machine.z.steps_per_mm) {
            longestAxis = machine.z.steps_per_mm;
        }
        if(direction != machine.z.endstop) {
            fprintf(stderr, "(line %u) Semantic Warning: Z axis homing to %s endstop" EOL, line_number, direction ? "maximum" : "minimum");
        }
    }
    
    // unit vector distance in mm
    double distance = magnitude(xyz_flag, &unitVector);
    // move duration in microseconds = distance / feedrate * 60,000,000
    double microseconds = distance / feedrate * 60000000.0;
    // time between steps for longest axis = microseconds / longestStep
    unsigned step_delay = (unsigned)round(microseconds / longestAxis);
    
    if(write_8(direction == ENDSTOP_IS_MIN ? 131 :132) == EOF) exit(1);
    
    // uint8: Axes bitfield. Axes whose bits are set will be moved.
    if(write_8(xyz_flag) == EOF) exit(1);
    
    // uint32: Feedrate, in microseconds between steps on the max delta. (DDA)
    if(write_32(step_delay) == EOF) exit(1);
    
    // uint16: Timeout, in seconds.
    if(write_16(machine.timeout) == EOF) exit(1);
}

// 133 - delay

static void delay(unsigned milliseconds)
{
    if(write_8(133) == EOF) exit(1);
    
    // uint32: delay, in milliseconds
    if(write_32(milliseconds) == EOF) exit(1);
}

// 134 - Change extruder
static void change_extruder(unsigned extruder_id)
{
    assert(extruder_id < machine.extruder_count);
    if(write_8(134) == EOF) exit(1);
    
    // uint8: ID of the extruder to switch to
    if(write_8(extruder_id) == EOF) exit(1);
}

// 135 - Wait for extruder ready

static void wait_for_extruder(unsigned extruder_id, unsigned timeout)
{
    assert(extruder_id < machine.extruder_count);
    if(write_8(135) == EOF) exit(1);
    
    // uint8: ID of the extruder to wait for
    if(write_8(extruder_id) == EOF) exit(1);
    
    // uint16: delay between query packets sent to the extruder, in ms (nominally 100 ms)
    if(write_16(100) == EOF) exit(1);
    
    // uint16: Timeout before continuing without extruder ready, in seconds (nominally 1 minute)
    if(write_16(timeout) == EOF) exit(1);
}
 
// 136 - extruder action command

static void set_extruder_temperature(unsigned extruder_id, unsigned temperature)
{
    assert(extruder_id < machine.extruder_count);
    if(write_8(136) == EOF) exit(1);
    
    // uint8: ID of the extruder to query
    if(write_8(extruder_id) == EOF) exit(1);
    
    // uint8: Action command to send to the extruder
    if(write_8(3) == EOF) exit(1);
    
    // uint8: Length of the extruder command payload (N)
    if(write_8(2) == EOF) exit(1);
    
    // int16: Desired target temperature, in Celsius
    if(write_16(temperature) == EOF) exit(1);
}

static void set_fan(unsigned extruder_id, unsigned state)
{
    assert(extruder_id < machine.extruder_count);
    if(write_8(136) == EOF) exit(1);
    
    // uint8: ID of the extruder to query
    if(write_8(extruder_id) == EOF) exit(1);
    
    // uint8: Action command to send to the extruder
    if(write_8(12) == EOF) exit(1);
    
    // uint8: Length of the extruder command payload (N)
    if(write_8(1) == EOF) exit(1);
    
    // uint8: 1 to enable, 0 to disable
    if(write_8(state) == EOF) exit(1);
}

static void set_valve(unsigned extruder_id, unsigned state)
{
    assert(extruder_id < machine.extruder_count);
    if(write_8(136) == EOF) exit(1);
    
    // uint8: ID of the extruder to query
    if(write_8(extruder_id) == EOF) exit(1);
    
    // uint8: Action command to send to the extruder
    if(write_8(13) == EOF) exit(1);
    
    // uint8: Length of the extruder command payload (N)
    if(write_8(1) == EOF) exit(1);
    
    // uint8: 1 to enable, 0 to disable
    if(write_8(state) == EOF) exit(1);
}

static void set_build_platform_temperature(unsigned extruder_id, unsigned temperature)
{
    assert(extruder_id < machine.extruder_count);
    if(write_8(136) == EOF) exit(1);
    
    // uint8: ID of the extruder to query
    if(write_8(extruder_id) == EOF) exit(1);
    
    // uint8: Action command to send to the extruder
    if(write_8(31) == EOF) exit(1);
    
    // uint8: Length of the extruder command payload (N)
    if(write_8(2) == EOF) exit(1);
    
    // int16: Desired target temperature, in Celsius
    if(write_16(temperature) == EOF) exit(1);
}

// 137 - Enable/disable axes steppers

static void set_steppers(unsigned axes, unsigned state)
{
    unsigned bitfield = axes & AXES_BIT_MASK;
    if(state) {
        bitfield |= 0x80;
    }
    if(write_8(137) == EOF) exit(1);
    
    // uint8: Bitfield codifying the command (see below)
    if(write_8(bitfield) == EOF) exit(1);
}
 
// 139 - Queue extended point

static void queue_absolute_point()
{
    long longestDDA = get_longest_dda();
    Point5d steps = mm_to_steps(&workTarget, &excess);
    
    if(write_8(139) == EOF) exit(1);
    
    // int32: X coordinate, in steps
    if(write_32((int)steps.x) == EOF) exit(1);
    
    // int32: Y coordinate, in steps
    if(write_32((int)steps.y) == EOF) exit(1);
    
    // int32: Z coordinate, in steps
    if(write_32((int)steps.z) == EOF) exit(1);
    
    // int32: A coordinate, in steps
    if(write_32((int)steps.a) == EOF) exit(1);
    
    // int32: B coordinate, in steps
    if(write_32((int)steps.b) == EOF) exit(1);
    
    // uint32: Feedrate, in microseconds between steps on the max delta. (DDA)
    if(write_32((int)longestDDA) == EOF) exit(1);
    
    currentPosition = machineTarget;
    positionKnown = 1;
}

// 140 - Set extended position

static void set_position()
{
    Point5d steps = mm_to_steps(&workTarget, &excess);
    if(write_8(140) == EOF) exit(1);
    
    // int32: X position, in steps
    if(write_32((int)steps.x) == EOF) exit(1);
    
    // int32: Y position, in steps
    if(write_32((int)steps.y) == EOF) exit(1);
    
    // int32: Z position, in steps
    if(write_32((int)steps.z) == EOF) exit(1);
    
    // int32: A position, in steps
    if(write_32((int)steps.a) == EOF) exit(1);
    
    // int32: B position, in steps
    if(write_32((int)steps.b) == EOF) exit(1);
}

// 141 - Wait for build platform ready

static void wait_for_build_platform(unsigned extruder_id, int timeout)
{
    assert(extruder_id < machine.extruder_count);
    if(write_8(141) == EOF) exit(1);
    
    // uint8: ID of the extruder platform to wait for
    if(write_8(extruder_id) == EOF) exit(1);
    
    // uint16: delay between query packets sent to the extruder, in ms (nominally 100 ms)
    if(write_16(100) == EOF) exit(1);
    
    // uint16: Timeout before continuing without extruder ready, in seconds (nominally 1 minute)
    if(write_16(timeout) == EOF) exit(1);
}

// 142 - Queue extended point, new style
 
// 143 - Store home positions

static void store_home_positions(void)
{
    if(write_8(143) == EOF) exit(1);
    
    // uint8: Axes bitfield to specify which axes' positions to store.
    // Any axis with a bit set should have its position stored.
    if(write_8(command.flag & AXES_BIT_MASK) == EOF) exit(1);
}

// 144 - Recall home positions

static void recall_home_positions(void)
{
    if(write_8(144) == EOF) exit(1);
    
    // uint8: Axes bitfield to specify which axes' positions to recall.
    // Any axis with a bit set should have its position recalled.
    if(write_8(command.flag & AXES_BIT_MASK) == EOF) exit(1);
}

// 145 - Set digital potentiometer value

static void set_pot_value(unsigned axis, unsigned value)
{
    assert(axis <= 4);
    assert(value <= 127);
    if(write_8(145) == EOF) exit(1);
    
    // uint8: axis value (valid range 0-4) which axis pot to set
    if(write_8(axis) == EOF) exit(1);
    
    // uint8: value (valid range 0-127), values over max will be capped at max
    if(write_8(value) == EOF) exit(1);
}
 
// 146 - Set RGB LED value
 
// 147 - Set Beep
 
// 148 - Pause for button

// 149 - Display message to LCD

void display_message(char *message, unsigned timeout, int wait_for_button)
{
    long bytesSent = 0;
    unsigned bitfield = 0;
    unsigned seconds = 0;

    unsigned vPos = command.flag & L_IS_SET ? (unsigned)command.l : 0;
    if(vPos > 3) vPos = 3;

    unsigned hPos = command.flag & Q_IS_SET ? (unsigned)command.q : 0;
    if(hPos > 19) hPos = 19;
    
    unsigned maxLength = hPos ? 20 - hPos : 20;
    
    // clip string so it fits in 4 x 20 lcd display buffer
    long length = strlen(message);
    if(vPos || hPos) {
        if(length > maxLength) length = maxLength;
    }
    else {
        if(length > 80) length = 80;
    }
    
    while(bytesSent < length) {
        if(bytesSent + maxLength >= length) {
            seconds = timeout;
            bitfield |= 0x02; // last message in group
            if(wait_for_button) {
                bitfield |= 0x04;
            }
        }
        if(bytesSent > 0 || vPos || hPos) {
            bitfield |= 0x01; //do not clear flag
        }
        
        if(write_8(149) == EOF) exit(1);
        
        // uint8: Options bitfield (see below)
        if(write_8(bitfield) == EOF) exit(1);
        // uint8: Horizontal position to display the message at (commonly 0-19)
        if(write_8(hPos) == EOF) exit(1);
        // uint8: Vertical position to display the message at (commonly 0-3)
        if(write_8(vPos) == EOF) exit(1);
        // uint8: Timeout, in seconds. If 0, this message will left on the screen
        if(write_8(seconds) == EOF) exit(1);
        // 1+N bytes: Message to write to the screen, in ASCII, terminated with a null character.
        bytesSent += fwrite(message + bytesSent, 1, length > maxLength ? maxLength : length, out);
        if(write_8('\0') == EOF) exit(1);
    }
}

// 150 - Set Build Percentage

static void set_build_percent(unsigned percent)
{
    if(percent > 100) percent = 100;
    
    if(write_8(150) == EOF) exit(1);
    
    // uint8: percent (0-100)
    if(write_8(percent) == EOF) exit(1);
    
    // uint8: 0 (reserved for future use) (reserved for future use)
    if(write_8(0) == EOF) exit(1);
}

// 151 - Queue Song

static void queue_song(unsigned song_id)
{
    // song ID 0: error tone with 4 cycles
    // song ID 1: done tone
    // song ID 2: error tone with 2 cycles
    
    assert(song_id <= 2);
    if(write_8(151) == EOF) exit(1);
    
    // uint8: songID: select from a predefined list of songs
    if(write_8(song_id) == EOF) exit(1);
}

// 152 - Restore to factory settings

// 153 - Build start notification

static void start_build()
{
    char name_of_build[] = "GPX";

    if(write_8(153) == EOF) exit(1);
    
    // uint32: 0 (reserved for future use)
    if(write_32(0) == EOF) exit(1);

    // 1+N bytes: Name of the build, in ASCII, null terminated
    fwrite(name_of_build, 1, 4, out);
}

// 154 - Build end notification

static void end_build()
{
    if(write_8(154) == EOF) exit(1);

    // uint8: 0 (reserved for future use)
    if(write_8(0) == EOF) exit(1);
}
 
// 155 - Queue extended point x3g

static void queue_point(double feedrate)
{
    Point5d deltaMM;
    Point5d deltaSteps;

    // Because we don't know our previous position, we can't calculate the feedrate or
    // distance correctly, so we use an unaccelerated command with a fixed DDA
    if(!positionKnown) {
        queue_absolute_point();
        return;
    }
    
    // compute the relative distance traveled along each axis and convert to steps
    if(command.flag & X_IS_SET) {
        deltaMM.x = machineTarget.x - currentPosition.x;
        deltaSteps.x = round(fabs(deltaMM.x) * machine.x.steps_per_mm);
    }
    else {
        deltaMM.x = 0;
        deltaSteps.x = 0;
    }
    
    if(command.flag & Y_IS_SET) {
        deltaMM.y = machineTarget.y - currentPosition.y;
        deltaSteps.y = round(fabs(deltaMM.y) * machine.y.steps_per_mm);
    }
    else {
        deltaMM.y = 0;
        deltaSteps.y = 0;
    }
    
    if(command.flag & Z_IS_SET) {
        deltaMM.z = machineTarget.z - currentPosition.z;
        deltaSteps.z = round(fabs(deltaMM.z) * machine.z.steps_per_mm);
    }
    else {
        deltaMM.z = 0;
        deltaSteps.z = 0;
    }
    
    if(command.flag & A_IS_SET) {
        deltaMM.a = workTarget.a - currentPosition.a;
        deltaSteps.a = round(fabs(deltaMM.a) * machine.a.steps_per_mm);
    }
    else {
        deltaMM.a = 0;
        deltaSteps.a = 0;
    }
    
    if(command.flag & B_IS_SET) {
        deltaMM.b = workTarget.b - currentPosition.b;
        deltaSteps.b = round(fabs(deltaMM.b) * machine.b.steps_per_mm);
    }
    else {
        deltaMM.b = 0;
        deltaSteps.b = 0;
    }
    
    // check that we have actually moved
    if(magnitude(command.flag, &deltaSteps) > 0) {
        double distance = magnitude(command.flag & XYZ_BIT_MASK, &deltaMM);
        
        workTarget.a = -deltaMM.a;
        workTarget.b = -deltaMM.b;
        
        deltaMM.x = fabs(deltaMM.x);
        deltaMM.y = fabs(deltaMM.y);
        deltaMM.z = fabs(deltaMM.z);
        deltaMM.a = fabs(deltaMM.a);
        deltaMM.b = fabs(deltaMM.b);
        
        double feedrate = get_safe_feedrate(command.flag, &deltaMM);
        double minutes = distance / feedrate;
        
        if(minutes == 0) {
            distance = 0;
            if(command.flag & A_IS_SET) {
                distance = deltaMM.a;
            }
            if(command.flag & B_IS_SET && distance < deltaMM.b) {
                distance = deltaMM.b;
            }
            minutes = distance / feedrate;
        }
        
        //convert feedrate to mm/sec
        feedrate /= 60.0;
        
        // if either a or b is 0, but their motor is on, 'simulate' a 5D extrusion distance for them
        if(deltaMM.a == 0.0) {
            if(tool[0].motor_enabled && tool[0].rpm) {
                // minute * revolution/minute
                double numRevolutions = minutes * tool[0].rpm;
                // steps/revolution * mm/steps
                double mmPerRevolution = machine.a.motor_steps * (1 / machine.a.steps_per_mm);
                // set distance
                deltaMM.a = numRevolutions * mmPerRevolution;
                deltaSteps.a = round(fabs(workTarget.a - currentPosition.a) * machine.a.steps_per_mm);
                workTarget.a = -deltaMM.a;
            }
        }
        if(deltaMM.b == 0.0) {
            if(tool[1].motor_enabled && tool[1].rpm) {
                // minute * revolution/minute
                double numRevolutions = minutes * tool[1].rpm;
                // steps/revolution * mm/steps
                double mmPerRevolution = machine.b.motor_steps * (1 / machine.b.steps_per_mm);
                // set distance
                deltaMM.b = numRevolutions * mmPerRevolution;
                deltaSteps.b = round(fabs(workTarget.b - currentPosition.b) * machine.b.steps_per_mm);
                workTarget.b = -deltaMM.b;
            }
        }

        Point5d steps = mm_to_steps(&workTarget, &excess);
        
        double usec = (60 * 1000 * 1000 * minutes);
        
        double dda_interval = usec / largest_axis(command.flag, &deltaSteps);
        
        // Convert dda_interval into dda_rate (dda steps per second on the longest axis)
        double dda_rate = 1000 * 1000 / dda_interval;

        if(write_8(155) == EOF) exit(1);
        
        // int32: X coordinate, in steps
        if(write_32((int)steps.x) == EOF) exit(1);
        
        // int32: Y coordinate, in steps
        if(write_32((int)steps.y) == EOF) exit(1);
        
        // int32: Z coordinate, in steps
        if(write_32((int)steps.z) == EOF) exit(1);
        
        // int32: A coordinate, in steps
        if(write_32((int)steps.a) == EOF) exit(1);
        
        // int32: B coordinate, in steps
        if(write_32((int)steps.b) == EOF) exit(1);

        // uint32: DDA Feedrate, in steps/s
        if(write_32((unsigned)dda_rate) == EOF) exit(1);
        
        // uint8: Axes bitfield to specify which axes are relative. Any axis with a bit set should make a relative movement.
        if(write_8(A_IS_SET|B_IS_SET) == EOF) exit(1);

        // float (single precision, 32 bit): mm distance for this move.  normal of XYZ if any of these axes are active, and AB for extruder only moves
        if(write_float((float)distance) == EOF) exit(1);
        
        // uint16: feedrate in mm/s, multiplied by 64 to assist fixed point calculation on the bot
        if(write_16((unsigned)(feedrate * 64.0)) == EOF) exit(1);
        
        currentPosition = machineTarget;
	}
}

// 156 - Set segment acceleration

static void set_acceleration(int state)
{
    if(write_8(156) == EOF) exit(1);
    
    // uint8: 1 to enable, 0 to disable
    if(write_8(state) == EOF) exit(1);
}


// 157 - Stream Version

// return the length of the given file in bytes

static long get_filesize(FILE *file)
{
    long filesize = -1;
    fseek(file, 0L, SEEK_END);
    filesize = ftell(file);
    fseek(file, 0L, SEEK_SET);
    return filesize;
}

static char *normalize_word(char* p)
{
    // we expect a letter followed by a digit
    // [ a-zA-Z] [ +-]? [ 0-9]+ ('.' [ 0-9]*)?
    char *s = p + 1;
    char *e = p;
    while(isspace(*s)) s++;
    if(*s == '+' || *s == '-') {
        *e++ = *s++;
    }
    while(1) {
        // skip spaces
        if(isspace(*s)) {
            s++;
        }
        // append digits
        else if(isdigit(*s)) {
            *e++ = *s++;
        }
        else {
            break;
        }
    }
    if(*s == '.') {
        *e++ = *s++;
        while(1) {
            // skip spaces
            if(isspace(*s)) {
                s++;
            }
            // append digits
            else if(isdigit(*s)) {
                *e++ = *s++;
            }
            else {
                break;
            }
        }        
    }
    *e = 0;
    return s;
}

static char *normalize_comment(char *p) {
    // strip white space from the end of comment
    char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)(*--e))) *e = '\0';
    // strip white space from the beginning of comment.
    while(isspace(*p)) p++;
    return p;
}

static void usage()
{
    fputs("GPX " GPX_VERSION " Copyright (c) 2013 WHPThomas, All rights reserved.", stderr);
    fputs("\nUsage: gpx [-m <MACHINE> | -c <CONFIG>] INPUT [OUTPUT]", stderr);
    fputs("\nSwitches:\n\t-p\toverride build percentage", stderr);
    fputs("\nMACHINE is the predefined machine type", stderr);
    fputs("\n\tr1  = Replicator 1 - single extruder", stderr);
    fputs("\tr1d  = Replicator 1 - dual extruder", stderr);
    fputs("\tr2 = Replicator 2 (default config)", stderr);
    fputs("\tr2x = Replicator 2X", stderr);
    fputs("\nCONFIG is the filename of a custom machine definition (ini)", stderr);
    fputs("\nINPUT is the name of the sliced gcode input filename", stderr);
    fputs("\nOUTPUT is the name of the x3g output filename", stderr);

    exit(1);
}

int main(int argc, char * argv[])
{
    long filesize = 0;
    unsigned progress = 0;
    int c, i;
    int next_line = 0;
    int command_line = 0;
    int build_percent = 0;

    initialize_globals();

    // READ COMMAND LINE
    
    // get the command line options
    while ((c = getopt(argc, argv, "pm:c:")) != -1) {
        switch (c) {
            case 'c':
                /*
                TODO
                if(!get_custom_definition(&machine, optarg)) {
                    usage();
                };
                */
                break;
            case 'm':
                if(strcasecmp(optarg, "r1") == 0) {
                    machine = replicator_1;
                }
                else if(strcasecmp(optarg, "r1d") == 0) {
                    machine = replicator_1D;
                }
                else if(strcasecmp(optarg, "r2") == 0) {
                    machine = replicator_2;                    
                }
                else if(strcasecmp(optarg, "r2x") == 0) {
                    machine = replicator_2X;
                }
                else {
                    usage();
                }
                break;
            case 'p':
                build_percent = 1;
                break;
            case '?':
            default:
                usage();
        }
    }
    argc -= optind;
    argv += optind;
    
    // OPEN FILES FOR INPUT AND OUTPUT
    
    // open the input filename if one is provided
    if(argc > 0) {
        char *filename = argv[0];
        if((in = fopen(filename, "rw")) == NULL) {
            perror("Error opening input");
            exit(1);
        }
        filesize = get_filesize(in);
        argc--;
        argv++;
        // use the output filename if one is provided
        if(argc > 0) {
            filename = argv[0];
        }
        else {
            // or use the input filename with a .x3g extension
            char *dot = strrchr(filename, '.');
            if(dot) {
                long l = dot - filename;
                memcpy(buffer, filename, l);
                filename = buffer + l;
            }
            // or just append one if no .gcode extension is present
            else {
                filename = stpncpy(buffer, filename, 256 - 5);
            }
            *filename++ = '.';
            *filename++ = 'x';
            *filename++ = '3';
            *filename++ = 'g';
            *filename++ = '\0';
            filename = buffer;
        }
        if((out = fopen(filename, "wb")) == NULL) {
            perror("Error creating output");
            out = stdout;
            exit(1);
        }
    }
    
    // READ INPUT AND CONVERT TO OUTPUT
    
    // at this point we have read the command line, set the machine definition
    // and both the input and output files are open, so its time to parse the
    // gcode input and convert it to x3g output
    while(fgets(buffer, 256, in) != NULL) {
        // reset flag state
        command.flag = 0;
        char *digits, *p = buffer;
        while(isspace(*p)) p++;
        // check for line number
        if(*p == 'n' || *p == 'N') {
            digits = p;
            p = normalize_word(p);
            if(*p == 0) {
                fprintf(stderr, "(line %u) Syntax Error: line number command word 'N' is missing digits" EOL, line_number);
                exit(1);
            }
            next_line = line_number = atoi(digits);
        }
        else {
            next_line = line_number + 1;
        }
        // parse command words in command line
        while(*p != 0) {
            if(isalpha(*p)) {
                int c = *p;
                digits = p;
                p = normalize_word(p);
                switch(c) {
                        
                    // PARAMETERS

                        // Xnnn	 X coordinate, usually to move to
                    case 'x':
                    case 'X':
                        command.x = strtod(digits, NULL);
                        command.flag |= X_IS_SET;
                        break;
                        
                        // Ynnn	 Y coordinate, usually to move to
                    case 'y':
                    case 'Y':
                        command.y = strtod(digits, NULL);
                        command.flag |= Y_IS_SET;
                        break;
                        
                        // Znnn	 Z coordinate, usually to move to
                    case 'z':
                    case 'Z':
                        command.z = strtod(digits, NULL);
                        command.flag |= Z_IS_SET;
                        break;

                        // Annn	 Length of extrudate in mm.
                    case 'a':
                    case 'A':
                        command.a = strtod(digits, NULL);
                        command.flag |= A_IS_SET;
                        break;

                        // Bnnn	 Length of extrudate in mm.
                    case 'b':
                    case 'B':
                        command.b = strtod(digits, NULL);
                        command.flag |= B_IS_SET;
                        if(machine.extruder_count < 2) {
                            fprintf(stderr, "(line %u) Semantic Warning: Bn cannot access non-existant extruder" EOL, line_number);
                        }
                        break;
                                                
                        // Ennn	 Length of extrudate in mm.
                    case 'e':
                    case 'E':
                        command.e = strtod(digits, NULL);
                        command.flag |= E_IS_SET;
                        break;
                        
                        // Fnnn	 Feedrate in mm per minute.
                    case 'f':
                    case 'F':
                        command.f = strtod(digits, NULL);
                        command.flag |= F_IS_SET;
                        break;
                        
                        // Lnnn	 Parameter - not currently used
                    case 'l':
                    case 'L':
                        command.l = strtod(digits, NULL);
                        command.flag |= L_IS_SET;
                        break;
                                                
                        // Pnnn	 Command parameter, such as a time in milliseconds
                    case 'p':
                    case 'P':
                        command.p = strtod(digits, NULL);
                        command.flag |= P_IS_SET;
                        break;
                        
                        // Qnnn	 Parameter - not currently used
                    case 'q':
                    case 'Q':
                        command.q = strtod(digits, NULL);
                        command.flag |= Q_IS_SET;
                        break;
                        
                        // Rnnn	 Command Parameter, such as RPM
                    case 'r':
                    case 'R':
                        command.r = strtod(digits, NULL);
                        command.flag |= R_IS_SET;
                        break;
                        
                        // Snnn	 Command parameter, such as temperature
                    case 's':
                    case 'S':
                        command.s = strtod(digits, NULL);
                        command.flag |= S_IS_SET;
                        break;
                        
                    // COMMANDS

                        // Gnnn GCode command, such as move to a point
                    case 'g':
                    case 'G':
                        command.g = atoi(digits);
                        command.flag |= G_IS_SET;
                        break;
                        // Mnnn	 RepRap-defined command
                    case 'm':
                    case 'M':
                        command.m = atoi(digits);
                        command.flag |= M_IS_SET;
                        break;
                        // Tnnn	 Select extruder nnn.
                    case 't':
                    case 'T':
                        command.t = atoi(digits);
                        command.flag |= T_IS_SET;
                        break;
                    
                    default:
                        fprintf(stderr, "(line %u) Syntax Warning: unrecognised command word '%c'" EOL, line_number, c);
                }
            }
            else if(*p == ';') {
                // Comment
                command.comment = normalize_comment(p + 1);
                command.flag |= COMMENT_IS_SET;
                *p = 0;
            }
            else if(*p == '(') {
                // Comment
                char *e = strchr(p + 1, ')');
                if(e) {
                    *e = 0;
                    command.comment = normalize_comment(p + 1);
                    command.flag |= COMMENT_IS_SET;
                    p = e + 1;
                }
                else {
                    fprintf(stderr, "(line %u) Syntax Warning: comment is missing closing ')'" EOL, line_number);
                    command.comment = normalize_comment(p + 1);
                    command.flag |= COMMENT_IS_SET;
                    *p = 0;                   
                }
            }
            else if(*p == '*') {
                // Checksum
                *p = 0;
            }
            else if(iscntrl(*p)) {
                break;
            }
            else {
                fprintf(stderr, "(line %u) Syntax Error: unrecognised gcode '%s'" EOL, line_number, p);
            }
        }
        
        // CALCULATE TARGET POINT
        
        // x
        if(command.flag & X_IS_SET) {
            machineTarget.x = isRelative ? (currentPosition.x + command.x) : command.x;
        }
        else {
            machineTarget.x = currentPosition.x;
        }
        
        // y
        if(command.flag & Y_IS_SET) {
            machineTarget.y = isRelative ? (currentPosition.y + command.y) : command.y;
        }
        else {
            machineTarget.y = currentPosition.y;
        }
        
        // z
        if(command.flag & Z_IS_SET) {
            machineTarget.z = isRelative ? (currentPosition.z + command.z) : command.z;
        }
        else {
            machineTarget.z = currentPosition.z;
        }
        
        // we treat e as short hand for a or b being set
        // depending on the state of the currentExtruder
        if(command.flag & E_IS_SET) {
            if(currentExtruder == 0) {
                // a = e
                machineTarget.a = isRelative ? (currentPosition.a + command.e) : command.e;
                command.flag |= A_IS_SET;
                command.a = command.e;
                
                // b
                if(command.flag & B_IS_SET) {
                    machineTarget.b = isRelative ? (currentPosition.b + command.b) : command.b;
                }
                else {
                    machineTarget.b = currentPosition.b;
                }
            }
            else {                
                // a
                if(command.flag & A_IS_SET) {
                    machineTarget.a = isRelative ? (currentPosition.a + command.a) : command.a;
                }
                else {
                    machineTarget.a = currentPosition.a;
                }
                
                // b = e
                machineTarget.b = isRelative ? (currentPosition.b + command.e) : command.e;
                command.flag |= B_IS_SET;
                command.b = command.e;
            }
        }
        else {        
            // a
            if(command.flag & A_IS_SET) {
                machineTarget.a = isRelative ? (currentPosition.a + command.a) : command.a;
            }
            else {
                machineTarget.a = currentPosition.a;
            }
            // b
            if(command.flag & B_IS_SET) {
                machineTarget.b = isRelative ? (currentPosition.b + command.b) : command.b;
            }
            else {
                machineTarget.b = currentPosition.b;
            }
        }
        
        // update current feedrate
        if(command.flag & F_IS_SET) {
            currentFeedrate = command.f;
        }
        
        // CALCULATE WORK OFFSET
        
        workTarget.x = machineTarget.x + offset[currentOffset].x;
        workTarget.y = machineTarget.y + offset[currentOffset].y;
        workTarget.z = machineTarget.z + offset[currentOffset].z;
        workTarget.a = machineTarget.a;
        workTarget.b = machineTarget.b;

        // INTERPRET COMMAND
        
        if(command.flag & G_IS_SET) {
            command_line++;
            switch(command.g) {
                    // G0 - Rapid Positioning
                case 0:
                    if(command.flag & F_IS_SET) {
                        queue_point(currentFeedrate);
                    }
                    else {
                        Point3d delta;
                        if(command.flag & X_IS_SET) delta.x = fabs(workTarget.x - currentPosition.x);
                        if(command.flag & Y_IS_SET) delta.y = fabs(workTarget.y - currentPosition.y);
                        if(command.flag & Z_IS_SET) delta.z = fabs(workTarget.z - currentPosition.z);
                        double length = magnitude(command.flag & XYZ_BIT_MASK, (Ptr5d)&delta);
                        double candidate, feedrate = DBL_MAX;
                        if(command.flag & X_IS_SET && delta.x != 0.0) {
                            feedrate = machine.x.max_feedrate * length / delta.x;
                        }
                        if(command.flag & Y_IS_SET && delta.y != 0.0) {
                            candidate = machine.y.max_feedrate * length / delta.y;
                            if(feedrate > candidate) {
                                feedrate = candidate;
                            }
                        }
                        if(command.flag & Z_IS_SET && delta.z != 0.0) {
                            candidate = machine.z.max_feedrate * length / delta.z;
                            if(feedrate > candidate) {
                                feedrate = candidate;
                            }
                        }
                        if(feedrate == DBL_MAX) {
                            feedrate = machine.x.max_feedrate;
                        }
                        queue_point(feedrate);
                    }
                    break;
                    
                    // G1 - Coordinated Motion
                case 1:
                    queue_point(currentFeedrate);
                    break;
                    
                    // G2 - Clockwise Arc
                    // G3 - Counter Clockwise Arc
                    
                    // G4 - Dwell
                case 4:
                    if(command.flag & P_IS_SET) {
                        delay(command.p);
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax Error: G4 is missing delay parameter, use Pn where n is milliseconds" EOL, line_number);
                        exit(1);                        
                    }
                    break;

                    // G10 - Create Coordinate System Offset from the Absolute one
                case 10:
                    if(command.flag & P_IS_SET && command.p >= 1.0 && command.p <= 6.0) {
                        i = (int)command.p;
                        if(command.flag & X_IS_SET) offset[i].x = machineTarget.x;
                        if(command.flag & Y_IS_SET) offset[i].y = machineTarget.y;
                        if(command.flag & Z_IS_SET) offset[i].z = machineTarget.z;
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax Error: G10 is missing coordiante system, use Pn where n is 1-6" EOL, line_number);
                        exit(1);
                    }
                    break;
                    
                    // G53 - Set absolute coordinate system
                case 53:
                    currentOffset = 0;
                    break;

                    // G54 - Use coordinate system from G10 P1
                case 54:
                    currentOffset = 1;
                    break;

                    // G55 - Use coordinate system from G10 P2
                case 55:
                    currentOffset = 2;
                    break;
                    
                    // G56 - Use coordinate system from G10 P3
                case 56:
                    currentOffset = 3;
                    break;
                    
                    // G57 - Use coordinate system from G10 P4
                case 57:
                    currentOffset = 4;
                    break;
                    
                    // G58 - Use coordinate system from G10 P5
                case 58:
                    currentOffset = 5;
                    break;
                    
                    // G59 - Use coordinate system from G10 P6
                case 59:
                    currentOffset = 6;
                    break;
                    
                    // G90 - Absolute Positioning
                case 90:
                    isRelative = 0;
                    break;

                    // G91 - Relative Positioning
                case 91:
                    if(positionKnown) {
                        isRelative = 1;
                    }
                    else {
                        fprintf(stderr, "(line %u) Semantic Error: G91 switch to relitive positioning prior to first absolute move" EOL, line_number);
                        exit(1);
                    }
                    break;

                    // G92 - Define current position on axes
                case 92:
                    set_position();
                    if(command.flag & X_IS_SET) currentPosition.x = machineTarget.x;
                    if(command.flag & Y_IS_SET) currentPosition.y = machineTarget.y;
                    if(command.flag & Z_IS_SET) currentPosition.z = machineTarget.z;
                    if(command.flag & A_IS_SET) currentPosition.a = machineTarget.a;
                    if(command.flag & B_IS_SET) currentPosition.b = machineTarget.b;
                    break;
                    
                    // G130 - Set given axes potentiometer Value
                case 130:
                    if(command.flag & X_IS_SET) set_pot_value(0, command.x < 0 ? 0 : command.x > 127 ? 127 : (unsigned)command.x);
                    if(command.flag & Y_IS_SET) set_pot_value(1, command.y < 0 ? 0 : command.y > 127 ? 127 : (unsigned)command.y);
                    if(command.flag & Z_IS_SET) set_pot_value(2, command.z < 0 ? 0 : command.z > 127 ? 127 : (unsigned)command.z);
                    if(command.flag & A_IS_SET) set_pot_value(3, command.a < 0 ? 0 : command.a > 127 ? 127 : (unsigned)command.a);
                    if(command.flag & B_IS_SET) set_pot_value(4, command.b < 0 ? 0 : command.b > 127 ? 127 : (unsigned)command.b);
                    break;
                    
                    // G161 - Home given axes to minimum
                case 161:
                    home_axes(ENDSTOP_IS_MIN);
                    positionKnown = 0;
                    excess.a = 0;
                    excess.b = 0;
                    break;
                    // G28 - Home given axes to maximum
                    // G162 - Home given axes to maximum
                case 28:
                case 162:
                    home_axes(ENDSTOP_IS_MAX);
                    positionKnown = 0;
                    excess.a = 0;
                    excess.b = 0;
                    break;
                default:
                    fprintf(stderr, "(line %u) Syntax Warning: unsupported gcode command 'G%u'" EOL, line_number, command.g);
            }
        }
        else if(command.flag & M_IS_SET) {
            command_line++;
            switch(command.m) {                    
                    // M2 - End program
                case 2:
                    if(program_is_running()) {
                        end_program();
                        set_build_percent(100);
                        end_build();
                        set_steppers(AXES_BIT_MASK, 0);
                    }
                    exit(0);
            
                    // M6 - Wait for extruder to reach (or exceed) temperature
                case 6: {
                    unsigned extruder_id = currentExtruder;
                    int timeout = command.flag & P_IS_SET ? (int)command.p : 0xFFFF;
                    if(command.flag & T_IS_SET) {
                        extruder_id = (unsigned)command.t;
                    }
                    if(extruder_id < machine.extruder_count) {
                        if(currentExtruder != extruder_id) {
                            currentExtruder = extruder_id;
                            change_extruder(extruder_id);
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Semantic Warning: M6 cannot select non-existant extruder T%u" EOL, line_number, extruder_id);
                        extruder_id = currentExtruder;
                    }
                    if(tool[currentExtruder].nozzle_temperature > 0.0) {
                        wait_for_extruder(currentExtruder, timeout);
                    }
                    // if we have a HBP wait for that too
                    if(machine.a.has_heated_build_platform && tool[0].build_platform_temperature > 0.0) {
                        wait_for_build_platform(0, timeout);
                    }
                    if(machine.b.has_heated_build_platform && tool[1].build_platform_temperature > 0.0) {
                        wait_for_build_platform(1, timeout);
                    }
                    break;
                }
                    
                    // M70 - Display message on LCD
                case 70:
                    if(command.flag & COMMENT_IS_SET) {
                        if(command.flag & P_IS_SET) {
                            display_message(command.comment, command.p, 0);
                        }
                        else {
                            display_message(command.comment, 0, 0);
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax Error: M70 is missing message text, use (text) where text is message" EOL, line_number);                        
                    }
                    break;

                    // M71 - Display message and wait for button press
                case 71:
                    if(command.flag & COMMENT_IS_SET) {
                        if(command.flag & P_IS_SET) {
                            display_message(command.comment, command.p, 0);
                        }
                        else {
                            display_message(command.comment, 0, 0);
                        }
                    }
                    else {
                        if(command.flag & P_IS_SET) {
                            display_message("Press M to continue", command.p, 0);
                        }
                        else {
                            display_message("Press M to continue", 0, 0);
                        }
                    }
                    break;
                    
                    // M72 - Queue a song or play a tone
                case 72:
                    if(command.flag & P_IS_SET) {
                        unsigned song_id = (unsigned)command.p;
                        if(song_id > 2) song_id = 2;
                        queue_song(song_id);
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax Warning: M72 is missing song number, use Pn where n is 0-2" EOL, line_number);
                    }
                    break;
                    
                    // M73 - Manual set build percentage
                case 73:
                    if(command.flag & P_IS_SET) {
                        unsigned percent = (unsigned) command.p;
                        if(percent > 100) percent = 100;
                        if(program_is_ready()) {
                            start_program();
                            start_build();
                            set_build_percent(0);
                        }
                        else if(program_is_running()) {
                            if(percent == 100) {
                                end_program();
                                set_build_percent(100);
                                end_build();
                            }
                            else if(filesize == 0 || build_percent == 0) {
                                set_build_percent(percent);
                            }
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax Warning: M73 is missing build percentage, use Pn where n is 0-100" EOL, line_number);
                    }
                    break;
                    
                    // M101 - Turn extruder on, forward
                    // M102 - Turn extruder on, reverse
                case 101:
                case 102:
                    if(command.flag & T_IS_SET) {
                        unsigned extruder_id = (unsigned)command.t;
                        if(extruder_id < machine.extruder_count) {
                            set_steppers(extruder_id == 0 ? A_IS_SET : B_IS_SET, 1);
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic Warning: M%u cannot select non-existant extruder T%u" EOL, line_number, command.m, extruder_id);
                        }

                    }
                    else {
                        set_steppers(currentExtruder == 0 ? A_IS_SET : B_IS_SET, 1);
                    }
                    break;
                    
                    // M103 - Turn extruder off
                case 103:
                    if(command.flag & T_IS_SET) {
                        unsigned extruder_id = (unsigned)command.t;
                        if(extruder_id < machine.extruder_count) {
                            set_steppers(extruder_id == 0 ? A_IS_SET : B_IS_SET, 0);
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic Warning: M103 cannot select non-existant extruder T%u" EOL, line_number, extruder_id);
                        }
                        
                    }
                    else {
                        set_steppers(currentExtruder == 0 ? A_IS_SET : B_IS_SET, 0);
                    }
                    break;
                    
                    // M104 - Set extruder temperature
                case 104:
                    if(command.flag & S_IS_SET) {
                        unsigned temperature = (unsigned)command.s;
                        if(temperature > 260) temperature = 260;
                        if(command.flag & T_IS_SET) {
                            unsigned extruder_id = (unsigned)command.t;
                            if(extruder_id < machine.extruder_count) {
                                set_extruder_temperature(extruder_id, temperature);
                                tool[extruder_id].nozzle_temperature = temperature;
                            }
                            else {
                                fprintf(stderr, "(line %u) Semantic Warning: M104 cannot select non-existant extruder T%u" EOL, line_number, extruder_id);
                            }
                        }
                        else {
                            set_extruder_temperature(currentExtruder, temperature);
                            tool[currentExtruder].nozzle_temperature = temperature;
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax Error: M104 is missing temperature, use Sn where n is 0-260" EOL, line_number);
                        exit(1);
                    }
                    break;
                    
                    // M106 - Turn cooling fan on
                case 106:
                    if(command.flag & T_IS_SET) {
                        unsigned extruder_id = (unsigned)command.t;
                        if(extruder_id < machine.extruder_count) {
                            set_fan(extruder_id, 1);
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic Warning: M106 cannot select non-existant extruder T%u" EOL, line_number, extruder_id);
                        }
                    }
                    else {
                        set_fan(currentExtruder, 1);
                    }
                    break;
                    
                    // M107 - Turn cooling fan off
                case 107:
                    if(command.flag & T_IS_SET) {
                        unsigned extruder_id = (unsigned)command.t;
                        if(extruder_id < machine.extruder_count) {
                            set_fan(extruder_id, 0);
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic Warning: M107 cannot select non-existant extruder T%u" EOL, line_number, extruder_id);
                        }
                    }
                    else {
                        set_fan(currentExtruder, 0);
                    }
                    break;
                    
                    // M109 - Set Build Platform Temperature
                    // M140 - Set Build Platform Temperature (skeinforge)
                case 109:
                case 140:
                    if(machine.a.has_heated_build_platform || machine.b.has_heated_build_platform) {
                        if(command.flag & S_IS_SET) {
                            unsigned extruder_id = machine.a.has_heated_build_platform ? 0 : 1;
                            unsigned temperature = (unsigned)command.s;
                            if(temperature > 160) temperature = 160;
                            if(command.flag & T_IS_SET) {
                                extruder_id = (unsigned)command.t;
                            }
                            if(extruder_id < machine.extruder_count && (extruder_id ? machine.b.has_heated_build_platform : machine.a.has_heated_build_platform)) {
                                set_build_platform_temperature(extruder_id, temperature);
                                tool[currentExtruder].build_platform_temperature = temperature;
                            }
                            else {
                                fprintf(stderr, "(line %u) Semantic Warning: M%u cannot select non-existant hbp extruder T%u" EOL, line_number, command.m, extruder_id);
                            }
                        }
                        else {
                            fprintf(stderr, "(line %u) Syntax Error: M%u is missing temperature, use Sn where n is 0-160" EOL, line_number, command.m);
                            exit(1);
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Semantic Warning: M%u cannot select non-existant heated build platform" EOL, line_number, command.m);                        
                    }
                    break;
                    
                    // M126 - Turn blower fan on (valve open)
                case 126:
                    if(command.flag & T_IS_SET) {
                        unsigned extruder_id = (unsigned)command.t;
                        if(extruder_id < machine.extruder_count) {
                            set_valve(extruder_id, 1);
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic Warning: M126 cannot select non-existant extruder T%u" EOL, line_number, extruder_id);
                        }
                    }
                    else {
                        set_valve(currentExtruder, 1);
                    }
                    break;

                    // M127 - Turn blower fan on (valve close)
                case 127:
                    if(command.flag & T_IS_SET) {
                        unsigned extruder_id = (unsigned)command.t;
                        if(extruder_id < machine.extruder_count) {
                            set_valve(extruder_id, 0);
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic Warning: M127 cannot select non-existant extruder T%u" EOL, line_number, extruder_id);
                        }
                    }
                    else {
                        set_valve(currentExtruder, 0);
                    }
                    break;

                    // M137 - Enable axes steppers
                case 137:
                    break;
                    
                    // M138 - Disable axes steppers
                case 138:
                    break;

                    // M146 - Set RGB LED value
                case 146:
                    break;
                    
                    // M147 - Set Beep
                case 147:
                    break;
                    
                    // M131 - Store Current Position to EEPROM
                case 131:
                    if(command.flag & AXES_BIT_MASK) {
                        store_home_positions();
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax Error: M131 is missing axes, use X Y Z A B" EOL, line_number);
                        exit(1);
                    }
                    break;
                    
                    // M132 - Load Current Position from EEPROM
                case 132:
                    if(command.flag & AXES_BIT_MASK) {
                        store_home_positions();
                        positionKnown = 0;
                        excess.a = 0;
                        excess.b = 0;
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax Error: M132 is missing axes, use X Y Z A B" EOL, line_number);
                        exit(1);
                    }
                    break;
                    
                    // M320 - Acceleration on for subsequent instructions
                case 320:
                    set_acceleration(1);
                    break;
                    
                    // M321 - Acceleration off for subsequent instructions
                case 321:
                    set_acceleration(0);
                    break;
                default:
                    fprintf(stderr, "(line %u) Syntax Warning: unsupported mcode command 'M%u'" EOL, line_number, command.m);
            }
        }
        else if(command.flag & T_IS_SET) {
            unsigned extruder_id = (unsigned)command.t;
            if(extruder_id < machine.extruder_count) {
                if(currentExtruder != extruder_id) {
                    currentExtruder = extruder_id;
                    change_extruder(extruder_id);
                    command_line++;
                }
            }
            else {
                fprintf(stderr, "(line %u) Semantic Warning: T%u cannot select non-existant extruder" EOL, line_number, extruder_id);
            }
        }
        else if(command.flag & AXES_BIT_MASK) {
            command_line++;
            queue_point(currentFeedrate);
        }
        // update progress
        if(filesize && build_percent && command_line) {
            unsigned percent = (unsigned)round(100.0 * (double)ftell(in) / (double)filesize);
            if(percent > progress) {
                if(program_is_ready()) {
                    start_program();
                    start_build();
                    set_build_percent(0);
                }
                else if(percent < 100) {
                    set_build_percent(percent);
                    progress = percent;
                }
                command_line = 0;
            }
        }
        line_number = next_line;
    }
    
    if(program_is_running()) {
        end_program();
        set_build_percent(100);
        end_build();
    }
    set_steppers(AXES_BIT_MASK, 0);
    
    exit(0);
}

