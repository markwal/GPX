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
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "gpx.h"

#define A 0
#define B 1

#define SHOW(FN) if(gpx->flag.logMessages) {FN;}
#define VERBOSE(FN) if(gpx->flag.verboseMode && gpx->flag.logMessages) {FN;}
#define CALL(FN) if((rval = FN) != SUCCESS) return rval

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
    0.85, // nominal packing density
    0.4, // nozzle diameter
    1,  // extruder count
    20, // timeout
    1,
};

static Machine cupcake_G4 = {
    {9600, 500, 47.069852, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 47.069852, ENDSTOP_IS_MIN}, // y axis
    {450, 450, 1280, ENDSTOP_IS_MIN},        // z axis
    {7200, 50.235478806907409, 400, 1}, // a extruder
    {7200, 50.235478806907409, 400, 0}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    1,  // extruder count
    20, // timeout
    2,
};

static Machine cupcake_P4 = {
    {9600, 500, 94.13970462, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 94.13970462, ENDSTOP_IS_MIN}, // y axis
    {450, 450, 2560, ENDSTOP_IS_MIN},        // z axis
    {7200, 50.235478806907409, 400, 1}, // a extruder
    {7200, 50.235478806907409, 400, 0}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    1,  // extruder count
    20, // timeout
    3,
};

static Machine cupcake_PP = {
    {9600, 500, 47.069852, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 47.069852, ENDSTOP_IS_MIN}, // y axis
    {450, 450, 1280, ENDSTOP_IS_MIN},        // z axis
    {7200, 100.470957613814818, 400, 1}, // a extruder
    {7200, 100.470957613814818, 400, 0}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    1,  // extruder count
    20, // timeout
    4,
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
    0.85, // nominal packing density
    0.4, // nozzle diameter
    1,  // extruder count
    20, // timeout
    5,
};

static Machine thing_o_matic_7D = {
    {9600, 500, 47.069852, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 47.069852, ENDSTOP_IS_MIN}, // y axis
    {1000, 500, 200, ENDSTOP_IS_MAX},        // z axis
    {1600, 50.235478806907409, 1600, 0}, // a extruder
    {1600, 50.235478806907409, 1600, 1}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    2,  // extruder count
    20, // timeout
    6,
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
    0.85, // nominal packing density
    0.4, // nozzle diameter
    1,  // extruder count
    20, // timeout
    7,
};

static Machine replicator_1D = {
    {18000, 2500, 94.139704, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 94.139704, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    2,  // extruder count
    20, // timeout
    8,
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
    0.97, // nominal packing density
    0.4, // nozzle diameter
    1,  // extruder count
    20, // timeout
    9,
};

static Machine replicator_2H = {
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    1.75, // nominal filament diameter
    0.97, // nominal packing density
    0.4, // nozzle diameter
    1,  // extruder count
    20, // timeout
    10,
};

static Machine replicator_2X = {
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 1}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    2,  // extruder count
    20, // timeout
    11,
};

#define MACHINE_IS(m) strcasecmp(machine, m) == 0

int gpx_set_machine(Gpx *gpx, char *machine)
{
    // only load/clobber the on-board machine definition if the one specified is different
    if(MACHINE_IS("c3")) {
        if(gpx->machine.type != 1) {
            gpx->machine = cupcake_G3;
            VERBOSE( fputs("Loading machine definition: Cupcake Gen3 XYZ, Mk5/6 + Gen4 Extruder" EOL, gpx->log) );
        }
        else {
            VERBOSE( fputs("Ignoring duplicate machine definition: -m c3" EOL, gpx->log) );
        }
    }
    else if(MACHINE_IS("c4")) {
        if(gpx->machine.type != 2) {
            gpx->machine = cupcake_G4;
            VERBOSE( fputs("Loading machine definition: Cupcake Gen4 XYZ, Mk5/6 + Gen4 Extruder" EOL, gpx->log) );
        }
        else {
            VERBOSE( fputs("Ignoring duplicate machine definition: -m c4" EOL, gpx->log) );
        }
    }
    else if(MACHINE_IS("cp4")) {
        if(gpx->machine.type != 3) {
            gpx->machine = cupcake_P4;
            VERBOSE( fputs("Loading machine definition: Cupcake Pololu XYZ, Mk5/6 + Gen4 Extruder" EOL, gpx->log) );
        }
        else {
            VERBOSE( fputs("Ignoring duplicate machine definition: -m cp4" EOL, gpx->log) );
        }
    }
    else if(MACHINE_IS("cpp")) {
        if(gpx->machine.type != 4) {
            gpx->machine = cupcake_PP;
            VERBOSE( fputs("Loading machine definition: Cupcake Pololu XYZ, Mk5/6 + Pololu Extruder" EOL, gpx->log) );
        }
        else {
            VERBOSE( fputs("Ignoring duplicate machine definition: -m cpp" EOL, gpx->log) );
        }
    }
    else if(MACHINE_IS("t6")) {
        if(gpx->machine.type != 5) {
            gpx->machine = thing_o_matic_7;
            VERBOSE( fputs("Loading machine definition: TOM Mk6 - single extruder" EOL, gpx->log) );
        }
        else {
            VERBOSE( fputs("Ignoring duplicate machine definition: -m t6" EOL, gpx->log) );
        }
    }
    else if(MACHINE_IS("t7")) {
        if(gpx->machine.type != 5) {
            gpx->machine = thing_o_matic_7;
            VERBOSE( fputs("Loading machine definition: TOM Mk7 - single extruder" EOL, gpx->log) );
        }
        else {
            VERBOSE( fputs("Ignoring duplicate machine definition: -m t7" EOL, gpx->log) );
        }
    }
    else if(MACHINE_IS("t7d")) {
        if(gpx->machine.type != 6) {
            gpx->machine = thing_o_matic_7D;
            VERBOSE( fputs("Loading machine definition: TOM Mk7 - dual extruder" EOL, gpx->log) );
        }
        else {
            VERBOSE( fputs("Ignoring duplicate machine definition: -m t7d" EOL, gpx->log) );
        }
    }
    else if(MACHINE_IS("r1")) {
        if(gpx->machine.type != 7) {
            gpx->machine = replicator_1;
            VERBOSE( fputs("Loading machine definition: Replicator 1 - single extruder" EOL, gpx->log) );
        }
        else {
            VERBOSE( fputs("Ignoring duplicate machine definition: -m r1" EOL, gpx->log) );
        }
    }
    else if(MACHINE_IS("r1d")) {
        if(gpx->machine.type != 8) {
            gpx->machine = replicator_1D;
            VERBOSE( fputs("Loading machine definition: Replicator 1 - dual extruder" EOL, gpx->log) );
        }
        else {
            VERBOSE( fputs("Ignoring duplicate machine definition: -m r1d" EOL, gpx->log) );
        }
    }
    else if(MACHINE_IS("r2")) {
        if(gpx->machine.type != 9) {
            gpx->machine = replicator_2;
            VERBOSE( fputs("Loading machine definition: Replicator 2" EOL, gpx->log) );
        }
        else {
            VERBOSE( fputs("Ignoring duplicate machine definition: -m r2" EOL, gpx->log) );
        }
    }
    else if(MACHINE_IS("r2h")) {
        if(gpx->machine.type != 10) {
            gpx->machine = replicator_2H;
            VERBOSE( fputs("Loading machine definition: Replicator 2 with HBP" EOL, gpx->log) );
        }
        else {
            VERBOSE( fputs("Ignoring duplicate machine definition: -m r2h" EOL, gpx->log) );
        }
    }
    else if(MACHINE_IS("r2x")) {
        if(gpx->machine.type != 11) {
            gpx->machine = replicator_2X;
            VERBOSE( fputs("Loading machine definition: Replicator 2X" EOL, gpx->log) );
        }
        else {
            VERBOSE( fputs("Ignoring duplicate machine definition: -m r2x" EOL, gpx->log) );
        }
    }
    else {
        return ERROR;
    }
    // update known position mask
    gpx->axis.mask = gpx->machine.extruder_count == 1 ? (XYZ_BIT_MASK | A_IS_SET) : AXES_BIT_MASK;;
    return SUCCESS;
}

// PRIVATE FUNCTION PROTOTYPES

static double get_home_feedrate(Gpx *gpx, int flag);
static int pause_at_zpos(Gpx *gpx, float z_positon);

// initialization of global variables

void gpx_initialize(Gpx *gpx, int firstTime)
{
    int i;
    gpx->buffer.ptr = gpx->buffer.out;
    // we default to using pipes
    
    // initialise machine
    if(firstTime) gpx->machine = replicator_2;
    
    // initialise command
    gpx->command.x = 0.0;
    gpx->command.y = 0.0;
    gpx->command.z = 0.0;
    gpx->command.a = 0.0;
    gpx->command.b = 0.0;
    
    gpx->command.e = 0.0;
    gpx->command.f = 0.0;
    
    gpx->command.p = 0.0;
    gpx->command.r = 0.0;
    gpx->command.s = 0.0;
    

    gpx->command.g = 0.0;
    gpx->command.m = 0.0;
    gpx->command.t = 0.0;
    
    gpx->command.comment = "";
        
    gpx->command.flag = 0;
    
    // initialize target position
    gpx->target.position.x = 0.0;
    gpx->target.position.y = 0.0;
    gpx->target.position.z = 0.0;
    
    gpx->target.position.a = 0.0;
    gpx->target.position.b = 0.0;

    gpx->target.extruder = 0;

    // initialize current position
    gpx->current.position.x = 0.0;
    gpx->current.position.y = 0.0;
    gpx->current.position.z = 0.0;
    
    gpx->current.position.a = 0.0;
    gpx->current.position.b = 0.0;

    gpx->current.feedrate = get_home_feedrate(gpx, XYZ_BIT_MASK);
    gpx->current.extruder = 0;
    gpx->current.offset = 0;
    gpx->current.percent = 0;

    gpx->axis.positionKnown = 0;
    gpx->axis.mask = gpx->machine.extruder_count == 1 ? (XYZ_BIT_MASK | A_IS_SET) : AXES_BIT_MASK;;

    // initialize the accumulated rounding error
    gpx->excess.a = 0.0;
    gpx->excess.b = 0.0;

    // initialize the G10 offsets
    for(i = 0; i < 7; i++) {
        gpx->offset[i].x = 0.0;
        gpx->offset[i].y = 0.0;
        gpx->offset[i].z = 0.0;
    }

    // initialize the command line offset
    if(firstTime) {
        gpx->user.offset.x = 0.0;
        gpx->user.offset.y = 0.0;
        gpx->user.offset.z = 0.0;
        gpx->user.scale = 1.0;
    }
    
    for(i = 0; i < 2; i++) {
        gpx->tool[i].motor_enabled = 0;
#if ENABLE_SIMULATED_RPM
        gpx->tool[i].rpm = 0;
#endif
        gpx->tool[i].nozzle_temperature = 0;
        gpx->tool[i].build_platform_temperature = 0;
        
        gpx->override[i].actual_filament_diameter = 0;
        gpx->override[i].filament_scale = 1.0;
        gpx->override[i].packing_density = 1.0;
        gpx->override[i].standby_temperature = 0;
        gpx->override[i].active_temperature = 0;
        gpx->override[i].build_platform_temperature = 0;
    }
    
    if(firstTime) {
        gpx->filament[0].colour = "_null_";
        gpx->filament[0].diameter = 0.0;
        gpx->filament[0].temperature = 0;
        gpx->filament[0].LED = 0;
        gpx->filamentLength = 1;
    }
    
    if(firstTime) {
        gpx->commandAtIndex = 0;
        gpx->commandAtLength = 0;
    }
    gpx->commandAtZ = 0.0;
    
    // SETTINGS
    
    if(firstTime) {
        gpx->sdCardPath = NULL;
        gpx->buildName = "GPX " GPX_VERSION;
    }

    gpx->flag.relativeCoordinates = 0;
    gpx->flag.extruderIsRelative = 0;

    if(firstTime) {
        gpx->flag.reprapFlavor = 1; // reprap flavor is enabled by default
        gpx->flag.dittoPrinting = 0;
        gpx->flag.buildProgress = 0;
        gpx->flag.verboseMode = 0;
        gpx->flag.logMessages = 1; // logging is enabled by default
        gpx->flag.rewrite5D = 0;
    }

    // STATE
    
    gpx->flag.programState = 0;
    gpx->flag.doPauseAtZPos = 0;
    gpx->flag.pausePending = 0;
    gpx->flag.macrosEnabled = 0;
    if(firstTime) {
        gpx->flag.loadMacros = 1;
        gpx->flag.runMacros = 1;
    }
    gpx->flag.framingEnabled = 0;

    gpx->longestDDA = 0;
    gpx->layerHeight = 0.34;
    gpx->lineNumber = 1;
    
    
    // STATISTICS
    
    gpx->accumulated.a = 0.0;
    gpx->accumulated.b = 0.0;
    gpx->accumulated.time = 0.0;
    gpx->accumulated.bytes = 0;
    
    if(firstTime) {
        gpx->total.length = 0.0;
        gpx->total.time = 0.0;
        gpx->total.bytes = 0;
    }
    
    // CALLBACK
    
    gpx->callbackHandler = NULL;
    gpx->callbackData = NULL;
    
    // LOGGING
    
    if(firstTime) gpx->log = stderr;
}

// PRINT STATE

#define start_program() gpx->flag.programState = RUNNING_STATE
#define end_program() gpx->flag.programState = ENDED_STATE

#define program_is_ready() gpx->flag.programState < RUNNING_STATE
#define program_is_running() gpx->flag.programState < ENDED_STATE

// IO FUNCTIONS

static void write_8(Gpx *gpx, unsigned char value)
{
    *gpx->buffer.ptr++ = value;
}

static unsigned char read_8(Gpx *gpx)
{
    return *gpx->buffer.ptr++;
}

static void write_16(Gpx *gpx, unsigned short value)
{
    union {
        unsigned short s;
        unsigned char b[2];
    } u;
    u.s = value;
    *gpx->buffer.ptr++ = u.b[0];
    *gpx->buffer.ptr++ = u.b[1];
}

static unsigned short read_16(Gpx *gpx)
{
    union {
        unsigned short s;
        unsigned char b[2];
    } u;
    u.b[0] = *gpx->buffer.ptr++;
    u.b[1] = *gpx->buffer.ptr++;
    return u.s;
}

static void write_32(Gpx *gpx, unsigned int value)
{
    union {
        unsigned int i;
        unsigned char b[4];
    } u;
    u.i = value;
    *gpx->buffer.ptr++ = u.b[0];
    *gpx->buffer.ptr++ = u.b[1];
    *gpx->buffer.ptr++ = u.b[2];
    *gpx->buffer.ptr++ = u.b[3];
}

static unsigned int read_32(Gpx *gpx)
{
    union {
        unsigned int i;
        unsigned char b[4];
    } u;
    u.b[0] = *gpx->buffer.ptr++;
    u.b[1] = *gpx->buffer.ptr++;
    u.b[2] = *gpx->buffer.ptr++;
    u.b[3] = *gpx->buffer.ptr++;
    return u.i;
}

static void write_float(Gpx *gpx, float value)
{
    union {
        float f;
        unsigned char b[4];
    } u;
    u.f = value;
    *gpx->buffer.ptr++ = u.b[0];
    *gpx->buffer.ptr++ = u.b[1];
    *gpx->buffer.ptr++ = u.b[2];
    *gpx->buffer.ptr++ = u.b[3];
}

static float read_float(Gpx *gpx)
{
    union {
        float f;
        unsigned char b[4];
    } u;
    u.b[0] = *gpx->buffer.ptr++;
    u.b[1] = *gpx->buffer.ptr++;
    u.b[2] = *gpx->buffer.ptr++;
    u.b[3] = *gpx->buffer.ptr++;
    return u.f;
}

static long write_bytes(Gpx *gpx, char *data, long length)
{
    long l = length;
    while(l--) {
        *gpx->buffer.ptr++ = *data++;
    }
    return length;
}

static long read_bytes(Gpx *gpx, char *data, long length)
{
    long l = length;
    while(l--) {
        *data++ = *gpx->buffer.ptr++;
    }
    return length;
}

static long write_string(Gpx *gpx, char *string, long length)
{
    long l = length;
    while(l--) {
        *gpx->buffer.ptr++ = *string++;
    }
    *gpx->buffer.ptr++ = '\0';
    return length;
}

// FRAMING

static unsigned char calculate_crc(unsigned char *addr, long len)
{
    unsigned char data, crc = 0;
    while(len--) {
        data = *addr++;
        // 8-bit iButton/Maxim/Dallas CRC loop unrolled
        crc = crc ^ data;
        // 1
        if (crc & 0x01) crc = (crc >> 1) ^ 0x8C;
        else crc >>= 1;
        
        // 2
        if (crc & 0x01) crc = (crc >> 1) ^ 0x8C;
        else crc >>= 1;
        
        // 3
        if (crc & 0x01) crc = (crc >> 1) ^ 0x8C;
        else crc >>= 1;
        
        // 4
        if (crc & 0x01) crc = (crc >> 1) ^ 0x8C;
        else crc >>= 1;
        
        // 5
        if (crc & 0x01) crc = (crc >> 1) ^ 0x8C;
        else crc >>= 1;
        
        // 6
        if (crc & 0x01) crc = (crc >> 1) ^ 0x8C;
        else crc >>= 1;
        
        // 7
        if (crc & 0x01) crc = (crc >> 1) ^ 0x8C;
        else crc >>= 1;
        
        // 8
        if (crc & 0x01) crc = (crc >> 1) ^ 0x8C;
        else crc >>= 1;
    }
    return crc;
}

static void begin_frame(Gpx *gpx)
{
    gpx->buffer.ptr = gpx->buffer.out;
    if(gpx->flag.framingEnabled) {
        gpx->buffer.out[0] = 0xD5;  // synchronization byte
        gpx->buffer.ptr += 2;
    }
}

static int end_frame(Gpx *gpx)
{
    if(gpx->flag.framingEnabled) {
        unsigned char *start = (unsigned char *)gpx->buffer.out + 2;
        unsigned char *end = (unsigned char *)gpx->buffer.ptr;
        size_t payload_length = end - start;
        gpx->buffer.out[1] = (unsigned char)payload_length;
        *gpx->buffer.ptr++ = calculate_crc(start, payload_length);
    }
    size_t length = gpx->buffer.ptr - gpx->buffer.out;
    gpx->accumulated.bytes += length;
    if(gpx->callbackHandler) return gpx->callbackHandler(gpx, gpx->callbackData, gpx->buffer.out, length);
    return SUCCESS;
}

// 5D VECTOR FUNCTIONS

// compute the filament scaling factor

static void set_filament_scale(Gpx *gpx, unsigned extruder_id, double filament_diameter)
{
    double actual_radius = filament_diameter / 2;
    double nominal_radius = gpx->machine.nominal_filament_diameter / 2;
    gpx->override[extruder_id].filament_scale = (nominal_radius * nominal_radius) / (actual_radius * actual_radius);
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
    double length, result = 0.0;
    if(flag & X_IS_SET) {
        result = fabs(vector->x);
    }
    if(flag & Y_IS_SET) {
        length = fabs(vector->y);
        if(result < length) result = length;
    }
    if(flag & Z_IS_SET) {
        length = fabs(vector->z);
        if(result < length) result = length;
    }
    if(flag & A_IS_SET) {
        length = fabs(vector->a);
        if(result < length) result = length;
    }
    if(flag & B_IS_SET) {
        length = fabs(vector->b);
        if(result < length) result = length;
    }
    return result;
}

// calculate the dda for the longest axis for the current machine definition

static int get_longest_dda(Gpx *gpx)
{
    // calculate once
    int longestDDA = gpx->longestDDA;
    if(longestDDA == 0) {
        longestDDA = (int)(60 * 1000000.0 / (gpx->machine.x.max_feedrate * gpx->machine.x.steps_per_mm));
    
        int axisDDA = (int)(60 * 1000000.0 / (gpx->machine.y.max_feedrate * gpx->machine.y.steps_per_mm));
        if(longestDDA < axisDDA) longestDDA = axisDDA;
    
        axisDDA = (int)(60 * 1000000.0 / (gpx->machine.z.max_feedrate * gpx->machine.z.steps_per_mm));
        if(longestDDA < axisDDA) longestDDA = axisDDA;
        gpx->longestDDA = longestDDA;
    }
    return longestDDA;
}

// return the maximum home feedrate

static double get_home_feedrate(Gpx *gpx, int flag) {
    double feedrate = 0.0;
    if(flag & X_IS_SET) {
        feedrate = gpx->machine.x.home_feedrate;
    }
    if(flag & Y_IS_SET && feedrate < gpx->machine.y.home_feedrate) {
        feedrate = gpx->machine.y.home_feedrate;
    }
    if(flag & Z_IS_SET && feedrate < gpx->machine.z.home_feedrate) {
        feedrate = gpx->machine.z.home_feedrate;
    }
    return feedrate;
}

// return the maximum safe feedrate

static double get_safe_feedrate(Gpx *gpx, int flag, Ptr5d delta) {
    
    double feedrate = gpx->current.feedrate;
    if(feedrate == 0.0) {
        feedrate = gpx->machine.x.max_feedrate;
        if(feedrate < gpx->machine.y.max_feedrate) {
            feedrate = gpx->machine.y.max_feedrate;
        }
        if(feedrate < gpx->machine.z.max_feedrate) {
            feedrate = gpx->machine.z.max_feedrate;
        }
        if(feedrate < gpx->machine.a.max_feedrate) {
            feedrate = gpx->machine.a.max_feedrate;
        }
        if(feedrate < gpx->machine.b.max_feedrate) {
            feedrate = gpx->machine.b.max_feedrate;
        }
    }

    double distance = magnitude(flag & XYZ_BIT_MASK, delta);
    if(flag & X_IS_SET && (feedrate * delta->x / distance) > gpx->machine.x.max_feedrate) {
        feedrate = gpx->machine.x.max_feedrate * distance / delta->x;
    }
    if(flag & Y_IS_SET && (feedrate * delta->y / distance) > gpx->machine.y.max_feedrate) {
        feedrate = gpx->machine.y.max_feedrate * distance / delta->y;
    }
    if(flag & Z_IS_SET && (feedrate * delta->z / distance) > gpx->machine.z.max_feedrate) {
        feedrate = gpx->machine.z.max_feedrate * distance / delta->z;
    }

    if(distance == 0) {
        if(flag & A_IS_SET && feedrate > gpx->machine.a.max_feedrate) {
            feedrate = gpx->machine.a.max_feedrate;
        }        
        if(flag & B_IS_SET && feedrate > gpx->machine.b.max_feedrate) {
            feedrate = gpx->machine.b.max_feedrate;
        }
    }
    else {
        if(flag & A_IS_SET && (feedrate * delta->a / distance) > gpx->machine.a.max_feedrate) {
            feedrate = gpx->machine.a.max_feedrate * distance / delta->a;
        }
        if(flag & B_IS_SET && (feedrate * delta->b / distance) > gpx->machine.b.max_feedrate) {
            feedrate = gpx->machine.b.max_feedrate * distance / delta->b;
        }
    }
    return feedrate;
}

// convert mm to steps using the current machine definition

// IMPORTANT: this command changes the global excess value which accumulates the rounding remainder

static Point5d mm_to_steps(Gpx *gpx, Ptr5d mm, Ptr2d excess)
{
    double value;
    Point5d result;
    result.x = round(mm->x * gpx->machine.x.steps_per_mm);
    result.y = round(mm->y * gpx->machine.y.steps_per_mm);
    result.z = round(mm->z * gpx->machine.z.steps_per_mm);
    if(excess) {
        // accumulate rounding remainder
        value = (mm->a * gpx->machine.a.steps_per_mm) + excess->a;
        result.a = round(value);
        // changes to excess
        excess->a = value - result.a;
        
        value = (mm->b * gpx->machine.b.steps_per_mm) + excess->b;
        result.b = round(value);
        // changes to excess
        excess->b = value - result.b;
    }
    else {
        result.a = round(mm->a * gpx->machine.a.steps_per_mm);
        result.b = round(mm->b * gpx->machine.b.steps_per_mm);        
    }
    return result;
}

static Point5d delta_mm(Gpx *gpx)
{
    Point5d deltaMM;
    // compute the relative distance traveled along each axis and convert to steps
    if(gpx->command.flag & X_IS_SET) deltaMM.x = gpx->target.position.x - gpx->current.position.x; else deltaMM.x = 0;
    if(gpx->command.flag & Y_IS_SET) deltaMM.y = gpx->target.position.y - gpx->current.position.y; else deltaMM.y = 0;
    if(gpx->command.flag & Z_IS_SET) deltaMM.z = gpx->target.position.z - gpx->current.position.z; else deltaMM.z = 0;
    if(gpx->command.flag & A_IS_SET) deltaMM.a = gpx->target.position.a - gpx->current.position.a; else deltaMM.a = 0;
    if(gpx->command.flag & B_IS_SET) deltaMM.b = gpx->target.position.b - gpx->current.position.b; else deltaMM.b = 0;
    return deltaMM;
}

static Point5d delta_steps(Gpx *gpx,Point5d deltaMM)
{
    Point5d deltaSteps;
    // compute the relative distance traveled along each axis and convert to steps
    if(gpx->command.flag & X_IS_SET) deltaSteps.x = round(fabs(deltaMM.x) * gpx->machine.x.steps_per_mm); else deltaSteps.x = 0;
    if(gpx->command.flag & Y_IS_SET) deltaSteps.y = round(fabs(deltaMM.y) * gpx->machine.y.steps_per_mm); else deltaSteps.y = 0;
    if(gpx->command.flag & Z_IS_SET) deltaSteps.z = round(fabs(deltaMM.z) * gpx->machine.z.steps_per_mm); else deltaSteps.z = 0;
    if(gpx->command.flag & A_IS_SET) deltaSteps.a = round(fabs(deltaMM.a) * gpx->machine.a.steps_per_mm); else deltaSteps.a = 0;
    if(gpx->command.flag & B_IS_SET) deltaSteps.b = round(fabs(deltaMM.b) * gpx->machine.b.steps_per_mm); else deltaSteps.b = 0;
    return deltaSteps;
}

// X3G QUERIES

#define COMMAND_OFFSET 2
#define EXTRUDER_ID_OFFSET 3
#define QUERY_COMMAND_OFFSET 4
#define EEPROM_LENGTH_OFFSET 8

// 00 - Get version

static int get_version(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 0);
    
    // uint16: host version
    write_16(gpx, HOST_VERSION);
    
    return end_frame(gpx);
}

/* 01 - Initialize firmware to boot state
        This is treated as a NOOP in the Sailfish firmware. */

static int initialize_firmware(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 1);
        
    return end_frame(gpx);
}

// 02 - Get available buffer size

static int get_buffer_size(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 2);
    
    return end_frame(gpx);
}

// 03 - Clear buffer (same as 07 and 17)

static int clear_buffer(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 3);
    
    return end_frame(gpx);
}

// 07 - Abort immediately

static int abort_immediately(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 7);
    
    return end_frame(gpx);
}

// 08 - Pause/Resume

static int pause_resume(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 8);
    
    return end_frame(gpx);
}

// 10 - Extruder Query Commands

// Query 00 - Query firmware version information

static int get_extruder_version(Gpx *gpx, unsigned extruder_id)
{
    begin_frame(gpx);
    
    write_8(gpx, 10);
    
    // uint8: ID of the extruder to query
    write_8(gpx, extruder_id);
    
    // uint8: Query command to send to the extruder
    write_8(gpx, 0);
    
    // uint8: Length of the extruder command payload (N)
    write_8(gpx, 2);
    
    // uint16: host version
    write_16(gpx, HOST_VERSION);
    
    return end_frame(gpx);
}

// Query 02 - Get extruder temperature

static int get_extruder_temperature(Gpx *gpx, unsigned extruder_id)
{
    begin_frame(gpx);
    
    write_8(gpx, 10);
    
    // uint8: ID of the extruder to query
    write_8(gpx, extruder_id);
    
    // uint8: Query command to send to the extruder
    write_8(gpx, 2);
    
    // uint8: Length of the extruder command payload (N)
    write_8(gpx, 0);
    
    return end_frame(gpx);
}

// Query 22 - Is extruder ready

static int is_extruder_ready(Gpx *gpx, unsigned extruder_id)
{
    begin_frame(gpx);
    
    write_8(gpx, 10);
    
    // uint8: ID of the extruder to query
    write_8(gpx, extruder_id);
    
    // uint8: Query command to send to the extruder
    write_8(gpx, 22);
    
    // uint8: Length of the extruder command payload (N)
    write_8(gpx, 0);
    
    return end_frame(gpx);
}

// Query 30 - Get build platform temperature

static int get_build_platform_temperature(Gpx *gpx, unsigned extruder_id)
{
    begin_frame(gpx);
    
    write_8(gpx, 10);
    
    // uint8: ID of the extruder to query
    write_8(gpx, extruder_id);
    
    // uint8: Query command to send to the extruder
    write_8(gpx, 30);
    
    // uint8: Length of the extruder command payload (N)
    write_8(gpx, 0);
    
    return end_frame(gpx);
}

// Query 32 - Get extruder target temperature

static int get_extruder_target_temperature(Gpx *gpx, unsigned extruder_id)
{
    begin_frame(gpx);
    
    write_8(gpx, 10);
    
    // uint8: ID of the extruder to query
    write_8(gpx, extruder_id);
    
    // uint8: Query command to send to the extruder
    write_8(gpx, 32);
    
    // uint8: Length of the extruder command payload (N)
    write_8(gpx, 0);
    
    return end_frame(gpx);
}

// Query 33 - Get build platform target temperature

static int get_build_platform_target_temperature(Gpx *gpx, unsigned extruder_id)
{
    begin_frame(gpx);
    
    write_8(gpx, 10);
    
    // uint8: ID of the extruder to query
    write_8(gpx, extruder_id);
    
    // uint8: Query command to send to the extruder
    write_8(gpx, 33);
    
    // uint8: Length of the extruder command payload (N)
    write_8(gpx, 0);
    
    return end_frame(gpx);
}

// Query 35 - Is build platform ready?

static int is_build_platform_ready(Gpx *gpx, unsigned extruder_id)
{
    begin_frame(gpx);
    
    write_8(gpx, 10);
    
    // uint8: ID of the extruder to query
    write_8(gpx, extruder_id);
    
    // uint8: Query command to send to the extruder
    write_8(gpx, 35);
    
    // uint8: Length of the extruder command payload (N)
    write_8(gpx, 0);
    
    return end_frame(gpx);
}

// Query 36 - Get extruder status

static int get_extruder_status(Gpx *gpx, unsigned extruder_id)
{
    begin_frame(gpx);
    
    write_8(gpx, 10);
    
    // uint8: ID of the extruder to query
    write_8(gpx, extruder_id);
    
    // uint8: Query command to send to the extruder
    write_8(gpx, 36);
    
    // uint8: Length of the extruder command payload (N)
    write_8(gpx, 0);
    
    return end_frame(gpx);
}

// Query 37 - Get PID state

static int get_PID_state(Gpx *gpx, unsigned extruder_id)
{
    begin_frame(gpx);
    
    write_8(gpx, 10);
    
    // uint8: ID of the extruder to query
    write_8(gpx, extruder_id);
    
    // uint8: Query command to send to the extruder
    write_8(gpx, 37);
    
    // uint8: Length of the extruder command payload (N)
    write_8(gpx, 0);
    
    return end_frame(gpx);
}

// 11 - Is ready

static int is_ready(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 11);
    
    return end_frame(gpx);
}

// 12 - Read from EEPROM

static int read_eeprom(Gpx *gpx, unsigned address, unsigned length)
{
    begin_frame(gpx);
    
    write_8(gpx, 12);
    
    // uint16: EEPROM memory offset to begin reading from
    write_16(gpx, address);
    
    // uint8: Number of bytes to read, N.
    write_8(gpx, length);
    
    return end_frame(gpx);
}

// 13 - Write to EEPROM

static int write_eeprom(Gpx *gpx, unsigned address, char *data, unsigned length)
{
    begin_frame(gpx);
    
    write_8(gpx, 13);
    
    // uint16: EEPROM memory offset to begin writing to
    write_16(gpx, address);
    
    // uint8: Number of bytes to write
    write_8(gpx, length);
    
    // N bytes: Data to write to EEPROM
    write_bytes(gpx, data, length);
    
    return end_frame(gpx);
}

// 14 - Capture to file

static int capture_to_file(Gpx *gpx, char *filename)
{
    begin_frame(gpx);
    
    write_8(gpx, 14);
    
    /* 1+N bytes: Filename to write to, in ASCII, terminated with a null character.
                  N can be 1-12 bytes long, not including the null character. */
    write_string(gpx, filename, strlen(filename));

    return end_frame(gpx);
}

// 15 - End capture to file

static int end_capture_to_file(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 15);
    
    return end_frame(gpx);
}

// 16 - Play back capture

static int play_back_capture(Gpx *gpx, char *filename)
{
    begin_frame(gpx);
    
    write_8(gpx, 16);
    
    /* 1+N bytes: Filename to write to, in ASCII, terminated with a null character.
     N can be 1-12 bytes long, not including the null character. */
    write_string(gpx, filename, strlen(filename));
    
    return end_frame(gpx);
}

// 17 - Reset

static int reset(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 17);
    
    return end_frame(gpx);
}

// 18 - Get next filename

static int get_next_filename(Gpx *gpx, unsigned restart)
{
    begin_frame(gpx);
    
    write_8(gpx, 18);
    
    // uint8: 0 if file listing should continue, 1 to restart listing.
    write_8(gpx, restart);
    
    return end_frame(gpx);
}

// 20 - Get build name

static int get_build_name(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 20);
    
    return end_frame(gpx);
}

// 21 - Get extended position

static int get_extended_position(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 21);
    
    return end_frame(gpx);
}

// 22 - Extended stop

static int extended_stop(Gpx *gpx, unsigned halt_steppers, unsigned clear_queue)
{
    unsigned flag = 0;
    if(halt_steppers) flag |= 0x1;
    if(clear_queue) flag |= 0x2;
    
    begin_frame(gpx);
    
    write_8(gpx, 22);
    
    /* uint8: Bitfield indicating which subsystems to shut down.
              If bit 0 is set, halt all stepper motion.
              If bit 1 is set, clear the command queue. */
    write_8(gpx, flag);
    
    return end_frame(gpx);
}

// 23 - Get motherboard status

static int get_motherboard_status(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 23);
    
    return end_frame(gpx);
}

// 24 - Get build statistics

static int get_build_statistics(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 24);
    
    return end_frame(gpx);
}

// 27 - Get advanced version number

static int get_advanced_version_number(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 27);
    
    // uint16: Host version
    write_16(gpx, HOST_VERSION);
    
    return end_frame(gpx);
}

// X3G COMMANDS

// 131 - Find axes minimums
// 132 - Find axes maximums

static int home_axes(Gpx *gpx, unsigned axes, unsigned direction)
{
    Point5d unitVector;
    double feedrate = gpx->command.flag & F_IS_SET ? gpx->current.feedrate : get_home_feedrate(gpx, gpx->command.flag);
    double longestAxis = 0.0;
    assert(direction <= 1);

    // compute the slowest feedrate
    if(axes & X_IS_SET) {
        if(gpx->machine.x.home_feedrate < feedrate) {
            feedrate = gpx->machine.x.home_feedrate;
        }
        unitVector.x = 1;
        longestAxis = gpx->machine.x.steps_per_mm;
        // confirm machine compatibility
        if(direction != gpx->machine.x.endstop) {
            SHOW( fprintf(gpx->log, "(line %u) Semantic warning: X axis homing to %s endstop" EOL, gpx->lineNumber, direction ? "maximum" : "minimum") );
        }
    }
    if(axes & Y_IS_SET) {
        if(gpx->machine.y.home_feedrate < feedrate) {
            feedrate = gpx->machine.y.home_feedrate;
        }
        unitVector.y = 1;
        if(longestAxis < gpx->machine.y.steps_per_mm) {
            longestAxis = gpx->machine.y.steps_per_mm;
        }
        if(direction != gpx->machine.y.endstop) {
            SHOW( fprintf(gpx->log, "(line %u) Semantic warning: Y axis homing to %s endstop" EOL, gpx->lineNumber, direction ? "maximum" : "minimum") );
        }
    }
    if(axes & Z_IS_SET) {
        if(gpx->machine.z.home_feedrate < feedrate) {
            feedrate = gpx->machine.z.home_feedrate;
        }
        unitVector.z = 1;
        if(longestAxis < gpx->machine.z.steps_per_mm) {
            longestAxis = gpx->machine.z.steps_per_mm;
        }
        if(direction != gpx->machine.z.endstop) {
            SHOW( fprintf(gpx->log, "(line %u) Semantic warning: Z axis homing to %s endstop" EOL, gpx->lineNumber, direction ? "maximum" : "minimum") );
        }
    }
    
    // unit vector distance in mm
    double distance = magnitude(axes, &unitVector);
    // move duration in microseconds = distance / feedrate * 60,000,000
    double microseconds = distance / feedrate * 60000000.0;
    // time between steps for longest axis = microseconds / longestStep
    unsigned step_delay = (unsigned)round(microseconds / longestAxis);
    
    gpx->accumulated.time += distance / feedrate * 60;

    begin_frame(gpx);

    write_8(gpx, direction == ENDSTOP_IS_MIN ? 131 :132);
    
    // uint8: Axes bitfield. Axes whose bits are set will be moved.
    write_8(gpx, axes);
    
    // uint32: Feedrate, in microseconds between steps on the max delta. (DDA)
    write_32(gpx, step_delay);
    
    // uint16: Timeout, in seconds.
    write_16(gpx, gpx->machine.timeout);
    
    return end_frame(gpx);
}

// 133 - delay

static int delay(Gpx *gpx, unsigned milliseconds)
{
    begin_frame(gpx);
    
    write_8(gpx, 133);
    
    // uint32: delay, in milliseconds
    write_32(gpx, milliseconds);
    
    return end_frame(gpx);
}

// 134 - Change extruder offset

// This is important to use on dual-head Replicators, because the machine needs to know
// the current toolhead in order to apply a calibration offset.

static int change_extruder_offset(Gpx *gpx, unsigned extruder_id)
{
    assert(extruder_id < gpx->machine.extruder_count);
    
    begin_frame(gpx);
    
    write_8(gpx, 134);
    
    // uint8: ID of the extruder to switch to
    write_8(gpx, extruder_id);
    
    return end_frame(gpx);
}

// 135 - Wait for extruder ready

static int wait_for_extruder(Gpx *gpx, unsigned extruder_id, unsigned timeout)
{
    assert(extruder_id < gpx->machine.extruder_count);
    
    begin_frame(gpx);

    write_8(gpx, 135);
    
    // uint8: ID of the extruder to wait for
    write_8(gpx, extruder_id);
    
    // uint16: delay between query packets sent to the extruder, in ms (nominally 100 ms)
    write_16(gpx, 100);
    
    // uint16: Timeout before continuing without extruder ready, in seconds (nominally 1 minute)
    write_16(gpx, timeout);
    
    return end_frame(gpx);
}
 
// 136 - extruder action command

// Action 03 - Set extruder target temperature

static int set_nozzle_temperature(Gpx *gpx, unsigned extruder_id, unsigned temperature)
{
    assert(extruder_id < gpx->machine.extruder_count);
    
    double tDelta = (double)temperature - (double)gpx->tool[extruder_id].nozzle_temperature - AMBIENT_TEMP;
    if(tDelta > 0.0) {
        gpx->accumulated.time += tDelta * NOZZLE_TIME;
    }
    
    begin_frame(gpx);

    write_8(gpx, 136);
    
    // uint8: ID of the extruder to query
    write_8(gpx, extruder_id);
    
    // uint8: Action command to send to the extruder
    write_8(gpx, 3);
    
    // uint8: Length of the extruder command payload (N)
    write_8(gpx, 2);
    
    // int16: Desired target temperature, in Celsius
    write_16(gpx, temperature);
    
    return end_frame(gpx);
}

// Action 12 - Enable / Disable fan

static int set_fan(Gpx *gpx, unsigned extruder_id, unsigned state)
{
    assert(extruder_id < gpx->machine.extruder_count);
    
    begin_frame(gpx);

    write_8(gpx, 136);
    
    // uint8: ID of the extruder to query
    write_8(gpx, extruder_id);
    
    // uint8: Action command to send to the extruder
    write_8(gpx, 12);
    
    // uint8: Length of the extruder command payload (N)
    write_8(gpx, 1);
    
    // uint8: 1 to enable, 0 to disable
    write_8(gpx, state);
    
    return end_frame(gpx);
}

// Action 13 - Enable / Disable extra output (blower fan)

/*
 WARNING: If you are using Gen 4 electronics (e.g. a Thing-o-Matic or a
 heavily modified Cupcake), THEN DO NOT USE M126 / M127. It can trigger
 a bug in the Gen 4 Extruder Controller firmware that will cause the
 HBP temperature to go wild.  Note that the Extruder Controller is a
 separate uprocessor on a separate board.  It has it's own firmware.
 It's not clear if the bug is firmware-only or if there is a problem
 with electronics as well (e.g. the HBP FET sees some residual current
 from the EXTRA FET and its Vgs/Igs threshold is met and it activates).
 But, there's no fix for the bug since no one has invested the time in
 diagnosing this Extruder Controller issue.
 
 - dnewman 22/11/2013
 */

static int set_valve(Gpx *gpx, unsigned extruder_id, unsigned state)
{
    assert(extruder_id < gpx->machine.extruder_count);
    if(gpx->machine.type >= MACHINE_TYPE_REPLICATOR_1) {

        begin_frame(gpx);
        
        write_8(gpx, 136);
        
        // uint8: ID of the extruder to query
        write_8(gpx, extruder_id);
        
        // uint8: Action command to send to the extruder
        write_8(gpx, 13);
        
        // uint8: Length of the extruder command payload (N)
        write_8(gpx, 1);
        
        // uint8: 1 to enable, 0 to disable
        write_8(gpx, state);
        
        return end_frame(gpx);
    }
    else if(gpx->flag.logMessages) {
        SHOW( fprintf(gpx->log, "(line %u) Semantic warning: ignoring M126/M127 with Gen 4 extruder electronics" EOL, gpx->lineNumber) );
    }
    return SUCCESS;
}

// Action 31 - Set build platform target temperature

static int set_build_platform_temperature(Gpx *gpx, unsigned extruder_id, unsigned temperature)
{
    assert(extruder_id < gpx->machine.extruder_count);

    double tDelta = (double)temperature - (double)gpx->tool[extruder_id].build_platform_temperature - AMBIENT_TEMP;
    if(tDelta > 0.0) {
        gpx->accumulated.time += tDelta * HBP_TIME;
    }
    
    begin_frame(gpx);
    
    write_8(gpx, 136);
    
    // uint8: ID of the extruder to query
    write_8(gpx, extruder_id);
    
    // uint8: Action command to send to the extruder
    write_8(gpx, 31);
    
    // uint8: Length of the extruder command payload (N)
    write_8(gpx, 2);
    
    // int16: Desired target temperature, in Celsius
    write_16(gpx, temperature);
    
    return end_frame(gpx);
}

// 137 - Enable / Disable axes steppers

static int set_steppers(Gpx *gpx, unsigned axes, unsigned state)
{
    unsigned bitfield = axes & AXES_BIT_MASK;
    if(state) {
        bitfield |= 0x80;
    }
    
    begin_frame(gpx);
    
    write_8(gpx, 137);
    
    // uint8: Bitfield codifying the command (see below)
    write_8(gpx, bitfield);
    
    return end_frame(gpx);
}
 
// 139 - Queue absolute point

static int queue_absolute_point(Gpx *gpx)
{
    long longestDDA = gpx->longestDDA ? gpx->longestDDA : get_longest_dda(gpx);
    Point5d steps = mm_to_steps(gpx, &gpx->target.position, &gpx->excess);
    
    begin_frame(gpx);
    
    write_8(gpx, 139);
    
    // int32: X coordinate, in steps
    write_32(gpx, (int)steps.x);
    
    // int32: Y coordinate, in steps
    write_32(gpx, (int)steps.y);
    
    // int32: Z coordinate, in steps
    write_32(gpx, (int)steps.z);
    
    // int32: A coordinate, in steps
    write_32(gpx, -(int)steps.a);
    
    // int32: B coordinate, in steps
    write_32(gpx, -(int)steps.b);
    
    // uint32: Feedrate, in microseconds between steps on the max delta. (DDA)
    write_32(gpx, (int)longestDDA);
    
    // reset current position
    gpx->axis.positionKnown = gpx->axis.mask;
    
    return end_frame(gpx);
}

// 140 - Set extended position

static int set_position(Gpx *gpx)
{
    Point5d steps = mm_to_steps(gpx, &gpx->current.position, NULL);
    
    begin_frame(gpx);
    
    write_8(gpx, 140);
    
    // int32: X position, in steps
    write_32(gpx, (int)steps.x);
    
    // int32: Y position, in steps
    write_32(gpx, (int)steps.y);
    
    // int32: Z position, in steps
    write_32(gpx, (int)steps.z);
    
    // int32: A position, in steps
    write_32(gpx, (int)steps.a);
    
    // int32: B position, in steps
    write_32(gpx, (int)steps.b);
    
    return end_frame(gpx);
}

// 141 - Wait for build platform ready

static int wait_for_build_platform(Gpx *gpx, unsigned extruder_id, int timeout)
{
    assert(extruder_id < gpx->machine.extruder_count);
    
    begin_frame(gpx);
    
    write_8(gpx, 141);
    
    // uint8: ID of the extruder platform to wait for
    write_8(gpx, extruder_id);
    
    // uint16: delay between query packets sent to the extruder, in ms (nominally 100 ms)
    write_16(gpx, 100);
    
    // uint16: Timeout before continuing without extruder ready, in seconds (nominally 1 minute)
    write_16(gpx, timeout);
    
    return end_frame(gpx);
}

// 142 - Queue extended point, new style

#if ENABLE_SIMULATED_RPM
static int queue_new_point(Gpx *gpx, unsigned milliseconds)
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
    if(gpx->tool[A].motor_enabled && gpx->tool[A].rpm) {
        double maxrpm = gpx->machine.a.max_feedrate * gpx->machine.a.steps_per_mm / gpx->machine.a.motor_steps;
        double rpm = gpx->tool[A].rpm > maxrpm ? maxrpm : gpx->tool[A].rpm;
        double minutes = milliseconds / 60000.0;
        // minute * revolution/minute
        double numRevolutions = minutes * (gpx->tool[A].motor_enabled > 0 ? rpm : -rpm);
        // steps/revolution * mm/steps
        double mmPerRevolution = gpx->machine.a.motor_steps * (1 / gpx->machine.a.steps_per_mm);
        target.a = -(numRevolutions * mmPerRevolution);
        gpx->command.flag |= A_IS_SET;
        gpx->accumulated.a += fabs(target.a);
    }
    
    if(gpx->tool[B].motor_enabled && gpx->tool[B].rpm) {
        double maxrpm = gpx->machine.b.max_feedrate * gpx->machine.b.steps_per_mm / gpx->machine.b.motor_steps;
        double rpm = gpx->tool[B].rpm > maxrpm ? maxrpm : gpx->tool[B].rpm;
        double minutes = milliseconds / 60000.0;
        // minute * revolution/minute
        double numRevolutions = minutes * (gpx->tool[B].motor_enabled > 0 ? rpm : -rpm);
        // steps/revolution * mm/steps
        double mmPerRevolution = gpx->machine.b.motor_steps * (1 / gpx->machine.b.steps_per_mm);
        target.b = -(numRevolutions * mmPerRevolution);
        gpx->command.flag |= B_IS_SET;
        gpx->accumulated.b += fabs(target.a);
    }

    Point5d steps = mm_to_steps(gpx, &target, &gpx->excess);

    gpx->accumulated.time += (milliseconds / 1000.0) * ACCELERATION_TIME;
    
    begin_frame(gpx);

    write_8(gpx, 142);
    
    // int32: X coordinate, in steps
    write_32(gpx, (int)steps.x);
    
    // int32: Y coordinate, in steps
    write_32(gpx, (int)steps.y);
    
    // int32: Z coordinate, in steps
    write_32(gpx, (int)steps.z);
    
    // int32: A coordinate, in steps
    write_32(gpx, (int)steps.a);
    
    // int32: B coordinate, in steps
    write_32(gpx, (int)steps.b);
    
    // uint32: Duration of the movement, in microseconds
    write_32(gpx, milliseconds * 1000.0);
    
    // uint8: Axes bitfield to specify which axes are relative. Any axis with a bit set should make a relative movement.
    write_8(gpx, AXES_BIT_MASK);
    
    return end_frame(gpx);
}
#endif

// 143 - Store home positions

static int store_home_positions(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 143);
    
    // uint8: Axes bitfield to specify which axes' positions to store.
    // Any axis with a bit set should have its position stored.
    write_8(gpx, gpx->command.flag & AXES_BIT_MASK);
    
    return end_frame(gpx);
}

// 144 - Recall home positions

static int recall_home_positions(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 144);
    
    // uint8: Axes bitfield to specify which axes' positions to recall.
    // Any axis with a bit set should have its position recalled.
    write_8(gpx, gpx->command.flag & AXES_BIT_MASK);
    
    return end_frame(gpx);
}

// 145 - Set digital potentiometer value

static int set_pot_value(Gpx *gpx, unsigned axis, unsigned value)
{
    assert(axis <= 4);
    assert(value <= 127);
    
    begin_frame(gpx);
    
    write_8(gpx, 145);
    
    // uint8: axis value (valid range 0-4) which axis pot to set
    write_8(gpx, axis);
    
    // uint8: value (valid range 0-127), values over max will be capped at max
    write_8(gpx, value);
    
    return end_frame(gpx);
}
 
// 146 - Set RGB LED value

static int set_LED(Gpx *gpx, unsigned red, unsigned green, unsigned blue, unsigned blink)
{
    begin_frame(gpx);
    
    write_8(gpx, 146);
    
    // uint8: red value (all pix are 0-255)
    write_8(gpx, red);
    
    // uint8: green
    write_8(gpx, green);
    
    // uint8: blue
    write_8(gpx, blue);
    
    // uint8: blink rate (0-255 valid)
    write_8(gpx, blink);
    
    // uint8: 0 (reserved for future use)
    write_8(gpx, 0);
    
    return end_frame(gpx);
}

static int set_LED_RGB(Gpx *gpx, unsigned rgb, unsigned blink)
{
    begin_frame(gpx);
    
    write_8(gpx, 146);
    
    // uint8: red value (all pix are 0-255)
    write_8(gpx, (rgb >> 16) & 0xFF);
    
    // uint8: green
    write_8(gpx, (rgb >> 8) & 0xFF);
    
    // uint8: blue
    write_8(gpx, rgb & 0xFF);
    
    // uint8: blink rate (0-255 valid)
    write_8(gpx, blink);
    
    // uint8: 0 (reserved for future use)
    write_8(gpx, 0);

    return end_frame(gpx);
}

// 147 - Set Beep

static int set_beep(Gpx *gpx, unsigned frequency, unsigned milliseconds)
{
    begin_frame(gpx);
    
    write_8(gpx, 147);
    
    // uint16: frequency
    write_16(gpx, frequency);

    // uint16: buzz length in ms
    write_16(gpx, milliseconds);

    // uint8: 0 (reserved for future use)
    write_8(gpx, 0);
    
    return end_frame(gpx);
}

// 148 - Pause for button

#define BUTTON_CENTER   0x01
#define BUTTON_RIGHT    0x02
#define BUTTON_LEFT     0x04
#define BUTTON_DOWN     0x08
#define BUTTON_UP       0x10
#define BUTTON_RESET    0x20

// Button options

#define READY_ON_TIMEOUT 0x01   // change to ready state on timeout
#define RESET_ON_TIMEOUT 0x02   // reset on timeout
#define CLEAR_ON_PRESS  0x04    // clear screen on button press

static int wait_for_button(Gpx *gpx, int button, unsigned timeout, int button_options)
{
    begin_frame(gpx);
    
    write_8(gpx, 148);
    
    // uint8: Bit field of buttons to wait for
    write_8(gpx, button);
    
    // uint16: Timeout, in seconds. A value of 0 indicates that the command should not time out.
    write_16(gpx, timeout);
    
    // uint8: Options bitfield
    write_8(gpx, button_options);
    
    return end_frame(gpx);
}

// 149 - Display message to LCD

static int display_message(Gpx *gpx, char *message, unsigned vPos, unsigned hPos, unsigned timeout, int wait_for_button)
{
    assert(vPos < 4);
    assert(hPos < 20);
    
    int rval;
    long bytesSent = 0;
    unsigned bitfield = 0;
    unsigned seconds = 0;
    
    unsigned maxLength = hPos ? 20 - hPos : 20;
    
    // clip string so it fits in 4 x 20 lcd display buffer
    long length = strlen(message);
    if(vPos || hPos) {
        if(length > maxLength) length = maxLength;
        bitfield |= 0x01; //do not clear flag
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
        
        begin_frame(gpx);
        
        write_8(gpx, 149);
        
        // uint8: Options bitfield (see below)
        write_8(gpx, bitfield);
        // uint8: Horizontal position to display the message at (commonly 0-19)
        write_8(gpx, hPos);
        // uint8: Vertical position to display the message at (commonly 0-3)
        write_8(gpx, vPos);
        // uint8: Timeout, in seconds. If 0, this message will left on the screen
        write_8(gpx, seconds);
        // 1+N bytes: Message to write to the screen, in ASCII, terminated with a null character.
        long rowLength = length - bytesSent;
        bytesSent += write_string(gpx, message + bytesSent, rowLength < maxLength ? rowLength : maxLength);
        
        CALL( end_frame(gpx) );
    }
    return SUCCESS;
}

// 150 - Set Build Percentage

static int set_build_progress(Gpx *gpx, unsigned percent)
{
    if(percent > 100) percent = 100;
    
    begin_frame(gpx);
    
    write_8(gpx, 150);
    
    // uint8: percent (0-100)
    write_8(gpx, percent);
    
    // uint8: 0 (reserved for future use)
    write_8(gpx, 0);
    
    return end_frame(gpx);
}

// 151 - Queue Song

static int queue_song(Gpx *gpx, unsigned song_id)
{
    // song ID 0: error tone with 4 cycles
    // song ID 1: done tone
    // song ID 2: error tone with 2 cycles
    
    assert(song_id <= 2);
    
    begin_frame(gpx);
    
    write_8(gpx, 151);
    
    // uint8: songID: select from a predefined list of songs
    write_8(gpx, song_id);
    
    return end_frame(gpx);
}

// 152 - Reset to factory defaults

static int factory_defaults(Gpx *gpx)
{
    begin_frame(gpx);

    write_8(gpx, 152);
    
    // uint8: 0 (reserved for future use)
    write_8(gpx, 0);
    
    return end_frame(gpx);
}


// 153 - Build start notification

static int start_build(Gpx *gpx, char * filename)
{
    begin_frame(gpx);
    
    write_8(gpx, 153);
    
    // uint32: 0 (reserved for future use)
    write_32(gpx, 0);

    // 1+N bytes: Name of the build, in ASCII, null terminated
    write_string(gpx, filename, strlen(filename));
    
    return end_frame(gpx);
}

// 154 - Build end notification

static int end_build(Gpx *gpx)
{
    begin_frame(gpx);
    
    write_8(gpx, 154);

    // uint8: 0 (reserved for future use)
    write_8(gpx, 0);
    
    return end_frame(gpx);
}
 
// 155 - Queue extended point x3g

// IMPORTANT: this command updates the parser state

static int queue_ext_point(Gpx *gpx, double feedrate)
{
    /* If we don't know our previous position on a command axis, we can't calculate the feedrate
       or distance correctly, so we use an unaccelerated command with a fixed DDA. */
    unsigned mask = gpx->command.flag & gpx->axis.mask;
    if((gpx->axis.positionKnown & mask) != mask) {
        return queue_absolute_point(gpx);
    }
    Point5d deltaMM = delta_mm(gpx);
    Point5d deltaSteps = delta_steps(gpx, deltaMM);
    
    // check that we have actually moved on at least one axis when the move is
    // rounded down to the nearest step
    if(magnitude(gpx->command.flag, &deltaSteps) > 0) {
        double distance = magnitude(gpx->command.flag & XYZ_BIT_MASK, &deltaMM);
        // are we moving and extruding?
        if(gpx->flag.rewrite5D && (gpx->command.flag & (A_IS_SET|B_IS_SET)) && distance > 0.0001) {
            double filament_radius, packing_area, packing_scale;
            if(A_IS_SET && deltaMM.a > 0.0001) {
                if(gpx->override[A].actual_filament_diameter > 0.0001) {
                    filament_radius = gpx->override[A].actual_filament_diameter / 2;
                    packing_area = M_PI * filament_radius * filament_radius * gpx->override[A].packing_density;
                }
                else {
                    filament_radius = gpx->machine.nominal_filament_diameter / 2;
                    packing_area = M_PI * filament_radius * filament_radius * gpx->machine.nominal_packing_density;
                }
                packing_scale = gpx->machine.nozzle_diameter * gpx->layerHeight / packing_area;
                if(deltaMM.a > 0) {
                    deltaMM.a = distance * packing_scale;
                }
                else {
                    deltaMM.a = -(distance * packing_scale);
                }
                gpx->target.position.a = gpx->current.position.a + deltaMM.a;
                deltaSteps.a = round(fabs(deltaMM.a) * gpx->machine.a.steps_per_mm);
            }
            if(B_IS_SET && deltaMM.b > 0.0001) {
                if(gpx->override[B].actual_filament_diameter > 0.0001) {
                    filament_radius = gpx->override[B].actual_filament_diameter / 2;
                    packing_area = M_PI * filament_radius * filament_radius * gpx->override[A].packing_density;
                }
                else {
                    filament_radius = gpx->machine.nominal_filament_diameter / 2;
                    packing_area = M_PI * filament_radius * filament_radius * gpx->machine.nominal_packing_density;
                }
                packing_scale = gpx->machine.nozzle_diameter * gpx->layerHeight / packing_area;
                if(deltaMM.b > 0) {
                    deltaMM.b = distance * packing_scale;
                }
                else {
                    deltaMM.b = -(distance * packing_scale);
                }
                gpx->target.position.b = gpx->current.position.b + deltaMM.b;
                deltaSteps.b = round(fabs(deltaMM.b) * gpx->machine.b.steps_per_mm);
            }
        }
        Point5d target = gpx->target.position;
        
        target.a = -deltaMM.a;
        target.b = -deltaMM.b;
        
        gpx->accumulated.a += deltaMM.a;
        gpx->accumulated.b += deltaMM.b;
        
        deltaMM.x = fabs(deltaMM.x);
        deltaMM.y = fabs(deltaMM.y);
        deltaMM.z = fabs(deltaMM.z);
        deltaMM.a = fabs(deltaMM.a);
        deltaMM.b = fabs(deltaMM.b);
        
        feedrate = get_safe_feedrate(gpx, gpx->command.flag, &deltaMM);
        double minutes = distance / feedrate;
        
        if(minutes == 0) {
            distance = 0;
            if(gpx->command.flag & A_IS_SET) {
                distance = deltaMM.a;
            }
            if(gpx->command.flag & B_IS_SET && distance < deltaMM.b) {
                distance = deltaMM.b;
            }
            minutes = distance / feedrate;
        }
        
        //convert feedrate to mm/sec
        feedrate /= 60.0;

#if ENABLE_SIMULATED_RPM
        // if either a or b is 0, but their motor is on and turning, 'simulate' a 5D extrusion distance
        if(deltaMM.a == 0.0 && gpx->tool[A].motor_enabled && gpx->tool[A].rpm) {
            double maxrpm = gpx->machine.a.max_feedrate * gpx->machine.a.steps_per_mm / gpx->machine.a.motor_steps;
            double rpm = gpx->tool[A].rpm > maxrpm ? maxrpm : gpx->tool[A].rpm;
            // minute * revolution/minute
            double numRevolutions = minutes * (gpx->tool[A].motor_enabled > 0 ? rpm : -rpm);
            // steps/revolution * mm/steps
            double mmPerRevolution = gpx->machine.a.motor_steps * (1 / gpx->machine.a.steps_per_mm);
            // set distance
            deltaMM.a = numRevolutions * mmPerRevolution;
            deltaSteps.a = round(fabs(deltaMM.a) * gpx->machine.a.steps_per_mm);
            target.a = -deltaMM.a;
        }
        else {
            // disable RPM as soon as we begin 5D printing
            gpx->tool[A].rpm = 0;
        }
        if(deltaMM.b == 0.0 && gpx->tool[B].motor_enabled && gpx->tool[B].rpm) {
            double maxrpm = gpx->machine.b.max_feedrate * gpx->machine.b.steps_per_mm / gpx->machine.b.motor_steps;
            double rpm = gpx->tool[B].rpm > maxrpm ? maxrpm : gpx->tool[B].rpm;
            // minute * revolution/minute
            double numRevolutions = minutes * (gpx->tool[B].motor_enabled > 0 ? rpm : -rpm);
            // steps/revolution * mm/steps
            double mmPerRevolution = gpx->machine.b.motor_steps * (1 / gpx->machine.b.steps_per_mm);
            // set distance
            deltaMM.b = numRevolutions * mmPerRevolution;
            deltaSteps.b = round(fabs(deltaMM.b) * gpx->machine.b.steps_per_mm);
            target.b = -deltaMM.b;
        }
        else {
            // disable RPM as soon as we begin 5D printing
            gpx->tool[B].rpm = 0;
        }
#endif
        
        Point5d steps = mm_to_steps(gpx, &target, &gpx->excess);
        
        double usec = (60000000.0 * minutes);
        
        double dda_interval = usec / largest_axis(gpx->command.flag, &deltaSteps);
        
        // Convert dda_interval into dda_rate (dda steps per second on the longest axis)
        double dda_rate = 1000000.0 / dda_interval;
        
        gpx->accumulated.time += (minutes * 60) * ACCELERATION_TIME;

        begin_frame(gpx);
        
        write_8(gpx, 155);
        
        // int32: X coordinate, in steps
        write_32(gpx, (int)steps.x);
        
        // int32: Y coordinate, in steps
        write_32(gpx, (int)steps.y);
        
        // int32: Z coordinate, in steps
        write_32(gpx, (int)steps.z);
        
        // int32: A coordinate, in steps
        write_32(gpx, (int)steps.a);
        
        // int32: B coordinate, in steps
        write_32(gpx, (int)steps.b);

        // uint32: DDA Feedrate, in steps/s
        write_32(gpx, (unsigned)dda_rate);
        
        // uint8: Axes bitfield to specify which axes are relative. Any axis with a bit set should make a relative movement.
        write_8(gpx, A_IS_SET|B_IS_SET);

        // float (single precision, 32 bit): mm distance for this move.  normal of XYZ if any of these axes are active, and AB for extruder only moves
        write_float(gpx, (float)distance);
        
        // uint16: feedrate in mm/s, multiplied by 64 to assist fixed point calculation on the bot
        write_16(gpx, (unsigned)(feedrate * 64.0));
        
        return end_frame(gpx);        
	}
    return SUCCESS;
}

// 156 - Set segment acceleration

static int set_acceleration(Gpx *gpx, int state)
{
    begin_frame(gpx);
    
    write_8(gpx, 156);
    
    // uint8: 1 to enable, 0 to disable
    write_8(gpx, state);
    
    return end_frame(gpx);
}

// 157 - Stream Version

static int stream_version(Gpx *gpx)
{
    if(gpx->machine.type >= MACHINE_TYPE_REPLICATOR_1) {
        begin_frame(gpx);
        
        write_8(gpx, 157);
        
        // uint8: x3g version high byte
        write_8(gpx, STREAM_VERSION_HIGH);
        
        // uint8: x3g version low byte
        write_8(gpx, STREAM_VERSION_LOW);
        
        // uint8: not implemented
        write_8(gpx, 0);
        
        // uint32: not implemented
        write_32(gpx, 0);
        
        // uint16: bot type: PID for the intended bot is sent
        // Repliator 2/2X (Might Two)
        if(gpx->machine.type >= MACHINE_TYPE_REPLICATOR_2) {
            write_16(gpx, 0xB015);
        }
        // Replicator (Might One)
        else {
            write_16(gpx, 0xD314);
        }
        
        // uint16: not implemented
        write_16(gpx, 0);
        
        // uint32: not implemented
        write_32(gpx, 0);
        
        // uint32: not implemented
        write_32(gpx, 0);
        
        // uint8: not implemented
        write_8(gpx, 0);
        
        return end_frame(gpx);
    }
    return SUCCESS;
}

// 158 - Pause @ zPos

static int pause_at_zpos(Gpx *gpx, float z_positon)
{
    begin_frame(gpx);
    
    write_8(gpx, 158);
    
    // uint8: pause at Z coordinate or 0.0 to disable
    write_float(gpx, z_positon);
    
    return end_frame(gpx);
}

// COMMAND @ ZPOS FUNCTIONS

// find an existing filament definition

static int find_filament(Gpx *gpx, char *filament_id)
{
    int i, index = -1;
    int l = gpx->filamentLength;
    // a brute force search is generally fastest for low n
    for(i = 0; i < l; i++) {
        if(strcmp(filament_id, gpx->filament[i].colour) == 0) {
            index = i;
            break;
        }
    }
    return index;
}

// add a new filament definition

static int add_filament(Gpx *gpx, char *filament_id, double diameter, unsigned temperature, unsigned LED)
{
    int index = find_filament(gpx, filament_id);
    if(index < 0) {
        if(gpx->filamentLength < FILAMENT_MAX) {
            index = gpx->filamentLength++;
            gpx->filament[index].colour = strdup(filament_id);
            gpx->filament[index].diameter = diameter;
            gpx->filament[index].temperature = temperature;
            gpx->filament[index].LED = LED;
        }
        else {
            SHOW( fprintf(gpx->log, "(line %u) Buffer overflow: too many @filament definitions (maximum = %i)" EOL, gpx->lineNumber, FILAMENT_MAX - 1) );
            index = 0;
        }
    }
    return index;
}

// append a new command at z function

static int add_command_at(Gpx *gpx, double z, char *filament_id, unsigned nozzle_temperature, unsigned build_platform_temperature)
{
    int rval;
    int index = filament_id ? find_filament(gpx, filament_id) : 0;
    if(index < 0) {
        SHOW( fprintf(gpx->log, "(line %u) Semantic error: @pause macro with undefined filament name '%s', use a @filament macro to define it" EOL, gpx->lineNumber, filament_id) );
        index = 0;
    }
    // insert command
    if(gpx->commandAtLength < COMMAND_AT_MAX) {
        if(gpx->flag.loadMacros) {
            int i = gpx->commandAtLength;
            if(z <= gpx->commandAtZ) {
                // make a space
                while(i > 0 && z <= gpx->commandAt[i - 1].z) {
                    gpx->commandAt[i] = gpx->commandAt[i - 1];
                    i--;
                }
                gpx->commandAt[i].z = z;
                gpx->commandAt[i].filament_index = index;
                gpx->commandAt[i].nozzle_temperature = nozzle_temperature;
                gpx->commandAt[i].build_platform_temperature = build_platform_temperature;
                gpx->commandAtZ = gpx->commandAt[gpx->commandAtLength].z;
            }
            // append command
            else {
                gpx->commandAt[i].z = z;
                gpx->commandAt[i].filament_index = index;
                gpx->commandAt[i].nozzle_temperature = nozzle_temperature;
                gpx->commandAt[i].build_platform_temperature = build_platform_temperature;
                gpx->commandAtZ = z;
            }
            // nonzero temperature signals a temperature change, not a pause @ zPos
            // so if its the first pause @ zPos que it up
            if(nozzle_temperature == 0 && build_platform_temperature == 0 && gpx->commandAtLength == 0) {
                if(gpx->flag.macrosEnabled) {
                    CALL( pause_at_zpos(gpx, z) );
                }
                else {
                    gpx->flag.pausePending = 1;
                }
            }
            gpx->commandAtLength++;
        }
    }
    else {
        SHOW( fprintf(gpx->log, "(line %u) Buffer overflow: too many @pause definitions (maximum = %i)" EOL, gpx->lineNumber, COMMAND_AT_MAX) );
    }
    return SUCCESS;
}

static int display_tag(Gpx *gpx) {
    int rval;
    CALL( display_message(gpx, "GPX " GPX_VERSION, 0, 0, 2, 0) );
    return SUCCESS;
}

// TARGET POSITION

// calculate target position

static int calculate_target_position(Gpx *gpx)
{
    int rval;
    // G10 ofset
    Point3d userOffset = gpx->offset[gpx->current.offset];
    double userScale = 1.0;

    if(gpx->flag.macrosEnabled) {
        // plus command line offset
        userOffset.x += gpx->user.offset.x;
        userOffset.y += gpx->user.offset.y;
        userOffset.z += gpx->user.offset.z;
        // multiply by command line scale
        userScale = gpx->user.scale;
    }
    
    // CALCULATE TARGET POSITION
    
    // x
    if(gpx->command.flag & X_IS_SET) {
        gpx->target.position.x = gpx->flag.relativeCoordinates ? (gpx->current.position.x + (gpx->command.x * userScale)) : ((gpx->command.x + userOffset.x) * userScale);
    }
    else {
        gpx->target.position.x = gpx->current.position.x;
    }
    
    // y
    if(gpx->command.flag & Y_IS_SET) {
        gpx->target.position.y = gpx->flag.relativeCoordinates ? (gpx->current.position.y + (gpx->command.y * userScale)) : ((gpx->command.y + userOffset.y) * userScale);
    }
    else {
        gpx->target.position.y = gpx->current.position.y;
    }
    
    // z
    if(gpx->command.flag & Z_IS_SET) {
        gpx->target.position.z = gpx->flag.relativeCoordinates ? (gpx->current.position.z + (gpx->command.z * userScale)) : ((gpx->command.z + userOffset.z) * userScale);
    }
    else {
        gpx->target.position.z = gpx->current.position.z;
    }

    // a
    if(gpx->command.flag & A_IS_SET) {
        double a = (gpx->override[A].filament_scale == 1.0) ? gpx->command.a : (gpx->command.a * gpx->override[A].filament_scale);
        gpx->target.position.a = (gpx->flag.relativeCoordinates || gpx->flag.extruderIsRelative) ? (gpx->current.position.a + a) : a;
    }
    else {
        gpx->target.position.a = gpx->current.position.a;
    }

    // b
    if(gpx->command.flag & B_IS_SET) {
        double b = (gpx->override[B].filament_scale == 1.0) ? gpx->command.b : (gpx->command.b * gpx->override[B].filament_scale);
        gpx->target.position.b = (gpx->flag.relativeCoordinates || gpx->flag.extruderIsRelative) ? (gpx->current.position.b + b) : b;
    }
    else {
        gpx->target.position.b = gpx->current.position.b;
    }
    
    // update current feedrate
    if(gpx->command.flag & F_IS_SET) {
        gpx->current.feedrate = gpx->command.f;
    }
    
    // DITTO PRINTING
    
    if(gpx->flag.dittoPrinting) {
        if(gpx->command.flag & A_IS_SET) {
            gpx->target.position.b = gpx->target.position.a;
            gpx->command.flag |= B_IS_SET;
        }
        else if(gpx->command.flag & B_IS_SET) {
            gpx->target.position.a = gpx->target.position.b;
            gpx->command.flag |= A_IS_SET;
        }
    }
    
    // CHECK FOR COMMAND @ Z POS
    
    // check if there are more commands on the stack
    if(gpx->flag.macrosEnabled && gpx->flag.runMacros && gpx->commandAtIndex < gpx->commandAtLength) {
        // check if the next command will cross the z threshold
        if(gpx->commandAt[gpx->commandAtIndex].z <= gpx->target.position.z) {
            // is this a temperature change macro?
            if(gpx->commandAt[gpx->commandAtIndex].nozzle_temperature || gpx->commandAt[gpx->commandAtIndex].build_platform_temperature) {
                unsigned nozzle_temperature = gpx->commandAt[gpx->commandAtIndex].nozzle_temperature;
                unsigned build_platform_temperature = gpx->commandAt[gpx->commandAtIndex].build_platform_temperature;
                // make sure the temperature has changed
                if(nozzle_temperature) {
                    if((gpx->current.extruder == A || gpx->tool[A].nozzle_temperature) && gpx->tool[A].nozzle_temperature != nozzle_temperature) {
                        CALL( set_nozzle_temperature(gpx, A, nozzle_temperature) );
                        gpx->tool[A].nozzle_temperature = gpx->override[A].active_temperature = nozzle_temperature;
                        VERBOSE( fprintf(gpx->log, "(@zPos %0.2f) Nozzle[A] temperature %uc" EOL,
                                         gpx->commandAt[gpx->commandAtIndex].z,
                                         nozzle_temperature) );
                    }
                    if((gpx->current.extruder == B || gpx->tool[B].nozzle_temperature) && gpx->tool[B].nozzle_temperature != nozzle_temperature) {
                        CALL( set_nozzle_temperature(gpx, B, nozzle_temperature) );
                        gpx->tool[B].nozzle_temperature = gpx->override[B].active_temperature = nozzle_temperature;
                        VERBOSE( fprintf(gpx->log, "(@zPos %0.2f) Nozzle[B] temperature %uc" EOL,
                                         gpx->commandAt[gpx->commandAtIndex].z,
                                         nozzle_temperature) );
                    }
                }
                if(build_platform_temperature) {
                    if(gpx->machine.a.has_heated_build_platform && gpx->tool[A].build_platform_temperature && gpx->tool[A].build_platform_temperature != build_platform_temperature) {
                        CALL( set_build_platform_temperature(gpx, A, build_platform_temperature) );
                        gpx->tool[A].build_platform_temperature = gpx->override[A].build_platform_temperature = build_platform_temperature;
                        VERBOSE( fprintf(gpx->log, "(@zPos %0.2f) Build platform[A] temperature %uc" EOL,
                                         gpx->commandAt[gpx->commandAtIndex].z,
                                         build_platform_temperature) );
                    }
                    else if(gpx->machine.b.has_heated_build_platform && gpx->tool[B].build_platform_temperature && gpx->tool[B].build_platform_temperature != build_platform_temperature) {
                        CALL( set_build_platform_temperature(gpx, B, build_platform_temperature) );
                        gpx->tool[B].build_platform_temperature = gpx->override[B].build_platform_temperature = build_platform_temperature;
                        VERBOSE( fprintf(gpx->log, "(@zPos %0.2f) Build platform[B] temperature %uc" EOL,
                                         gpx->commandAt[gpx->commandAtIndex].z,
                                         build_platform_temperature) );
                    }
                }
                gpx->commandAtIndex++;
            }
            // no its a pause macro
            else if(gpx->commandAt[gpx->commandAtIndex].z <= gpx->target.position.z) {
                int index = gpx->commandAt[gpx->commandAtIndex].filament_index;
                VERBOSE( fprintf(gpx->log, "(@zPos %0.2f) %s",
                                 gpx->commandAt[gpx->commandAtIndex].z,
                                 gpx->filament[index].colour) );
                // override filament diameter
                double filament_diameter = gpx->filament[index].diameter;
                if(filament_diameter > 0.0001) {
                    VERBOSE( fprintf(gpx->log, ", %0.2fmm", filament_diameter) );
                    if(gpx->flag.dittoPrinting) {
                        set_filament_scale(gpx, B, filament_diameter);
                        set_filament_scale(gpx, A, filament_diameter);
                    }
                    else {
                        set_filament_scale(gpx, gpx->current.extruder, filament_diameter);
                    }
                }
                unsigned temperature = gpx->filament[index].temperature;
                // override nozzle temperature
                if(temperature) {
                    VERBOSE( fprintf(gpx->log, ", %uc", temperature) );                    
                    if(gpx->tool[gpx->current.extruder].nozzle_temperature != temperature) {
                        if(gpx->flag.dittoPrinting) {
                            CALL( set_nozzle_temperature(gpx, B, temperature) );
                            CALL( set_nozzle_temperature(gpx, A, temperature));
                            gpx->tool[A].nozzle_temperature = gpx->tool[B].nozzle_temperature = temperature;
                        }
                        else {
                            CALL( set_nozzle_temperature(gpx, gpx->current.extruder, temperature) );
                            gpx->tool[gpx->current.extruder].nozzle_temperature = temperature;
                        }
                    }
                }
                // override LED colour
                if(gpx->filament[index].LED) {
                    CALL( set_LED_RGB(gpx, gpx->filament[index].LED, 0) );
                }
                gpx->commandAtIndex++;
                if(gpx->commandAtIndex < gpx->commandAtLength) {
                    gpx->flag.doPauseAtZPos = COMMAND_QUE_MAX;
                }
                VERBOSE( fputs(EOL, gpx->log) );
            }
        }
    }
    return SUCCESS;
}

static void update_current_position(Gpx *gpx)
{
    // the current position to tracks where the print head currently is
    if(gpx->target.position.z != gpx->current.position.z) {
        // calculate layer height
        gpx->layerHeight = fabs(gpx->target.position.z - gpx->current.position.z);
        // check upper bounds
        if(gpx->layerHeight > (gpx->machine.nozzle_diameter * 0.85)) {
            gpx->layerHeight = gpx->machine.nozzle_diameter * 0.85;
        }
    }
    gpx->current.position = gpx->target.position;
    if(!gpx->flag.relativeCoordinates) gpx->axis.positionKnown |= gpx->command.flag & gpx->axis.mask;
}

// TOOL CHANGE

static int do_tool_change(Gpx *gpx, int timeout) {
    int rval;
    // set the temperature of current tool to standby (if standby is different to active)
    if(gpx->override[gpx->current.extruder].standby_temperature
       && gpx->override[gpx->current.extruder].standby_temperature != gpx->tool[gpx->current.extruder].nozzle_temperature) {
        unsigned temperature = gpx->override[gpx->current.extruder].standby_temperature;
        CALL( set_nozzle_temperature(gpx, gpx->current.extruder, temperature) );
        gpx->tool[gpx->current.extruder].nozzle_temperature = temperature;
    }
    // set the temperature of selected tool to active (if active is different to standby)
    if(gpx->override[gpx->target.extruder].active_temperature
       && gpx->override[gpx->target.extruder].active_temperature != gpx->tool[gpx->target.extruder].nozzle_temperature) {
        unsigned temperature = gpx->override[gpx->target.extruder].active_temperature;
        CALL( set_nozzle_temperature(gpx, gpx->target.extruder, temperature) );
        gpx->tool[gpx->target.extruder].nozzle_temperature = temperature;
        // wait for nozzle to head up
        // CALL( wait_for_extruder(gpx, gpx->target.extruder, timeout) );
    }
    // switch any active G10 offset (G54 or G55)
    if(gpx->current.offset == gpx->current.extruder + 1) {
        gpx->current.offset = gpx->target.extruder + 1;
    }
    // change current toolhead in order to apply the calibration offset
    CALL( change_extruder_offset(gpx, gpx->target.extruder) );
    // set current extruder so changes in E are expressed as changes to A or B
    gpx->current.extruder = gpx->target.extruder;
    return SUCCESS;
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
 PRINTER:= ('printer' | 'machine' | 'slicer') (TYPE | PACKING_DENSITY | DIAMETER | TEMP | RGB)+
 TYPE:=  S+ ('c3' | 'c4' | 'cp4' | 'cpp' | 't6' | 't7' | 't7d' | 'r1' | 'r1d' | 'r2' | 'r2h' | 'r2x')
 PACKING_DENSITY:= S+ DIGIT+ ('.' DIGIT+)?
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

static int parse_macro(Gpx *gpx, const char* macro, char *p)
{
    int rval;
    char *name = NULL;
    double z = 0.0;
    double diameter = 0.0;
    unsigned nozzle_temperature = 0;
    unsigned build_platform_temperature = 0;
    unsigned LED = 0;

    while(*p != 0) {
        // trim any leading white space
        while(isspace(*p)) p++;
        if(isalpha(*p)) {
            name = p;
            while(*p && isalnum(*p)) p++;
            if(*p) *p++ = 0;
        }
        else if(isdigit(*p)) {
            char *t = p;
            while(*p && !isspace(*p)) p++;
            if(*(p - 1) == 'm') {
                diameter = strtod(t, NULL);
            }
            else if(*(p - 1) == 'c') {
                unsigned temperature = atoi(t);
                if(temperature > HBP_MAX) {
                    nozzle_temperature = temperature;
                }
                else {
                    build_platform_temperature = temperature;
                }
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
            char *t = strchr(p + 1, ')');
            if(t) {
                *t = 0;
                p = t + 1;
            }
            else {
                *p = 0;
            }
        }
        else {
            SHOW( fprintf(gpx->log, "(line %u) Syntax error: unrecognised macro parameter" EOL, gpx->lineNumber) );
            break;
        }
    }
    // ;@printer <TYPE> <PACKING_DENSITY> <DIAMETER>mm <HBP-TEMP>c #<LED-COLOUR>
    if(MACRO_IS("machine") || MACRO_IS("printer") || MACRO_IS("slicer")) {
        if(name) {
            if(gpx_set_machine(gpx, name)) {
                SHOW( fprintf(gpx->log, "(line %u) Semantic error: @%s macro with unrecognised type '%s'" EOL, gpx->lineNumber, macro, name) );
            }
            gpx->override[A].packing_density = gpx->machine.nominal_packing_density;
            gpx->override[B].packing_density = gpx->machine.nominal_packing_density;
        }
        if(z > 0.0001) {
            gpx->machine.nominal_packing_density = z;
        }
        if(diameter > 0.0001) gpx->machine.nominal_filament_diameter = diameter;
        if(build_platform_temperature) {
            if(gpx->machine.a.has_heated_build_platform) gpx->override[A].build_platform_temperature = build_platform_temperature;
            else if(gpx->machine.b.has_heated_build_platform) gpx->override[B].build_platform_temperature = build_platform_temperature;
            else {
                SHOW( fprintf(gpx->log, "(line %u) Semantic warning: @%s macro cannot override non-existant heated build platform" EOL, gpx->lineNumber, macro) );
            }
        }
        if(LED) {
            CALL( set_LED_RGB(gpx, LED, 0) );
        }
    }
    // ;@enable ditto
    // ;@enable progress
    else if(MACRO_IS("enable")) {
        if(name) {
            if(NAME_IS("ditto")) {
                if(gpx->machine.extruder_count == 1) {
                    SHOW( fprintf(gpx->log, "(line %u) Semantic warning: ditto printing cannot access non-existant second extruder" EOL, gpx->lineNumber) );
                    gpx->flag.dittoPrinting = 0;
                }
                else {
                    gpx->flag.dittoPrinting = 1;
                }
            }
            else if(NAME_IS("progress")) gpx->flag.buildProgress = 1;
            else {
                SHOW( fprintf(gpx->log, "(line %u) Semantic error: @enable macro with unrecognised parameter '%s'" EOL, gpx->lineNumber, name) );
            }
        }
        else {
            SHOW( fprintf(gpx->log, "(line %u) Syntax error: @enable macro with missing parameter" EOL, gpx->lineNumber) );
        }
    }
    // ;@filament <NAME> <DIAMETER>mm <TEMP>c #<LED-COLOUR>
    else if(MACRO_IS("filament")) {
        if(name) {
            add_filament(gpx, name, diameter, nozzle_temperature, LED);
        }
        else {
            SHOW( fprintf(gpx->log, "(line %u) Semantic error: @filament macro with missing name" EOL, gpx->lineNumber) );
        }
    }
    // ;@right <NAME> <PACKING_DENSITY> <DIAMETER>mm <TEMP>c
    else if(MACRO_IS("right")) {
        if(name) {
            int index = find_filament(gpx, name);
            if(index > 0) {
                if(gpx->filament[index].diameter > 0.0001) set_filament_scale(gpx, A, gpx->filament[index].diameter);
                if(gpx->filament[index].temperature) gpx->override[A].active_temperature = gpx->filament[index].temperature;
                return SUCCESS;
            }
        }
        if(z > 0.0001) gpx->override[A].packing_density = z;
        if(diameter > 0.0001) set_filament_scale(gpx, A, diameter);
        if(nozzle_temperature) gpx->override[A].active_temperature = nozzle_temperature;
    }
    // ;@left <NAME> <PACKING_DENSITY> <DIAMETER>mm <TEMP>c
    else if(MACRO_IS("left")) {
        if(name) {
            int index = find_filament(gpx, name);
            if(index > 0) {
                if(gpx->filament[index].diameter > 0.0001) set_filament_scale(gpx, B, gpx->filament[index].diameter);
                if(gpx->filament[index].temperature) gpx->override[B].active_temperature = gpx->filament[index].temperature;
                return SUCCESS;
            }
        }
        if(z > 0.0001) gpx->override[A].packing_density = z;
        if(diameter > 0.0001) set_filament_scale(gpx, B, diameter);
        if(nozzle_temperature) gpx->override[B].active_temperature = nozzle_temperature;
    }
    // ;@pause <ZPOS> <NAME>
    else if(MACRO_IS("pause")) {
        if(z > 0.0001) {
            CALL( add_command_at(gpx, z, name, 0, 0) );
        }
        else if(diameter > 0.0001) {
            CALL( add_command_at(gpx, diameter, name, 0, 0) );
        }
        else {
            SHOW( fprintf(gpx->log, "(line %u) Semantic error: @pause macro with missing zPos" EOL, gpx->lineNumber) );
        }
    }
    // ;@temp <ZPOS> <TEMP>c
    // ;@temperature <ZPOS> <TEMP>c
    else if(MACRO_IS("temp") || MACRO_IS("temperature")) {
        if(nozzle_temperature || build_platform_temperature) {
            if(z > 0.0001) {
                CALL( add_command_at(gpx, z, NULL, nozzle_temperature, build_platform_temperature) );
            }
            else if(diameter > 0.0001) {
                CALL( add_command_at(gpx, diameter, NULL, nozzle_temperature, build_platform_temperature) );
            }
            else {
                SHOW( fprintf(gpx->log, "(line %u) Semantic error: @%s macro with missing zPos" EOL, gpx->lineNumber, macro) );
            }
        }
        else {
            SHOW( fprintf(gpx->log, "(line %u) Semantic error: @%s macro with missing temperature" EOL, gpx->lineNumber, macro) );
        }
    }
    // ;@start <NAME> <TEMP>c
    else if(MACRO_IS("start")) {
        if(nozzle_temperature || build_platform_temperature) {
            if(nozzle_temperature) {
                VERBOSE( fprintf(gpx->log, "(@start) Nozzle temperature %uc" EOL, nozzle_temperature) );
                if(gpx->tool[A].nozzle_temperature && gpx->tool[A].nozzle_temperature != nozzle_temperature) {
                    if(program_is_running()) {
                        CALL( set_nozzle_temperature(gpx, A, nozzle_temperature) );
                    }
                    gpx->tool[A].nozzle_temperature = gpx->override[A].active_temperature = nozzle_temperature;
                }
                else {
                    gpx->override[A].active_temperature = nozzle_temperature;
                }
                if(gpx->tool[B].nozzle_temperature && gpx->tool[B].nozzle_temperature != nozzle_temperature) {
                    if(program_is_running()) {
                        CALL( set_nozzle_temperature(gpx, B, nozzle_temperature) );
                    }
                    gpx->tool[B].nozzle_temperature = gpx->override[B].active_temperature = nozzle_temperature;
                }
                else {
                    gpx->override[B].active_temperature = nozzle_temperature;
                }
            }
            if(build_platform_temperature) {
                VERBOSE( fprintf(gpx->log, "(@start) Build platform temperature %uc" EOL, build_platform_temperature) );
                if(gpx->machine.a.has_heated_build_platform && gpx->tool[A].build_platform_temperature && gpx->tool[A].build_platform_temperature != build_platform_temperature) {
                    if(program_is_running()) {
                        CALL( set_build_platform_temperature(gpx, A, build_platform_temperature) );
                    }
                    gpx->tool[A].build_platform_temperature = gpx->override[A].build_platform_temperature = build_platform_temperature;
                }
                else if(gpx->machine.b.has_heated_build_platform && gpx->tool[B].build_platform_temperature && gpx->tool[B].build_platform_temperature != build_platform_temperature) {
                    if(program_is_running()) {
                        CALL( set_build_platform_temperature(gpx, B, build_platform_temperature) );
                    }
                    gpx->tool[B].build_platform_temperature = gpx->override[B].build_platform_temperature = build_platform_temperature;
                }
            }
        }
        else if(name) {
            int index = find_filament(gpx, name);
            if(index > 0) {
                VERBOSE( fprintf(gpx->log, "(@start) %s", name) );
                if(gpx->filament[index].diameter > 0.0001) {
                    VERBOSE( fprintf(gpx->log, ", %0.2fmm", gpx->filament[index].diameter) );
                    if(gpx->flag.dittoPrinting) {
                        set_filament_scale(gpx, B, gpx->filament[index].diameter);
                        set_filament_scale(gpx, A, gpx->filament[index].diameter);
                    }
                    else {
                        set_filament_scale(gpx, gpx->current.extruder, gpx->filament[index].diameter);
                    }
                }
                if(gpx->filament[index].LED) {
                    CALL( set_LED_RGB(gpx, gpx->filament[index].LED, 0) );
                }
                nozzle_temperature = gpx->filament[index].temperature;
                if(nozzle_temperature) {
                    VERBOSE( fprintf(gpx->log, ", %uc", nozzle_temperature) );
                    if(gpx->tool[A].nozzle_temperature && gpx->tool[A].nozzle_temperature != nozzle_temperature) {
                        if(program_is_running()) {
                            CALL( set_nozzle_temperature(gpx, A, nozzle_temperature) );
                        }
                        gpx->tool[A].nozzle_temperature = gpx->override[A].active_temperature = nozzle_temperature;
                    }
                    else {
                        gpx->override[A].active_temperature = nozzle_temperature;
                    }
                    if(gpx->tool[B].nozzle_temperature && gpx->tool[B].nozzle_temperature != nozzle_temperature) {
                        if(program_is_running()) {
                            CALL( set_nozzle_temperature(gpx, B, nozzle_temperature) );
                        }
                        gpx->tool[B].nozzle_temperature = gpx->override[B].active_temperature = nozzle_temperature;
                    }
                    else {
                        gpx->override[B].active_temperature = nozzle_temperature;
                    }
                }
                VERBOSE( fputs(EOL, gpx->log) );
            }
            else {
                SHOW( fprintf(gpx->log, "(line %u) Semantic error: @start with undefined filament name '%s', use a @filament macro to define it" EOL, gpx->lineNumber, name ? name : "") );
            }
        }
    }
    // ;@body
    else if(MACRO_IS("body")) {
        if(gpx->flag.pausePending) {
            CALL( pause_at_zpos(gpx, gpx->commandAt[0].z) );
            gpx->flag.pausePending = 0;
        }
        gpx->flag.macrosEnabled = 1;
    }
    // ;@header
    // ;@footer
    else if(MACRO_IS("header") && MACRO_IS("footer")) {
        gpx->flag.macrosEnabled = 0;
    }
    return SUCCESS;
}

/*
 
 SIMPLE .INI FILE PARSER

 ini.c is released under the New BSD license (see LICENSE.txt). Go to the project
 home page for more info: http://code.google.com/p/inih/

 Parse given INI-style file. May have [section]s, name=value pairs
 (whitespace stripped), and comments starting with ';' (semicolon). Section
 is "" if name=value pair parsed before any section heading. name:value
 pairs are also supported as a concession to Python's ConfigParser.
 
 For each name=value pair parsed, call handler function with given user
 pointer as well as section, name, and value (data only valid for duration
 of handler call). Handler should return 0 on success, nonzero on error.
 
 Returns 0 on success, line number of first error on parse error (doesn't
 stop on first error), -1 on file open error.

*/

#define INI_SECTION_MAX 64
#define INI_NAME_MAX 64

/* Nonzero to allow multi-line value parsing, in the style of Python's
 ConfigParser. If allowed, ini_parse() will call the handler with the same
 name for each subsequent line parsed. */

#ifndef INI_ALLOW_MULTILINE
#define INI_ALLOW_MULTILINE 1
#endif

/* Nonzero to allow a UTF-8 BOM sequence (0xEF 0xBB 0xBF) at the start of
 the file. See http://code.google.com/p/inih/issues/detail?id=21 */

#ifndef INI_ALLOW_BOM
#define INI_ALLOW_BOM 1
#endif

/* Strip whitespace chars off end of given string, in place. Return s. */
static char* rstrip(char* s)
{
    char* p = s + strlen(s);
    while (p > s && isspace((unsigned char)(*--p))) *p = '\0';
    return s;
}

/* Return pointer to first non-whitespace char in given string. */
static char* lskip(const char* s)
{
    while (*s && isspace((unsigned char)(*s))) s++;
    return (char*)s;
}

/* Return pointer to first char c or ';' comment in given string, or pointer to
 null at end of string if neither found. ';' must be prefixed by a whitespace
 character to register as a comment. */
static char* find_char_or_comment(const char* s, char c)
{
    int was_whitespace = 0;
    while (*s && *s != c && !(was_whitespace && *s == ';')) {
        was_whitespace = isspace((unsigned char)(*s));
        s++;
    }
    return (char*)s;
}

/* Version of strncpy that ensures dest (size bytes) is null-terminated. */
static char* strncpy0(char* dest, const char* src, size_t size)
{
    strncpy(dest, src, size);
    dest[size - 1] = '\0';
    return dest;
}

/* See documentation in header file. */
static int ini_parse_file(Gpx* gpx, FILE* file, int (*handler)(Gpx*, const char*, const char*, char*))
{
    /* Uses a fair bit of stack (use heap instead if you need to) */
    char section[INI_SECTION_MAX] = "";
    char prev_name[INI_NAME_MAX] = "";
    
    char* start;
    char* end;
    char* name;
    char* value;
    int error = 0;
    gpx->lineNumber = 0;

    /* Scan through file line by line */
    while(fgets(gpx->buffer.in, BUFFER_MAX, file) != NULL) {
        gpx->lineNumber++;
        
        start = gpx->buffer.in;
#if INI_ALLOW_BOM
        if(gpx->lineNumber == 1 && (unsigned char)start[0] == 0xEF &&
            (unsigned char)start[1] == 0xBB &&
            (unsigned char)start[2] == 0xBF) {
            start += 3;
        }
#endif
        start = lskip(rstrip(start));
        
        if(*start == ';' || *start == '#') {
            /* Per Python ConfigParser, allow '#' comments at start of line */
        }
#if INI_ALLOW_MULTILINE
        else if(*prev_name && *start && start > gpx->buffer.in) {
            /* Non-black line with leading whitespace, treat as continuation
             of previous name's value (as per Python ConfigParser). */
            if (handler(gpx, section, prev_name, start) && !error)
                error = gpx->lineNumber;
        }
#endif
        else if(*start == '[') {
            /* A "[section]" line */
            end = find_char_or_comment(start + 1, ']');
            if(*end == ']') {
                *end = '\0';
                strncpy0(section, start + 1, sizeof(section));
                *prev_name = '\0';
            }
            else if(!error) {
                /* No ']' found on section line */
                error = gpx->lineNumber;
            }
        }
        else if(*start && *start != ';') {
            /* Not a comment, must be a name[=:]value pair */
            end = find_char_or_comment(start, '=');
            if(*end != '=') {
                end = find_char_or_comment(start, ':');
            }
            if(*end == '=' || *end == ':') {
                *end = '\0';
                name = rstrip(start);
                value = lskip(end + 1);
                end = find_char_or_comment(value, '\0');
                if (*end == ';')
                    *end = '\0';
                rstrip(value);
                
                /* Valid name[=:]value pair found, call handler */
                strncpy0(prev_name, name, sizeof(prev_name));
                if(handler(gpx, section, name, value) && !error)
                    error = gpx->lineNumber;
            }
            else if(!error) {
                /* No '=' or ':' found on name[=:]value line */
                error = gpx->lineNumber;
            }
        }
    }    
    return error;
}

/* See documentation in header file. */

static int ini_parse(Gpx* gpx, const char* filename,
                     int (*handler)(Gpx*, const char*, const char*, char*))
{
    FILE* file;
    int error;
    unsigned ln = gpx->lineNumber;
    file = fopen(filename, "r");
    if(!file) return ERROR;
    error = ini_parse_file(gpx, file, handler);
    gpx->lineNumber = ln;
    fclose(file);
    return error;
}

// Custom machine definition ini handler

#define SECTION_IS(s) strcasecmp(section, s) == 0
#define PROPERTY_IS(n) strcasecmp(property, n) == 0
#define VALUE_IS(v) strcasecmp(value, v) == 0

int gpx_set_property(Gpx *gpx, const char* section, const char* property, char* value)
{
    int rval;
    if(SECTION_IS("") || SECTION_IS("macro")) {
        if(PROPERTY_IS("slicer")
           || PROPERTY_IS("filament")
           || PROPERTY_IS("pause")
           || PROPERTY_IS("start")
           || PROPERTY_IS("temp")
           || PROPERTY_IS("temperature")) {
            CALL( parse_macro(gpx, property, value) );
        }
        else if(PROPERTY_IS("verbose")) {
            gpx->flag.verboseMode = atoi(value);
        }
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("printer") || SECTION_IS("slicer")) {
        if(PROPERTY_IS("ditto_printing")) gpx->flag.dittoPrinting = atoi(value);
        else if(PROPERTY_IS("build_progress")) gpx->flag.buildProgress = atoi(value);
        else if(PROPERTY_IS("packing_density")) gpx->machine.nominal_packing_density = strtod(value, NULL);
        else if(PROPERTY_IS("recalculate_5d")) gpx->flag.rewrite5D = atoi(value);
        else if(PROPERTY_IS("nominal_filament_diameter")
                || PROPERTY_IS("slicer_filament_diameter")
                || PROPERTY_IS("filament_diameter")) {
            gpx->machine.nominal_filament_diameter = strtod(value, NULL);
        }
        else if(PROPERTY_IS("machine_type")) {
            // only load/clobber the on-board machine definition if the one specified is different
            if(gpx_set_machine(gpx, value)) {
                SHOW( fprintf(gpx->log, "(line %u) Configuration error: unrecognised machine type '%s'" EOL, gpx->lineNumber, value) );
                return gpx->lineNumber;
            }
            gpx->override[A].packing_density = gpx->machine.nominal_packing_density;
            gpx->override[B].packing_density = gpx->machine.nominal_packing_density;
        }
        else if(PROPERTY_IS("gcode_flavor")) {
            // use on-board machine definition
            if(VALUE_IS("reprap")) gpx->flag.reprapFlavor = 1;
            else if(VALUE_IS("makerbot")) gpx->flag.reprapFlavor = 0;
            else {
                SHOW( fprintf(gpx->log, "(line %u) Configuration error: unrecognised GCODE flavor '%s'" EOL, gpx->lineNumber, value) );
                return gpx->lineNumber;
            }
        }
        else if(PROPERTY_IS("build_platform_temperature")) {
            if(gpx->machine.a.has_heated_build_platform) gpx->override[A].build_platform_temperature = atoi(value);
            else if(gpx->machine.b.has_heated_build_platform) gpx->override[B].build_platform_temperature = atoi(value);
        }
        else if(PROPERTY_IS("sd_card_path")) {
            gpx->sdCardPath = strdup(value);
        }
        else if(PROPERTY_IS("verbose")) {
            gpx->flag.verboseMode = atoi(value);
        }
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("x")) {
        if(PROPERTY_IS("max_feedrate")) gpx->machine.x.max_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("home_feedrate")) gpx->machine.x.home_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("steps_per_mm")) gpx->machine.x.steps_per_mm = strtod(value, NULL);
        else if(PROPERTY_IS("endstop")) gpx->machine.x.endstop = atoi(value);
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("y")) {
        if(PROPERTY_IS("max_feedrate")) gpx->machine.y.max_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("home_feedrate")) gpx->machine.y.home_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("steps_per_mm")) gpx->machine.y.steps_per_mm = strtod(value, NULL);
        else if(PROPERTY_IS("endstop")) gpx->machine.y.endstop = atoi(value);
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("z")) {
        if(PROPERTY_IS("max_feedrate")) gpx->machine.z.max_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("home_feedrate")) gpx->machine.z.home_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("steps_per_mm")) gpx->machine.z.steps_per_mm = strtod(value, NULL);
        else if(PROPERTY_IS("endstop")) gpx->machine.z.endstop = atoi(value);
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("a")) {
        if(PROPERTY_IS("max_feedrate")) gpx->machine.a.max_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("steps_per_mm")) gpx->machine.a.steps_per_mm = strtod(value, NULL);
        else if(PROPERTY_IS("motor_steps")) gpx->machine.a.motor_steps = strtod(value, NULL);
        else if(PROPERTY_IS("has_heated_build_platform")) gpx->machine.a.has_heated_build_platform = atoi(value);
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("right")) {
        if(PROPERTY_IS("active_temperature")
           || PROPERTY_IS("nozzle_temperature")) gpx->override[A].active_temperature = atoi(value);
        else if(PROPERTY_IS("standby_temperature")) gpx->override[A].standby_temperature = atoi(value);
        else if(PROPERTY_IS("build_platform_temperature")) gpx->override[A].build_platform_temperature = atoi(value);
        else if(PROPERTY_IS("actual_filament_diameter")) gpx->override[A].actual_filament_diameter = strtod(value, NULL);
        else if(PROPERTY_IS("packing_density")) gpx->override[A].packing_density = strtod(value, NULL);
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("b")) {
        if(PROPERTY_IS("max_feedrate")) gpx->machine.b.max_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("steps_per_mm")) gpx->machine.b.steps_per_mm = strtod(value, NULL);
        else if(PROPERTY_IS("motor_steps")) gpx->machine.b.motor_steps = strtod(value, NULL);
        else if(PROPERTY_IS("has_heated_build_platform")) gpx->machine.b.has_heated_build_platform = atoi(value);
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("left")) {
        if(PROPERTY_IS("active_temperature")
           || PROPERTY_IS("nozzle_temperature")) gpx->override[B].active_temperature = atoi(value);
        else if(PROPERTY_IS("standby_temperature")) gpx->override[B].standby_temperature = atoi(value);
        else if(PROPERTY_IS("build_platform_temperature")) gpx->override[B].build_platform_temperature = atoi(value);
        else if(PROPERTY_IS("actual_filament_diameter")) gpx->override[B].actual_filament_diameter = strtod(value, NULL);
        else if(PROPERTY_IS("packing_density")) gpx->override[B].packing_density = strtod(value, NULL);
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("machine")) {
        if(PROPERTY_IS("nominal_filament_diameter")
           || PROPERTY_IS("slicer_filament_diameter")) gpx->machine.nominal_filament_diameter = strtod(value, NULL);
        else if(PROPERTY_IS("packing_density")) gpx->machine.nominal_packing_density = strtod(value, NULL);
        else if(PROPERTY_IS("nozzle_diameter")) gpx->machine.nozzle_diameter = strtod(value, NULL);
        else if(PROPERTY_IS("extruder_count")) {
            gpx->machine.extruder_count = atoi(value);
            gpx->axis.mask = gpx->machine.extruder_count == 1 ? (XYZ_BIT_MASK | A_IS_SET) : AXES_BIT_MASK;;
        }
        else if(PROPERTY_IS("timeout")) gpx->machine.timeout = atoi(value);
        else goto SECTION_ERROR;
    }
    else {
        SHOW( fprintf(gpx->log, "(line %u) Configuration error: unrecognised section [%s]" EOL, gpx->lineNumber, section) );
        return gpx->lineNumber;
    }
    return SUCCESS;
    
SECTION_ERROR:
    SHOW( fprintf(gpx->log, "(line %u) Configuration error: [%s] section contains unrecognised property %s = %s" EOL, gpx->lineNumber, section, property, value) );
    return gpx->lineNumber;
}

int gpx_load_config(Gpx *gpx, const char *filename)
{
    return ini_parse(gpx, filename, gpx_set_property);
}

void gpx_register_callback(Gpx *gpx, int (*callbackHandler)(Gpx*, void*, char*, size_t), void *callbackData)
{
    gpx->callbackHandler = callbackHandler;
    gpx->callbackData = callbackData;
}

void gpx_start_convert(Gpx *gpx, char *buildName)
{
    if(buildName) gpx->buildName = buildName;

    if(gpx->flag.dittoPrinting && gpx->machine.extruder_count == 1) {
        SHOW( fputs("Configuration error: ditto printing cannot access non-existant second extruder" EOL, gpx->log) );
        gpx->flag.dittoPrinting = 0;
    }
    
    // CALCULATE FILAMENT SCALING
    
    if(gpx->override[A].actual_filament_diameter > 0.0001
       && gpx->override[A].actual_filament_diameter != gpx->machine.nominal_filament_diameter) {
        set_filament_scale(gpx, A, gpx->override[A].actual_filament_diameter);
    }
    
    if(gpx->override[B].actual_filament_diameter > 0.0001
       && gpx->override[B].actual_filament_diameter != gpx->machine.nominal_filament_diameter) {
        set_filament_scale(gpx, B, gpx->override[B].actual_filament_diameter);
    }
}

int gpx_convert_line(Gpx *gpx, char *gcode_line)
{
    int i, rval;
    int next_line = 0;
    int command_emitted = 0;

    // reset flag state
    gpx->command.flag = 0;
    char *digits;
    char *p = gcode_line; // current parser location
    while(isspace(*p)) p++;
    // check for line number
    if(*p == 'n' || *p == 'N') {
        digits = p;
        p = normalize_word(p);
        if(*p == 0) {
            SHOW( fprintf(gpx->log, "(line %u) Syntax error: line number command word 'N' is missing digits" EOL, gpx->lineNumber) );
            next_line = gpx->lineNumber + 1;
        }
        else {
            next_line = gpx->lineNumber = atoi(digits);
        }
    }
    else {
        next_line = gpx->lineNumber + 1;
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
                    gpx->command.x = strtod(digits, NULL);
                    gpx->command.flag |= X_IS_SET;
                    break;
                    
                    // Ynnn	 Y coordinate, usually to move to
                case 'y':
                case 'Y':
                    gpx->command.y = strtod(digits, NULL);
                    gpx->command.flag |= Y_IS_SET;
                    break;
                    
                    // Znnn	 Z coordinate, usually to move to
                case 'z':
                case 'Z':
                    gpx->command.z = strtod(digits, NULL);
                    gpx->command.flag |= Z_IS_SET;
                    break;
                    
                    // Annn	 Length of extrudate in mm.
                case 'a':
                case 'A':
                    gpx->command.a = strtod(digits, NULL);
                    gpx->command.flag |= A_IS_SET;
                    break;
                    
                    // Bnnn	 Length of extrudate in mm.
                case 'b':
                case 'B':
                    gpx->command.b = strtod(digits, NULL);
                    gpx->command.flag |= B_IS_SET;
                    break;
                    
                    // Ennn	 Length of extrudate in mm.
                case 'e':
                case 'E':
                    gpx->command.e = strtod(digits, NULL);
                    gpx->command.flag |= E_IS_SET;
                    break;
                    
                    // Fnnn	 Feedrate in mm per minute.
                case 'f':
                case 'F':
                    gpx->command.f = strtod(digits, NULL);
                    gpx->command.flag |= F_IS_SET;
                    break;
                    
                    // Pnnn	 Command parameter, such as a time in milliseconds
                case 'p':
                case 'P':
                    gpx->command.p = strtod(digits, NULL);
                    gpx->command.flag |= P_IS_SET;
                    break;
                    
                    // Rnnn	 Command Parameter, such as RPM
                case 'r':
                case 'R':
                    gpx->command.r = strtod(digits, NULL);
                    gpx->command.flag |= R_IS_SET;
                    break;
                    
                    // Snnn	 Command parameter, such as temperature
                case 's':
                case 'S':
                    gpx->command.s = strtod(digits, NULL);
                    gpx->command.flag |= S_IS_SET;
                    break;
                    
                    // COMMANDS
                    
                    // Gnnn GCode command, such as move to a point
                case 'g':
                case 'G':
                    gpx->command.g = atoi(digits);
                    gpx->command.flag |= G_IS_SET;
                    break;
                    // Mnnn	 RepRap-defined command
                case 'm':
                case 'M':
                    gpx->command.m = atoi(digits);
                    gpx->command.flag |= M_IS_SET;
                    break;
                    // Tnnn	 Select extruder nnn.
                case 't':
                case 'T':
                    gpx->command.t = atoi(digits);
                    gpx->command.flag |= T_IS_SET;
                    break;
                    
                default:
                    SHOW( fprintf(gpx->log, "(line %u) Syntax warning: unrecognised command word '%c'" EOL, gpx->lineNumber, c) );
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
                    CALL( parse_macro(gpx, macro, normalize_comment(s)) );
                    *p = 0;
                    break;
                }
            }
            // Comment
            gpx->command.comment = normalize_comment(p + 1);
            gpx->command.flag |= COMMENT_IS_SET;
            *p = 0;
            break;
        }
        else if(*p == '(') {
            if(*(p + 1) == '@') {
                char *s = p + 2;
                if(isalpha(*s)) {
                    char *macro = s;
                    char *e = strrchr(p + 1, ')');
                    // skip any no space characters
                    while(*s && !isspace(*s)) s++;
                    // null terminate
                    if(*s) *s++ = 0;
                    if(e) *e = 0;
                    CALL( parse_macro(gpx, macro, normalize_comment(s)) );
                    *p = 0;
                    break;
                }
            }
            // Comment
            char *s = strchr(p + 1, '(');
            char *e = strchr(p + 1, ')');
            // check for nested comment
            if(s && e && s < e) {
                SHOW( fprintf(gpx->log, "(line %u) Syntax warning: nested comment detected" EOL, gpx->lineNumber) );
                e = strrchr(p + 1, ')');
            }
            if(e) {
                *e = 0;
                gpx->command.comment = normalize_comment(p + 1);
                gpx->command.flag |= COMMENT_IS_SET;
                p = e + 1;
            }
            else {
                SHOW( fprintf(gpx->log, "(line %u) Syntax warning: comment is missing closing ')'" EOL, gpx->lineNumber) );
                gpx->command.comment = normalize_comment(p + 1);
                gpx->command.flag |= COMMENT_IS_SET;
                *p = 0;
                break;
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
            SHOW( fprintf(gpx->log, "(line %u) Syntax error: unrecognised gcode '%s'" EOL, gpx->lineNumber, p) );
            break;
        }
    }
    
    // revert tool selection to current extruder (Makerbot Tn is not sticky)
    if(!gpx->flag.reprapFlavor) gpx->target.extruder = gpx->current.extruder;
    
    // change the extruder selection (in the virtual tool carosel)
    if(gpx->command.flag & T_IS_SET && !gpx->flag.dittoPrinting) {
        unsigned tool_id = (unsigned)gpx->command.t;
        if(tool_id < gpx->machine.extruder_count) {
            gpx->target.extruder = tool_id;
        }
        else {
            SHOW( fprintf(gpx->log, "(line %u) Semantic warning: T%u cannot select non-existant extruder" EOL, gpx->lineNumber, tool_id) );
        }
    }
    
    // we treat E as short hand for A or B being set, depending on the state of the gpx->current.extruder
    
    if(gpx->command.flag & E_IS_SET) {
        if(gpx->current.extruder == 0) {
            // a = e
            gpx->command.flag |= A_IS_SET;
            gpx->command.a = gpx->command.e;
        }
        else {
            // b = e
            gpx->command.flag |= B_IS_SET;
            gpx->command.b = gpx->command.e;
        }
    }
    
    // INTERPRET COMMAND
    
    if(gpx->command.flag & G_IS_SET) {
        switch(gpx->command.g) {
                // G0 - Rapid Positioning
            case 0:
                if(gpx->command.flag & F_IS_SET) {
                    CALL( calculate_target_position(gpx) );
                    CALL( queue_ext_point(gpx, 0.0) );
                    update_current_position(gpx);
                    command_emitted++;
                }
                else {
                    Point3d delta;
                    CALL( calculate_target_position(gpx) );
                    if(gpx->command.flag & X_IS_SET) delta.x = fabs(gpx->target.position.x - gpx->current.position.x);
                    if(gpx->command.flag & Y_IS_SET) delta.y = fabs(gpx->target.position.y - gpx->current.position.y);
                    if(gpx->command.flag & Z_IS_SET) delta.z = fabs(gpx->target.position.z - gpx->current.position.z);
                    double length = magnitude(gpx->command.flag & XYZ_BIT_MASK, (Ptr5d)&delta);
                    double candidate, feedrate = DBL_MAX;
                    if(gpx->command.flag & X_IS_SET && delta.x != 0.0) {
                        feedrate = gpx->machine.x.max_feedrate * length / delta.x;
                    }
                    if(gpx->command.flag & Y_IS_SET && delta.y != 0.0) {
                        candidate = gpx->machine.y.max_feedrate * length / delta.y;
                        if(feedrate > candidate) {
                            feedrate = candidate;
                        }
                    }
                    if(gpx->command.flag & Z_IS_SET && delta.z != 0.0) {
                        candidate = gpx->machine.z.max_feedrate * length / delta.z;
                        if(feedrate > candidate) {
                            feedrate = candidate;
                        }
                    }
                    if(feedrate == DBL_MAX) {
                        feedrate = gpx->machine.x.max_feedrate;
                    }
                    CALL( queue_ext_point(gpx, feedrate) );
                    update_current_position(gpx);
                    command_emitted++;
                }
                break;
                
                // G1 - Coordinated Motion
            case 1:
                CALL( calculate_target_position(gpx) );
                CALL( queue_ext_point(gpx, 0.0) );
                update_current_position(gpx);
                command_emitted++;
                break;
                
                // G2 - Clockwise Arc
                // G3 - Counter Clockwise Arc
                
                // G4 - Dwell
            case 4:
                if(gpx->command.flag & P_IS_SET) {
#if ENABLE_SIMULATED_RPM
                    if(gpx->tool[gpx->current.extruder].motor_enabled && gpx->tool[gpx->current.extruder].rpm) {
                        CALL( calculate_target_position(gpx) );
                        CALL( queue_new_point(gpx, gpx->command.p) );
                        command_emitted++;
                    }
                    else
#endif
                    {
                        CALL( delay(gpx, gpx->command.p) );
                        command_emitted++;
                    }
                    
                }
                else {
                    SHOW( fprintf(gpx->log, "(line %u) Syntax error: G4 is missing delay parameter, use Pn where n is milliseconds" EOL, gpx->lineNumber) );
                }
                break;
                
                // G10 - Create Coordinate System Offset from the Absolute one
            case 10:
                if(gpx->command.flag & P_IS_SET && gpx->command.p >= 1.0 && gpx->command.p <= 6.0) {
                    i = (int)gpx->command.p;
                    if(gpx->command.flag & X_IS_SET) gpx->offset[i].x = gpx->command.x;
                    if(gpx->command.flag & Y_IS_SET) gpx->offset[i].y = gpx->command.y;
                    if(gpx->command.flag & Z_IS_SET) gpx->offset[i].z = gpx->command.z;
                    // set standby temperature
                    if(gpx->command.flag & R_IS_SET) {
                        unsigned temperature = (unsigned)gpx->command.r;
                        if(temperature > NOZZLE_MAX) temperature = NOZZLE_MAX;
                        switch(i) {
                            case 1:
                                gpx->override[A].standby_temperature = temperature;
                                break;
                            case 2:
                                gpx->override[B].standby_temperature = temperature;
                                break;
                        }
                    }
                    // set tool temperature
                    if(gpx->command.flag & S_IS_SET) {
                        unsigned temperature = (unsigned)gpx->command.s;
                        if(temperature > NOZZLE_MAX) temperature = NOZZLE_MAX;
                        switch(i) {
                            case 1:
                                gpx->override[A].active_temperature = temperature;
                                break;
                            case 2:
                                gpx->override[B].active_temperature = temperature;
                                break;
                        }
                    }
                }
                else {
                    SHOW( fprintf(gpx->log, "(line %u) Syntax error: G10 is missing coordiante system, use Pn where n is 1-6" EOL, gpx->lineNumber) );
                }
                break;
                
                // G21 - Use Milimeters as Units (IGNORED)
                // G71 - Use Milimeters as Units (IGNORED)
            case 21:
            case 71:
                break;
                
                // G28 - Home given axes to machine defined endstop
            case 28: {
                unsigned endstop_max = 0;
                unsigned endstop_min = 0;
                if(gpx->command.flag & F_IS_SET) gpx->current.feedrate = gpx->command.f;
                
                if(gpx->command.flag & X_IS_SET) {
                    if(gpx->machine.x.endstop) {
                        endstop_max |= X_IS_SET;
                    }
                    else {
                        endstop_min |= X_IS_SET;
                    }
                }
                
                if(gpx->command.flag & Y_IS_SET) {
                    if(gpx->machine.y.endstop) {
                        endstop_max |= Y_IS_SET;
                    }
                    else {
                        endstop_min |= Y_IS_SET;
                    }
                }
                
                if(gpx->command.flag & Z_IS_SET) {
                    if(gpx->machine.z.endstop) {
                        endstop_max |= Z_IS_SET;
                    }
                    else {
                        endstop_min |= Z_IS_SET;
                    }
                }
                // home xy before z
                if(gpx->machine.x.endstop) {
                    if(endstop_max) {
                        CALL( home_axes(gpx, endstop_max, ENDSTOP_IS_MAX) );
                    }
                    if(endstop_min) {
                        CALL( home_axes(gpx, endstop_min, ENDSTOP_IS_MIN) );
                    }
                }
                else {
                    if(endstop_min) {
                        CALL( home_axes(gpx, endstop_min, ENDSTOP_IS_MAX) );
                    }
                    if(endstop_max) {
                        CALL( home_axes(gpx, endstop_max, ENDSTOP_IS_MIN) );
                    }
                }
                command_emitted++;
                gpx->axis.positionKnown &= ~(gpx->command.flag & gpx->axis.mask);
                gpx->excess.a = 0;
                gpx->excess.b = 0;
                break;
            }

                // G53 - Set absolute coordinate system
            case 53:
                gpx->current.offset = 0;
                break;
                
                // G54 - Use coordinate system from G10 P1
            case 54:
                gpx->current.offset = 1;
                break;
                
                // G55 - Use coordinate system from G10 P2
            case 55:
                gpx->current.offset = 2;
                break;
                
                // G56 - Use coordinate system from G10 P3
            case 56:
                gpx->current.offset = 3;
                break;
                
                // G57 - Use coordinate system from G10 P4
            case 57:
                gpx->current.offset = 4;
                break;
                
                // G58 - Use coordinate system from G10 P5
            case 58:
                gpx->current.offset = 5;
                break;
                
                // G59 - Use coordinate system from G10 P6
            case 59:
                gpx->current.offset = 6;
                break;
                
                // G90 - Absolute Positioning
            case 90:
                gpx->flag.relativeCoordinates = 0;
                break;
                
                // G91 - Relative Positioning
            case 91:
                if((gpx->axis.positionKnown & XYZ_BIT_MASK) == XYZ_BIT_MASK) {
                    gpx->flag.relativeCoordinates = 1;
                }
                else {
                    SHOW( fprintf(gpx->log, "(line %u) Semantic error: G91 switch to relitive positioning prior to first absolute move" EOL, gpx->lineNumber) );
                    return ERROR;
                }
                break;
                
                // G92 - Define current position on axes
            case 92: {
                double userScale = gpx->flag.macrosEnabled ? gpx->user.scale : 1.0;
                if(gpx->command.flag & X_IS_SET) gpx->current.position.x = gpx->command.x * userScale;
                if(gpx->command.flag & Y_IS_SET) gpx->current.position.y = gpx->command.y * userScale;
                if(gpx->command.flag & Z_IS_SET) gpx->current.position.z = gpx->command.z * userScale;
                if(gpx->command.flag & A_IS_SET) gpx->current.position.a = gpx->command.a;
                if(gpx->command.flag & B_IS_SET) gpx->current.position.b = gpx->command.b;
                CALL( set_position(gpx) );
                command_emitted++;
                // flag axes that are known
                gpx->axis.positionKnown |= (gpx->command.flag & gpx->axis.mask);
                break;
            }
                
                // G130 - Set given axes potentiometer Value
            case 130:
                if(gpx->command.flag & X_IS_SET) {
                    CALL( set_pot_value(gpx, 0, gpx->command.x < 0 ? 0 : gpx->command.x > 127 ? 127 : (unsigned)gpx->command.x) );
                }
                
                if(gpx->command.flag & Y_IS_SET) {
                    CALL( set_pot_value(gpx, 1, gpx->command.y < 0 ? 0 : gpx->command.y > 127 ? 127 : (unsigned)gpx->command.y) );
                }
                
                if(gpx->command.flag & Z_IS_SET) {
                    CALL( set_pot_value(gpx, 2, gpx->command.z < 0 ? 0 : gpx->command.z > 127 ? 127 : (unsigned)gpx->command.z) );
                }
                
                if(gpx->command.flag & A_IS_SET) {
                    CALL( set_pot_value(gpx, 3, gpx->command.a < 0 ? 0 : gpx->command.a > 127 ? 127 : (unsigned)gpx->command.a) );
                }
                
                if(gpx->command.flag & B_IS_SET) {
                    CALL( set_pot_value(gpx, 4, gpx->command.b < 0 ? 0 : gpx->command.b > 127 ? 127 : (unsigned)gpx->command.b) );
                }
                break;
                
                // G161 - Home given axes to minimum
            case 161:
                if(gpx->command.flag & F_IS_SET) gpx->current.feedrate = gpx->command.f;
                CALL( home_axes(gpx, gpx->command.flag & XYZ_BIT_MASK, ENDSTOP_IS_MIN) );
                command_emitted++;
                // clear homed axes
                gpx->axis.positionKnown &= ~(gpx->command.flag & gpx->axis.mask);
                gpx->excess.a = 0;
                gpx->excess.b = 0;
                break;
                // G162 - Home given axes to maximum
            case 162:
                if(gpx->command.flag & F_IS_SET) gpx->current.feedrate = gpx->command.f;
                CALL( home_axes(gpx, gpx->command.flag & XYZ_BIT_MASK, ENDSTOP_IS_MAX) );
                command_emitted++;
                // clear homed axes
                gpx->axis.positionKnown &= ~(gpx->command.flag & gpx->axis.mask);
                gpx->excess.a = 0;
                gpx->excess.b = 0;
                break;
            default:
                SHOW( fprintf(gpx->log, "(line %u) Syntax warning: unsupported gcode command 'G%u'" EOL, gpx->lineNumber, gpx->command.g) );
        }
    }
    else if(gpx->command.flag & M_IS_SET) {
        switch(gpx->command.m) {
                // M0 - Program stop
            case 0:
                break;
                // M1 - Program pause
            case 1:
                break;
                // M2 - Program end
            case 2:
                if(program_is_running()) {
                    end_program();
                    CALL( set_build_progress(gpx, 100) );
                    CALL( end_build(gpx) );
                }
                return END_OF_FILE;
                
                // M6 - Automatic tool change (AND)
                // M116 - Wait for extruder AND build platfrom to reach (or exceed) temperature
            case 6:
            case 116: {
                int timeout = gpx->command.flag & P_IS_SET ? (int)gpx->command.p : MAX_TIMEOUT;
                if(!gpx->flag.dittoPrinting &&
#if !ENABLE_TOOL_CHANGE_ON_WAIT
                   gpx->command.m == 6 &&
#endif
                   gpx->target.extruder != gpx->current.extruder) {
                    CALL( do_tool_change(gpx, timeout) );
                    command_emitted++;
                }
                // wait for heated build platform
                if(gpx->machine.a.has_heated_build_platform && gpx->tool[A].build_platform_temperature > 0) {
                    CALL( wait_for_build_platform(gpx, A, timeout) );
                    command_emitted++;
                }
                else if(gpx->machine.b.has_heated_build_platform && gpx->tool[B].build_platform_temperature > 0) {
                    CALL( wait_for_build_platform(gpx, B, timeout) );
                    command_emitted++;
                }
                // wait for extruder
                if(gpx->flag.dittoPrinting) {
                    if(gpx->tool[B].nozzle_temperature > 0) {
                        CALL( wait_for_extruder(gpx, B, timeout) );
                    }
                    if(gpx->tool[A].nozzle_temperature > 0) {
                        CALL( wait_for_extruder(gpx, A, timeout) );
                        if(gpx->flag.verboseMode) {
                            CALL( display_tag(gpx) );
                        }
                    }
                    command_emitted++;
                }
                else {
                    if(gpx->tool[gpx->target.extruder].nozzle_temperature > 0) {
                        CALL( wait_for_extruder(gpx, gpx->target.extruder, timeout) );
                        command_emitted++;
                        if(gpx->flag.verboseMode) {
                            CALL( display_tag(gpx) );
                        }
                    }
                }
                break;
            }
                
                // M17 - Enable axes steppers
            case 17:
                if(gpx->command.flag & AXES_BIT_MASK) {
                    CALL( set_steppers(gpx, gpx->command.flag & AXES_BIT_MASK, 1) );
                    command_emitted++;
                    if(gpx->command.flag & A_IS_SET) gpx->tool[A].motor_enabled = 1;
                    if(gpx->command.flag & B_IS_SET) gpx->tool[B].motor_enabled = 1;
                }
                else {
                    CALL( set_steppers(gpx, gpx->machine.extruder_count == 1 ? (XYZ_BIT_MASK | A_IS_SET) : AXES_BIT_MASK, 1) );
                    command_emitted++;
                    gpx->tool[A].motor_enabled = 1;
                    if(gpx->machine.extruder_count == 2) gpx->tool[B].motor_enabled = 1;
                }
                break;
                
                // M18 - Disable axes steppers
            case 18:
                if(gpx->command.flag & AXES_BIT_MASK) {
                    CALL( set_steppers(gpx, gpx->command.flag & AXES_BIT_MASK, 0) );
                    command_emitted++;
                    if(gpx->command.flag & A_IS_SET) gpx->tool[A].motor_enabled = 0;
                    if(gpx->command.flag & B_IS_SET) gpx->tool[B].motor_enabled = 0;
                }
                else {
                    CALL( set_steppers(gpx, gpx->machine.extruder_count == 1 ? (XYZ_BIT_MASK | A_IS_SET) : AXES_BIT_MASK, 0) );
                    command_emitted++;
                    gpx->tool[A].motor_enabled = 0;
                    if(gpx->machine.extruder_count == 2) gpx->tool[B].motor_enabled = 0;
                }
                break;
                
                // M20 - List SD card
            case 20:
                break;
                
                // M21 - Init SD card
            case 21:
                break;
                
                // M22 - Release SD card
            case 22:
                break;
                
                // M23 - Select SD file
            case 23:
                break;
                
                // M24 - Start/resume SD print
            case 24:
                break;
                
                // M25 - Pause SD print
            case 25:
                break;
                
                // M26 - Set SD position
            case 26:
                break;
                
                // M27 - Report SD print status
            case 27:
                break;
                
                // M28 - Begin write to SD card
            case 28:
                break;
                
                // M29 - Stop writing to SD card
            case 29:
                break;
                
                // M30 - Delete file from SD card
            case 30:
                break;
                
                // M31 - Output time since last M109 or SD card start to serial
            case 31:
                break;
                
                // M70 - Display message on LCD
            case 70:
                if(gpx->command.flag & COMMENT_IS_SET) {
                    unsigned vPos = gpx->command.flag & Y_IS_SET ? (unsigned)gpx->command.y : 0;
                    if(vPos > 3) vPos = 3;
                    unsigned hPos = gpx->command.flag & X_IS_SET ? (unsigned)gpx->command.x : 0;
                    if(hPos > 19) hPos = 19;
                    int timeout = gpx->command.flag & P_IS_SET ? gpx->command.p : 0;
                    CALL( display_message(gpx, gpx->command.comment, vPos, hPos, timeout, 0) );
                    command_emitted++;
                }
                else {
                    SHOW( fprintf(gpx->log, "(line %u) Syntax error: M70 is missing message text, use (text) where text is message" EOL, gpx->lineNumber) );
                }
                break;
                
                // M71 - Display message and wait for button press
            case 71: {
                char *message = gpx->command.flag & COMMENT_IS_SET ? gpx->command.comment : "Press M to continue";
                unsigned vPos = gpx->command.flag & Y_IS_SET ? (unsigned)gpx->command.y : 0;
                if(vPos > 3) vPos = 3;
                unsigned hPos = gpx->command.flag & X_IS_SET ? (unsigned)gpx->command.x : 0;
                if(hPos > 19) hPos = 19;
                int timeout = gpx->command.flag & P_IS_SET ? gpx->command.p : 0;
                CALL( display_message(gpx, message, vPos, hPos, timeout, 1) );
                command_emitted++;
                break;
            }
                
                // M72 - Queue a song or play a tone
            case 72:
                if(gpx->command.flag & P_IS_SET) {
                    unsigned song_id = (unsigned)gpx->command.p;
                    if(song_id > 2) song_id = 2;
                    CALL( queue_song(gpx, song_id) );
                    command_emitted++;
                }
                else {
                    SHOW( fprintf(gpx->log, "(line %u) Syntax warning: M72 is missing song number, use Pn where n is 0-2" EOL, gpx->lineNumber) );
                }
                break;
                
                // M73 - Manual set build percentage
            case 73:
                if(gpx->command.flag & P_IS_SET) {
                    unsigned percent = (unsigned)gpx->command.p;
                    if(percent > 100) percent = 100;
                    if(program_is_ready()) {
                        start_program();
                        CALL( start_build(gpx, gpx->buildName) );
                        CALL( set_build_progress(gpx, 0) );
                        // start extruder in a known state
                        CALL( change_extruder_offset(gpx, gpx->current.extruder) );
                    }
                    else if(program_is_running()) {
                        if(percent == 100) {
                            // disable macros in footer
                            gpx->flag.macrosEnabled = 0;
                            end_program();
                            CALL( set_build_progress(gpx, 100) );
                            CALL( end_build(gpx) );
                            gpx->current.percent = 100;
                        }
                        else {
                            // enable macros in object body
                            if(!gpx->flag.macrosEnabled && percent > 0) {
                                if(gpx->flag.pausePending) {
                                    CALL( pause_at_zpos(gpx, gpx->commandAt[0].z) );
                                    gpx->flag.pausePending = 0;
                                }
                                gpx->flag.macrosEnabled = 1;
                            }
                            if(gpx->current.percent < percent && (percent == 1 || gpx->total.time == 0.0 || gpx->flag.buildProgress == 0)) {
                                CALL( set_build_progress(gpx, percent) );
                                gpx->current.percent = percent;
                            }
                        }
                    }
                }
                else {
                    SHOW( fprintf(gpx->log, "(line %u) Syntax warning: M73 is missing build percentage, use Pn where n is 0-100" EOL, gpx->lineNumber) );
                }
                break;
                
                // M82 - set extruder to absolute mode
            case 82:
                gpx->flag.extruderIsRelative = 0;
                break;
                
                // M83 - set extruder to relative mode
            case 83:
                gpx->flag.extruderIsRelative = 1;
                break;
                
                // M84 - Disable steppers until next move
            case 84:
                CALL( set_steppers(gpx, gpx->machine.extruder_count == 1 ? (XYZ_BIT_MASK | A_IS_SET) : AXES_BIT_MASK, 0) );
                command_emitted++;
                gpx->tool[A].motor_enabled = 0;
                if(gpx->machine.extruder_count == 2) gpx->tool[B].motor_enabled = 0;
                break;
                
                // M101 - Turn extruder on, forward
                // M102 - Turn extruder on, reverse
            case 101:
            case 102:
                if(gpx->flag.dittoPrinting) {
                    CALL( set_steppers(gpx, A_IS_SET|B_IS_SET, 1) );
                    command_emitted++;
                    gpx->tool[A].motor_enabled = gpx->tool[B].motor_enabled = gpx->command.m == 101 ? 1 : -1;
                }
                else {
                    CALL( set_steppers(gpx, gpx->target.extruder == 0 ? A_IS_SET : B_IS_SET, 1) );
                    command_emitted++;
                    gpx->tool[gpx->target.extruder].motor_enabled = gpx->command.m == 101 ? 1 : -1;
                }
                break;
                
                // M103 - Turn extruder off
            case 103:
                if(gpx->flag.dittoPrinting) {
                    CALL( set_steppers(gpx, A_IS_SET|B_IS_SET, 1) );
                    command_emitted++;
                    gpx->tool[A].motor_enabled = gpx->tool[B].motor_enabled = 0;
                }
                else {
                    CALL( set_steppers(gpx, gpx->target.extruder == 0 ? A_IS_SET : B_IS_SET, 0) );
                    command_emitted++;
                    gpx->tool[gpx->target.extruder].motor_enabled = 0;
                }
                break;
                
                // M104 - Set extruder temperature
            case 104:
                if(gpx->command.flag & S_IS_SET) {
                    unsigned temperature = (unsigned)gpx->command.s;
                    if(temperature > NOZZLE_MAX) temperature = NOZZLE_MAX;
                    if(gpx->flag.dittoPrinting) {
                        if(temperature && gpx->override[gpx->current.extruder].active_temperature) {
                            temperature = gpx->override[gpx->current.extruder].active_temperature;
                        }
                        CALL( set_nozzle_temperature(gpx, B, temperature) );
                        CALL( set_nozzle_temperature(gpx, A, temperature) );
                        command_emitted++;
                        gpx->tool[A].nozzle_temperature = gpx->tool[B].nozzle_temperature = temperature;
                    }
                    else {
                        if(temperature && gpx->override[gpx->target.extruder].active_temperature) {
                            temperature = gpx->override[gpx->target.extruder].active_temperature;
                        }
                        CALL( set_nozzle_temperature(gpx, gpx->target.extruder, temperature) );
                        command_emitted++;
                        gpx->tool[gpx->target.extruder].nozzle_temperature = temperature;
                    }
                }
                else {
                    SHOW( fprintf(gpx->log, "(line %u) Syntax error: M104 is missing temperature, use Sn where n is 0-280" EOL, gpx->lineNumber) );
                }
                break;
                
                // M105 - Get extruder temperature
            case 105:
                break;
                
                // M106 - Turn cooling fan on
            case 106: {
                int state = (gpx->command.flag & S_IS_SET) ? ((unsigned)gpx->command.s ? 1 : 0) : 1;
                if(gpx->flag.reprapFlavor && gpx->machine.type >= MACHINE_TYPE_REPLICATOR_1) {
                    if(gpx->flag.dittoPrinting) {
                        CALL( set_valve(gpx, B, state) );
                        CALL( set_valve(gpx, A, state) );
                        command_emitted++;
                    }
                    else {
                        CALL( set_valve(gpx, gpx->target.extruder, state) );
                        command_emitted++;
                    }
                }
                else {
                    if(gpx->flag.dittoPrinting) {
                        CALL( set_fan(gpx, B, state) );
                        CALL( set_fan(gpx, A, state) );
                        command_emitted++;
                    }
                    else {
                        CALL( set_fan(gpx, gpx->target.extruder, state) );
                        command_emitted++;
                    }
                }
                break;
            }
                
                // M107 - Turn cooling fan off
            case 107:
                if(gpx->flag.reprapFlavor && gpx->machine.type >= MACHINE_TYPE_REPLICATOR_1) {
                    if(gpx->flag.dittoPrinting) {
                        CALL( set_valve(gpx, B, 0) );
                        CALL( set_valve(gpx, A, 0) );
                        command_emitted++;
                    }
                    else {
                        CALL( set_valve(gpx, gpx->target.extruder, 0) );
                        command_emitted++;
                    }
                }
                else {
                    if(gpx->flag.dittoPrinting) {
                        CALL( set_fan(gpx, B, 0) );
                        CALL( set_fan(gpx, A, 0) );
                        command_emitted++;
                    }
                    else {
                        CALL( set_fan(gpx, gpx->target.extruder, 0) );
                        command_emitted++;
                    }
                }
                break;
                
                // M108 - Set extruder motor 5D 'simulated' RPM
            case 108:
#if ENABLE_SIMULATED_RPM
                if(gpx->command.flag & R_IS_SET) {
                    if(gpx->flag.dittoPrinting) {
                        gpx->tool[A].rpm = gpx->tool[B].rpm = gpx->command.r;
                    }
                    else {
                        gpx->tool[gpx->target.extruder].rpm = gpx->command.r;
                    }
                }
                else {
                    SHOW( fprintf(gpx->log, "(line %u) Syntax error: M108 is missing motor RPM, use Rn where n is 0-5" EOL, gpx->lineNumber) );
                }
#endif
                break;
                
                
                // M109 - Set extruder temperature and wait
            case 109:
                if(gpx->flag.reprapFlavor) {
                    if(gpx->command.flag & S_IS_SET) {
                        int timeout = gpx->command.flag & P_IS_SET ? (int)gpx->command.p : MAX_TIMEOUT;
                        unsigned temperature = (unsigned)gpx->command.s;
                        if(temperature > NOZZLE_MAX) temperature = NOZZLE_MAX;
                        if(gpx->flag.dittoPrinting) {
                            unsigned tempB = temperature;
                            // set extruder temperatures
                            if(temperature) {
                                if(gpx->override[B].active_temperature) {
                                    tempB = gpx->override[B].active_temperature;
                                }
                                if(gpx->override[A].active_temperature) {
                                    temperature = gpx->override[A].active_temperature;
                                }
                            }
                            CALL( set_nozzle_temperature(gpx, B, tempB) );
                            CALL( set_nozzle_temperature(gpx, A, temperature) );
                            gpx->tool[B].nozzle_temperature = tempB;
                            gpx->tool[A].nozzle_temperature = temperature;
                            // wait for extruders to reach (or exceed) temperature
                            if(gpx->tool[B].nozzle_temperature > 0) {
                                CALL( wait_for_extruder(gpx, B, timeout) );
                            }
                            if(gpx->tool[A].nozzle_temperature > 0) {
                                CALL( wait_for_extruder(gpx, A, timeout) );
                                if(gpx->flag.verboseMode) {
                                    CALL( display_tag(gpx) );
                                }
                            }
                            command_emitted++;
                        }
                        else {
#if ENABLE_TOOL_CHANGE_ON_WAIT
                            // because there is a wait we do a tool change
                            if(gpx->target.extruder != gpx->current.extruder) {
                                CALL( do_tool_change(gpx, timeout) );
                            }
#endif
                            // set extruder temperature
                            if(temperature && gpx->override[gpx->target.extruder].active_temperature) {
                                temperature = gpx->override[gpx->target.extruder].active_temperature;
                            }
                            CALL( set_nozzle_temperature(gpx, gpx->target.extruder, temperature) );
                            gpx->tool[gpx->target.extruder].nozzle_temperature = temperature;
                            // wait for extruder to reach (or exceed) temperature
                            if(gpx->tool[gpx->target.extruder].nozzle_temperature > 0) {
                                CALL( wait_for_extruder(gpx, gpx->target.extruder, timeout) );
                                if(gpx->flag.verboseMode) {
                                    CALL( display_tag(gpx) );
                                }
                            }
                            command_emitted++;
                        }
                    }
                    else {
                        SHOW( fprintf(gpx->log, "(line %u) Syntax error: M109 is missing temperature, use Sn where n is 0-280" EOL, gpx->lineNumber) );
                    }
                    break;
                }
                // fall through to M140 for Makerbot/ReplicatorG flavor
                
                // M140 - Set build platform temperature
            case 140:
                if(gpx->machine.a.has_heated_build_platform || gpx->machine.b.has_heated_build_platform) {
                    if(gpx->command.flag & S_IS_SET) {
                        unsigned temperature = (unsigned)gpx->command.s;
                        if(temperature > HBP_MAX) temperature = HBP_MAX;
                        unsigned tool_id = gpx->machine.a.has_heated_build_platform ? A : B;
                        if(gpx->command.flag & T_IS_SET) {
                            tool_id = gpx->target.extruder;
                        }
                        if(tool_id ? gpx->machine.b.has_heated_build_platform : gpx->machine.a.has_heated_build_platform) {
                            if(temperature && gpx->override[tool_id].build_platform_temperature) {
                                temperature = gpx->override[tool_id].build_platform_temperature;
                            }
                            CALL( set_build_platform_temperature(gpx, tool_id, temperature) );
                            command_emitted++;
                            gpx->tool[tool_id].build_platform_temperature = temperature;
                        }
                        else {
                            SHOW( fprintf(gpx->log, "(line %u) Semantic warning: M%u cannot select non-existant heated build platform T%u" EOL, gpx->lineNumber, gpx->command.m, tool_id) );
                        }
                    }
                    else {
                        SHOW( fprintf(gpx->log, "(line %u) Syntax error: M%u is missing temperature, use Sn where n is 0-120" EOL, gpx->lineNumber, gpx->command.m) );
                    }
                }
                else {
                    SHOW( fprintf(gpx->log, "(line %u) Semantic warning: M%u cannot select non-existant heated build platform" EOL, gpx->lineNumber, gpx->command.m) );
                }
                break;
                
                // M110 - Set current line number
            case 110:
                break;
                
                // M111 - Set debug level
            case 111:
                
                // M126 - Turn blower fan on (valve open)
            case 126: {
                int state = (gpx->command.flag & S_IS_SET) ? ((unsigned)gpx->command.s ? 1 : 0) : 1;
                if(gpx->flag.dittoPrinting) {
                    CALL( set_valve(gpx, B, state) );
                    CALL( set_valve(gpx, A, state) );
                    command_emitted++;
                }
                else {
                    CALL( set_valve(gpx, gpx->target.extruder, state) );
                    command_emitted++;
                }
                break;
            }
                
                // M127 - Turn blower fan off (valve close)
            case 127:
                if(gpx->flag.dittoPrinting) {
                    CALL( set_valve(gpx, B, 0) );
                    CALL( set_valve(gpx, A, 0) );
                    command_emitted++;
                }
                else {
                    CALL( set_valve(gpx, gpx->target.extruder, 0) );
                    command_emitted++;
                }
                break;
                
                // M131 - Store Current Position to EEPROM
            case 131:
                if(gpx->command.flag & AXES_BIT_MASK) {
                    CALL( store_home_positions(gpx) );
                    command_emitted++;
                }
                else {
                    SHOW( fprintf(gpx->log, "(line %u) Syntax error: M131 is missing axes, use X Y Z A B" EOL, gpx->lineNumber) );
                }
                break;
                
                // M132 - Load Current Position from EEPROM
            case 132:
                if(gpx->command.flag & AXES_BIT_MASK) {
                    CALL( recall_home_positions(gpx) );
                    command_emitted++;
                    // clear loaded axes
                    gpx->axis.positionKnown &= ~(gpx->command.flag & gpx->axis.mask);;
                    gpx->excess.a = 0;
                    gpx->excess.b = 0;
                }
                else {
                    SHOW( fprintf(gpx->log, "(line %u) Syntax error: M132 is missing axes, use X Y Z A B" EOL, gpx->lineNumber) );
                }
                break;
                
                // M133 - Wait for extruder
            case 133: {
                int timeout = gpx->command.flag & P_IS_SET ? (int)gpx->command.p : MAX_TIMEOUT;
                // changing the
                if(gpx->flag.dittoPrinting) {
                    if(gpx->tool[B].nozzle_temperature > 0) {
                        CALL( wait_for_extruder(gpx, B, timeout) );
                    }
                    if(gpx->tool[A].nozzle_temperature > 0) {
                        CALL( wait_for_extruder(gpx, A, timeout) );
                        if(gpx->flag.verboseMode) {
                            CALL( display_tag(gpx) );
                        }
                    }
                    command_emitted++;
                }
                else {
#if ENABLE_TOOL_CHANGE_ON_WAIT
                    // because there is a wait we do a tool change
                    if(gpx->target.extruder != gpx->current.extruder) {
                        CALL( do_tool_change(gpx, timeout) );
                    }
#endif
                    // any tool changes have already occured
                    if(gpx->tool[gpx->target.extruder].nozzle_temperature > 0) {
                        CALL( wait_for_extruder(gpx, gpx->target.extruder, timeout) );
                        if(gpx->flag.verboseMode) {
                            CALL( display_tag(gpx) );
                        }
                    }
                    command_emitted++;
                }
                break;
            }
                
                // M134
                // M190 - Wait for build platform to reach (or exceed) temperature
            case 134:
            case 190: {
                if(gpx->machine.a.has_heated_build_platform || gpx->machine.b.has_heated_build_platform) {
                    int timeout = gpx->command.flag & P_IS_SET ? (int)gpx->command.p : MAX_TIMEOUT;
                    unsigned tool_id = gpx->machine.a.has_heated_build_platform ? A : B;
                    if(gpx->command.flag & T_IS_SET) {
                        tool_id = gpx->target.extruder;
                    }
                    if(tool_id ? gpx->machine.b.has_heated_build_platform : gpx->machine.a.has_heated_build_platform
                       && gpx->tool[tool_id].build_platform_temperature > 0) {
                        CALL( wait_for_build_platform(gpx, tool_id, timeout) );
                        command_emitted++;
                    }
                    else {
                        SHOW( fprintf(gpx->log, "(line %u) Semantic warning: M%u cannot select non-existant heated build platform T%u" EOL, gpx->lineNumber, gpx->command.m, tool_id) );
                    }
                }
                else {
                    SHOW( fprintf(gpx->log, "(line %u) Semantic warning: M%u cannot select non-existant heated build platform" EOL, gpx->lineNumber, gpx->command.m) );
                }
                break;
            }
                
                // M135 - Change tool
            case 135:
                if(!gpx->flag.dittoPrinting && gpx->target.extruder != gpx->current.extruder) {
                    int timeout = gpx->command.flag & P_IS_SET ? (int)gpx->command.p : MAX_TIMEOUT;
                    CALL( do_tool_change(gpx, timeout) );
                    command_emitted++;
                }
                break;
                
                // M136 - Build start notification
            case 136:
                if(program_is_ready()) {
                    start_program();
                    CALL( start_build(gpx, gpx->buildName) );
                    CALL( set_build_progress(gpx, 0) );
                    // start extruder in a known state
                    CALL( change_extruder_offset(gpx, gpx->current.extruder) );
                }
                break;
                
                // M137 - Build end notification
            case 137:
                if(program_is_running()) {
                    // disable macros in footer
                    gpx->flag.macrosEnabled = 0;
                    end_program();
                    CALL( set_build_progress(gpx, 100) );
                    CALL( end_build(gpx) );
                    gpx->current.percent = 100;
                }
                break;
                
                // M300 - Set Beep (SP)
            case 300: {
                unsigned frequency = 300;
                if(gpx->command.flag & S_IS_SET) frequency = (unsigned)gpx->command.s & 0xFFFF;
                unsigned milliseconds = 1000;
                if(gpx->command.flag & P_IS_SET) milliseconds = (unsigned)gpx->command.p & 0xFFFF;
                CALL( set_beep(gpx, frequency, milliseconds) );
                command_emitted++;
                break;
            }
                
                // M320 - Acceleration on for subsequent instructions
            case 320:
                CALL( set_acceleration(gpx, 1) );
                command_emitted++;
                break;
                
                // M321 - Acceleration off for subsequent instructions
            case 321:
                CALL( set_acceleration(gpx, 0) );
                command_emitted++;
                break;
                
                // M322 - Pause @ zPos
            case 322:
                if(gpx->command.flag & Z_IS_SET) {
                    float conditional_z = gpx->offset[gpx->current.offset].z;
                    
                    if(gpx->flag.macrosEnabled) {
                        conditional_z += gpx->user.offset.z;
                    }
                    
                    double z = gpx->flag.relativeCoordinates ? (gpx->current.position.z + gpx->command.z) : (gpx->command.z + conditional_z);
                    CALL( pause_at_zpos(gpx, z) );
                }
                else {
                    SHOW( fprintf(gpx->log, "(line %u) Syntax warning: M322 is missing Z axis" EOL, gpx->lineNumber) );
                }
                command_emitted++;
                break;
                
                // M420 - Set RGB LED value (REB - P)
            case 420: {
                unsigned red = 0;
                if(gpx->command.flag & R_IS_SET) red = (unsigned)gpx->command.r & 0xFF;
                unsigned green = 0;
                if(gpx->command.flag & E_IS_SET) green = (unsigned)gpx->command.e & 0xFF;
                unsigned blue = 0;
                if(gpx->command.flag & B_IS_SET) blue = (unsigned)gpx->command.b & 0xFF;
                unsigned blink = 0;
                if(gpx->command.flag & P_IS_SET) blink = (unsigned)gpx->command.p & 0xFF;
                CALL( set_LED(gpx, red, green, blue, blink) );
                command_emitted++;
                break;
            }
                
                // M500 - Write paramters to EEPROM
                // M501 - Read parameters from EEPROM
                // M502 - Revert to default "factory settings"
                // M503 - Print/log current settings                
            default:
                SHOW( fprintf(gpx->log, "(line %u) Syntax warning: unsupported mcode command 'M%u'" EOL, gpx->lineNumber, gpx->command.m) );
        }
    }
    else {
        // X,Y,Z,A,B,E,F
        if(gpx->command.flag & (AXES_BIT_MASK | F_IS_SET)) {
            CALL( calculate_target_position(gpx) );
            CALL( queue_ext_point(gpx, 0.0) );
            update_current_position(gpx);
            command_emitted++;
        }
        // Tn
        else if(!gpx->flag.dittoPrinting && gpx->target.extruder != gpx->current.extruder) {
            int timeout = gpx->command.flag & P_IS_SET ? (int)gpx->command.p : MAX_TIMEOUT;
            CALL( do_tool_change(gpx, timeout) );
            command_emitted++;
        }
    }
    // check for pending pause @ zPos
    if(gpx->flag.doPauseAtZPos) {
        gpx->flag.doPauseAtZPos--;
        // issue next pause @ zPos after command buffer is flushed
        if(gpx->flag.doPauseAtZPos == 0) {
            CALL( pause_at_zpos(gpx, gpx->commandAt[gpx->commandAtIndex].z) );
        }
    }
    // update progress
    if(gpx->total.time > 0.0001 && gpx->accumulated.time > 0.0001 && gpx->flag.buildProgress && command_emitted) {
        unsigned percent = (unsigned)round(100.0 * gpx->accumulated.time / gpx->total.time);
        if(percent > gpx->current.percent) {
            if(program_is_ready()) {
                start_program();
                CALL( start_build(gpx, gpx->buildName) );
                CALL( set_build_progress(gpx, 0) );
                // start extruder in a known state
                CALL( change_extruder_offset(gpx, gpx->current.extruder) );
            }
            else if(percent < 100 && program_is_running()) {
                if(gpx->current.percent) {
                    CALL( set_build_progress(gpx, percent) );
                    gpx->current.percent = percent;
                }
                // force 1%
                else {
                    CALL( set_build_progress(gpx, 1) );
                    gpx->current.percent = 1;
                }
            }
            command_emitted = 0;
        }
    }
    gpx->lineNumber = next_line;
    return SUCCESS;
}

typedef struct tFile {
    FILE *in;
    FILE *out;
    FILE *out2;
} File;

static int file_handler(Gpx *gpx, File *file, char *buffer, size_t length)
{
    if(length) {
        ssize_t bytes = fwrite(buffer, 1, length, file->out);
        if(bytes != length) return ERROR;
        if(file->out2) {
            bytes = fwrite(buffer, 1, length, file->out2);
            if(bytes != length) return ERROR;            
        }
    }
    return SUCCESS;
}

int gpx_convert(Gpx *gpx, FILE *file_in, FILE *file_out, FILE *file_out2)
{
    int i, rval;
    File file;
    file.in = stdin;
    file.out = stdout;
    file.out2 = NULL;
    int logMessages = gpx->flag.logMessages;

    if(file_in && file_in != stdin) {
        // Multi-pass
        file.in = file_in;
        i = 0;
        gpx->flag.runMacros = 0;
        gpx->callbackHandler = NULL;
        gpx->callbackData = NULL;
    }
    else {
        // Single-pass
        i = 1;
        gpx->callbackHandler = (int (*)(Gpx*, void*, char*, size_t))file_handler;;
        gpx->callbackData = &file;
    }
    
    if(file_out) {
        file.out = file_out;
    }
    
    file.out2 = file_out2;

    for(;;) {
        int overflow = 0;
        
        while(fgets(gpx->buffer.in, BUFFER_MAX, file.in) != NULL) {
            // detect input buffer overflow and ignore overflow input
            if(overflow) {
                if(strlen(gpx->buffer.in) != BUFFER_MAX - 1) {
                    overflow = 0;
                }
                continue;
            }
            if(strlen(gpx->buffer.in) == BUFFER_MAX - 1) {
                overflow = 1;
                SHOW( fprintf(gpx->log, "(line %u) Buffer overflow: input exceeds %u character limit, remaining characters in line will be ignored" EOL, gpx->lineNumber, BUFFER_MAX) );
            }
            
            rval = gpx_convert_line(gpx, gpx->buffer.in);
            // normal exit
            if(rval == END_OF_FILE) break;
            // error
            if(rval < 0) return rval;
        }
        
        if(program_is_running()) {
            end_program();
            CALL( set_build_progress(gpx, 100) );
            CALL( end_build(gpx) );
        }

        CALL( set_steppers(gpx, AXES_BIT_MASK, 0) );

        gpx->total.length = gpx->accumulated.a + gpx->accumulated.b;
        gpx->total.time = gpx->accumulated.time;
        gpx->total.bytes = gpx->accumulated.bytes;

        if(++i > 1) break;

        // rewind for second pass
        fseek(file.in, 0L, SEEK_SET);
        gpx_initialize(gpx, 0);
        gpx->flag.loadMacros = 0;
        gpx->flag.runMacros = 1;
        //gpx->flag.logMessages = 0;
        gpx->callbackHandler = (int (*)(Gpx*, void*, char*, size_t))file_handler;
        gpx->callbackData = &file;
    }
    gpx->flag.logMessages = logMessages;;
    return SUCCESS;
}

typedef struct tSio {
    FILE *in;
    int port;
    unsigned bytes_out;
    unsigned bytes_in;
    
    union {
        struct {
            unsigned short version;
            unsigned char variant;
        } firmware;
        
        unsigned int bufferSize;
        unsigned short temperature;
        unsigned int isReady;
        
        union {
            unsigned char bitfield;
            struct {
                unsigned char ready: 1;             // The extruder has reached target temperature
                unsigned char notPluggedIn: 1;      // The tool or platform is not plugged in.
                unsigned char softwareCutoff: 1;    // Temperature was recorded above maximum allowable.
                unsigned char notHeating: 1;        // Heater is not heating up as expected.
                unsigned char temperatureDropping: 1; // Heater temperature dropped below target temp.
                unsigned char reserved: 1;
                unsigned char buildPlateError: 1;   // An error was detected with the platform heater.
                unsigned char extruderError: 1;     // An error was detected with the extruder heater.
            } flag;
        } extruder;

        struct {
            char buffer[31];
            unsigned char length;
        } eeprom;
        
        struct {
            short extruderError;
            short extruderDelta;
            short extruderOutput;
            
            short buildPlateError;
            short buildPlateDelta;
            short buildPlateOutput;
        } pid;
        
        struct {
            unsigned int length;
            char filename[65];
            unsigned char status;
        } sd;
        
        struct {
            int x;
            int y;
            int z;
            int a;
            int b;
            
            union {
                unsigned short bitfield;
                struct {
                    unsigned short xMin: 1; // X min switch pressed
                    unsigned short xMax: 1; // X max switch pressed
                    
                    unsigned short yMin: 1; // Y min switch pressed
                    unsigned short yMax: 1; // Y max switch pressed
                    
                    unsigned short zMin: 1; // Z min switch pressed
                    unsigned short zMax: 1; // Z max switch pressed
                    
                    unsigned short aMin: 1; // A min switch pressed
                    unsigned short aMax: 1; // A max switch pressed
                    
                    unsigned short bMin: 1; // B min switch pressed
                    unsigned short bMax: 1; // B max switch pressed
                } flag;
            } endstop;
        } position;
        
        union {
            unsigned char bitfield;
            struct {
                unsigned char preheat: 1;         // Onboard preheat active
                unsigned char manualMode: 1;      // Manual move mode active
                unsigned char onboardScript: 1;   // Bot is running an onboard script
                unsigned char onboardProcess: 1;  // Bot is running an onboard process
                unsigned char waitForButton: 1;   // Bot is waiting for button press
                unsigned char buildCancelling: 1; // Watchdog reset flag was set at restart
                unsigned char heatShutdown: 1;    // Heaters were shutdown after 30 minutes of inactivity
                unsigned char powerError: 1;      // An error was detected with the system power.
            } flag;            
        } motherboard;
        
        struct {
            unsigned lineNumber;
            unsigned char status;
            unsigned char hours;
            unsigned char minutes;
        } build;

    } response;

} Sio;

char *sd_status[] = {
    "operation successful",
    "SD Card not present",
    "SD Card initialization failed",
    "partition table could not be read",
    "filesystem could not be opened",
    "root directory could not be opened",
    "SD Card is locked",
    "unknown status"
};

static char *get_sd_status(unsigned int status)
{
    return sd_status[status < 7 ? status : 7];
}

char *build_status[] = {
    "no build initialized (boot state)",
    "build running",
    "build finished normally",
    "build paused",
    "build cancelled",
    "build sleeping",
    "unknown status"
};

static char *get_build_status(unsigned int status)
{
    return sd_status[status < 6 ? status : 6];
}

static void read_extruder_query_response(Gpx *gpx, Sio *sio, unsigned command, char *buffer)
{
    unsigned extruder_id = buffer[EXTRUDER_ID_OFFSET];

    switch(command) {
            // Query 00 - Query firmware version information
        case 0:
            // uint16: Firmware Version
            sio->response.firmware.version = read_16(gpx);
            VERBOSE( fprintf(gpx->log, "Extruder T%u firmware v%u.%u" EOL,
                        extruder_id,
                        sio->response.firmware.version / 100,
                        sio->response.firmware.version % 100) );
            break;
    
            // Query 02 - Get extruder temperature
        case 2:
            // int16: Current temperature, in Celsius
            sio->response.temperature = read_16(gpx);
            VERBOSE( fprintf(gpx->log, "Extruder T%u temperature: %uc" EOL,
                        extruder_id,
                        sio->response.temperature) );
            break;
    
            // Query 22 - Is extruder ready
        case 22:
            // uint8: 1 if ready, 0 otherwise.
            sio->response.isReady = read_8(gpx);
            VERBOSE( fprintf(gpx->log, "Extruder T%u is%sready" EOL,
                        extruder_id,
                        sio->response.isReady ? " " : " not ") );
            break;
            
            // Query 30 - Get build platform temperature
        case 30:
            // int16: Current temperature, in Celsius
            sio->response.temperature = read_16(gpx);
            VERBOSE( fprintf(gpx->log, "Build platform T%u temperature: %uc" EOL,
                        extruder_id,
                        sio->response.temperature) );
            break;
    
            // Query 32 - Get extruder target temperature
        case 32:
            // int16: Current temperature, in Celsius
            sio->response.temperature = read_16(gpx);
            VERBOSE( fprintf(gpx->log, "Extruder T%u target temperature: %uc" EOL,
                        extruder_id,
                        sio->response.temperature) );
            break;
    
            // Query 33 - Get build platform target temperature
        case 33:
            // int16: Current temperature, in Celsius
            sio->response.temperature = read_16(gpx);
            VERBOSE( fprintf(gpx->log, "Build platform T%u target temperature: %uc" EOL,
                        extruder_id,
                        sio->response.temperature) );
            break;
    
            // Query 35 - Is build platform ready?
        case 35:
            // uint8: 1 if ready, 0 otherwise.
            sio->response.isReady = read_8(gpx);
            VERBOSE( fprintf(gpx->log, "Build platform T%u is%sready" EOL,
                        extruder_id,
                        sio->response.isReady ? " " : " not ") );
            break;
    
            // Query 36 - Get extruder status
        case 36:
            // uint8: Bitfield containing status information
            sio->response.extruder.bitfield = read_8(gpx);

            if(gpx->flag.verboseMode && gpx->flag.logMessages) {
                fprintf(gpx->log, "Extruder T%u status" EOL, extruder_id);
                if(sio->response.extruder.flag.ready) fputs("Target temperature reached" EOL, gpx->log);
                if(sio->response.extruder.flag.notPluggedIn) fputs("The extruder or build plate is not plugged in" EOL, gpx->log);
                if(sio->response.extruder.flag.softwareCutoff) fputs("Above maximum allowable temperature recorded: heater shutdown for safety" EOL, gpx->log);
                if(sio->response.extruder.flag.temperatureDropping) fputs("Heater temperature dropped below target temperature" EOL, gpx->log);
                if(sio->response.extruder.flag.buildPlateError) fputs("An error was detected with the build plate heater or sensor" EOL, gpx->log);
                if(sio->response.extruder.flag.extruderError) fputs("An error was detected with the extruder heater or sensor" EOL, gpx->log);
            }
            break;
    
            // Query 37 - Get PID state
        case 37:
            // int16: Extruder heater error term
            sio->response.pid.extruderError = read_16(gpx);
            
            // int16: Extruder heater delta term
            sio->response.pid.extruderDelta = read_16(gpx);
            
            // int16: Extruder heater last output
            sio->response.pid.extruderOutput = read_16(gpx);
            
            // int16: Platform heater error term
            sio->response.pid.buildPlateError = read_16(gpx);
            
            // int16: Platform heater delta term
            sio->response.pid.buildPlateDelta = read_16(gpx);
            
            // int16: Platform heater last output
            sio->response.pid.buildPlateOutput = read_16(gpx);
            
            break;
            
        default:
            abort();
    }
}

static void read_query_response(Gpx *gpx, Sio *sio, unsigned command, char *buffer)
{
    gpx->buffer.ptr = gpx->buffer.in + 2;
    switch(command) {
            // 00 - Query firmware version information
        case 0:
            // uint16: Firmware Version
            sio->response.firmware.version = read_16(gpx);
            VERBOSE( fprintf(gpx->log, "Motherboard firmware v%u.%u" EOL,
                        sio->response.firmware.version / 100, sio->response.firmware.version % 100) );
            break;
            
            // 02 - Get available buffer size
        case 2:
            // uint32: Number of bytes availabe in the command buffer
            sio->response.bufferSize = read_32(gpx);
            break;
            
            // 10 - Extruder query command
        case 10: {
            unsigned query_command = buffer[QUERY_COMMAND_OFFSET];
            read_extruder_query_response(gpx, sio, query_command, buffer);
            break;
        }

            // 11 - Is ready
        case 11:
            // uint8: 1 if ready, 0 otherwise.
            sio->response.isReady = read_8(gpx);
            VERBOSE( fprintf(gpx->log, "Printer is%sready" EOL,
                             sio->response.isReady ? " " : " not ") );
            break;
            
            // 12 - Read from EEPROM
        case 12:
            // N bytes: Data read from the EEPROM
            sio->response.eeprom.length = buffer[EEPROM_LENGTH_OFFSET];
            read_bytes(gpx, sio->response.eeprom.buffer, sio->response.eeprom.length);
            break;

            // 13 - Write to EEPROM
        case 13:
            // uint8: Number of bytes successfully written to the EEPROM
            sio->response.eeprom.length = read_8(gpx);
            break;
            
            // 14 - Capture to file
        case 14:
            // uint8: SD response code
            sio->response.sd.status = read_8(gpx);
            VERBOSE( fprintf(gpx->log, "Capture to file: %s" EOL,
                        get_sd_status(sio->response.sd.status)) );
            break;
            
            // 15 - End capture to file
        case 15:
            // uint32: Number of bytes captured to file.
            sio->response.sd.length = read_32(gpx);
            VERBOSE( fprintf(gpx->log, "Capture to file ended: %u bytes written" EOL,
                        sio->response.sd.length) );
            break;
            
            // 16 - Play back capture
        case 16:
            // uint8: SD response code
            sio->response.sd.status = read_8(gpx);
            VERBOSE( fprintf(gpx->log, "Play back captured file: %s" EOL,
                        get_sd_status(sio->response.sd.status)) );
            break;
            
            // 18 - Get next filename
        case 18:
            // uint8: SD Response code.
            sio->response.sd.status = read_8(gpx);
            /* 1+N bytes: Name of the next file, in ASCII, terminated with a null character.
                          If the operation was unsuccessful, this will be a null character */
            strncpy0(sio->response.sd.filename, gpx->buffer.ptr, 65);
            VERBOSE( fprintf(gpx->log, "Get next filename: '%s' %s" EOL,
                        sio->response.sd.filename,
                        get_sd_status(sio->response.sd.status)) );
            break;
            
            // 20 - Get build name
        case 20:
            // 1+N bytes: A null terminated string representing the filename of the current build.
            strncpy0(sio->response.sd.filename, gpx->buffer.ptr, 65);
            VERBOSE( fprintf(gpx->log, "Get build name: '%s'" EOL, sio->response.sd.filename) );
            break;
            
            // 21 - Get extended position
        case 21:
            // int32: X position, in steps
            sio->response.position.x = read_32(gpx);
            
            // int32: Y position, in steps
            sio->response.position.y = read_32(gpx);
            
            // int32: Z position, in steps
            sio->response.position.z = read_32(gpx);
            
            // int32: A position, in steps
            sio->response.position.a = read_32(gpx);
            
            // int32: B position, in steps
            sio->response.position.b = read_32(gpx);
            
            // uint16: bitfield corresponding to the endstop status:
            sio->response.position.endstop.bitfield = read_16(gpx);
            
            if(gpx->flag.verboseMode && gpx->flag.logMessages) {
                fputs("Current position" EOL, gpx->log);
                fprintf(gpx->log, "X = %0.2fmm%s%s" EOL,
                        (double)sio->response.position.x / gpx->machine.x.steps_per_mm,
                        sio->response.position.endstop.flag.xMax ? ", at max endstop" : "",
                        sio->response.position.endstop.flag.xMin ? ", at min endstop" : "");
                fprintf(gpx->log, "Y = %0.2fmm%s%s" EOL,
                        (double)sio->response.position.y / gpx->machine.y.steps_per_mm,
                        sio->response.position.endstop.flag.yMax ? ", at max endstop" : "",
                        sio->response.position.endstop.flag.yMin ? ", at min endstop" : "");
                fprintf(gpx->log, "Z = %0.2fmm%s%s" EOL,
                        (double)sio->response.position.z / gpx->machine.z.steps_per_mm,
                        sio->response.position.endstop.flag.zMax ? ", at max endstop" : "",
                        sio->response.position.endstop.flag.zMin ? ", at min endstop" : "");
                fprintf(gpx->log, "A = %0.2fmm%s%s" EOL,
                        (double)sio->response.position.a / gpx->machine.a.steps_per_mm,
                        sio->response.position.endstop.flag.aMax ? ", at max endstop" : "",
                        sio->response.position.endstop.flag.aMin ? ", at min endstop" : "");
                fprintf(gpx->log, "B = %0.2fmm%s%s" EOL,
                        (double)sio->response.position.b / gpx->machine.b.steps_per_mm,
                        sio->response.position.endstop.flag.bMax ? ", at max endstop" : "",
                        sio->response.position.endstop.flag.bMin ? ", at min endstop" : "");
            }
            break;
                        
            // 22 - Extended stop
        case 22:
            // int8: 0 (reserved for future use)
            read_8(gpx);
            VERBOSE( fputs("Build stopped" EOL, gpx->log) );
            break;
            
            // 23 - Get motherboard status
        case 23:
            // uint8: bitfield containing status information
            sio->response.motherboard.bitfield = read_8(gpx);
            if(gpx->flag.verboseMode && gpx->flag.logMessages) {
                fputs("Motherboard status" EOL, gpx->log);
                if(sio->response.motherboard.flag.preheat) fputs("Onboard preheat active" EOL, gpx->log);
                if(sio->response.motherboard.flag.manualMode) fputs("Manual move active" EOL, gpx->log);
                if(sio->response.motherboard.flag.onboardScript) fputs("Running onboard script" EOL, gpx->log);
                if(sio->response.motherboard.flag.onboardProcess) fputs("Running onboard process" EOL, gpx->log);
                if(sio->response.motherboard.flag.waitForButton) fputs("Waiting for buttons press" EOL, gpx->log);
                if(sio->response.motherboard.flag.buildCancelling) fputs("Build cancelling" EOL, gpx->log);
                if(sio->response.motherboard.flag.heatShutdown) fputs("Heaters were shutdown after 30 minutes of inactivity" EOL, gpx->log);
                if(sio->response.motherboard.flag.powerError) fputs("Error detected in system power" EOL, gpx->log);
            }
            break;
            
            // 24 - Get build statistics
        case 24:
            // uint8 : Build status
            sio->response.build.status = read_8(gpx);
            
            // uint8 : Hours elapsed on print
            sio->response.build.hours = read_8(gpx);
            
            // uint8 : Minutes elapsed on print (add hours for total time)
            sio->response.build.minutes = read_8(gpx);

            // uint32: Line number (number of commands processed)
            sio->response.build.lineNumber = read_32(gpx);

            // uint32: Reserved for future use
            read_32(gpx);
            
            VERBOSE( fprintf(gpx->log, "(line %u) Build status: %s, %u hours, %u minutes" EOL,
                        sio->response.build.lineNumber,
                        get_build_status(sio->response.build.status),
                        sio->response.build.hours,
                        sio->response.build.minutes) );
            break;

            // 27 - Get advanced version number
        case 27:
            // uint16_t Firmware version
            sio->response.firmware.version = read_16(gpx);
            
            // uint16_t Internal version
            read_16(gpx);
            
            // uint8_t Software variant (0x01 MBI Official, 0x80 Sailfish)
            sio->response.firmware.variant = read_8(gpx);
            
            // uint8_t Reserved for future use
            read_8(gpx);

            // uint16_t Reserved for future use
            read_16(gpx);
            
            if(gpx->flag.verboseMode && gpx->flag.logMessages) {
                char *varient = "Unknown";
                switch(sio->response.firmware.variant) {
                    case 0x01:
                        varient = "Makerbot";
                        break;
                    case 0x80:
                        varient = "Sailfish";
                        break;
                }
                fprintf(gpx->log, "%s firmware v%u.%u" EOL, varient, sio->response.firmware.version / 100, sio->response.firmware.version % 100);
            }
            break;
    }
}

// 02 - Get available buffer size

char buffer_size_query[] = {
    0xD5,   // start byte
    1,      // length
    2,      // query command
    0       // crc
};

static int port_handler(Gpx *gpx, Sio *sio, char *buffer, size_t length)
{
    int rval = SUCCESS;
    if(length) {
        ssize_t bytes;
        int retry_count = 0;
        do {
            // send the packet
            if((bytes = write(sio->port, buffer, length)) == -1) {
                return errno;
            }
            else if(bytes != length) {
                return ESIOWRITE;
            }
            sio->bytes_out += length;
            
            if(sio->bytes_in) {
                // recieve the response
                if((bytes = read(sio->port, gpx->buffer.in, 2)) == -1) {
                    return errno;
                }
                else if(bytes != 2) {
                    return ESIOREAD;
                }
                // invalid start byte
                if(gpx->buffer.in[0] != 0xD5) {
                    return ESIOFRAME;
                }
            }
            else {
                // first read
                for(;;) {
                    // read start byte
                    if((bytes = read(sio->port, gpx->buffer.in, 1)) == -1) {
                        return errno;
                    }
                    else if(bytes != 1) {
                        return ESIOREAD;
                    }
                    // loop until we get a valid start byte
                    if(gpx->buffer.in[0] == 0xD5) break;
                }
                // read length
                if((bytes = read(sio->port, gpx->buffer.in + 1, 1)) == -1) {
                    return errno;
                }
                else if(bytes != 1) {
                    return ESIOREAD;                    
                }
            }
            size_t payload_length = gpx->buffer.in[1];
            // recieve payload
            if((bytes = read(sio->port, gpx->buffer.in + 2, payload_length + 1)) == -1) {
                return errno;
            }
            else if(bytes != payload_length + 1) {
                return ESIOREAD;                
            }
            // check CRC
            unsigned crc = (unsigned char)gpx->buffer.in[2 + payload_length];
            if(crc != calculate_crc((unsigned char*)gpx->buffer.in + 2, payload_length)) {
                fprintf(gpx->log, "(retry %u) Input CRC mismatch: packet discarded" EOL, retry_count);
                rval = ESIOCRC;
                goto L_RETRY;
            }
            // check response code
            rval = gpx->buffer.in[2];
            switch((unsigned)gpx->buffer.in[2]) {
                    // 0x80 - Generic Packet error, packet discarded (retry)
                case 0x80:
                    VERBOSE( fprintf(gpx->log, "(retry %u) Generic Packet error: packet discarded" EOL, retry_count) );
                    break;
        
                    // 0x81 - Success
                case 0x81: {
                    unsigned command = (unsigned)buffer[COMMAND_OFFSET];
                    if ((command & 0x80) == 0) {
                        read_query_response(gpx, sio, command, buffer);
                    }
                    return SUCCESS;
                }
                    
                    // 0x82 - Action buffer overflow, entire packet discarded
                case 0x82:
                    do {
                        // wait for 1/10 seconds
                        usleep(100000);
                        // query buffer size
                        buffer_size_query[3] = calculate_crc((unsigned char *)buffer_size_query + 2, 1);
                        CALL( port_handler(gpx, sio, buffer_size_query, 4) );
                        // loop until buffer has space for the next command
                    } while(sio->response.bufferSize < length);
                    break;
                    
                    // 0x83 - CRC mismatch, packet discarded. (retry)
                case 0x83:
                    VERBOSE( fprintf(gpx->log, "(retry %u) Output CRC mismatch: packet discarded" EOL, retry_count) );
                    break;
                    
                    // 0x84 - Query packet too big, packet discarded
                case 0x84:
                    VERBOSE( fprintf(gpx->log, "(retry %u) Query packet too big: packet discarded" EOL, retry_count) );
                    goto L_ABORT;

                    // 0x85 - Command not supported/recognized
                case 0x85:
                    VERBOSE( fprintf(gpx->log, "(retry %u) Command not supported or recognized" EOL, retry_count) );
                    goto L_ABORT;
                    
                    // 0x87 - Downstream timeout
                case 0x87:
                    VERBOSE( fprintf(gpx->log, "(retry %u) Downstream timeout" EOL, retry_count) );
                    goto L_ABORT;
                    
                    // 0x88 - Tool lock timeout (retry)
                case 0x88:
                    VERBOSE( fprintf(gpx->log, "(retry %u) Tool lock timeout" EOL, retry_count) );
                    break;
                    
                // 0x89 - Cancel build (retry)
                case 0x89:
                    VERBOSE( fprintf(gpx->log, "(retry %u) Cancel build" EOL, retry_count) );
                    break;
                    
                    // 0x8A - Bot is building from SD
                case 0x8A:
                    VERBOSE( fprintf(gpx->log, "(retry %u) Bot is Building from SD card" EOL, retry_count) );
                    goto L_ABORT;
                    
                    // 0x8B - Bot is shutdown due to overheating
                case 0x8B:
                    VERBOSE( fprintf(gpx->log, "(retry %u) Bot is shutdown due to overheating" EOL, retry_count) );
                    goto L_ABORT;
                    
                    // 0x8C - Packet timeout error, packet discarded (retry)
                case 0x8C:
                    VERBOSE( fprintf(gpx->log, "(retry %u) Packet timeout error: packet discarded" EOL, retry_count) );
                    break;
            }
L_RETRY:
            // wait for 2 seconds
            sleep(2);
        } while(++retry_count < 5);
    }

L_ABORT:
    return rval;
}

int gpx_convert_and_send(Gpx *gpx, FILE *file_in, int sio_port)
{
    int i, rval;
    Sio sio;
    sio.in = stdin;
    sio.port = -1;
    sio.bytes_out = 0;
    sio.bytes_in = 0;
    int logMessages = gpx->flag.logMessages;

    if(file_in && file_in != stdin) {
        // Multi-pass
        sio.in = file_in;
        i = 0;
        gpx->flag.logMessages = 0;
        gpx->flag.framingEnabled = 0;
        gpx->callbackHandler = NULL;
        gpx->callbackData = NULL;
    }
    else {
        // Single-pass
        i = 1;
        gpx->flag.framingEnabled = 1;
        gpx->callbackHandler = (int (*)(Gpx*, void*, char*, size_t))port_handler;;
        gpx->callbackData = &sio;
    }
    
    if(sio_port > 2) {
        sio.port = sio_port;
    }
    
    for(;;) {
        int overflow = 0;
        
        while(fgets(gpx->buffer.in, BUFFER_MAX, sio.in) != NULL) {
            // detect input buffer overflow and ignore overflow input
            if(overflow) {
                if(strlen(gpx->buffer.in) != BUFFER_MAX - 1) {
                    overflow = 0;
                }
                continue;
            }
            if(strlen(gpx->buffer.in) == BUFFER_MAX - 1) {
                overflow = 1;
                SHOW( fprintf(gpx->log, "(line %u) Buffer overflow: input exceeds %u character limit, remaining characters in line will be ignored" EOL, gpx->lineNumber, BUFFER_MAX) );
            }
            
            rval = gpx_convert_line(gpx, gpx->buffer.in);
            // normal exit
            if(rval > 0) break;
            // error
            if(rval < 0) return rval;
        }
        
        if(program_is_running()) {
            end_program();
            CALL( set_build_progress(gpx, 100) );
            CALL( end_build(gpx) );
        }
        
        CALL( set_steppers(gpx, AXES_BIT_MASK, 0) );
        
        gpx->total.length = gpx->accumulated.a + gpx->accumulated.b;
        gpx->total.time = gpx->accumulated.time;
        gpx->total.bytes = gpx->accumulated.bytes;
        
        if(++i > 1) break;
        
        // rewind for second pass
        fseek(sio.in, 0L, SEEK_SET);
        gpx_initialize(gpx, 0);
        
        gpx->flag.logMessages = 1;
        gpx->flag.framingEnabled = 1;
        gpx->callbackHandler = (int (*)(Gpx*, void*, char*, size_t))port_handler;;
        gpx->callbackData = &sio;
    }
    gpx->flag.logMessages = logMessages;;
    return SUCCESS;
}

void gpx_end_convert(Gpx *gpx)
{
    if(gpx->flag.verboseMode && gpx->flag.logMessages) {
        long seconds = round(gpx->accumulated.time);
        long minutes = seconds / 60;
        long hours = minutes / 60;
        minutes %= 60;
        seconds %= 60;
        fprintf(gpx->log, "Extrusion length: %#0.3f metres" EOL, round(gpx->accumulated.a + gpx->accumulated.b) / 1000);
        fputs("Estimated print time: ", gpx->log);
        if(hours) fprintf(gpx->log, "%lu hours ", hours);
        if(minutes) fprintf(gpx->log, "%lu minutes ", minutes);
        fprintf(gpx->log, "%lu seconds" EOL, seconds);
        fprintf(gpx->log, "X3G output filesize: %lu bytes" EOL, gpx->accumulated.bytes);
    }
}

// EEPROM

static int write_eeprom_8(Gpx *gpx, Sio *sio, unsigned address, unsigned char value)
{
    int rval;
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    write_8(gpx, value);
    CALL( write_eeprom(gpx, address, sio->response.eeprom.buffer, 1) );
    return SUCCESS;
}

static int read_eeprom_8(Gpx *gpx, Sio *sio, unsigned address, unsigned char *value)
{
    int rval;
    CALL( read_eeprom(gpx, address, 1) );
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    *value = read_8(gpx);
    return SUCCESS;
}

static int write_eeprom_32(Gpx *gpx, Sio *sio, unsigned address, unsigned value)
{
    int rval;
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    write_32(gpx, value);
    CALL( write_eeprom(gpx, address, sio->response.eeprom.buffer, 4) );
    return SUCCESS;
}

static int read_eeprom_32(Gpx *gpx, Sio *sio, unsigned address, unsigned *value)
{
    int rval;
    CALL( read_eeprom(gpx, address, 4) );
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    *value = read_32(gpx);
    return SUCCESS;
}

static int write_eeprom_float(Gpx *gpx, Sio *sio, unsigned address, float value)
{
    int rval;
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    write_float(gpx, value);
    CALL( write_eeprom(gpx, address, sio->response.eeprom.buffer, 4) );
    return SUCCESS;
}

static int read_eeprom_float(Gpx *gpx, Sio *sio, unsigned address, float *value)
{
    int rval;
    CALL( read_eeprom(gpx, address, 4) );
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    *value = read_float(gpx);
    return SUCCESS;
}

static int eeprom_set_property(Gpx *gpx, const char* section, const char* property, char* value)
{
    int rval;
    unsigned int address = (unsigned int)strtol(property, NULL, 0);
    if(SECTION_IS("byte")) {
        unsigned char b = (unsigned char)strtol(value, NULL, 0);
        CALL( write_eeprom_8(gpx, (Sio *)gpx->callbackData, address, b) );
    }
    else if(SECTION_IS("integer")) {
        unsigned int i = (unsigned int)strtol(value, NULL, 0);
        CALL( write_eeprom_32(gpx, (Sio *)gpx->callbackData, address, i) );
    }
    else if(SECTION_IS("hex") || SECTION_IS("hexadecimal")) {
        unsigned int h = (unsigned int)strtol(value, NULL, 16);
        unsigned length = (unsigned)strlen(value) / 2;
        if(length > 4) length = 4;
        gpx->buffer.ptr = ((Sio *)gpx->callbackData)->response.eeprom.buffer;
        write_32(gpx, h);
        CALL( write_eeprom(gpx, address, ((Sio *)gpx->callbackData)->response.eeprom.buffer, length) );
    }
    else if(SECTION_IS("float")) {
        float f = strtof(value, NULL);
        CALL( write_eeprom_float(gpx, (Sio *)gpx->callbackData, address, f) );
    }
    else if(SECTION_IS("string")) {
        unsigned length = (unsigned)strlen(value);
        CALL( write_eeprom(gpx, address, value, length) );
    }
    else {
        SHOW( fprintf(gpx->log, "(line %u) Configuration error: unrecognised section [%s]" EOL, gpx->lineNumber, section) );
        return gpx->lineNumber;
    }
    return SUCCESS;
}

int eeprom_load_config(Gpx *gpx, const char *filename)
{
    return ini_parse(gpx, filename, eeprom_set_property);
}

