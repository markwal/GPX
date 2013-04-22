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
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
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
static void set_nozzle_temperature(unsigned extruder_id, unsigned temperature);
static void set_LED_RGB(unsigned rgb, unsigned blink);
static void pause_at_zpos(float z_positon);

// GLOBAL VARIABLES

Command command;            // the gcode command line
Point5d currentPosition;    // the current position of the extruder in 5D space
Point5d targetPosition;     // the target poaition the extruder will move to (including G10 offsets)
Point2d excess;             // the accumulated rounding error in mm to step conversion
int currentExtruder;        // the currently selectd extruder using Tn
double currentFeedrate;     // the current feed rate
int currentOffset;          // current G10 offset
Point3d offset[7];          // G10 offsets
Tool tool[2];               // tool state
Override override[2];       // gcode override
int isRelative;             // signals relitive or absolute coordinates
int positionKnown;          // is the current extruder position known
int programState;           // gcode program state used to trigger start and end code sequences
int dittoPrinting;          // enable ditto printing
int buildProgress;          // override build percent
unsigned lineNumber;        // the current line number in the gcode file
static char buffer[300];    // the statically allocated parse-in-place buffer

Filament filament[FILAMENT_MAX];
int filamentLength;

PauseAt pauseAt[PAUSE_AT_MAX];
int pauseAtIndex;
int pauseAtLength;

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
    
    currentExtruder = 0;
    
    for(i = 0; i < 2; i++) {
        tool[i].motor_enabled = 0;
#if ENABLE_RPM
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
    positionKnown = 0;
    programState = 0;

    dittoPrinting = 0;
    buildProgress = 0;
    
    lineNumber = 1;
    
    filament[0].colour = "_null_";
    filament[0].diameter = 0.0;
    filament[0].temperature = 0;
    filament[0].LED = 0;
    filamentLength = 1;

    pauseAtIndex = 0;
    pauseAtLength = 0;
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

// Custom machine definition ini handler

#define SECTION_IS(s) strcasecmp(section, s) == 0
#define NAME_IS(n) strcasecmp(name, n) == 0
#define VALUE_IS(v) strcasecmp(value, v) == 0

static int config_handler(void* user, const char* section, const char* name, const char* value)
{
    if(SECTION_IS("printer")) {
        if(NAME_IS("ditto_printing")) dittoPrinting = atoi(value);
        else if(NAME_IS("build_progress")) buildProgress = atoi(value);
        else if(NAME_IS("nominal_filament_diameter")
                || NAME_IS("slicer_filament_diameter")) machine.nominal_filament_diameter = strtod(value, NULL);
        else if(NAME_IS("machine_type")) {
            // use on-board machine definition
            if(VALUE_IS("r1")) machine = replicator_1;
            else if(VALUE_IS("r1d")) machine = replicator_1D;
            else if(VALUE_IS("r2")) machine = replicator_2;
            else if(VALUE_IS("r2x")) machine = replicator_2X;
            else {
                fprintf(stderr, "Configuration error: unrecognised machine type '%s'" EOL, value);
            }
        }
        else if(NAME_IS("build_platform_temperature")) {
            if(machine.a.has_heated_build_platform) override[A].build_platform_temperature = atoi(value);
            else if(machine.b.has_heated_build_platform) override[B].build_platform_temperature = atoi(value);
        }
        else return 0;
    }
    else if(SECTION_IS("x")) {
        if(NAME_IS("max_feedrate")) machine.x.max_feedrate = strtod(value, NULL);
        else if(NAME_IS("home_feedrate")) machine.x.home_feedrate = strtod(value, NULL);
        else if(NAME_IS("steps_per_mm")) machine.x.steps_per_mm = strtod(value, NULL);
        else if(NAME_IS("endstop")) machine.x.endstop = atoi(value);
        else return 0;
    }
    else if(SECTION_IS("y")) {
        if(NAME_IS("max_feedrate")) machine.y.max_feedrate = strtod(value, NULL);
        else if(NAME_IS("home_feedrate")) machine.y.home_feedrate = strtod(value, NULL);
        else if(NAME_IS("steps_per_mm")) machine.y.steps_per_mm = strtod(value, NULL);
        else if(NAME_IS("endstop")) machine.y.endstop = atoi(value);
        else return 0;
    }
    else if(SECTION_IS("z")) {
        if(NAME_IS("max_feedrate")) machine.z.max_feedrate = strtod(value, NULL);
        else if(NAME_IS("home_feedrate")) machine.z.home_feedrate = strtod(value, NULL);
        else if(NAME_IS("steps_per_mm")) machine.z.steps_per_mm = strtod(value, NULL);
        else if(NAME_IS("endstop")) machine.z.endstop = atoi(value);
        else return 0;
    }
    else if(SECTION_IS("a")) {
        if(NAME_IS("max_feedrate")) machine.a.max_feedrate = strtod(value, NULL);
        else if(NAME_IS("steps_per_mm")) machine.a.steps_per_mm = strtod(value, NULL);
        else if(NAME_IS("motor_steps")) machine.a.motor_steps = strtod(value, NULL);
        else if(NAME_IS("has_heated_build_platform")) machine.a.has_heated_build_platform = atoi(value);
        else return 0;
    }
    else if(SECTION_IS("right")) {
        if(NAME_IS("active_temperature")
           || NAME_IS("nozzle_temperature")) override[A].active_temperature = atoi(value);
        else if(NAME_IS("standby_temperature")) override[A].standby_temperature = atoi(value);
        else if(NAME_IS("build_platform_temperature")) override[A].build_platform_temperature = atoi(value);
        else if(NAME_IS("actual_filament_diameter")) override[A].actual_filament_diameter = strtod(value, NULL);
        else return 0;
    }
    else if(SECTION_IS("b")) {
        if(NAME_IS("max_feedrate")) machine.b.max_feedrate = strtod(value, NULL);
        else if(NAME_IS("steps_per_mm")) machine.b.steps_per_mm = strtod(value, NULL);
        else if(NAME_IS("motor_steps")) machine.b.motor_steps = strtod(value, NULL);
        else if(NAME_IS("has_heated_build_platform")) machine.b.has_heated_build_platform = atoi(value);
        else return 0;
    }
    else if(SECTION_IS("left")) {
        if(NAME_IS("active_temperature")
           || NAME_IS("nozzle_temperature")) override[B].active_temperature = atoi(value);
        else if(NAME_IS("standby_temperature")) override[B].standby_temperature = atoi(value);
        else if(NAME_IS("build_platform_temperature")) override[B].build_platform_temperature = atoi(value);
        else if(NAME_IS("actual_filament_diameter")) override[B].actual_filament_diameter = strtod(value, NULL);
        else return 0;
    }
    else if(SECTION_IS("machine")) {
        if(NAME_IS("nominal_filament_diameter")
           || NAME_IS("slicer_filament_diameter")) machine.nominal_filament_diameter = strtod(value, NULL);
        else if(NAME_IS("extruder_count")) machine.extruder_count = atoi(value);
        else if(NAME_IS("timeout")) machine.timeout = atoi(value);
        else return 0;
    }
    else {
        return 0; // unknown section/name, error
    }
    return 1;
}

// PAUSE @ ZPOS FUNCTIONS

// find an existing filament definition

static int find_filament(char *filament_id)
{
    int index = -1;
    // a brute force search is generally fastest for low n
    for(int i = 0; i < filamentLength; i++) {
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

// append a new pause at z function

static int add_pause_at(double z, char *filament_id)
{
    static double previous_z = 0.0;
    if(z <= previous_z) {
        fprintf(stderr, "(line %u) Semantic error: @pause at z pos %f lower than previous z %f" EOL, lineNumber, z, previous_z);
        exit(1);
    }
    int index = filament_id ? find_filament(filament_id) : 0;
    if(index < 0) {
        fprintf(stderr, "(line %u) Semantic error: @pause macro with undefined filament name '%s', use a @filament macro to define it" EOL, lineNumber, filament_id);
        index = 0;
    }
    if(pauseAtLength < PAUSE_AT_MAX) {
        pauseAt[pauseAtLength].z = z;
        pauseAt[pauseAtLength].filament_index = index;
        pauseAtLength++;
        if(pauseAtLength == 1) {
            pause_at_zpos(z);
        }
    }
    else {
        fprintf(stderr, "(line %u) Buffer overflow: too many @pause definitions (maximum = %i)" EOL, lineNumber, PAUSE_AT_MAX);
        index = 0;
    }
    return index;
}

// 5D VECTOR FUNCTIONS

// compute the filament scaling factor

static void set_filament_scale(unsigned extruder_id, double filament_diameter) {
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

// calculate target position

static int calculate_target_position(void)
{
    int do_pause_at_zpos = 0;

    // CALCULATE TARGET POSITION
    
    // x
    if(command.flag & X_IS_SET) {
        targetPosition.x = isRelative ? (currentPosition.x + command.x) : (command.x + offset[currentOffset].x);
    }
    else {
        targetPosition.x = currentPosition.x;
    }
    
    // y
    if(command.flag & Y_IS_SET) {
        targetPosition.y = isRelative ? (currentPosition.y + command.y) : (command.y + offset[currentOffset].y);
    }
    else {
        targetPosition.y = currentPosition.y;
    }
    
    // z
    if(command.flag & Z_IS_SET) {
        targetPosition.z = isRelative ? (currentPosition.z + command.z) : (command.z + offset[currentOffset].z);
    }
    else {
        targetPosition.z = currentPosition.z;
    }
    
    // a
    if(command.flag & A_IS_SET) {
        targetPosition.a = isRelative ? (currentPosition.a + command.a) : command.a;
    }
    else {
        targetPosition.a = currentPosition.a;
    }
    // b
    if(command.flag & B_IS_SET) {
        targetPosition.b = isRelative ? (currentPosition.b + command.b) : command.b;
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
    
    // CHECK FOR PAUSE @ Z POS
    
    if(pauseAtIndex < pauseAtLength) {
        // check if the next command will cross the threshold
        if(pauseAt[pauseAtIndex].z <= targetPosition.z) {
            int index = pauseAt[pauseAtIndex].filament_index;
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
            if(filament[index].temperature
               && tool[currentExtruder].nozzle_temperature != filament[index].temperature) {
                if(dittoPrinting) {
                    set_nozzle_temperature(A, filament[index].temperature);
                    set_nozzle_temperature(B, filament[index].temperature);
                    tool[A].nozzle_temperature = tool[B].nozzle_temperature = filament[index].temperature;
                }
                else {
                    set_nozzle_temperature(currentExtruder, filament[index].temperature);
                    tool[currentExtruder].nozzle_temperature = filament[index].temperature;
                }
            }
            // override LED colour
            if(filament[index].LED) {
                set_LED_RGB(filament[index].LED, 0);
            }
            pauseAtIndex++;
            if(pauseAtIndex < pauseAtLength) {
                do_pause_at_zpos = 1;
            }
        }
    }
    
    // SCALE FILAMENT INDEPENDENTLY
    
    if(command.flag & A_IS_SET && override[A].filament_scale != 1.0) targetPosition.a *= override[A].filament_scale;
    if(command.flag & B_IS_SET && override[B].filament_scale != 1.0) targetPosition.b *= override[B].filament_scale;

    return do_pause_at_zpos;
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

// 134 - Change extruder offset

// This is important to use on dual-head Replicators, because the machine needs to know
// the current toolhead in order to apply a calibration offset.

static void change_extruder_offset(unsigned extruder_id)
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

// Action 03 - Set extruder target temperature

static void set_nozzle_temperature(unsigned extruder_id, unsigned temperature)
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

// Action 12 - Enable / Disable fan

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

// Action 13 - Enable / Disable extra output (blower fan)

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

// Action 31 - Set build platform target temperature

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

// 137 - Enable / Disable axes steppers

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
 
// 139 - Queue absolute point

static void queue_absolute_point()
{
    long longestDDA = get_longest_dda();
    Point5d steps = mm_to_steps(&targetPosition, &excess);
    
    if(write_8(139) == EOF) exit(1);
    
    // int32: X coordinate, in steps
    if(write_32((int)steps.x) == EOF) exit(1);
    
    // int32: Y coordinate, in steps
    if(write_32((int)steps.y) == EOF) exit(1);
    
    // int32: Z coordinate, in steps
    if(write_32((int)steps.z) == EOF) exit(1);
    
    // int32: A coordinate, in steps
    if(write_32(-(int)steps.a) == EOF) exit(1);
    
    // int32: B coordinate, in steps
    if(write_32(-(int)steps.b) == EOF) exit(1);
    
    // uint32: Feedrate, in microseconds between steps on the max delta. (DDA)
    if(write_32((int)longestDDA) == EOF) exit(1);
}

// 140 - Set extended position

static void set_position()
{
    Point5d steps = mm_to_steps(&currentPosition, NULL);
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

#if ENABLE_RPM
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

    if(write_8(142) == EOF) exit(1);
    
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
    
    // uint32: Duration of the movement, in microseconds
    if(write_32(milliseconds * 1000) == EOF) exit(1);
    
    // uint8: Axes bitfield to specify which axes are relative. Any axis with a bit set should make a relative movement.
    if(write_8(AXES_BIT_MASK) == EOF) exit(1);
}
#endif

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

static void set_LED(unsigned red, unsigned green, unsigned blue, unsigned blink)
{
    if(write_8(146) == EOF) exit(1);
    
    // uint8: red value (all pix are 0-255)
    if(write_8(red) == EOF) exit(1);
    
    // uint8: green
    if(write_8(green) == EOF) exit(1);
    
    // uint8: blue
    if(write_8(blue) == EOF) exit(1);
    
    // uint8: blink rate (0-255 valid)
    if(write_8(blink) == EOF) exit(1);
    
    // uint8: 0 (reserved for future use)
    if(write_8(0) == EOF) exit(1);
}

static void set_LED_RGB(unsigned rgb, unsigned blink)
{
    if(write_8(146) == EOF) exit(1);
    
    // uint8: red value (all pix are 0-255)
    if(write_8((rgb >> 16) & 0xFF) == EOF) exit(1);
    
    // uint8: green
    if(write_8((rgb >> 8) & 0xFF) == EOF) exit(1);
    
    // uint8: blue
    if(write_8(rgb & 0xFF) == EOF) exit(1);
    
    // uint8: blink rate (0-255 valid)
    if(write_8(blink) == EOF) exit(1);
    
    // uint8: 0 (reserved for future use)
    if(write_8(0) == EOF) exit(1);

}

// 147 - Set Beep

static void set_beep(unsigned frequency, unsigned milliseconds)
{
    if(write_8(147) == EOF) exit(1);
    
    // uint16: frequency
    if(write_16(frequency) == EOF) exit(1);

    // uint16: buzz length in ms
    if(write_16(milliseconds) == EOF) exit(1);

    // uint8: 0 (reserved for future use)
    if(write_8(0) == EOF) exit(1);
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
        long rowLength = length - bytesSent;
        bytesSent += fwrite(message + bytesSent, 1, rowLength < maxLength ? rowLength : maxLength, out);
        if(write_8('\0') == EOF) exit(1);
    }
}

// 150 - Set Build Percentage

static void set_build_progress(unsigned percent)
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
    
    // check that we have actually moved
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

#if ENABLE_RPM
        // if either a or b is 0, but their motor is on, 'simulate' a 5D extrusion distance
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

// 158 - Pause @ zPos

static void pause_at_zpos(float z_positon)
{
    if(write_8(158) == EOF) exit(1);
    
    // uint8: pause at Z coordinate or 0.0 to disable
    if(write_float(z_positon) == EOF) exit(1);
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
 TYPE:=  S+ ('r1' | 'r1d' | 'r2' | 'r2x')
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
 START:= 'start' FILAMENT_ID
 PAUSE:= 'pause' (ZPOS | FILAMENT_ID)+
 ZPOS:= S+ DIGIT+ ('.' DIGIT+)?

 */

#define MACRO_IS(token) strcmp(token, macro) == 0

static void parse_macro(char *p)
{
    char *macro;
    char *name = NULL;
    double z = 0.0;
    double diameter = 0.0;
    unsigned temperature = 0;
    unsigned LED = 0;
    if(!isalpha(*p)) {
        return;
    }
    macro = p;
    p++;
    while(*p && !isspace(*p)) p++;
    if(*p) *p++ = 0;
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
            if(NAME_IS("r1")) machine = replicator_1;
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
    // ;@filament <NAME> <DIAMETER>mm <TEMP>c #<LED-COLOUR> (MESSAGE)
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
        add_pause_at(z, name);
    }
    else if(MACRO_IS("start")) {
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
            index = 0;
        }
    }
}

// display usage and exit

static void usage()
{
    fputs("GPX " GPX_VERSION " Copyright (c) 2013 WHPThomas, All rights reserved." EOL, stderr);
    fputs(EOL "Usage: gpx [-ps] [-m <MACHINE>] [-c <CONFIG>] INPUT [OUTPUT]" EOL, stderr);
    fputs(EOL "Switches:" EOL EOL, stderr);
    fputs("\t-p\toverride build percentage" EOL, stderr);
    fputs("\t-s\tenable stdin and stdout support for command pipes" EOL, stderr);
    fputs(EOL "MACHINE is the predefined machine type" EOL EOL, stderr);
    fputs("\tr1  = Replicator 1 - single extruder" EOL, stderr);
    fputs("\tr1d = Replicator 1 - dual extruder" EOL, stderr);
    fputs("\tr2  = Replicator 2 (default config)" EOL, stderr);
    fputs("\tr2x = Replicator 2X" EOL, stderr);
    fputs(EOL "CONFIG is the filename of a custom machine definition (ini)" EOL, stderr);
    fputs(EOL "INPUT is the name of the sliced gcode input filename" EOL, stderr);
    fputs(EOL "OUTPUT is the name of the x3g output filename" EOL EOL, stderr);
    fputs("This program is free software; you can redistribute it and/or modify" EOL, stderr);
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

    initialize_globals();

    // READ COMMAND LINE
    
    // get the command line options
    while ((c = getopt(argc, argv, "c:m:ps")) != -1) {
        command_emitted++;
        switch (c) {
            case 'c':
                i = ini_parse(optarg, config_handler, NULL);
                if (i < 0) {
                    fprintf(stderr, "Command line error: cannot load custom machine definition '%s'" EOL, optarg);
                    usage();
                }
                else if (i > 0) {
                    fprintf(stderr, "(line %u) Condifuration syntax error: unrecognised paremeters" EOL, i);
                    usage();
                }
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
                buildProgress = 1;
                break;
            case 's':
                standard_io = 1;
                break;
            case '?':
            default:
                usage();
        }
    }
    
    // READ GPX.INI
    
    // if no command line arguments then read gpx.ini file from program directory
    if(command_emitted == 0) {
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
        i = ini_parse(filename, config_handler, NULL);
        if (i > 0) {
            fprintf(stderr, "(line %u) Condifuration syntax error in gpx.ini: unrecognised paremeters" EOL, i);
            usage();
        }
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
        if((out = fopen(filename, "wb")) == NULL) {
            perror("Error creating output");
            out = stdout;
            exit(1);
        }
    }
    else if(!standard_io) {
        usage();
    }
    
    // READ INPUT AND CONVERT TO OUTPUT
    
    // at this point we have read the command line, set the machine definition
    // and both the input and output files are open, so its time to parse the
    // gcode input and convert it to x3g output
    while(fgets(buffer, 256, in) != NULL) {
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
                    parse_macro(normalize_comment(p + 2));
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

        // APPLY ANY TOOL CHANGES
        
        if(command.flag & T_IS_SET && !dittoPrinting) {
            unsigned extruder_id = (unsigned)command.t;
            int timeout = command.flag & P_IS_SET ? (int)command.p : 0xFFFF;
            if(extruder_id < machine.extruder_count) {
                if(currentExtruder != extruder_id) {
                    // set the temperature of current tool to standby (if standby is different to active)
                    if(override[currentExtruder].standby_temperature
                       && override[currentExtruder].standby_temperature != tool[currentExtruder].nozzle_temperature) {
                        unsigned temperature = override[currentExtruder].standby_temperature;
                        set_nozzle_temperature(currentExtruder, temperature);
                        tool[currentExtruder].nozzle_temperature = temperature;
                    }
                    // set the temperature of new tool to active (if active is different to standby)
                    if(override[extruder_id].active_temperature
                       && override[extruder_id].active_temperature != tool[extruder_id].nozzle_temperature) {
                        unsigned temperature = override[extruder_id].active_temperature;
                        set_nozzle_temperature(extruder_id, temperature);
                        tool[extruder_id].nozzle_temperature = temperature;
                        wait_for_extruder(extruder_id, timeout);
                    }
                    // switch any active G10 offset (G54 or G55)
                    if(currentOffset == currentExtruder + 1) {
                        currentOffset = extruder_id + 1;
                    }
                    // change current toolhead in order to apply the calibration offset
                    change_extruder_offset(extruder_id);
                    command_emitted++;
                    // set current extruder so changes in E are expressed as changes to A or B
                    currentExtruder = extruder_id;
                }
            }
            else {
                fprintf(stderr, "(line %u) Semantic warning: T%u cannot select non-existant extruder" EOL, lineNumber, extruder_id);
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
#if ENABLE_RPM
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
            
                    // M6 - Wait for extruder to reach (or exceed) temperature
                case 6: {
                    int timeout = command.flag & P_IS_SET ? (int)command.p : 0xFFFF;
                    // changing the 
                    if(dittoPrinting) {
                        wait_for_extruder(A, timeout);
                        wait_for_extruder(B, timeout);
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
                    
                    // M101 - Turn extruder on, forward
                    // M102 - Turn extruder on, reverse
                case 101:
                case 102:
                    if(dittoPrinting) {
                        set_steppers(A_IS_SET|B_IS_SET, 1);
                        command_emitted++;
                        tool[A].motor_enabled = tool[B].motor_enabled = command.m == 101 ? 1 : -1;
                    }
                    else if(command.flag & T_IS_SET) {
                        unsigned extruder_id = (unsigned)command.t;
                        if(extruder_id < machine.extruder_count) {
                            set_steppers(extruder_id == 0 ? A_IS_SET : B_IS_SET, 1);
                            command_emitted++;
                            tool[extruder_id].motor_enabled = command.m == 101 ? 1 : -1;
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic warning: M%u cannot select non-existant extruder T%u" EOL, lineNumber, command.m, extruder_id);
                        }
                    }
                    else {
                        set_steppers(currentExtruder == 0 ? A_IS_SET : B_IS_SET, 1);
                        command_emitted++;
                        tool[currentExtruder].motor_enabled = command.m == 101 ? 1 : -1;
                    }
                    break;
                    
                    // M103 - Turn extruder off
                case 103:
                    if(dittoPrinting) {
                        set_steppers(A_IS_SET|B_IS_SET, 1);
                        command_emitted++;
                        tool[A].motor_enabled = tool[B].motor_enabled = command.m == 101 ? 1 : -1;
                    }
                    else if(command.flag & T_IS_SET) {
                        unsigned extruder_id = (unsigned)command.t;
                        if(extruder_id < machine.extruder_count) {
                            set_steppers(extruder_id == 0 ? A_IS_SET : B_IS_SET, 0);
                            command_emitted++;
                            tool[extruder_id].motor_enabled = 0;
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic warning: M103 cannot select non-existant extruder T%u" EOL, lineNumber, extruder_id);
                        }
                        
                    }
                    else {
                        set_steppers(currentExtruder == 0 ? A_IS_SET : B_IS_SET, 0);
                        command_emitted++;
                        tool[currentExtruder].motor_enabled = 0;
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
                        else if(command.flag & T_IS_SET) {
                            unsigned extruder_id = (unsigned)command.t;
                            if(extruder_id < machine.extruder_count) {
                                if(temperature && override[extruder_id].active_temperature) {
                                    temperature = override[extruder_id].active_temperature;
                                }
                                set_nozzle_temperature(extruder_id, temperature);
                                command_emitted++;
                                tool[extruder_id].nozzle_temperature = temperature;
                            }
                            else {
                                fprintf(stderr, "(line %u) Semantic warning: M104 cannot select non-existant extruder T%u" EOL, lineNumber, extruder_id);
                            }
                        }
                        else {
                            if(temperature && override[currentExtruder].active_temperature) {
                                temperature = override[currentExtruder].active_temperature;
                            }
                            set_nozzle_temperature(currentExtruder, temperature);
                            command_emitted++;
                            tool[currentExtruder].nozzle_temperature = temperature;
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax error: M104 is missing temperature, use Sn where n is 0-260" EOL, lineNumber);
                    }
                    break;
                    
                    // M106 - Turn cooling fan on
                case 106:
                    if(dittoPrinting) {
                        set_fan(A, 1);
                        set_fan(B, 1);
                        command_emitted++;
                    }
                    else if(command.flag & T_IS_SET) {
                        unsigned extruder_id = (unsigned)command.t;
                        if(extruder_id < machine.extruder_count) {
                            set_fan(extruder_id, 1);
                            command_emitted++;
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic warning: M106 cannot select non-existant extruder T%u" EOL, lineNumber, extruder_id);
                        }
                    }
                    else {
                        set_fan(currentExtruder, 1);
                    }
                    break;
                    
                    // M107 - Turn cooling fan off
                case 107:
                    if(dittoPrinting) {
                        set_fan(A, 0);
                        set_fan(B, 0);
                        command_emitted++;
                    }
                    else if(command.flag & T_IS_SET) {
                        unsigned extruder_id = (unsigned)command.t;
                        if(extruder_id < machine.extruder_count) {
                            set_fan(extruder_id, 0);
                            command_emitted++;
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic warning: M107 cannot select non-existant extruder T%u" EOL, lineNumber, extruder_id);
                        }
                    }
                    else {
                        set_fan(currentExtruder, 0);
                    }
                    break;
                    
                    // M108 - set extruder motor 5D 'simulated' RPM
                case 108:
#if ENABLE_RPM
                    if(command.flag & R_IS_SET) {
                        if(dittoPrinting) {
                            tool[A].rpm = tool[B].rpm = command.r;
                        }
                        else if(command.flag & T_IS_SET) {
                            unsigned extruder_id = (unsigned)command.t;
                            if(extruder_id < machine.extruder_count) {
                                tool[extruder_id].rpm = command.r;
                            }
                            else {
                                fprintf(stderr, "(line %u) Semantic warning: M108 cannot select non-existant extruder T%u" EOL, lineNumber, extruder_id);
                            }
                        }
                        else {
                            tool[currentExtruder].rpm = command.r;
                        }
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax error: M108 is missing motor RPM, use Rn where n is 0-5" EOL, lineNumber);
                    }
#endif
                    break;
                    
                    // M109 - Set Build Platform Temperature
                    // M140 - Set Build Platform Temperature (skeinforge)
                case 109:
                case 140:
                    if(machine.a.has_heated_build_platform || machine.b.has_heated_build_platform) {
                        if(command.flag & S_IS_SET) {
                            unsigned extruder_id = machine.a.has_heated_build_platform ? A : B;
                            unsigned temperature = (unsigned)command.s;
                            if(temperature > 160) temperature = 160;
                            if(!dittoPrinting && command.flag & T_IS_SET) {
                                extruder_id = (unsigned)command.t;
                            }
                            if(extruder_id < machine.extruder_count
                               && (extruder_id ? machine.b.has_heated_build_platform : machine.a.has_heated_build_platform)) {
                                if(temperature && override[extruder_id].build_platform_temperature) {
                                    temperature = override[extruder_id].build_platform_temperature;
                                }
                                set_build_platform_temperature(extruder_id, temperature);
                                command_emitted++;
                                tool[currentExtruder].build_platform_temperature = temperature;
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
                case 126:
                    if(dittoPrinting) {
                        set_valve(A, 1);
                        set_valve(B, 1);
                        command_emitted++;
                    }
                    else if(command.flag & T_IS_SET) {
                        unsigned extruder_id = (unsigned)command.t;
                        if(extruder_id < machine.extruder_count) {
                            set_valve(extruder_id, 1);
                            command_emitted++;
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic warning: M126 cannot select non-existant extruder T%u" EOL, lineNumber, extruder_id);
                        }
                    }
                    else {
                        set_valve(currentExtruder, 1);
                        command_emitted++;
                    }
                    break;

                    // M127 - Turn blower fan off (valve close)
                case 127:
                    if(dittoPrinting) {
                        set_valve(A, 0);
                        set_valve(B, 0);
                        command_emitted++;
                    }
                    else if(command.flag & T_IS_SET) {
                        unsigned extruder_id = (unsigned)command.t;
                        if(extruder_id < machine.extruder_count) {
                            set_valve(extruder_id, 0);
                            command_emitted++;
                        }
                        else {
                            fprintf(stderr, "(line %u) Semantic warning: M127 cannot select non-existant extruder T%u" EOL, lineNumber, extruder_id);
                        }
                    }
                    else {
                        set_valve(currentExtruder, 0);
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
                        double z = (isRelative ? (currentPosition.z + command.z) : command.z) + offset[currentOffset].z;
                        pause_at_zpos(z);
                    }
                    else {
                        fprintf(stderr, "(line %u) Syntax warning: M322 is missing Z axes, assuming zero (0)" EOL, lineNumber);
                        pause_at_zpos(0.0);
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
        }
        // check for pending pause @ zPos
        if(do_pause_at_zpos) {
            pause_at_zpos(pauseAt[pauseAtIndex].z);
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

