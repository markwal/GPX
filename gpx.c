//
//  gpx.c
//
//  Created by WHPThomas <me(at)henri(dot)net> on 1/04/13.
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

#ifdef _WIN32
#   include "getopt.h"
#else
#   include <unistd.h>
#endif

#include "gpx.h"
#include "ini.h"

#define A 0
#define B 1

// Machine definitions

//  Axis - max_feedrate, home_feedrate, steps_per_mm, endstop;
//  Extruder - max_feedrate, steps_per_mm, motor_steps, has_heated_build_platform;

static Machine cupcake_G3 = {
    {9600, 500, 11.767463, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 11.767463, ENDSTOP_IS_MIN}, // y axis
    {450, 450, 320, ENDSTOP_IS_MIN},        // z axis
    {7200, 50.235478806907409, 400, 1}, // a extruder
    {7200, 50.235478806907409, 400, 0}, // b extruder
    1.75, // nominal filament diameter
    1,  // extruder count
    20, // timeout
};

static Machine cupcake_G4 = {
    {9600, 500, 47.069852, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 47.069852, ENDSTOP_IS_MIN}, // y axis
    {450, 450, 1280, ENDSTOP_IS_MIN},        // z axis
    {7200, 50.235478806907409, 400, 1}, // a extruder
    {7200, 50.235478806907409, 400, 0}, // b extruder
    1.75, // nominal filament diameter
    1,  // extruder count
    20, // timeout
};

static Machine cupcake_P4 = {
    {9600, 500, 94.13970462, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 94.13970462, ENDSTOP_IS_MIN}, // y axis
    {450, 450, 2560, ENDSTOP_IS_MIN},        // z axis
    {7200, 50.235478806907409, 400, 1}, // a extruder
    {7200, 50.235478806907409, 400, 0}, // b extruder
    1.75, // nominal filament diameter
    1,  // extruder count
    20, // timeout
};

static Machine cupcake_PP = {
    {9600, 500, 47.069852, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 47.069852, ENDSTOP_IS_MIN}, // y axis
    {450, 450, 1280, ENDSTOP_IS_MIN},        // z axis
    {7200, 100.470957613814818, 400, 1}, // a extruder
    {7200, 100.470957613814818, 400, 0}, // b extruder
    1.75, // nominal filament diameter
    1,  // extruder count
    20, // timeout
};

//  Axis - max_feedrate, home_feedrate, steps_per_mm, endstop;
//  Extruder - max_feedrate, steps_per_mm, motor_steps, has_heated_build_platform;

static Machine thing_o_matic_7 = {
    {9600, 500, 47.069852, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 47.069852, ENDSTOP_IS_MIN}, // y axis
    {1000, 500, 200, ENDSTOP_IS_MAX},        // z axis
    {1600, 50.235478806907409, 1600, 1}, // a extruder
    {1600, 50.235478806907409, 1600, 0}, // b extruder
    1.75, // nominal filament diameter
    1,  // extruder count
    20, // timeout
};

static Machine thing_o_matic_7D = {
    {9600, 500, 47.069852, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 47.069852, ENDSTOP_IS_MIN}, // y axis
    {1000, 500, 200, ENDSTOP_IS_MAX},        // z axis
    {1600, 50.235478806907409, 1600, 0}, // a extruder
    {1600, 50.235478806907409, 1600, 1}, // b extruder
    1.75, // nominal filament diameter
    2,  // extruder count
    20, // timeout
};


//  Axis - max_feedrate, home_feedrate, steps_per_mm, endstop;
//  Extruder - max_feedrate, steps_per_mm, motor_steps, has_heated_build_platform;

static Machine replicator_1 = {
    {18000, 2500, 94.139704, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 94.139704, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    1.75, // nominal filament diameter
    1,  // extruder count
    20, // timeout
};

static Machine replicator_1D = {
    {18000, 2500, 94.139704, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 94.139704, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    1.75, // nominal filament diameter
    2,  // extruder count
    20, // timeout
};

//  Axis - max_feedrate, home_feedrate, steps_per_mm, endstop;
//  Extruder - max_feedrate, steps_per_mm, motor_steps, has_heated_build_platform;

static Machine replicator_2 = {
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 0}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    1.75, // nominal filament diameter
    1,  // extruder count
    20, // timeout
};

static Machine replicator_2X = {
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 1}, // b extruder
    1.75, // nominal filament diameter
    2,  // extruder count
    20, // timeout
};

// The default machine definition is the Replicator 2

Machine machine = {
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 0}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    1.75, // nominal filament diameter
    1,  // extruder count
    20, // timeout
};

// PRIVATE FUNCTION PROTOTYPES

static double get_home_feedrate(int flag);
static void pause_at_zpos(float z_positon);

// GLOBAL VARIABLES

Command command;            // the gcode command line
Point5d currentPosition;    // the current position of the extruder in 5D space
Point5d targetPosition;     // the target poaition the extruder will move to (including G10 offsets)
Point2d excess;             // the accumulated rounding error in mm to step conversion
int selectedExtruder;       // the current extruder selection (on the virtual tool carosel)
int currentExtruder;        // the currently selectd extruder being used by the bot
double currentFeedrate;     // the current feed rate
int currentOffset;          // current G10 offset
Point3d offset[7];          // G10 offsets
Point3d userOffset;         // command line offset
Tool tool[2];               // tool state
Override override[2];       // gcode override
int isRelative;             // signals relitive or absolute coordinates
int extruderIsRelative;     // signals relitive or absolute coordinates for extruder
int positionKnown;          // is the current extruder position known
int programState;           // gcode program state used to trigger start and end code sequences
int dittoPrinting;          // enable ditto printing
int buildProgress;          // override build percent
int verboseMode;
unsigned lineNumber;        // the current line number in the gcode file
static char buffer[BUFFER_MAX + 1];    // the statically allocated parse-in-place buffer

Filament filament[FILAMENT_MAX];
int filamentLength;

CommandAt commandAt[COMMAND_AT_MAX];
int commandAtIndex;
int commandAtLength;

int macroStarted;           // ;@body macro encountered
int pausePending;           // signals a pause is pending before the macro script has started

FILE *in;                   // the gcode input file stream
FILE *out;                  // the x3g output file stream
FILE *out2;                 // secondary output path
char *sdCardPath;

// cleanup code in case we encounter an error that causes the program to exit

static void exit_handler(void)
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
        if(out2) {
            fclose(out2);
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
    out2 = NULL;
    sdCardPath = NULL;
    
    // register cleanup function
    atexit(exit_handler);
    
    command.flag = 0;
    
    // initialize current position to zero
    
    currentPosition.x = 0.0;
    currentPosition.y = 0.0;
    currentPosition.z = 0.0;
    
    currentPosition.a = 0.0;
    currentPosition.b = 0.0;
    
    command.e = 0.0;
    command.f = 0.0;
    command.p = 0.0;
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

    userOffset.x = 0.0;
    userOffset.y = 0.0;
    userOffset.z = 0.0;
    
    selectedExtruder = 0;
    currentExtruder = 0;
    
    for(i = 0; i < 2; i++) {
        tool[i].motor_enabled = 0;
#if ENABLE_SIMULATED_RPM
        tool[i].rpm = 0;
#endif
        tool[i].nozzle_temperature = 0;
        tool[i].build_platform_temperature = 0;
        
        override[i].actual_filament_diameter = 0;
        override[i].filament_scale = 1.0;
        override[i].standby_temperature = 0;
        override[i].active_temperature = 0;
        override[i].build_platform_temperature = 0;
    }

    isRelative = 0;
    extruderIsRelative = 0;
    positionKnown = 0;
    programState = 0;

    dittoPrinting = 0;
    buildProgress = 0;
    verboseMode = 0;
    
    lineNumber = 1;
    
    filament[0].colour = "_null_";
    filament[0].diameter = 0.0;
    filament[0].temperature = 0;
    filament[0].LED = 0;
    filamentLength = 1;

    commandAtIndex = 0;
    commandAtLength = 0;
    macroStarted = 0;
    pausePending = 0;
}

// STATE

#define start_program() programState = RUNNING_STATE
#define end_program() programState = ENDED_STATE

#define program_is_ready() programState < RUNNING_STATE
#define program_is_running() programState < ENDED_STATE

// IO FUNCTIONS

static void write_8(unsigned char value)
{
    if(fputc(value, out) == EOF) exit(1);
    
    if(out2) {
        if(fputc(value, out2) == EOF) exit(1);
    }
}

static void write_16(unsigned short value)
{
    union {
        unsigned short s;
        unsigned char b[2];
    } u;
    u.s = value;
    
    if(fputc(u.b[0], out) == EOF) exit(1);
    if(fputc(u.b[1], out) == EOF) exit(1);
    
    if(out2) {
        if(fputc(u.b[0], out2) == EOF) exit(1);
        if(fputc(u.b[1], out2) == EOF) exit(1);
    }
}

static void write_32(unsigned int value)
{
    union {
        unsigned int i;
        unsigned char b[4];
    } u;
    u.i = value;
    
    if(fputc(u.b[0], out) == EOF) exit(1);
    if(fputc(u.b[1], out) == EOF) exit(1);
    if(fputc(u.b[2], out) == EOF) exit(1);
    if(fputc(u.b[3], out) == EOF) exit(1);
    
    if(out2) {
        if(fputc(u.b[0], out2) == EOF) exit(1);
        if(fputc(u.b[1], out2) == EOF) exit(1);
        if(fputc(u.b[2], out2) == EOF) exit(1);
        if(fputc(u.b[3], out2) == EOF) exit(1);
    }
}

static void write_float(float value)
{
    union {
        float f;
        unsigned char b[4];
    } u;
    u.f = value;
    
    if(fputc(u.b[0], out) == EOF) exit(1);
    if(fputc(u.b[1], out) == EOF) exit(1);
    if(fputc(u.b[2], out) == EOF) exit(1);
    if(fputc(u.b[3], out) == EOF) exit(1);
    
    if(out2) {
        if(fputc(u.b[0], out2) == EOF) exit(1);
        if(fputc(u.b[1], out2) == EOF) exit(1);
        if(fputc(u.b[2], out2) == EOF) exit(1);
        if(fputc(u.b[3], out2) == EOF) exit(1);
    }
}

static size_t write_string(char *string, long length)
{
    size_t bytes_sent = fwrite(string, 1, length, out);
    if(fputc('\0', out) == EOF) exit(1);
    
    if(out2) {
        bytes_sent = fwrite(string, 1, length, out2);
        if(fputc('\0', out2) == EOF) exit(1);
    }
    return bytes_sent;
}

// COMMAND @ ZPOS FUNCTIONS

// find an existing filament definition

static int find_filament(char *filament_id)
{
    int i, index = -1;
    // a brute force search is generally fastest for low n
    for(i = 0; i < filamentLength; i++) {
        if(strcmp(filament_id, filament[i].colour) == 0) {
            index = i;
            break;
        }
    }
    return index;
}

// add a new filament definition

static int add_filament(char *filament_id, double diameter, unsigned temperature, unsigned LED)
{
    int index = find_filament(filament_id);
    if(index < 0) {
        if(filamentLength < FILAMENT_MAX) {
            index = filamentLength++;
            filament[index].colour = strdup(filament_id);
            filament[index].diameter = diameter;
            filament[index].temperature = temperature;
            filament[index].LED = LED;
        }
        else {
            fprintf(stderr, "(line %u) Buffer overflow: too many @filament definitions (maximum = %i)" EOL, lineNumber, FILAMENT_MAX - 1);
            index = 0;
        }
    }
    return index;
}

// append a new command at z function

static void add_command_at(double z, char *filament_id, unsigned temperature)
{
    static double previous_z = 0.0;
    int index = filament_id ? find_filament(filament_id) : 0;
    if(index < 0) {
        fprintf(stderr, "(line %u) Semantic error: @pause macro with undefined filament name '%s', use a @filament macro to define it" EOL, lineNumber, filament_id);
        index = 0;
    }
    // insert command
    if(commandAtLength < COMMAND_AT_MAX) {
        if(z <= previous_z) {
            int i = commandAtLength;
            // make a space
            while(i > 0 && z <= commandAt[i - 1].z) {
                commandAt[i] = commandAt[i - 1];
                i--;
            }
            commandAt[i].z = z;
            commandAt[i].filament_index = index;
            commandAt[i].temperature = temperature;
            previous_z = commandAt[commandAtLength].z;
        }
        // append command
        else {
            commandAt[commandAtLength].z = z;
            commandAt[commandAtLength].filament_index = index;
            commandAt[commandAtLength].temperature = temperature;
            previous_z = z;
        }
        // nonzero temperature signals a tmperature change, not a pause @ zPos
        if(temperature == 0 && commandAtLength == 0) {
            if(macroStarted) {
                pause_at_zpos(z);
            }
            else {
                pausePending = 1;
            }
        }
        commandAtLength++;
    }
    else {
        fprintf(stderr, "(line %u) Buffer overflow: too many @pause definitions (maximum = %i)" EOL, lineNumber, COMMAND_AT_MAX);
    }
}

// 5D VECTOR FUNCTIONS

// compute the filament scaling factor

static void set_filament_scale(unsigned extruder_id, double filament_diameter)
{
    double actual_radius = filament_diameter / 2;
    double nominal_radius = machine.nominal_filament_diameter / 2;
    override[extruder_id].filament_scale = (nominal_radius * nominal_radius) / (actual_radius * actual_radius);
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
        longestDDA = (int)(60 * 1000000.0 / (machine.x.max_feedrate * machine.x.steps_per_mm));
    
        int axisDDA = (int)(60 * 1000000.0 / (machine.y.max_feedrate * machine.y.steps_per_mm));
        if(longestDDA < axisDDA) longestDDA = axisDDA;
    
        axisDDA = (int)(60 * 1000000.0 / (machine.z.max_feedrate * machine.z.steps_per_mm));
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

static double get_safe_feedrate(int flag, Ptr5d delta) {
    
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

// convert mm to steps using the current machine definition

// IMPORTANT: this command changes the global excess value which accumulates the rounding remainder

static Point5d mm_to_steps(Ptr5d mm, Ptr2d excess)
{
    double value;
    Point5d result;
    result.x = round(mm->x * machine.x.steps_per_mm);
    result.y = round(mm->y * machine.y.steps_per_mm);
    result.z = round(mm->z * machine.z.steps_per_mm);
    if(excess) {
        // accumulate rounding remainder
        value = (mm->a * machine.a.steps_per_mm) + excess->a;
        result.a = round(value);
        // changes to excess
        excess->a = value - result.a;
        
        value = (mm->b * machine.b.steps_per_mm) + excess->b;
        result.b = round(value);
        // changes to excess
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
            fprintf(stderr, "(line %u) Semantic warning: X axis homing to %s endstop" EOL, lineNumber, direction ? "maximum" : "minimum");
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
            fprintf(stderr, "(line %u) Semantic warning: Y axis homing to %s endstop" EOL, lineNumber, direction ? "maximum" : "minimum");
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
            fprintf(stderr, "(line %u) Semantic warning: Z axis homing to %s endstop" EOL, lineNumber, direction ? "maximum" : "minimum");
        }
    }
    
    // unit vector distance in mm
    double distance = magnitude(xyz_flag, &unitVector);
    // move duration in microseconds = distance / feedrate * 60,000,000
    double microseconds = distance / feedrate * 60000000.0;
    // time between steps for longest axis = microseconds / longestStep
    unsigned step_delay = (unsigned)round(microseconds / longestAxis);
    
    write_8(direction == ENDSTOP_IS_MIN ? 131 :132);
    
    // uint8: Axes bitfield. Axes whose bits are set will be moved.
    write_8(xyz_flag);
    
    // uint32: Feedrate, in microseconds between steps on the max delta. (DDA)
    write_32(step_delay);
    
    // uint16: Timeout, in seconds.
    write_16(machine.timeout);
}

// 133 - delay

static void delay(unsigned milliseconds)
{
    write_8(133);
    
    // uint32: delay, in milliseconds
    write_32(milliseconds);
}

// 134 - Change extruder offset

// This is important to use on dual-head Replicators, because the machine needs to know
// the current toolhead in order to apply a calibration offset.

static void change_extruder_offset(unsigned extruder_id)
{
    assert(extruder_id < machine.extruder_count);
    write_8(134);
    
    // uint8: ID of the extruder to switch to
    write_8(extruder_id);
}

// 135 - Wait for extruder ready

static void wait_for_extruder(unsigned extruder_id, unsigned timeout)
{
    assert(extruder_id < machine.extruder_count);
    write_8(135);
    
    // uint8: ID of the extruder to wait for
    write_8(extruder_id);
    
    // uint16: delay between query packets sent to the extruder, in ms (nominally 100 ms)
    write_16(100);
    
    // uint16: Timeout before continuing without extruder ready, in seconds (nominally 1 minute)
    write_16(timeout);
}
 
// 136 - extruder action command

// Action 03 - Set extruder target temperature

static void set_nozzle_temperature(unsigned extruder_id, unsigned temperature)
{
    assert(extruder_id < machine.extruder_count);
    write_8(136);
    
    // uint8: ID of the extruder to query
    write_8(extruder_id);
    
    // uint8: Action command to send to the extruder
    write_8(3);
    
    // uint8: Length of the extruder command payload (N)
    write_8(2);
    
    // int16: Desired target temperature, in Celsius
    write_16(temperature);
}

// Action 12 - Enable / Disable fan

static void set_fan(unsigned extruder_id, unsigned state)
{
    assert(extruder_id < machine.extruder_count);
    write_8(136);
    
    // uint8: ID of the extruder to query
    write_8(extruder_id);
    
    // uint8: Action command to send to the extruder
    write_8(12);
    
    // uint8: Length of the extruder command payload (N)
    write_8(1);
    
    // uint8: 1 to enable, 0 to disable
    write_8(state);
}

// Action 13 - Enable / Disable extra output (blower fan)

static void set_valve(unsigned extruder_id, unsigned state)
{
    assert(extruder_id < machine.extruder_count);
    write_8(136);
    
    // uint8: ID of the extruder to query
    write_8(extruder_id);
    
    // uint8: Action command to send to the extruder
    write_8(13);
    
    // uint8: Length of the extruder command payload (N)
    write_8(1);
    
    // uint8: 1 to enable, 0 to disable
    write_8(state);
}

// Action 31 - Set build platform target temperature

static void set_build_platform_temperature(unsigned extruder_id, unsigned temperature)
{
    assert(extruder_id < machine.extruder_count);
    write_8(136);
    
    // uint8: ID of the extruder to query
    write_8(extruder_id);
    
    // uint8: Action command to send to the extruder
    write_8(31);
    
    // uint8: Length of the extruder command payload (N)
    write_8(2);
    
    // int16: Desired target temperature, in Celsius
    write_16(temperature);
}

// 137 - Enable / Disable axes steppers

static void set_steppers(unsigned axes, unsigned state)
{
    unsigned bitfield = axes & AXES_BIT_MASK;
    if(state) {
        bitfield |= 0x80;
    }
    write_8(137);
    
    // uint8: Bitfield codifying the command (see below)
    write_8(bitfield);
}
 
// 139 - Queue absolute point

static void queue_absolute_point()
{
    long longestDDA = get_longest_dda();
    Point5d steps = mm_to_steps(&targetPosition, &excess);
    
    write_8(139);
    
    // int32: X coordinate, in steps
    write_32((int)steps.x);
    
    // int32: Y coordinate, in steps
    write_32((int)steps.y);
    
    // int32: Z coordinate, in steps
    write_32((int)steps.z);
    
    // int32: A coordinate, in steps
    write_32(-(int)steps.a);
    
    // int32: B coordinate, in steps
    write_32(-(int)steps.b);
    
    // uint32: Feedrate, in microseconds between steps on the max delta. (DDA)
    write_32((int)longestDDA);
}

// 140 - Set extended position

static void set_position()
{
    Point5d steps = mm_to_steps(&currentPosition, NULL);
    write_8(140);
    
    // int32: X position, in steps
    write_32((int)steps.x);
    
    // int32: Y position, in steps
    write_32((int)steps.y);
    
    // int32: Z position, in steps
    write_32((int)steps.z);
    
    // int32: A position, in steps
    write_32((int)steps.a);
    
    // int32: B position, in steps
    write_32((int)steps.b);
}

// 141 - Wait for build platform ready

static void wait_for_build_platform(unsigned extruder_id, int timeout)
{
    assert(extruder_id < machine.extruder_count);
    write_8(141);
    
    // uint8: ID of the extruder platform to wait for
    write_8(extruder_id);
    
    // uint16: delay between query packets sent to the extruder, in ms (nominally 100 ms)
    write_16(100);
    
    // uint16: Timeout before continuing without extruder ready, in seconds (nominally 1 minute)
    write_16(timeout);
}

// 142 - Queue extended point, new style

#if ENABLE_SIMULATED_RPM
static void queue_new_point(unsigned milliseconds)
{
    Point5d target;
    
    // the function is only called by dwell, which is by definition stationary,
    // so set zero relitive position change

    target.x = 0;
    target.y = 0;
    target.z = 0;
    target.a = 0;
    target.b = 0;

    // if we have a G4 dwell and either the a or b motor is on, 'simulate' a 5D extrusion distance
    if(tool[A].motor_enabled && tool[A].rpm) {
        double maxrpm = machine.a.max_feedrate * machine.a.steps_per_mm / machine.a.motor_steps;
        double rpm = tool[A].rpm > maxrpm ? maxrpm : tool[A].rpm;
        double minutes = milliseconds / 60000.0;
        // minute * revolution/minute
        double numRevolutions = minutes * (tool[A].motor_enabled > 0 ? rpm : -rpm);
        // steps/revolution * mm/steps
        double mmPerRevolution = machine.a.motor_steps * (1 / machine.a.steps_per_mm);
        target.a = -(numRevolutions * mmPerRevolution);
        command.flag |= A_IS_SET;
    }
    
    if(tool[B].motor_enabled && tool[B].rpm) {
        double maxrpm = machine.b.max_feedrate * machine.b.steps_per_mm / machine.b.motor_steps;
        double rpm = tool[B].rpm > maxrpm ? maxrpm : tool[B].rpm;
        double minutes = milliseconds / 60000.0;
        // minute * revolution/minute
        double numRevolutions = minutes * (tool[B].motor_enabled > 0 ? rpm : -rpm);
        // steps/revolution * mm/steps
        double mmPerRevolution = machine.b.motor_steps * (1 / machine.b.steps_per_mm);
        target.b = -(numRevolutions * mmPerRevolution);
        command.flag |= B_IS_SET;
    }

    Point5d steps = mm_to_steps(&target, &excess);

    write_8(142);
    
    // int32: X coordinate, in steps
    write_32((int)steps.x);
    
    // int32: Y coordinate, in steps
    write_32((int)steps.y);
    
    // int32: Z coordinate, in steps
    write_32((int)steps.z);
    
    // int32: A coordinate, in steps
    write_32((int)steps.a);
    
    // int32: B coordinate, in steps
    write_32((int)steps.b);
    
    // uint32: Duration of the movement, in microseconds
    write_32(milliseconds * 1000);
    
    // uint8: Axes bitfield to specify which axes are relative. Any axis with a bit set should make a relative movement.
    write_8(AXES_BIT_MASK);
}
#endif

// 143 - Store home positions

static void store_home_positions(void)
{
    write_8(143);
    
    // uint8: Axes bitfield to specify which axes' positions to store.
    // Any axis with a bit set should have its position stored.
    write_8(command.flag & AXES_BIT_MASK);
}

// 144 - Recall home positions

static void recall_home_positions(void)
{
    write_8(144);
    
    // uint8: Axes bitfield to specify which axes' positions to recall.
    // Any axis with a bit set should have its position recalled.
    write_8(command.flag & AXES_BIT_MASK);
}

// 145 - Set digital potentiometer value

static void set_pot_value(unsigned axis, unsigned value)
{
    assert(axis <= 4);
    assert(value <= 127);
    write_8(145);
    
    // uint8: axis value (valid range 0-4) which axis pot to set
    write_8(axis);
    
    // uint8: value (valid range 0-127), values over max will be capped at max
    write_8(value);
}
 
// 146 - Set RGB LED value

static void set_LED(unsigned red, unsigned green, unsigned blue, unsigned blink)
{
    write_8(146);
    
    // uint8: red value (all pix are 0-255)
    write_8(red);
    
    // uint8: green
    write_8(green);
    
    // uint8: blue
    write_8(blue);
    
    // uint8: blink rate (0-255 valid)
    write_8(blink);
    
    // uint8: 0 (reserved for future use)
    write_8(0);
}

static void set_LED_RGB(unsigned rgb, unsigned blink)
{
    write_8(146);
    
    // uint8: red value (all pix are 0-255)
    write_8((rgb >> 16) & 0xFF);
    
    // uint8: green
    write_8((rgb >> 8) & 0xFF);
    
    // uint8: blue
    write_8(rgb & 0xFF);
    
    // uint8: blink rate (0-255 valid)
    write_8(blink);
    
    // uint8: 0 (reserved for future use)
    write_8(0);

}

// 147 - Set Beep

static void set_beep(unsigned frequency, unsigned milliseconds)
{
    write_8(147);
    
    // uint16: frequency
    write_16(frequency);

    // uint16: buzz length in ms
    write_16(milliseconds);

    // uint8: 0 (reserved for future use)
    write_8(0);
}

// 148 - Pause for button

// 149 - Display message to LCD

static void display_message(char *message, unsigned vPos, unsigned hPos, unsigned timeout, int wait_for_button)
{
    assert(vPos < 4);
    assert(hPos < 20);
    
    long bytesSent = 0;
    unsigned bitfield = 0;
    unsigned seconds = 0;
    
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
        if(bytesSent > 0) {
            bitfield |= 0x01; //do not clear flag
        }
        
        write_8(149);
        
        // uint8: Options bitfield (see below)
        write_8(bitfield);
        // uint8: Horizontal position to display the message at (commonly 0-19)
        write_8(hPos);
        // uint8: Vertical position to display the message at (commonly 0-3)
        write_8(vPos);
        // uint8: Timeout, in seconds. If 0, this message will left on the screen
        write_8(seconds);
        // 1+N bytes: Message to write to the screen, in ASCII, terminated with a null character.
        long rowLength = length - bytesSent;
        bytesSent += write_string(message + bytesSent, rowLength < maxLength ? rowLength : maxLength);
    }
}

// 150 - Set Build Percentage

static void set_build_progress(unsigned percent)
{
    if(percent > 100) percent = 100;
    
    write_8(150);
    
    // uint8: percent (0-100)
    write_8(percent);
    
    // uint8: 0 (reserved for future use) (reserved for future use)
    write_8(0);
}

// 151 - Queue Song

static void queue_song(unsigned song_id)
{
    // song ID 0: error tone with 4 cycles
    // song ID 1: done tone
    // song ID 2: error tone with 2 cycles
    
    assert(song_id <= 2);
    write_8(151);
    
    // uint8: songID: select from a predefined list of songs
    write_8(song_id);
}

// 152 - Restore to factory settings

// 153 - Build start notification

static void start_build()
{
    write_8(153);
    
    // uint32: 0 (reserved for future use)
    write_32(0);

    // 1+N bytes: Name of the build, in ASCII, null terminated
    write_string("GPX", 3);
}

// 154 - Build end notification

static void end_build()
{
    write_8(154);

    // uint8: 0 (reserved for future use)
    write_8(0);
}
 
// 155 - Queue extended point x3g

// IMPORTANT: this command updates the parser state

static void queue_ext_point(double feedrate)
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
        deltaMM.x = targetPosition.x - currentPosition.x;
        deltaSteps.x = round(fabs(deltaMM.x) * machine.x.steps_per_mm);
    }
    else {
        deltaMM.x = 0;
        deltaSteps.x = 0;
    }
    
    if(command.flag & Y_IS_SET) {
        deltaMM.y = targetPosition.y - currentPosition.y;
        deltaSteps.y = round(fabs(deltaMM.y) * machine.y.steps_per_mm);
    }
    else {
        deltaMM.y = 0;
        deltaSteps.y = 0;
    }
    
    if(command.flag & Z_IS_SET) {
        deltaMM.z = targetPosition.z - currentPosition.z;
        deltaSteps.z = round(fabs(deltaMM.z) * machine.z.steps_per_mm);
    }
    else {
        deltaMM.z = 0;
        deltaSteps.z = 0;
    }
    
    if(command.flag & A_IS_SET) {
        deltaMM.a = targetPosition.a - currentPosition.a;
        deltaSteps.a = round(fabs(deltaMM.a) * machine.a.steps_per_mm);
    }
    else {
        deltaMM.a = 0;
        deltaSteps.a = 0;
    }
    
    if(command.flag & B_IS_SET) {
        deltaMM.b = targetPosition.b - currentPosition.b;
        deltaSteps.b = round(fabs(deltaMM.b) * machine.b.steps_per_mm);
    }
    else {
        deltaMM.b = 0;
        deltaSteps.b = 0;
    }
    
    // check that we have actually moved on at least one axis when the move is
    // rounded down to the nearest step
    if(magnitude(command.flag, &deltaSteps) > 0) {
        double distance = magnitude(command.flag & XYZ_BIT_MASK, &deltaMM);
        Point5d target = targetPosition;
        
        target.a = -deltaMM.a;
        target.b = -deltaMM.b;
        
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

#if ENABLE_SIMULATED_RPM
        // if either a or b is 0, but their motor is on and turning, 'simulate' a 5D extrusion distance
        if(deltaMM.a == 0.0 && tool[A].motor_enabled && tool[A].rpm) {
            double maxrpm = machine.a.max_feedrate * machine.a.steps_per_mm / machine.a.motor_steps;
            double rpm = tool[A].rpm > maxrpm ? maxrpm : tool[A].rpm;
            // minute * revolution/minute
            double numRevolutions = minutes * (tool[A].motor_enabled > 0 ? rpm : -rpm);
            // steps/revolution * mm/steps
            double mmPerRevolution = machine.a.motor_steps * (1 / machine.a.steps_per_mm);
            // set distance
            deltaMM.a = numRevolutions * mmPerRevolution;
            deltaSteps.a = round(fabs(deltaMM.a) * machine.a.steps_per_mm);
            target.a = -deltaMM.a;
        }
        else {
            // disable RPM as soon as we begin 5D printing
            tool[A].rpm = 0;
        }
        if(deltaMM.b == 0.0 && tool[B].motor_enabled && tool[B].rpm) {
            double maxrpm = machine.b.max_feedrate * machine.b.steps_per_mm / machine.b.motor_steps;
            double rpm = tool[B].rpm > maxrpm ? maxrpm : tool[B].rpm;
            // minute * revolution/minute
            double numRevolutions = minutes * (tool[B].motor_enabled > 0 ? rpm : -rpm);
            // steps/revolution * mm/steps
            double mmPerRevolution = machine.b.motor_steps * (1 / machine.b.steps_per_mm);
            // set distance
            deltaMM.b = numRevolutions * mmPerRevolution;
            deltaSteps.b = round(fabs(deltaMM.b) * machine.b.steps_per_mm);
            target.b = -deltaMM.b;
        }
        else {
            // disable RPM as soon as we begin 5D printing
            tool[B].rpm = 0;
        }
#endif
        
        Point5d steps = mm_to_steps(&target, &excess);
        
        double usec = (60000000.0 * minutes);
        
        double dda_interval = usec / largest_axis(command.flag, &deltaSteps);
        
        // Convert dda_interval into dda_rate (dda steps per second on the longest axis)
        double dda_rate = 1000000.0 / dda_interval;

        write_8(155);
        
        // int32: X coordinate, in steps
        write_32((int)steps.x);
        
        // int32: Y coordinate, in steps
        write_32((int)steps.y);
        
        // int32: Z coordinate, in steps
        write_32((int)steps.z);
        
        // int32: A coordinate, in steps
        write_32((int)steps.a);
        
        // int32: B coordinate, in steps
        write_32((int)steps.b);

        // uint32: DDA Feedrate, in steps/s
        write_32((unsigned)dda_rate);
        
        // uint8: Axes bitfield to specify which axes are relative. Any axis with a bit set should make a relative movement.
        write_8(A_IS_SET|B_IS_SET);

        // float (single precision, 32 bit): mm distance for this move.  normal of XYZ if any of these axes are active, and AB for extruder only moves
        write_float((float)distance);
        
        // uint16: feedrate in mm/s, multiplied by 64 to assist fixed point calculation on the bot
        write_16((unsigned)(feedrate * 64.0));
	}
}

// 156 - Set segment acceleration

static void set_acceleration(int state)
{
    write_8(156);
    
    // uint8: 1 to enable, 0 to disable
    write_8(state);
}

// 157 - Stream Version

// 158 - Pause @ zPos

static void pause_at_zpos(float z_positon)
{
    write_8(158);
    
    // uint8: pause at Z coordinate or 0.0 to disable
    write_float(z_positon);
}

// TARGET POSITION

// calculate target position

static int calculate_target_position(void)
{
    int do_pause_at_zpos = 0;
    
    // CALCULATE TARGET POSITION
    
    // x
    if(command.flag & X_IS_SET) {
        targetPosition.x = isRelative ? (currentPosition.x + command.x) : (command.x + offset[currentOffset].x + userOffset.x);
    }
    else {
        targetPosition.x = currentPosition.x;
    }
    
    // y
    if(command.flag & Y_IS_SET) {
        targetPosition.y = isRelative ? (currentPosition.y + command.y) : (command.y + offset[currentOffset].y + userOffset.y);
    }
    else {
        targetPosition.y = currentPosition.y;
    }
    
    // z
    if(command.flag & Z_IS_SET) {
        targetPosition.z = isRelative ? (currentPosition.z + command.z) : (command.z + offset[currentOffset].z + userOffset.z);
    }
    else {
        targetPosition.z = currentPosition.z;
    }
    
    // a
    if(command.flag & A_IS_SET) {
        targetPosition.a = (isRelative || extruderIsRelative) ? (currentPosition.a + command.a) : command.a;
    }
    else {
        targetPosition.a = currentPosition.a;
    }
    // b
    if(command.flag & B_IS_SET) {
        targetPosition.b = (isRelative || extruderIsRelative) ? (currentPosition.b + command.b) : command.b;
    }
    else {
        targetPosition.b = currentPosition.b;
    }
    
    // update current feedrate
    if(command.flag & F_IS_SET) {
        currentFeedrate = command.f;
    }
    
    // DITTO PRINTING
    
    if(dittoPrinting) {
        if(command.flag & A_IS_SET) {
            targetPosition.b = targetPosition.a;
            command.flag |= B_IS_SET;
        }
        else if(command.flag & B_IS_SET) {
            targetPosition.a = targetPosition.b;
            command.flag |= A_IS_SET;
        }
    }
    
    // CHECK FOR COMMAND @ Z POS
    
    // check if there are more commands on the stack
    if(macroStarted && commandAtIndex < commandAtLength) {
        // check if the next command will cross the z threshold
        if(commandAt[commandAtIndex].z <= targetPosition.z) {
            // is this a temperature change macro?
            if(commandAt[commandAtIndex].temperature) {
                unsigned temperature = commandAt[commandAtIndex].temperature;
                // make sure the temperature has changed
                if(tool[currentExtruder].nozzle_temperature != temperature) {
                    if(dittoPrinting) {
                        set_nozzle_temperature(A, temperature);
                        set_nozzle_temperature(B, temperature);
                        tool[A].nozzle_temperature = tool[B].nozzle_temperature = temperature;
                    }
                    else {
                        set_nozzle_temperature(currentExtruder, temperature);
                        tool[currentExtruder].nozzle_temperature = temperature;
                    }
                }
                commandAtIndex++;
            }
            // no its a pause macro
            else {
                int index = commandAt[commandAtIndex].filament_index;
                // override filament diameter
                if(filament[index].diameter > 0.0001) {
                    if(dittoPrinting) {
                        set_filament_scale(A, filament[index].diameter);
                        set_filament_scale(B, filament[index].diameter);
                    }
                    else {
                        set_filament_scale(currentExtruder, filament[index].diameter);
                    }
                }
                // override nozzle temperature
                if(filament[index].temperature) {
                    unsigned temperature = filament[index].temperature;
                    if(tool[currentExtruder].nozzle_temperature != temperature) {
                        if(dittoPrinting) {
                            set_nozzle_temperature(A, temperature);
                            set_nozzle_temperature(B, temperature);
                            tool[A].nozzle_temperature = tool[B].nozzle_temperature = temperature;
                        }
                        else {
                            set_nozzle_temperature(currentExtruder, temperature);
                            tool[currentExtruder].nozzle_temperature = temperature;
                        }
                    }
                }
                // override LED colour
                if(filament[index].LED) {
                    set_LED_RGB(filament[index].LED, 0);
                }
                commandAtIndex++;
                if(commandAtIndex < commandAtLength) {
                    do_pause_at_zpos = 1;
                }
            }
        }
    }
    
    // SCALE FILAMENT INDEPENDENTLY
    
    if(command.flag & A_IS_SET && override[A].filament_scale != 1.0) targetPosition.a *= override[A].filament_scale;
    if(command.flag & B_IS_SET && override[B].filament_scale != 1.0) targetPosition.b *= override[B].filament_scale;
    
    return do_pause_at_zpos;
}

// TOOL CHANGE

void do_tool_change(int timeout) {
    // set the temperature of current tool to standby (if standby is different to active)
    if(override[currentExtruder].standby_temperature
       && override[currentExtruder].standby_temperature != tool[currentExtruder].nozzle_temperature) {
        unsigned temperature = override[currentExtruder].standby_temperature;
        set_nozzle_temperature(currentExtruder, temperature);
        tool[currentExtruder].nozzle_temperature = temperature;
    }
    // set the temperature of selected tool to active (if active is different to standby)
    if(override[selectedExtruder].active_temperature
       && override[selectedExtruder].active_temperature != tool[selectedExtruder].nozzle_temperature) {
        unsigned temperature = override[selectedExtruder].active_temperature;
        set_nozzle_temperature(selectedExtruder, temperature);
        tool[selectedExtruder].nozzle_temperature = temperature;
        // wait for nozzle to head up
        wait_for_extruder(selectedExtruder, timeout);
    }
    // switch any active G10 offset (G54 or G55)
    if(currentOffset == currentExtruder + 1) {
        currentOffset = selectedExtruder + 1;
    }
    // change current toolhead in order to apply the calibration offset
    change_extruder_offset(selectedExtruder);
    // set current extruder so changes in E are expressed as changes to A or B
    currentExtruder = selectedExtruder;
}

// PARSER PRE-PROCESSOR

// return the length of the given file in bytes

static long get_filesize(FILE *file)
{
    long filesize = -1;
    fseek(file, 0L, SEEK_END);
    filesize = ftell(file);
    fseek(file, 0L, SEEK_SET);
    return filesize;
}

// clean up the gcode command for processing

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

// clean up the gcode comment for processing

static char *normalize_comment(char *p) {
    // strip white space from the end of comment
    char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)(*--e))) *e = '\0';
    // strip white space from the beginning of comment.
    while(isspace(*p)) p++;
    return p;
}

// MACRO PARSER

/* format
 
 ;@<STRING> <STRING> <FLOAT> <FLOAT>mm <INTEGER>c #<HEX> (<STRING>)

 MACRO:= ';' '@' COMMAND COMMENT EOL
 COMMAND:= PRINTER | ENABLE | FILAMENT | EXTRUDER | SLICER | START| PAUSE
 COMMENT:= S+ '(' [^)]* ')' S+
 PRINTER:= ('printer' | 'machine') (TYPE | DIAMETER | TEMP | RGB)+
 TYPE:=  S+ ('c3' | 'c4' | 'cp4' | 'cpp' | 't6' | 't7' | 't7d' | 'r1' | 'r1d' | 'r2' | 'r2x')
 DIAMETER:= S+ DIGIT+ ('.' DIGIT+)? 'm' 'm'?
 TEMP:= S+ DIGIT+ 'c'
 RGB:= S+ '#' HEX HEX HEX HEX HEX HEX                   ; LED colour
 ENABLE:= 'enable' (DITTO | PROGRESS)
 DITTO:= S+ 'ditto'                                     ; Simulated ditto printing
 PROGRESS:= S+ 'progress'                               ; Override build progress
 FILAMENT:= 'filament' FILAMENT_ID (DIAMETER | TEMP | RGB)+
 FILAMENT_ID:= S+ ALPHA+ ALPHA_NUMERIC*
 EXTRUDER:= ('right' | 'left')  (FILAMENT_ID | DIAMETER | TEMP)+
 SLICER:= 'slicer' DIAMETER                             ; Nominal filament diameter
 START:= 'start' (FILAMENT_ID | TEMPERATURE)
 PAUSE:= 'pause' (ZPOS | FILAMENT_ID | TEMPERATURE)+
 ZPOS:= S+ DIGIT+ ('.' DIGIT+)?

 */

#define MACRO_IS(token) strcmp(token, macro) == 0
#define NAME_IS(n) strcasecmp(name, n) == 0

static void parse_macro(const char* macro, char *p)
{
    char *name = NULL;
    double z = 0.0;
    double diameter = 0.0;
    unsigned temperature = 0;
    unsigned LED = 0;

    while(*p != 0) {
        // trim any leading white space
        while(isspace(*p)) p++;
        if(isalpha(*p)) {
            name = p;
            while(*p && !isspace(*p)) p++;
            if(*p) *p++ = 0;
        }
        else if(isdigit(*p)) {
            char *t = p;
            while(*p && !isspace(*p)) p++;
            if(*(p - 1) == 'm') {
                diameter = strtod(t, NULL);
            }
            else if(*(p - 1) == 'c') {
                temperature = atoi(t);
            }
            else {
                z = strtod(t, NULL);
            }
            if(*p) *p++ = 0;
        }
        else if(*p == '#') {
            char *t = ++p;
            while(*p && !isspace(*p)) p++;
            if(*p) *p++ = 0;
            LED = (unsigned)strtol(t, NULL, 16);
        }
        else if(*p == '(') {
            char *t = strrchr(p + 1, ')');
            if(t) {
                *t = 0;
                p = t + 1;
            }
            else {
                *p = 0;
            }
        }
        else {
            fprintf(stderr, "(line %u) Syntax error: unrecognised macro parameter" EOL, lineNumber);
            break;
        }
    }
    // ;@printer <TYPE> <DIAMETER>mm <HBP-TEMP>c #<LED-COLOUR>
    if(MACRO_IS("machine") || MACRO_IS("printer") || MACRO_IS("slicer")) {
        if(name) {
            if(NAME_IS("c3")) machine = cupcake_G3;
            else if(NAME_IS("c4")) machine = cupcake_G4;
            else if(NAME_IS("cp4")) machine = cupcake_P4;
            else if(NAME_IS("cpp")) machine = cupcake_PP;
            else if(NAME_IS("t6")) machine = thing_o_matic_7;
            else if(NAME_IS("t7")) machine = thing_o_matic_7;
            else if(NAME_IS("t7d")) machine = thing_o_matic_7D;
            else if(NAME_IS("r1")) machine = replicator_1;
            else if(NAME_IS("r1d")) machine = replicator_1D;
            else if(NAME_IS("r2")) machine = replicator_2;
            else if(NAME_IS("r2x")) machine = replicator_2X;
            else {
                fprintf(stderr, "(line %u) Semantic error: @printer macro with unrecognised type '%s'" EOL, lineNumber, name);
            }
        }
        if(diameter > 0.0001) machine.nominal_filament_diameter = diameter;
        if(temperature) {
            if(machine.a.has_heated_build_platform) override[A].build_platform_temperature = temperature;
            else if(machine.b.has_heated_build_platform) override[B].build_platform_temperature = temperature;
            else {
                fprintf(stderr, "(line %u) Semantic warning: @printer macro cannot override non-existant heated build platform" EOL, lineNumber);                
            }
        }
        if(LED) set_LED_RGB(LED, 0);
    }
    // ;@enable ditto
    // ;@enable progress
    else if(MACRO_IS("enable")) {
        if(name) {
            if(NAME_IS("ditto")) {
                if(machine.extruder_count == 1) {
                    fputs("Configuration error: ditto printing cannot access non-existant second extruder" EOL, stderr);
                    dittoPrinting = 0;
                }
                else {
                    dittoPrinting = 1;
                }
            }
            else if(NAME_IS("progress")) buildProgress = 1;
            else {
                fprintf(stderr, "(line %u) Semantic error: @enable macro with unrecognised parameter '%s'" EOL, lineNumber, name);
            }
        }
        else {
            fprintf(stderr, "(line %u) Syntax error: @enable macro with missing parameter" EOL, lineNumber);
        }
    }
    // ;@filament <NAME> <DIAMETER>mm <TEMP>c #<LED-COLOUR>
    else if(MACRO_IS("filament")) {
        if(name) {
            add_filament(name, diameter, temperature, LED);
        }
        else {
            fprintf(stderr, "(line %u) Semantic error: @filament macro with missing name" EOL, lineNumber);
        }
    }
    // ;@right <NAME> <DIAMETER>mm <TEMP>c
    else if(MACRO_IS("right")) {
        if(name) {
            int index = find_filament(name);
            if(index > 0) {
                if(filament[index].diameter > 0.0001) set_filament_scale(A, filament[index].diameter);
                if(filament[index].temperature) override[A].active_temperature = filament[index].temperature;
                return;
            }
        }
        if(diameter > 0.0001) set_filament_scale(A, diameter);
        if(temperature) override[A].active_temperature = temperature;
    }
    // ;@left <NAME> <DIAMETER>mm <TEMP>c
    else if(MACRO_IS("left")) {
        if(name) {
            int index = find_filament(name);
            if(index > 0) {
                if(filament[index].diameter > 0.0001) set_filament_scale(B, filament[index].diameter);
                if(filament[index].temperature) override[B].active_temperature = filament[index].temperature;
                return;
            }
        }
        if(diameter > 0.0001) set_filament_scale(B, diameter);
        if(temperature) override[B].active_temperature = temperature;
    }
    // ;@pause <ZPOS> <NAME>
    else if(MACRO_IS("pause")) {
        if(z > 0.0001) {
            add_command_at(z, name, 0);
        }
        else {
            fprintf(stderr, "(line %u) Semantic error: @pause macro with missing zPos" EOL, lineNumber);            
        }
    }
    // ;@temp <ZPOS> <TEMP>c
    // ;@temperature <ZPOS> <TEMP>c
    else if(MACRO_IS("temp") || MACRO_IS("temperature")) {
        if(temperature) {
            if(z > 0.0001) {
                add_command_at(z, NULL, temperature);
            }
            else {
                fprintf(stderr, "(line %u) Semantic error: @%s macro with missing zPos" EOL, lineNumber, macro);
            }
        }
        else {
            fprintf(stderr, "(line %u) Semantic error: @%s macro with missing temperature" EOL, lineNumber, macro);
        }
    }
    // ;@start <NAME> <TEMP>c
    else if(MACRO_IS("start")) {
        if(temperature) {
            if(dittoPrinting) {
                override[A].active_temperature = override[B].active_temperature = temperature;
            }
            else {
                override[currentExtruder].active_temperature = temperature;
            }
        }
        else {
            int index = find_filament(name);
            if(index > 0) {
                if(dittoPrinting) {
                    if(filament[index].diameter > 0.0001) {
                        set_filament_scale(A, filament[index].diameter);
                        set_filament_scale(B, filament[index].diameter);
                    }
                    if(filament[index].temperature)  {
                        override[A].active_temperature = override[B].active_temperature = filament[index].temperature;
                    }
                }
                else {
                    if(filament[index].diameter > 0.0001) set_filament_scale(currentExtruder, filament[index].diameter);
                    if(filament[index].temperature) override[currentExtruder].active_temperature = filament[index].temperature;
                    if(filament[index].LED) set_LED_RGB(filament[index].LED, 0);
                }
            }
            else {
                fprintf(stderr, "(line %u) Semantic error: @start with undefined filament name '%s', use a @filament macro to define it" EOL, lineNumber, name ? name : "");
            }
        }
    }
    // ;@body
    else if(MACRO_IS("body")) {
        if(pausePending) {
            pause_at_zpos(commandAt[0].z);
            pausePending = 0;
        }
        macroStarted = 1;
    }
}

// INI FILE HANDLER

// Custom machine definition ini handler

#define SECTION_IS(s) strcasecmp(section, s) == 0
#define PROPERTY_IS(n) strcasecmp(property, n) == 0
#define VALUE_IS(v) strcasecmp(value, v) == 0

static int config_handler(unsigned lineno, const char* section, const char* property, char* value)
{
    if(SECTION_IS("") || SECTION_IS("macro")) {
        if(PROPERTY_IS("slicer")
           || PROPERTY_IS("filament")
           || PROPERTY_IS("pause")
           || PROPERTY_IS("start")
           || PROPERTY_IS("temp")
           || PROPERTY_IS("temperature")) {
            parse_macro(property, value);
        }
        else if(PROPERTY_IS("verbose")) {
            verboseMode = atoi(value);
        }
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("printer")) {
        if(PROPERTY_IS("ditto_printing")) dittoPrinting = atoi(value);
        else if(PROPERTY_IS("build_progress")) buildProgress = atoi(value);
        else if(PROPERTY_IS("nominal_filament_diameter")
                || PROPERTY_IS("slicer_filament_diameter")) machine.nominal_filament_diameter = strtod(value, NULL);
        else if(PROPERTY_IS("machine_type")) {
            // use on-board machine definition
            if(VALUE_IS("c3")) machine = cupcake_G3;
            else if(VALUE_IS("c4")) machine = cupcake_G4;
            else if(VALUE_IS("cp4")) machine = cupcake_P4;
            else if(VALUE_IS("cpp")) machine = cupcake_PP;
            else if(VALUE_IS("t7")) machine = thing_o_matic_7;
            else if(VALUE_IS("t6")) machine = thing_o_matic_7;
            else if(VALUE_IS("t7")) machine = thing_o_matic_7;
            else if(VALUE_IS("t7d")) machine = thing_o_matic_7D;
            else if(VALUE_IS("r1")) machine = replicator_1;
            else if(VALUE_IS("r1d")) machine = replicator_1D;
            else if(VALUE_IS("r2")) machine = replicator_2;
            else if(VALUE_IS("r2x")) machine = replicator_2X;
            else {
                fprintf(stderr, "(line %u) Configuration error: unrecognised machine type '%s'" EOL, lineno, value);
                return 0;
            }
        }
        else if(PROPERTY_IS("build_platform_temperature")) {
            if(machine.a.has_heated_build_platform) override[A].build_platform_temperature = atoi(value);
            else if(machine.b.has_heated_build_platform) override[B].build_platform_temperature = atoi(value);
        }
        else if(PROPERTY_IS("sd_card_path")) {
            sdCardPath = strdup(value);
        }
        else if(PROPERTY_IS("verbose")) {
            verboseMode = atoi(value);
        }
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("x")) {
        if(PROPERTY_IS("max_feedrate")) machine.x.max_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("home_feedrate")) machine.x.home_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("steps_per_mm")) machine.x.steps_per_mm = strtod(value, NULL);
        else if(PROPERTY_IS("endstop")) machine.x.endstop = atoi(value);
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("y")) {
        if(PROPERTY_IS("max_feedrate")) machine.y.max_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("home_feedrate")) machine.y.home_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("steps_per_mm")) machine.y.steps_per_mm = strtod(value, NULL);
        else if(PROPERTY_IS("endstop")) machine.y.endstop = atoi(value);
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("z")) {
        if(PROPERTY_IS("max_feedrate")) machine.z.max_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("home_feedrate")) machine.z.home_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("steps_per_mm")) machine.z.steps_per_mm = strtod(value, NULL);
        else if(PROPERTY_IS("endstop")) machine.z.endstop = atoi(value);
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("a")) {
        if(PROPERTY_IS("max_feedrate")) machine.a.max_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("steps_per_mm")) machine.a.steps_per_mm = strtod(value, NULL);
        else if(PROPERTY_IS("motor_steps")) machine.a.motor_steps = strtod(value, NULL);
        else if(PROPERTY_IS("has_heated_build_platform")) machine.a.has_heated_build_platform = atoi(value);
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("right")) {
        if(PROPERTY_IS("active_temperature")
           || PROPERTY_IS("nozzle_temperature")) override[A].active_temperature = atoi(value);
        else if(PROPERTY_IS("standby_temperature")) override[A].standby_temperature = atoi(value);
        else if(PROPERTY_IS("build_platform_temperature")) override[A].build_platform_temperature = atoi(value);
        else if(PROPERTY_IS("actual_filament_diameter")) override[A].actual_filament_diameter = strtod(value, NULL);
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("b")) {
        if(PROPERTY_IS("max_feedrate")) machine.b.max_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("steps_per_mm")) machine.b.steps_per_mm = strtod(value, NULL);
        else if(PROPERTY_IS("motor_steps")) machine.b.motor_steps = strtod(value, NULL);
        else if(PROPERTY_IS("has_heated_build_platform")) machine.b.has_heated_build_platform = atoi(value);
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("left")) {
        if(PROPERTY_IS("active_temperature")
           || PROPERTY_IS("nozzle_temperature")) override[B].active_temperature = atoi(value);
        else if(PROPERTY_IS("standby_temperature")) override[B].standby_temperature = atoi(value);
        else if(PROPERTY_IS("build_platform_temperature")) override[B].build_platform_temperature = atoi(value);
        else if(PROPERTY_IS("actual_filament_diameter")) override[B].actual_filament_diameter = strtod(value, NULL);
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("machine")) {
        if(PROPERTY_IS("nominal_filament_diameter")
           || PROPERTY_IS("slicer_filament_diameter")) machine.nominal_filament_diameter = strtod(value, NULL);
        else if(PROPERTY_IS("extruder_count")) machine.extruder_count = atoi(value);
        else if(PROPERTY_IS("timeout")) machine.timeout = atoi(value);
        else goto SECTION_ERROR;
    }
    else {
        fprintf(stderr, "(line %u) Configuration error: unrecognised section [%s]" EOL, lineno, section);
        return 0;
    }
    return 1;
    
SECTION_ERROR:
    fprintf(stderr, "(line %u) Configuration error: [%s] section contains unrecognised property %s=..." EOL, lineno, section, property);
    return 0;
}

// display usage and exit

static void usage()
{
    fputs("GPX " GPX_VERSION " Copyright (c) 2013 WHPThomas, All rights reserved." EOL, stderr);
    fputs(EOL "Usage: gpx [-ps] [-x <X>] [-y <Y>] [-z <Z>] [-m <M>] [-c <C>] <IN> [<OUT>]" EOL, stderr);
    fputs(EOL "Switches:" EOL EOL, stderr);
    fputs("\t-p\toverride build percentage" EOL, stderr);
    fputs("\t-s\tenable stdin and stdout support for command pipes" EOL, stderr);
    fputs(EOL "X,Y & Z are the coordinate system offsets for the conversion" EOL EOL, stderr);
    fputs("\tX = the x axis offset" EOL, stderr);
    fputs("\tY = the y axis offset" EOL, stderr);
    fputs("\tZ = the z axis offset" EOL, stderr);
    fputs(EOL "M is the predefined machine type" EOL EOL, stderr);
    fputs("\tc3  = Cupcake Gen3 XYZ, Mk5/6 + Gen4 Extruder" EOL, stderr);
    fputs("\tc4  = Cupcake Gen4 XYZ, Mk5/6 + Gen4 Extruder" EOL, stderr);
    fputs("\tcp4 = Cupcake Pololu XYZ, Mk5/6 + Gen4 Extruder" EOL, stderr);
    fputs("\tcpp = Cupcake Pololu XYZ, Mk5/6 + Pololu Extruder" EOL, stderr);
    fputs("\tt6  = TOM Mk6 - single extruder" EOL, stderr);
    fputs("\tt7  = TOM Mk7 - single extruder" EOL, stderr);
    fputs("\tt7d = TOM Mk7 - dual extruder" EOL, stderr);
    fputs("\tr1  = Replicator 1 - single extruder" EOL, stderr);
    fputs("\tr1d = Replicator 1 - dual extruder" EOL, stderr);
    fputs("\tr2  = Replicator 2 (default config)" EOL, stderr);
    fputs("\tr2x = Replicator 2X" EOL, stderr);
    fputs(EOL "C is the filename of a custom machine definition (ini)" EOL, stderr);
    fputs(EOL "IN is the name of the sliced gcode input filename" EOL, stderr);
    fputs(EOL "OUT is the name of the x3g output filename" EOL, stderr);
    fputs(EOL "Examples:" EOL, stderr);
    fputs("\tgpx -p -m r2 my-sliced-model.gcode" EOL, stderr);
    fputs("\tgpx -c custom-tom.ini example.gcode /volumes/things/example.x3g" EOL, stderr);
    fputs("\tgpx -x 3 -y -3 offset-model.gcode" EOL, stderr);
    fputs(EOL "This program is free software; you can redistribute it and/or modify" EOL, stderr);
    fputs("it under the terms of the GNU General Public License as published by" EOL, stderr);
    fputs("the Free Software Foundation; either version 2 of the License, or" EOL, stderr);
    fputs("(at your option) any later version." EOL EOL, stderr);

    exit(1);
}

// GPX program entry point

int main(int argc, char * argv[])
{
    long filesize = 0;
    unsigned progress = 0;
    int c, i;
    int next_line = 0;
    int command_emitted = 0;
    int do_pause_at_zpos = 0;
    int standard_io = 0;
    char *config = NULL;

    initialize_globals();
    
    // READ GPX.INI
    
    // if present, read the gpx.ini file from the program directory
    {
        char *filename = argv[0];
        // check for .exe extension
        char *dot = strrchr(filename, '.');
        if(dot) {
            long l = dot - filename;
            memcpy(buffer, filename, l);
            filename = buffer + l;
        }
        // or just append .ini if no extension is present
        else {
            size_t sl = strlen(filename);
            memcpy(buffer, filename, sl);
            filename = buffer + sl;
        }
        *filename++ = '.';
        *filename++ = 'i';
        *filename++ = 'n';
        *filename++ = 'i';
        *filename++ = '\0';
        filename = buffer;
        i = ini_parse(filename, config_handler);
        if(i == 0) {
            if(verboseMode) fprintf(stderr, "Loaded config: %s" EOL, filename);
        }
        else if (i > 0) {
            fprintf(stderr, "(ini line %u) Configuration syntax error in gpx.ini: unrecognised paremeters" EOL, i);
            usage();
        }
    }

    // READ COMMAND LINE
    
    // get the command line options
    while ((c = getopt(argc, argv, "psm:c:vx:y:z:")) != -1) {
        switch (c) {
            case 'c':
                config = optarg;
                break;
            case 'm':
                if(strcasecmp(optarg, "c3") == 0) machine = cupcake_G3;
                else if(strcasecmp(optarg, "c4") == 0) machine = cupcake_G4;
                else if(strcasecmp(optarg, "cp4") == 0) machine = cupcake_P4;
                else if(strcasecmp(optarg, "cpp") == 0) machine = cupcake_PP;
                else if(strcasecmp(optarg, "t6") == 0) machine = thing_o_matic_7;
                else if(strcasecmp(optarg, "t7") == 0) machine = thing_o_matic_7;
                else if(strcasecmp(optarg, "t7d") == 0) machine = thing_o_matic_7D;
                else if(strcasecmp(optarg, "r1") == 0) machine = replicator_1;
                else if(strcasecmp(optarg, "r1d") == 0) machine = replicator_1D;
                else if(strcasecmp(optarg, "r2") == 0) machine = replicator_2;
                else if(strcasecmp(optarg, "r2x") == 0) machine = replicator_2X;
                else usage();
                break;
            case 'p':
                buildProgress = 1;
                break;
            case 's':
                standard_io = 1;
                break;
            case 'v':
                verboseMode = 1;
                break;
            case 'x':
                userOffset.x = strtod(optarg, NULL);
                break;
            case 'y':
                userOffset.y = strtod(optarg, NULL);
                break;
            case 'z':
                userOffset.z = strtod(optarg, NULL);
                break;
            case '?':
            default:
                usage();
        }
    }
    
    // READ CONFIGURATION
    
    if(config) {
        i = ini_parse(config, config_handler);
        if(i == 0) {
            if(verboseMode) fprintf(stderr, "Loaded config: %s" EOL, config);
        }
        else if (i < 0) {
            fprintf(stderr, "Command line error: cannot load configuration file '%s'" EOL, config);
            usage();
        }
        else if (i > 0) {
            fprintf(stderr, "(line %u) Configuration syntax error in %s: unrecognised paremeters" EOL, i, config);
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
                size_t sl = strlen(filename);
                memcpy(buffer, filename, sl);
                filename = buffer + sl;
            }
            *filename++ = '.';
            *filename++ = 'x';
            *filename++ = '3';
            *filename++ = 'g';
            *filename++ = '\0';
            filename = buffer;
        }
        out = fopen(filename, "wb");
        if(out) {
            if(verboseMode) fprintf(stderr, "Writing to: %s" EOL, filename);
        }
        else {
            perror("Error creating output");
            out = stdout;
            exit(1);
        }
        if(sdCardPath) {
            char sd_filename[300];
            long sl = strlen(sdCardPath);
            if(sdCardPath[sl - 1] == DELIM) {
                sdCardPath[--sl] = 0;
            }
            char *delim = strrchr(filename, DELIM);
            if(delim) {
                memcpy(sd_filename, sdCardPath, sl);
                long l = strlen(delim);
                memcpy(sd_filename + sl, delim, l);
                sd_filename[sl + l] = 0;
            }
            else {
                memcpy(sd_filename, sdCardPath, sl);
                sd_filename[sl++] = DELIM;
                long l = strlen(filename);
                memcpy(sd_filename + sl, filename, l);
                sd_filename[sl + l] = 0;                
            }
            out2 = fopen(sd_filename, "wb");
            if(out2) {
                if(verboseMode) fprintf(stderr, "Writing to: %s" EOL, sd_filename);
            }
        }
    }
    else if(!standard_io) {
        usage();
    }
    
    if(dittoPrinting && machine.extruder_count == 1) {
        fputs("Configuration error: ditto printing cannot access non-existant second extruder" EOL, stderr);
        dittoPrinting = 0;
    }
    
    // CALCULATE FILAMENT SCALING
    
    if(override[A].actual_filament_diameter > 0.0001
       && override[A].actual_filament_diameter != machine.nominal_filament_diameter) {
        set_filament_scale(A, override[A].actual_filament_diameter);
    }
    
    if(override[B].actual_filament_diameter > 0.0001
       && override[B].actual_filament_diameter != machine.nominal_filament_diameter) {
        set_filament_scale(B, override[B].actual_filament_diameter);
    }

    // READ INPUT AND CONVERT TO OUTPUT
    
    // at this point we have read the command line, set the machine definition
    // and both the input and output files are open, so its time to parse the
    // gcode input and convert it to x3g output
    while(fgets(buffer, BUFFER_MAX, in) != NULL) {
        // reset flag state
        command.flag = 0;
        char *digits;
        char *p = buffer; // current parser location
        while(isspace(*p)) p++;
        // check for line number
        if(*p == 'n' || *p == 'N') {
            digits = p;
            p = normalize_word(p);
            if(*p == 0) {
                fprintf(stderr, "(line %u) Syntax error: line number command word 'N' is missing digits" EOL, lineNumber);
                next_line = lineNumber + 1;
            }
            else {
                next_line = lineNumber = atoi(digits);
            }
        }
        else {
            next_line = lineNumber + 1;
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
                                                
                        // Pnnn	 Command parameter, such as a time in milliseconds
                    case 'p':
                    case 'P':
                        command.p = strtod(digits, NULL);
                        command.flag |= P_IS_SET;
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
                        fprintf(stderr, "(line %u) Syntax warning: unrecognised command word '%c'" EOL, lineNumber, c);
                }
            }
            else if(*p == ';') {
                if(*(p + 1) == '@') {
                    char *s = p + 2;
                    if(isalpha(*s)) {
                        char *macro = s;
                        // skip any no space characters
                        while(*s && !isspace(*s)) s++;
                        // null terminate
                        if(*s) *s++ = 0;
                        parse_macro(macro, normalize_comment(s));
                    }
                }
                else {
                    // Comment
                    command.comment = normalize_comment(p + 1);
                    command.flag |= COMMENT_IS_SET;
                }
                *p = 0;
            }
            else if(*p == '(') {
                // Comment
                char *s = strchr(p + 1, '(');
                char *e = strchr(p + 1, ')');
                // check for nested comment
                if(s && e && s < e) {
                    fprintf(stderr, "(line %u) Syntax warning: nested comment detected" EOL, lineNumber);
                    e = strrchr(p + 1, ')');
                }
                if(e) {
                    *e = 0;
                    command.comment = normalize_comment(p + 1);
                    command.flag |= COMMENT_IS_SET;
                    p = e + 1;
                }
                else {
                    fprintf(stderr, "(line %u) Syntax warning: comment is missing closing ')'" EOL, lineNumber);
                    command.comment = normalize_comment(p + 1);
                    command.flag |= COMMENT_IS_SET;
                    *p = 0;                   
                }
            }
            else if(*p == '*') {
                // Checksum
                *p = 0;
                break;
            }
            else if(iscntrl(*p)) {
                break;
            }
            else {
                fprintf(stderr, "(line %u) Syntax error: unrecognised gcode '%s'" EOL, lineNumber, p);
                break;
            }
        }

        // revert to tool selection to current extruder
        selectedExtruder = currentExtruder;

        // change the extruder selection (in virtual tool carosel)
        if(command.flag & T_IS_SET && !dittoPrinting) {
            unsigned extruder_id = (unsigned)command.t;
            if(extruder_id < machine.extruder_count) {
                selectedExtruder = extruder_id;
            }
            else {
                fprintf(stderr, "(line %u) Semantic warning: T%u cannot select non-existant extruder" EOL, lineNumber, extruder_id);
            }
        }

        // we treat E as short hand for A or B being set, depending on the state of the currentExtruder
        
        if(command.flag & E_IS_SET) {
            if(currentExtruder == 0) {
                // a = e
                command.flag |= A_IS_SET;
                command.a = command.e;
            }
            else {
                // b = e
                command.flag |= B_IS_SET;
                command.b = command.e;
            }
        }
        
        // INTERPRET COMMAND
        
        if(command.flag & G_IS_SET) {
            switch(command.g) {
                    // G0 - Rapid Positioning
                case 0:
                    if(command.flag & F_IS_SET) {
                        do_pause_at_zpos = calculate_target_position();
                        queue_ext_point(currentFeedrate);
                        command_emitted++;
                        currentPosition = targetPosition;
                        positionKnown = 1;
                    }
                    else {
                        Point3d delta;
                        do_pause_at_zpos = calculate_target_position();
                        if(command.flag & X_IS_SET) delta.x = fabs(targetPosition.x - currentPosition.x);
                        if(command.flag & Y_IS_SET) delta.y = fabs(targetPosition.y - currentPosition.y);
                        if(command.flag & Z_IS_SET) delta.z = fabs(targetPosition.z - currentPosition.z);
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
                        queue_ext_point(feedrate);
                        command_emitted++;
                        currentPosition = targetPosition;
                        positionKnown = 1;
                    }
                    break;
                    
                    // G1 - Coordinated Motion
                case 1:
                    do_pause_at_zpos = calculate_target_position();
                    queue_ext_point(currentFeedrate);
                    command_emitted++;
                    currentPosition = targetPosition;
                    positionKnown = 1;
                    break;
                    
                    // G2 - Clockwise Arc
                    // G3 - Counter Clockwise Arc
                    
                    // G4 - Dwell
                case 4:
                    if(command.flag & P_IS_SET) {
#if ENABLE_SIMULATED_RPM
                        if(tool[currentExtruder].motor_enabled && tool[currentExtruder].rpm) {
                            do_pause_at_zpos = calculate_target_position();
                            queue_new_point(command.p);
                            command_emitted++;
                        }
                        else
#endif
                        {
                            delay(command.p);
                            command_emitted++;
                        }

                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax error: G4 is missing delay parameter, use Pn where n is milliseconds" EOL, lineNumber);
                    }
                    break;

                    // G10 - Create Coordinate System Offset from the Absolute one
                case 10:
                    if(command.flag & P_IS_SET && command.p >= 1.0 && command.p <= 6.0) {
                        i = (int)command.p;
                        if(command.flag & X_IS_SET) offset[i].x = command.x;
                        if(command.flag & Y_IS_SET) offset[i].y = command.y;
                        if(command.flag & Z_IS_SET) offset[i].z = command.z;
                        // set standby temperature
                        if(command.flag & R_IS_SET) {
                            switch(i) {
                                case 1:
                                    override[A].standby_temperature = (unsigned)command.r;
                                    break;
                                case 2:
                                    override[B].standby_temperature = (unsigned)command.r;
                                    break;
                            }
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax error: G10 is missing coordiante system, use Pn where n is 1-6" EOL, lineNumber);
                    }
                    break;
                
                    // G21 - Use Milimeters as Units (IGNORED)
                    // G71 - Use Milimeters as Units (IGNORED)
                case 21:
                case 71:
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
                        fprintf(stderr, "(line %u) Semantic error: G91 switch to relitive positioning prior to first absolute move" EOL, lineNumber);
                        exit(1);
                    }
                    break;

                    // G92 - Define current position on axes
                case 92: {
                    if(command.flag & X_IS_SET) currentPosition.x = command.x;
                    if(command.flag & Y_IS_SET) currentPosition.y = command.y;
                    if(command.flag & Z_IS_SET) currentPosition.z = command.z;
                    if(command.flag & A_IS_SET) currentPosition.a = command.a;
                    if(command.flag & B_IS_SET) currentPosition.b = command.b;
                    set_position();
                    command_emitted++;
                    // check if we know where we are
                    int mask = machine.extruder_count == 1 ? (XYZ_BIT_MASK | A_IS_SET) : AXES_BIT_MASK;
                    if((command.flag & mask) == mask) positionKnown = 1;
                    break;
                }
                    
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
                    if(command.flag & F_IS_SET) currentFeedrate = command.f;
                    home_axes(ENDSTOP_IS_MIN);
                    command_emitted++;
                    positionKnown = 0;
                    excess.a = 0;
                    excess.b = 0;
                    break;
                    // G28 - Home given axes to maximum
                    // G162 - Home given axes to maximum
                case 28:
                case 162:
                    if(command.flag & F_IS_SET) currentFeedrate = command.f;
                    home_axes(ENDSTOP_IS_MAX);
                    command_emitted++;
                    positionKnown = 0;
                    excess.a = 0;
                    excess.b = 0;
                    break;
                default:
                    fprintf(stderr, "(line %u) Syntax warning: unsupported gcode command 'G%u'" EOL, lineNumber, command.g);
            }
        }
        else if(command.flag & M_IS_SET) {
            switch(command.m) {                    
                    // M2 - End program
                case 2:
                    if(program_is_running()) {
                        end_program();
                        set_build_progress(100);
                        end_build();
                        set_steppers(AXES_BIT_MASK, 0);
                    }
                    exit(0);
                    
                    
                    // M6 - Tool change AND wait for extruder AND build platfrom to reach (or exceed) temperature
                case 6:
                    if(!dittoPrinting && selectedExtruder != currentExtruder) {
                        int timeout = command.flag & P_IS_SET ? (int)command.p : 0xFFFF;
                        do_tool_change(timeout);
                        command_emitted++;
                    }
#if M6_TOOL_CHANGE_ONLY
                    break;
#endif
                    // M116 - WAIT: for extruder AND build platfrom to reach (or exceed) temperature
                case 116: {
                    int timeout = command.flag & P_IS_SET ? (int)command.p : 0xFFFF;
                    // changing the
                    if(dittoPrinting) {
                        if(tool[A].nozzle_temperature > 0) {
                            wait_for_extruder(A, timeout);
                        }
                        if(tool[B].nozzle_temperature > 0) {
                            wait_for_extruder(B, timeout);
                        }
                        command_emitted++;
                    }
                    else {
                        // any tool changes have already occured
                        if(tool[currentExtruder].nozzle_temperature > 0) {
                            wait_for_extruder(currentExtruder, timeout);
                            command_emitted++;
                        }
                    }
                    // if we have a HBP wait for that too
                    if(machine.a.has_heated_build_platform && tool[A].build_platform_temperature > 0) {
                        wait_for_build_platform(A, timeout);
                        command_emitted++;
                    }
                    if(machine.b.has_heated_build_platform && tool[B].build_platform_temperature > 0) {
                        wait_for_build_platform(B, timeout);
                        command_emitted++;
                    }
                    break;
                }

                    // M17 - Enable axes steppers
                case 17:
                    if(command.flag & AXES_BIT_MASK) {
                        set_steppers(command.flag & AXES_BIT_MASK, 1);
                        command_emitted++;
                        if(command.flag & A_IS_SET) tool[A].motor_enabled = 1;
                        if(command.flag & B_IS_SET) tool[B].motor_enabled = 1;
                    }
                    else {
                        set_steppers(machine.extruder_count == 1 ? (XYZ_BIT_MASK | A_IS_SET) : AXES_BIT_MASK, 1);
                        command_emitted++;
                        tool[A].motor_enabled = 1;
                        if(machine.extruder_count == 2) tool[B].motor_enabled = 1;
                    }
                    break;
                    
                    // M18 - Disable axes steppers
                case 18:
                    if(command.flag & AXES_BIT_MASK) {
                        set_steppers(command.flag & AXES_BIT_MASK, 0);
                        command_emitted++;
                        if(command.flag & A_IS_SET) tool[A].motor_enabled = 0;
                        if(command.flag & B_IS_SET) tool[B].motor_enabled = 0;
                    }
                    else {
                        set_steppers(machine.extruder_count == 1 ? (XYZ_BIT_MASK | A_IS_SET) : AXES_BIT_MASK, 0);
                        command_emitted++;
                        tool[A].motor_enabled = 0;
                        if(machine.extruder_count == 2) tool[B].motor_enabled = 0;
                    }
                    break;
                    
                    // M70 - Display message on LCD
                case 70:
                    if(command.flag & COMMENT_IS_SET) {
                        unsigned vPos = command.flag & Y_IS_SET ? (unsigned)command.y : 0;
                        if(vPos > 3) vPos = 3;
                        unsigned hPos = command.flag & X_IS_SET ? (unsigned)command.x : 0;
                        if(hPos > 19) hPos = 19;
                        if(command.flag & P_IS_SET) {
                            display_message(command.comment, vPos, hPos, command.p, 0);
                        }
                        else {
                            display_message(command.comment, vPos, hPos, 0, 0);
                        }
                        command_emitted++;
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax error: M70 is missing message text, use (text) where text is message" EOL, lineNumber);                        
                    }
                    break;

                    // M71 - Display message and wait for button press
                case 71: {
                    unsigned vPos = command.flag & Y_IS_SET ? (unsigned)command.y : 0;
                    if(vPos > 3) vPos = 3;
                    unsigned hPos = command.flag & X_IS_SET ? (unsigned)command.x : 0;
                    if(hPos > 19) hPos = 19;
                    if(command.flag & COMMENT_IS_SET) {
                        if(command.flag & P_IS_SET) {
                            display_message(command.comment, vPos, hPos, command.p, 1);
                        }
                        else {
                            display_message(command.comment, vPos, hPos, 0, 1);
                        }
                    }
                    else {
                        if(command.flag & P_IS_SET) {
                            display_message("Press M to continue", vPos, hPos, command.p, 1);
                        }
                        else {
                            display_message("Press M to continue", vPos, hPos, 0, 1);
                        }
                    }
                    command_emitted++;
                    break;
                }
                    
                    // M72 - Queue a song or play a tone
                case 72:
                    if(command.flag & P_IS_SET) {
                        unsigned song_id = (unsigned)command.p;
                        if(song_id > 2) song_id = 2;
                        queue_song(song_id);
                        command_emitted++;
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax warning: M72 is missing song number, use Pn where n is 0-2" EOL, lineNumber);
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
                            set_build_progress(0);
                            // start extruder in a known state
                            change_extruder_offset(currentExtruder);
                        }
                        else if(program_is_running()) {
                            if(percent == 100) {
                                end_program();
                                set_build_progress(100);
                                end_build();
                            }
                            else if(filesize == 0 || buildProgress == 0) {
                                set_build_progress(percent);
                            }
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax warning: M73 is missing build percentage, use Pn where n is 0-100" EOL, lineNumber);
                    }
                    break;
                    
                    // M82 - set extruder to absolute mode
                case 82:
                    extruderIsRelative = 0;
                    break;
                    
                    // M83 - set extruder to relative mode
                case 83:
                    extruderIsRelative = 1;
                    break;
                    
                    // M84 - Stop idle hold
                case 84:
                    set_steppers(machine.extruder_count == 1 ? (XYZ_BIT_MASK | A_IS_SET) : AXES_BIT_MASK, 0);
                    command_emitted++;
                    tool[A].motor_enabled = 0;
                    if(machine.extruder_count == 2) tool[B].motor_enabled = 0;
                    break;
                    
                    // M101 - Turn extruder on, forward
                    // M102 - Turn extruder on, reverse
                case 101:
                case 102:
                    if(dittoPrinting) {
                        set_steppers(A_IS_SET|B_IS_SET, 1);
                        command_emitted++;
                        tool[A].motor_enabled = tool[B].motor_enabled = command.m == 101 ? 1 : -1;
                    }
                    else {
                        set_steppers(selectedExtruder == 0 ? A_IS_SET : B_IS_SET, 1);
                        command_emitted++;
                        tool[selectedExtruder].motor_enabled = command.m == 101 ? 1 : -1;
                    }
                    break;
                    
                    // M103 - Turn extruder off
                case 103:
                    if(dittoPrinting) {
                        set_steppers(A_IS_SET|B_IS_SET, 1);
                        command_emitted++;
                        tool[A].motor_enabled = tool[B].motor_enabled = 0;
                    }
                    else {
                        set_steppers(selectedExtruder == 0 ? A_IS_SET : B_IS_SET, 0);
                        command_emitted++;
                        tool[selectedExtruder].motor_enabled = 0;
                    }
                    break;
                    
                    // M104 - Set extruder temperature
                case 104:
                    if(command.flag & S_IS_SET) {
                        unsigned temperature = (unsigned)command.s;
                        if(temperature > 260) temperature = 260;
                        if(dittoPrinting) {
                            if(temperature && override[currentExtruder].active_temperature) {
                                temperature = override[currentExtruder].active_temperature;
                            }
                            set_nozzle_temperature(A, temperature);
                            set_nozzle_temperature(B, temperature);
                            command_emitted++;
                            tool[A].nozzle_temperature = tool[B].nozzle_temperature = temperature;
                        }
                        else {
                            if(temperature && override[selectedExtruder].active_temperature) {
                                temperature = override[selectedExtruder].active_temperature;
                            }
                            set_nozzle_temperature(selectedExtruder, temperature);
                            command_emitted++;
                            tool[selectedExtruder].nozzle_temperature = temperature;
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax error: M104 is missing temperature, use Sn where n is 0-260" EOL, lineNumber);
                    }
                    break;
                    
                    // M106 - Turn cooling fan on
                case 106: {
                    int state = (command.flag & S_IS_SET) ? ((unsigned)command.s ? 1 : 0) : 1;
                    if(dittoPrinting) {
                        set_fan(A, state);
                        set_fan(B, state);
                        command_emitted++;
                    }
                    else {
                        set_fan(selectedExtruder, state);
                        command_emitted++;
                    }
                    break;
                }
                    
                    // M107 - Turn cooling fan off
                case 107:
                    if(dittoPrinting) {
                        set_fan(A, 0);
                        set_fan(B, 0);
                        command_emitted++;
                    }
                    else {
                        set_fan(selectedExtruder, 0);
                        command_emitted++;
                    }
                    break;
                    
                    // M108 - set extruder motor 5D 'simulated' RPM
                case 108:
#if ENABLE_SIMULATED_RPM
                    if(command.flag & R_IS_SET) {
                        if(dittoPrinting) {
                            tool[A].rpm = tool[B].rpm = command.r;
                        }
                        else {
                            tool[selectedExtruder].rpm = command.r;
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax error: M108 is missing motor RPM, use Rn where n is 0-5" EOL, lineNumber);
                    }
#endif
                    break;
                    
                    
                    // M109 - Set Extruder Temperature and Wait
                case 109:
                    if(command.flag & S_IS_SET) {
                        int timeout = command.flag & P_IS_SET ? (int)command.p : 0xFFFF;
                        unsigned temperature = (unsigned)command.s;
                        if(temperature > 260) temperature = 260;
                        if(dittoPrinting) {
                            unsigned tempB = temperature;
                            // set extruder temperatures
                            if(temperature) {
                                if(override[A].active_temperature) {
                                    temperature = override[A].active_temperature;
                                }
                                if(override[B].active_temperature) {
                                    tempB = override[B].active_temperature;
                                }
                            }
                            set_nozzle_temperature(A, temperature);
                            set_nozzle_temperature(B, tempB);
                            tool[A].nozzle_temperature = temperature;
                            tool[B].nozzle_temperature = tempB;
                            // wait for extruders to reach (or exceed) temperature
                            if(tool[A].nozzle_temperature > 0) {
                                wait_for_extruder(A, timeout);
                            }
                            if(tool[B].nozzle_temperature > 0) {
                                wait_for_extruder(B, timeout);
                            }
                            command_emitted++;
                        }
                        else {
                            // set extruder temperature
                            if(temperature && override[selectedExtruder].active_temperature) {
                                temperature = override[selectedExtruder].active_temperature;
                            }
                            set_nozzle_temperature(selectedExtruder, temperature);
                            tool[selectedExtruder].nozzle_temperature = temperature;
                            // wait for extruder to reach (or exceed) temperature
                            if(tool[selectedExtruder].nozzle_temperature > 0) {
                                wait_for_extruder(selectedExtruder, timeout);
                            }
                            command_emitted++;
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax error: M109 is missing temperature, use Sn where n is 0-260" EOL, lineNumber);
                    }

                    // M140 - Set Build Platform Temperature (skeinforge)
                case 140:
                    if(machine.a.has_heated_build_platform || machine.b.has_heated_build_platform) {
                        if(command.flag & S_IS_SET) {
                            unsigned temperature = (unsigned)command.s;
                            if(temperature > 160) temperature = 160;
                            unsigned extruder_id = machine.a.has_heated_build_platform ? A : B;
                            if(command.flag & T_IS_SET) {
                                extruder_id = selectedExtruder;
                            }
                            if(extruder_id ? machine.b.has_heated_build_platform : machine.a.has_heated_build_platform) {
                                if(temperature && override[extruder_id].build_platform_temperature) {
                                    temperature = override[extruder_id].build_platform_temperature;
                                }
                                set_build_platform_temperature(extruder_id, temperature);
                                command_emitted++;
                                tool[extruder_id].build_platform_temperature = temperature;
                            }
                            else {
                                fprintf(stderr, "(line %u) Semantic warning: M%u cannot select non-existant heated build platform T%u" EOL, lineNumber, command.m, extruder_id);
                            }
                        }
                        else {
                            fprintf(stderr, "(line %u) Syntax error: M%u is missing temperature, use Sn where n is 0-160" EOL, lineNumber, command.m);
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Semantic warning: M%u cannot select non-existant heated build platform" EOL, lineNumber, command.m);
                    }
                    break;

                    // M126 - Turn blower fan on (valve open)
                case 126: {
                    int state = (command.flag & S_IS_SET) ? ((unsigned)command.s ? 1 : 0) : 1;
                    if(dittoPrinting) {
                        set_valve(A, state);
                        set_valve(B, state);
                        command_emitted++;
                    }
                    else {
                        set_valve(selectedExtruder, state);
                        command_emitted++;
                    }
                    break;
                }

                    // M127 - Turn blower fan off (valve close)
                case 127:
                    if(dittoPrinting) {
                        set_valve(A, 0);
                        set_valve(B, 0);
                        command_emitted++;
                    }
                    else {
                        set_valve(selectedExtruder, 0);
                        command_emitted++;
                    }
                    break;
                    
                    // M131 - Store Current Position to EEPROM
                case 131:
                    if(command.flag & AXES_BIT_MASK) {
                        store_home_positions();
                        command_emitted++;
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax error: M131 is missing axes, use X Y Z A B" EOL, lineNumber);
                    }
                    break;
                    
                    // M132 - Load Current Position from EEPROM
                case 132:
                    if(command.flag & AXES_BIT_MASK) {
                        recall_home_positions();
                        command_emitted++;
                        positionKnown = 0;
                        excess.a = 0;
                        excess.b = 0;
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax error: M132 is missing axes, use X Y Z A B" EOL, lineNumber);
                    }
                    break;
                    
                    // M190 - Wait for build platform to reach (or exceed) temperature
                case 190: {
                    if(machine.a.has_heated_build_platform || machine.b.has_heated_build_platform) {
                        int timeout = command.flag & P_IS_SET ? (int)command.p : 0xFFFF;
                        unsigned extruder_id = machine.a.has_heated_build_platform ? A : B;
                        if(command.flag & T_IS_SET) {
                            extruder_id = selectedExtruder;
                        }
                        if(extruder_id ? machine.b.has_heated_build_platform : machine.a.has_heated_build_platform) {
                            wait_for_build_platform(extruder_id, timeout);
                            command_emitted++;
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic warning: M190 cannot select non-existant heated build platform T%u" EOL, lineNumber, extruder_id);
                        }                        
                    }
                    else {
                        fprintf(stderr, "(line %u) Semantic warning: M190 cannot select non-existant heated build platform" EOL, lineNumber);
                    }
                    break;
                }

                    // M300 - Set Beep (SP)
                case 300: {
                    unsigned frequency = 300;
                    if(command.flag & S_IS_SET) frequency = (unsigned)command.s & 0xFFFF;
                    unsigned milliseconds = 1000;
                    if(command.flag & P_IS_SET) milliseconds = (unsigned)command.p & 0xFFFF;
                    set_beep(frequency, milliseconds);
                    command_emitted++;
                    break;
                }
                    
                    // M320 - Acceleration on for subsequent instructions
                case 320:
                    set_acceleration(1);
                    command_emitted++;
                    break;
                    
                    // M321 - Acceleration off for subsequent instructions
                case 321:
                    set_acceleration(0);
                    command_emitted++;
                    break;
                    
                    // M322 - Pause @ zPos
                case 322:
                    if(command.flag & Z_IS_SET) {
                        double z = isRelative ? (currentPosition.z + command.z) : (command.z + offset[currentOffset].z + userOffset.z);
                        pause_at_zpos(z);
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax warning: M322 is missing Z axis" EOL, lineNumber);
                    }
                    command_emitted++;
                    break;
                    
                    // M420 - Set RGB LED value (REB - P)
                case 420: {
                    unsigned red = 0;
                    if(command.flag & R_IS_SET) red = (unsigned)command.r & 0xFF;
                    unsigned green = 0;
                    if(command.flag & E_IS_SET) green = (unsigned)command.e & 0xFF;
                    unsigned blue = 0;
                    if(command.flag & B_IS_SET) blue = (unsigned)command.b & 0xFF;
                    unsigned blink = 0;
                    if(command.flag & P_IS_SET) blink = (unsigned)command.p & 0xFF;
                    set_LED(red, green, blue, blink);
                    command_emitted++;
                    break;
                }
                    
                default:
                    fprintf(stderr, "(line %u) Syntax warning: unsupported mcode command 'M%u'" EOL, lineNumber, command.m);
            }
        }
        else {
            if(command.flag & AXES_BIT_MASK) {
                do_pause_at_zpos = calculate_target_position();
                queue_ext_point(currentFeedrate);
                command_emitted++;
                currentPosition = targetPosition;
                positionKnown = 1;
            }
            else if(!dittoPrinting && selectedExtruder != currentExtruder) {
                int timeout = command.flag & P_IS_SET ? (int)command.p : 0xFFFF;
                do_tool_change(timeout);
                command_emitted++;
            }
        }
        // check for pending pause @ zPos
        if(do_pause_at_zpos) {
            pause_at_zpos(commandAt[commandAtIndex].z);
            do_pause_at_zpos = 0;
        }
        // update progress
        if(filesize && buildProgress && command_emitted) {
            unsigned percent = (unsigned)round(100.0 * (double)ftell(in) / (double)filesize);
            if(percent > progress) {
                if(program_is_ready()) {
                    start_program();
                    start_build();
                    set_build_progress(0);
                    // start extruder in a known state
                    change_extruder_offset(currentExtruder);
                }
                else if(percent < 100 && program_is_running()) {
                    set_build_progress(percent);
                    progress = percent;
                }
                command_emitted = 0;
            }
        }
        lineNumber = next_line;
    }
    
    if(program_is_running()) {
        end_program();
        set_build_progress(100);
        end_build();
    }
    set_steppers(AXES_BIT_MASK, 0);
    
    exit(0);
}

