//
//  gpx.c
//
//  Created by WHPThomas on 1/04/13.
//
//  Copyright (c) 2013 WHPThomas.
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
    2,  // tool count
    20, // timeout
};

static Machine replicator_2 = {
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 0}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    1,  // tool count
    20, // timeout
};

static Machine replicator_2X = {
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    2,  // tool count
    20, // timeout
};

// The default machine definition is the Replicator 2

Machine machine = {
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // y axis
    {1170, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 0}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    1,  // tool count
    20, // timeout
};

// PRIVATE FUNCTION PROTOTYPES

static double get_home_feedrate(int flag);

// GLOBAL VARIABLES

Command command;            // command line
Point5d currentPosition;    // current point
Point3d machineTarget;      // machine target point
Point5d workTarget;         // work target point
Point2d excess;
int currentOffset;          // current G10 offset
Point3d offset[7];          // G10 offsets
int currentTool;
unsigned temperature[4];
int isRelative;
int positionKnown;
int programState;
unsigned line_number;
static char buffer[256];

FILE *in;
FILE *out;

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
    command.f = get_home_feedrate(XYZ_BIT_MASK);
    command.l = 0.0;
    command.p = 0.0;
    command.q = 0.0;
    command.r = 0.0;
    command.s = 0.0;
    
    command.comment = "";
    
    excess.a = 0.0;
    excess.b = 0.0;
    
    currentOffset = 0;
    
    for(i = 0; i < 7; i++) {
        offset[i].x = 0.0;
        offset[i].y = 0.0;
        offset[i].z = 0.0;
    }
    
    currentTool = 0;
    
    for(i = 0; i < 4; i++) {
        temperature[i] = 0;
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

// PRIVATE FUNCTIONS

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

static double get_max_feedrate(int flag)
{
    double feedrate;
    if(flag & F_IS_SET) {
        feedrate = command.f;
    }
    else {
        feedrate = 0.0;
        if(flag & X_IS_SET && feedrate < machine.x.max_feedrate) {
            feedrate = machine.x.home_feedrate;
        }
        if(flag & Y_IS_SET && feedrate < machine.y.max_feedrate) {
            feedrate = machine.y.home_feedrate;
        }
        if(flag & Z_IS_SET && feedrate < machine.z.max_feedrate) {
            feedrate = machine.z.home_feedrate;
        }
    }
    return feedrate;
}

static double get_home_feedrate(int flag) {
    double feedrate;
    if(flag & F_IS_SET) {
        feedrate = command.f;
    }
    else {
        feedrate = 0.0;
        if(flag & X_IS_SET && feedrate < machine.x.home_feedrate) {
            feedrate = machine.x.home_feedrate;
        }
        if(flag & Y_IS_SET && feedrate < machine.y.home_feedrate) {
            feedrate = machine.y.home_feedrate;
        }
        if(flag & Z_IS_SET && feedrate < machine.z.home_feedrate) {
            feedrate = machine.z.home_feedrate;
        }
    }
    return feedrate;
}

static Point5d mm_to_steps(Ptr5d mm, Ptr2d excess)
{
    Point5d result;
    result.x = round(mm->x * machine.x.steps_per_mm);
    result.y = round(mm->y * machine.y.steps_per_mm);
    result.z = round(mm->z * machine.z.steps_per_mm);
    if(excess) {
        result.a = round((mm->a * machine.a.steps_per_mm) + excess->a);
        result.b = round((mm->b * machine.b.steps_per_mm) + excess->b);
    }
    else {
        result.a = round(mm->a * machine.a.steps_per_mm);
        result.b = round(mm->b * machine.b.steps_per_mm);        
    }
    return result;
}

static unsigned feedrate_to_microseconds(int flag, Ptr5d origin, Ptr5d vector, double feedrate) {
    Point5d deltaMM;
    Point5d deltaSteps;
    double longestStep = 0.0;
    // feedrate is in mm/min
    if(flag & X_IS_SET) {
        deltaMM.x = fabs(vector->x - origin->x);
        deltaSteps.x = deltaMM.x * machine.x.steps_per_mm;
        longestStep = deltaSteps.x;
    }
    
    if(flag & Y_IS_SET) {
        deltaMM.y = fabs(vector->y - origin->y);
        deltaSteps.y = deltaMM.y * machine.y.steps_per_mm;
        if(longestStep < deltaSteps.y) {
            longestStep = deltaSteps.y;
        }
    }
    
    if(flag & Z_IS_SET) {
        deltaMM.z = fabs(vector->z - origin->z);
        deltaSteps.z = deltaMM.z * machine.z.steps_per_mm;
        if(longestStep < deltaSteps.z) {
            longestStep = deltaSteps.z;
        }
    }
    
    if(flag & A_IS_SET) {
        deltaMM.a = fabs(vector->a - origin->a);
        deltaSteps.a = deltaMM.a * machine.a.steps_per_mm;
        if(longestStep < deltaSteps.a) {
            longestStep = deltaSteps.a;
        }
    }
    
    if(flag & B_IS_SET) {
        deltaMM.b = fabs(vector->b - origin->b);
        deltaSteps.b = deltaMM.b * machine.b.steps_per_mm;
        if(longestStep < deltaSteps.b) {
            longestStep = deltaSteps.b;
        }
    }
    // distance is in mm
    double distance = magnitude(flag, &deltaMM);
    // move duration in microseconds = distance / feedrate * 60,000,000
    double microseconds = distance / feedrate * 60000000.0;
    // time between steps for longest axis = microseconds / longestStep
    double step_delay = microseconds / longestStep;
    return (unsigned)round(step_delay);
}

// X3G COMMANDS

// 131 - Find axes minimums
// 132 - Find axes maximums

static void home_axes(unsigned direction)
{
    Point3d origin, vector;
    int xyz_flag = command.flag & XYZ_BIT_MASK;
    double feedrate = get_home_feedrate(command.flag);
    assert(direction <= 1);
    // compute the slowest feedrate
    if(xyz_flag & X_IS_SET) {
        if(machine.x.home_feedrate < feedrate) {
            feedrate = machine.x.home_feedrate;
        }
        origin.x = 0;
        vector.x = 1;
        // confirm machine compatibility
        if(direction != machine.x.endstop) {
            fprintf(stderr, "(line %u) Semantic Warning: X axis homing to %s endstop", line_number, direction ? "maximum" : "minimum");
        }
    }
    if(xyz_flag & Y_IS_SET) {
        if(machine.y.home_feedrate < feedrate) {
            feedrate = machine.y.home_feedrate;
        }
        origin.y = 0;
        vector.y = 1;
        if(direction != machine.y.endstop) {
            fprintf(stderr, "(line %u) Semantic Warning: Y axis homing to %s endstop", line_number, direction ? "maximum" : "minimum");
        }
    }
    if(xyz_flag & Z_IS_SET) {
        if(machine.z.home_feedrate < feedrate) {
            feedrate = machine.z.home_feedrate;
        }
        origin.z = 0;
        vector.z = 1;
        if(direction != machine.z.endstop) {
            fprintf(stderr, "(line %u) Semantic Warning: Z axis homing to %s endstop", line_number, direction ? "maximum" : "minimum");
        }
    }
    unsigned microseconds = feedrate_to_microseconds(xyz_flag, (Ptr5d)&origin, (Ptr5d)&vector, feedrate);
    
    if(write_8(direction == ENDSTOP_IS_MIN ? 131 :132) == EOF) exit(1);
    
    // uint8: Axes bitfield. Axes whose bits are set will be moved.
    if(write_8(xyz_flag) == EOF) exit(1);
    
    // uint32: Feedrate, in microseconds between steps on the max delta. (DDA)
    if(write_32(microseconds) == EOF) exit(1);
    
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

// 134 - Change Tool
static void change_tool(unsigned tool_id)
{
    assert(tool_id < machine.tool_count);
    if(write_8(134) == EOF) exit(1);
    
    // uint8: Tool ID of the tool to switch to
    if(write_8(tool_id) == EOF) exit(1);
}

// 135 - Wait for tool ready

static void wait_for_tool(unsigned tool_id, unsigned timeout)
{
    assert(tool_id < machine.tool_count);
    if(write_8(135) == EOF) exit(1);
    
    // uint8: Tool ID of the tool to wait for
    if(write_8(tool_id) == EOF) exit(1);
    
    // uint16: delay between query packets sent to the tool, in ms (nominally 100 ms)
    if(write_16(100) == EOF) exit(1);
    
    // uint16: Timeout before continuing without tool ready, in seconds (nominally 1 minute)
    if(write_16(timeout) == EOF) exit(1);
}
 
// 136 - Tool action command

static void set_extruder_temperature(unsigned tool_id, unsigned temperature)
{
    assert(tool_id < machine.tool_count);
    if(write_8(136) == EOF) exit(1);
    
    // uint8: Tool ID of the tool to query
    if(write_8(tool_id) == EOF) exit(1);
    
    // uint8: Action command to send to the tool
    if(write_8(3) == EOF) exit(1);
    
    // uint8: Length of the tool command payload (N)
    if(write_8(2) == EOF) exit(1);
    
    // int16: Desired target temperature, in Celsius
    if(write_16(temperature) == EOF) exit(1);
}

static void set_fan(unsigned tool_id, unsigned state)
{
    assert(tool_id < machine.tool_count);
    if(write_8(136) == EOF) exit(1);
    
    // uint8: Tool ID of the tool to query
    if(write_8(tool_id) == EOF) exit(1);
    
    // uint8: Action command to send to the tool
    if(write_8(12) == EOF) exit(1);
    
    // uint8: Length of the tool command payload (N)
    if(write_8(1) == EOF) exit(1);
    
    // uint8: 1 to enable, 0 to disable
    if(write_8(state) == EOF) exit(1);
}

static void set_valve(unsigned tool_id, unsigned state)
{
    assert(tool_id < machine.tool_count);
    if(write_8(136) == EOF) exit(1);
    
    // uint8: Tool ID of the tool to query
    if(write_8(tool_id) == EOF) exit(1);
    
    // uint8: Action command to send to the tool
    if(write_8(13) == EOF) exit(1);
    
    // uint8: Length of the tool command payload (N)
    if(write_8(1) == EOF) exit(1);
    
    // uint8: 1 to enable, 0 to disable
    if(write_8(state) == EOF) exit(1);
}

static void set_build_platform_temperature(unsigned tool_id, unsigned temperature)
{
    assert(tool_id < machine.tool_count);
    if(write_8(136) == EOF) exit(1);
    
    // uint8: Tool ID of the tool to query
    if(write_8(tool_id) == EOF) exit(1);
    
    // uint8: Action command to send to the tool
    if(write_8(31) == EOF) exit(1);
    
    // uint8: Length of the tool command payload (N)
    if(write_8(2) == EOF) exit(1);
    
    // int16: Desired target temperature, in Celsius
    if(write_16(temperature) == EOF) exit(1);
}

// 137 - Enable/disable axes

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

// 141 - Wait for platform ready

static void wait_for_platform(unsigned tool_id, int timeout)
{
    assert(tool_id < machine.tool_count);
    if(write_8(141) == EOF) exit(1);
    
    // uint8: Tool ID of the tool to wait for
    if(write_8(tool_id) == EOF) exit(1);
    
    // uint16: delay between query packets sent to the tool, in ms (nominally 100 ms)
    if(write_16(100) == EOF) exit(1);
    
    // uint16: Timeout before continuing without tool ready, in seconds (nominally 1 minute)
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
    unsigned hPos = command.flag & Q_IS_SET ? (unsigned)command.q : 0;
    unsigned vPos = command.flag & L_IS_SET ? (unsigned)command.l : 0;
    long length = strlen(message);
    if(hPos > 19) hPos = 19;
    if(vPos > 3) vPos = 3;
    
    while(bytesSent < length) {
        if(bytesSent + 20 > length) {
            seconds = timeout;
            bitfield |= 0x02; // last message in group
            if(wait_for_button) {
                bitfield |= 0x04;
            }
        }
        if(bytesSent > 0) {
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
        if(write_8(timeout) == EOF) exit(1);
        // 1+N bytes: Message to write to the screen, in ASCII, terminated with a null character.
        bytesSent += fwrite(message + bytesSent, 1, length > 20 ? 20 : length, out);
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

// song ID 0: error tone with 4 cycles
// song ID 1: done tone
// song ID 2: error tone with 2 cycles

static void queue_song(unsigned song_id)
{
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
// int32: X coordinate, in steps
// int32: Y coordinate, in steps
// int32: Z coordinate, in steps
// int32: A coordinate, in steps
// int32: B coordinate, in steps
// uint32: DDA Feedrate, in steps/s
// uint8: Axes bitfield to specify which axes are relative. Any axis with a bit set should make a relative movement.
// float (single precision, 32 bit): mm distance for this move.  normal of XYZ if any of these axes are active, and AB for extruder only moves
// uint16: feedrate in mm/s, multiplied by 64 to assist fixed point calculation on the bot
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
    puts("Usage: gpx [-m <MACHINE> | -c <CONFIG>] INPUT [OUTPUT]");
    puts("\nMACHINE is the predefined machine type");
    puts("\n\t r|r1  = Replicator 1");
    puts("\tr2 = Replicator 2 (default)");
    puts("\tr2x = Replicator 2X");
    puts("\nCONFIG is the filename of a custom machine definition (ini)");
    puts("\nINPUT is the name of the sliced gcode input filename");
    puts("\nOUTPUT is the name of the x3g output filename");
    puts("\nCopyright (c) 2013 WHPThomas, All rights reserved.");

    exit(1);
}

int main(int argc, char * argv[])
{
    long filesize = 0;
    unsigned progress = 0;
    int c, i;

    initialize_globals();

    // READ COMMAND LINE
    
    // get the command line options
    while ((c = getopt(argc, argv, "om:c:")) != -1) {
        switch (c) {
            case 'm':
                if(strcasecmp(optarg, "r") == 0
                   || strcasecmp(optarg, "r1") == 0) {
                    machine = replicator_1;
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
            case 'c':
                /*
                TODO
                if(!get_custom_definition(&machine, optarg)) {
                    usage();
                };
                break;
                */
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
                fprintf(stderr, "(line %u) Syntax Error: line number command word 'N' is missing digits", line_number);
                exit(1);
            }
            line_number = atoi(digits);
        }
        else {
            line_number++;
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
                        // Tnnn	 Select tool nnn. In RepRap, tools are extruders
                    case 't':
                    case 'T':
                        command.t = atoi(digits);
                        command.flag |= T_IS_SET;
                        break;
                    
                    default:
                        fprintf(stderr, "(line %u) Syntax Warning: unrecognised command word '%c'", line_number, c);
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
                    fprintf(stderr, "(line %u) Syntax Warning: comment is missing closing ')'", line_number);
                    command.comment = normalize_comment(p + 1);
                    command.flag |= COMMENT_IS_SET;
                    *p = 0;                   
                }
            }
            else if(*p == '*') {
                // Checksum
                *p = 0;
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
        
        if(command.flag & E_IS_SET) {
            if(currentTool == 0) {
                // a = e
                workTarget.a = isRelative ? (currentPosition.a + command.e) : command.e;
                
                // b
                if(command.flag & B_IS_SET) {
                    workTarget.b = isRelative ? (currentPosition.b + command.b) : command.b;
                }
                else {
                    workTarget.b = currentPosition.b;
                }
            }
            else {                
                // a
                if(command.flag & A_IS_SET) {
                    workTarget.a = isRelative ? (currentPosition.a + command.a) : command.a;
                }
                else {
                    workTarget.a = currentPosition.a;
                }
                
                // b = e
                workTarget.b = isRelative ? (currentPosition.b + command.e) : command.e;
            }
        }
        else {        
            // a
            if(command.flag & A_IS_SET) {
                workTarget.a = isRelative ? (currentPosition.a + command.a) : command.a;
            }
            else {
                workTarget.a = currentPosition.a;
            }
            // b
            if(command.flag & B_IS_SET) {
                workTarget.b = isRelative ? (currentPosition.b + command.b) : command.b;
            }
            else {
                workTarget.b = currentPosition.b;
            }
       }
        
        // CALCULATE OFFSET
        
        workTarget.x = machineTarget.x + offset[currentOffset].x;
        workTarget.y = machineTarget.y + offset[currentOffset].y;
        workTarget.z = machineTarget.z + offset[currentOffset].z;

        // INTERPRET COMMAND
        
        if(command.flag & G_IS_SET) {
            switch(command.g) {
                    // G0 - Rapid Positioning
                case 0:
                    if(command.flag & F_IS_SET) {
                        queue_point(command.f);
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
                    queue_point(command.f);
                    break;
                    
                    // G2 - Clockwise Arc
                    // G3 - Counter Clockwise Arc
                    
                    // G4 - Dwell
                case 4:
                    if(command.flag & P_IS_SET) {
                        delay(command.p);
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax Error: G4 is missing delay parameter, use Pn where n is milliseconds", line_number);
                        exit(1);                        
                    }
                    break;

                    // G10 - Create Coordinate System Offset from the Absolute one
                case 10:
                    if(command.flag & P_IS_SET && command.p >= 1.0 && command.p <= 6.0) {
                        i = (int)command.p - 1;
                        if(command.flag & X_IS_SET) offset[i].x = machineTarget.x;
                        if(command.flag & Y_IS_SET) offset[i].y = machineTarget.y;
                        if(command.flag & Z_IS_SET) offset[i].z = machineTarget.z;
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax Error: G10 is missing coordiante system, use Pn where n is 1-6", line_number);
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
                        fprintf(stderr, "(line %u) Semantic Error: G91 switch to relitive positioning prior to first absolute move", line_number);
                        exit(1);
                    }
                    break;

                    // G92 - Define current position on axes
                case 92:
                    if(command.flag & X_IS_SET) currentPosition.x = machineTarget.x;
                    if(command.flag & Y_IS_SET) currentPosition.y = machineTarget.y;
                    if(command.flag & Z_IS_SET) currentPosition.z = machineTarget.z;
                    if(command.flag & A_IS_SET) currentPosition.a = workTarget.a;
                    if(command.flag & B_IS_SET) currentPosition.b = workTarget.b;

                    if(command.flag & E_IS_SET) {
                        if(currentTool == 0) {
                            currentPosition.a = workTarget.a;
                        }
                        else {
                            currentPosition.b = workTarget.b;
                        }
                    }
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
                    break;
                    // G28 - Home given axes to maximum
                    // G162 - Home given axes to maximum
                case 28:
                case 162:
                    home_axes(ENDSTOP_IS_MAX);
                    positionKnown = 0;
                    break;
                default:
                    fprintf(stderr, "(line %u) Syntax Warning: unsupported gcode command 'G%u'", line_number, command.g);
            }
        }
        else if(command.flag & M_IS_SET) {
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
            
                    // M6 - Wait for toolhead to come up to reach (or exceed) temperature
                case 6:
                    if(command.flag & T_IS_SET) {
                        int timeout = command.flag & P_IS_SET ? (int)command.p : 0xFFFF;
                        unsigned tool_id = (unsigned)command.t;
                        if(tool_id < machine.tool_count) {
                            if(currentTool != tool_id) {
                                currentTool = tool_id;
                                change_tool(tool_id);
                            }
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic Warning: M6 cannot select non-existant tool T%u", line_number, tool_id);
                            tool_id = currentTool;
                        }
                        if(temperature[currentTool] > 0.0) {
                            wait_for_tool(tool_id, timeout);
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax Error: M6 is missing tool number, use Tn where n is 0-1", line_number);
                        exit(1);
                    }
                    break;
                    
                    // M70 - Display Message On Machine
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
                        fprintf(stderr, "(line %u) Syntax Error: M70 is missing message text, use (text) where text is message", line_number);                        
                    }
                    break;

                    // M71 - Display Message, Wait For User Button Press
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
                    
                    // M72 - Play a Tone or Song
                case 72:
                    if(command.flag & P_IS_SET) {
                        unsigned song_id = (unsigned)command.p;
                        if(song_id > 2) song_id = 2;
                        queue_song(song_id);
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax Warning: M72 is missing song number, use Pn where n is 0-2", line_number);
                    }
                    break;
                    
                    // M73 - Manual Set Build %
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
                            else if(filesize == 0) {
                                set_build_percent(percent);
                            }
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax Warning: M73 is missing build percentage, use Pn where n is 0-100", line_number);
                    }
                    break;
                    
                    // M101 - Turn Extruder On, Forward
                    // M102 - Turn Extruder On, Reverse
                case 101:
                case 102:
                    if(command.flag & T_IS_SET) {
                        unsigned tool_id = (unsigned)command.t;
                        if(tool_id < machine.tool_count) {
                            set_steppers(tool_id == 0 ? A_IS_SET : B_IS_SET, 1);
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic Warning: M%u cannot select non-existant tool T%u", line_number, command.m, tool_id);
                        }

                    }
                    else {
                        set_steppers(currentTool == 0 ? A_IS_SET : B_IS_SET, 1);
                    }
                    break;
                    
                    // M103 - Turn Extruder Off
                case 103:
                    if(command.flag & T_IS_SET) {
                        unsigned tool_id = (unsigned)command.t;
                        if(tool_id < machine.tool_count) {
                            set_steppers(tool_id == 0 ? A_IS_SET : B_IS_SET, 0);
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic Warning: M103 cannot select non-existant tool T%u", line_number, tool_id);
                        }
                        
                    }
                    else {
                        set_steppers(currentTool == 0 ? A_IS_SET : B_IS_SET, 0);
                    }
                    break;
                    
                    // M104 - Set extruder temperature
                case 104:
                    if(command.flag & S_IS_SET) {
                        unsigned temp = (unsigned)command.t;
                        if(temp > 260) temp = 260;
                        if(command.flag & T_IS_SET) {
                            unsigned tool_id = (unsigned)command.t;
                            if(tool_id < machine.tool_count) {
                                set_extruder_temperature(tool_id, temp);
                                temperature[tool_id] = temp;
                            }
                            else {
                                fprintf(stderr, "(line %u) Semantic Warning: M104 cannot select non-existant tool T%u", line_number, tool_id);
                            }
                        }
                        else {
                            set_extruder_temperature(currentTool, temp);
                            temperature[currentTool] = temp;
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax Error: M104 is missing temperature, use Sn where n is 0-260", line_number);
                        exit(1);
                    }
                    break;
                    
                    // M106 - Turn cooling fan on
                case 106:
                    if(command.flag & T_IS_SET) {
                        unsigned tool_id = (unsigned)command.t;
                        if(tool_id < machine.tool_count) {
                            set_fan(tool_id, 1);
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic Warning: M106 cannot select non-existant tool T%u", line_number, tool_id);
                        }
                    }
                    else {
                        set_fan(currentTool, 1);
                    }
                    break;
                    
                    // M107 - Turn cooling fan off
                case 107:
                    if(command.flag & T_IS_SET) {
                        unsigned tool_id = (unsigned)command.t;
                        if(tool_id < machine.tool_count) {
                            set_fan(tool_id, 0);
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic Warning: M107 cannot select non-existant tool T%u", line_number, tool_id);
                        }
                    }
                    else {
                        set_fan(currentTool, 0);
                    }
                    break;
                    
                    // M109 - Set Build Platform Temperature
                    // M140 - Set Build Platform Temperature (skeinforge)
                case 109:
                case 140:
                    if(machine.a.has_heated_build_platform || machine.b.has_heated_build_platform) {
                        if(command.flag & S_IS_SET) {
                            unsigned tool_id = machine.a.has_heated_build_platform ? 0 : 1;
                            unsigned temp = (unsigned)command.t;
                            if(temp > 160) temp = 160;
                            if(command.flag & T_IS_SET) {
                                tool_id = (unsigned)command.t;
                            }
                            if(tool_id < machine.tool_count && (tool_id ? machine.b.has_heated_build_platform : machine.a.has_heated_build_platform)) {
                                set_build_platform_temperature(tool_id, temp);
                                temperature[tool_id + 2] = temp;
                            }
                            else {
                                fprintf(stderr, "(line %u) Semantic Warning: M%u cannot select non-existant hbp tool T%u", line_number, command.m, tool_id);
                            }
                        }
                        else {
                            fprintf(stderr, "(line %u) Syntax Error: M%u is missing temperature, use Sn where n is 0-160", line_number, command.m);
                            exit(1);
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Semantic Warning: M%u cannot select non-existant heated build platform", line_number, command.m);                        
                    }
                    break;
                    
                    // M126 - Turn blower fan on (valve open)
                case 126:
                    if(command.flag & T_IS_SET) {
                        unsigned tool_id = (unsigned)command.t;
                        if(tool_id < machine.tool_count) {
                            set_valve(tool_id, 1);
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic Warning: M126 cannot select non-existant tool T%u", line_number, tool_id);
                        }
                    }
                    else {
                        set_valve(currentTool, 1);
                    }
                    break;

                    // M127 - Turn blower fan on (valve close)
                case 127:
                    if(command.flag & T_IS_SET) {
                        unsigned tool_id = (unsigned)command.t;
                        if(tool_id < machine.tool_count) {
                            set_valve(tool_id, 0);
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic Warning: M127 cannot select non-existant tool T%u", line_number, tool_id);
                        }
                    }
                    else {
                        set_valve(currentTool, 0);
                    }
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
                        fprintf(stderr, "(line %u) Syntax Error: M131 is missing axes, use X Y Z A B", line_number);
                        exit(1);
                    }
                    break;
                    
                    // M132 - Load Current Position from EEPROM
                case 132:
                    if(command.flag & AXES_BIT_MASK) {
                        store_home_positions();
                        positionKnown = 0;
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax Error: M132 is missing axes, use X Y Z A B", line_number);
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
                    fprintf(stderr, "(line %u) Syntax Warning: unsupported mcode command 'M%u'", line_number, command.m);
            }
        }
        else if(command.flag & T_IS_SET) {
            unsigned tool_id = (unsigned)command.t;
            if(tool_id < machine.tool_count) {
                if(currentTool != tool_id) {
                    currentTool = tool_id;
                    change_tool(tool_id);
                }
            }
            else {
                fprintf(stderr, "(line %u) Semantic Warning: T%u cannot select non-existant tool", line_number, tool_id);
            }
        }
        else if(command.flag & PARAMETER_BIT_MASK) {
            queue_point(command.f);
        }
        // update progress
        if(filesize) {
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
            }
        }
    }
    
    if(program_is_running()) {
        end_program();
        set_build_percent(100);
        end_build();
    }
    set_steppers(AXES_BIT_MASK, 0);
    
    exit(0);
}

