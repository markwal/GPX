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
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

#include <libgen.h>

#include "portable_endian.h"
#include "gpx.h"

#define A 0
#define B 1

#define VERBOSESIO(FN) if(gpx->flag.verboseSioMode) {FN;}
#define CALL(FN) if((rval = FN) != SUCCESS) return rval

// Machine definitions
#ifndef MACHINE_ARRAY
#define MACHINE_ARRAY
#endif
#define MACHINE_ALIAS_ARRAY

#include "std_machines.h"
#include "classic_machines.h"
#include "std_eeprommaps.h"

#undef MACHINE_ARRAY

void short_sleep(long nsec)
{
#ifdef HAVE_NANOSLEEP
    // some old mingw32 compiler, in particular the cross compiler still in use
    // on MacOS, lack nanosleep
    struct timespec ts = {0, nsec};
    nanosleep(&ts, NULL);
#else
    usleep(nsec / 1000);
#endif
}

void long_sleep(time_t sec)
{
#ifdef HAVE_NANOSLEEP
    struct timespec ts = {sec, 0};
    nanosleep(&ts, NULL);
#else
    usleep(sec * 1000);
#endif
}


// send a result to the result handler or log it if there isn't one
int gcodeResult(Gpx *gpx, const char *fmt, ...)
{
    int result = 0;
    va_list args;

    va_start(args, fmt);
    if(gpx->resultHandler != NULL) {
        result = gpx->resultHandler(gpx, gpx->callbackData, fmt, args);
    }
    else if(gpx->flag.logMessages) {
        result = vfprintf(gpx->log, fmt, args);
    }
    va_end(args);
    return result;
}

static void show_current_pos(Gpx *gpx)
{
    gcodeResult(gpx, "X:%0.2f Y:%0.2f Z:%0.2f A:%0.2f B:%0.2f\n",
        gpx->current.position.x, gpx->current.position.y, gpx->current.position.z,
        gpx->current.position.a, gpx->current.position.b);
}

void gpx_list_machines(FILE *fp)
{
     Machine **ptr = machines;

     while(*ptr) {
         fprintf(fp, "\t%-3s = %s" EOL, (*ptr)->type, (*ptr)->desc);
         ptr++;
     }

     MachineAlias **pma = machine_aliases;
     for (; *pma; pma++) {
         fprintf(fp, "\t%-3s = %s" EOL, (*pma)->alias, (*pma)->desc);
     }
}

#define MACHINE_IS(m) strcasecmp(machine, m) == 0

Machine *gpx_find_machine(const char *machine)
{
    Machine **all_machines[] = {machines, wrong_machines, NULL};
    Machine ***ptr_all;
    Machine **ptr;

    // check the aliases
    MachineAlias **pma;
    for (pma = machine_aliases; *pma; pma++) {
        if(MACHINE_IS((*pma)->alias)) {
            machine = (*pma)->type;
            break;
        }
    }

    // check the machine list
    for (ptr_all = all_machines; *ptr_all != NULL; ptr_all++) {
        for (ptr = *ptr_all; *ptr; ptr++) {
            if(MACHINE_IS((*ptr)->type))
                return *ptr;
        }
    }

    // Machine not found
    return NULL;
}

static int ini_parse(Gpx* gpx, const char* filename,
                     int (*handler)(Gpx*, const char*, const char*, char*));
int gpx_set_property(Gpx *gpx, const char* section, const char* property, char* value);

int gpx_set_machine(Gpx *gpx, const char *machine_type, int init)
{
    VERBOSE( fprintf(gpx->log, "gpx_set_machine: %s" EOL, machine_type) );
    Machine *machine = gpx_find_machine(machine_type);
    if(machine == NULL) {
        return ERROR;
        VERBOSE( fprintf(gpx->log, "gpx_set_machine FAILED to find: %s" EOL, machine_type) );
    }

    // only load/clobber the on-board machine definition if the one specified is differenti
    // or if we're initializing
    if(init || gpx->machine.id != machine->id) {
        memcpy(&gpx->machine, machine, sizeof(Machine));
        VERBOSE( fprintf(gpx->log, "Loading machine definition: %s" EOL, machine->desc) );
        if(gpx->iniPath != NULL) {
            // if there's a gpx->iniPath + "/" + machine_type + ".ini" load it
            // here recursively
            char machineIni[1024];
            machineIni[0] = 0;
            int i = snprintf(machineIni, sizeof(machineIni), "%s/%s.ini", gpx->iniPath, machine_type);
            if(i > 0 && i < sizeof(machineIni)) {
                if(access(machineIni, R_OK) == SUCCESS) {
                    VERBOSE( fprintf(gpx->log, "Using custom machine definition from: %s" EOL, machineIni) );
                    ini_parse(gpx, machineIni, gpx_set_property);
                }
                else if(errno != ENOENT) {
                    VERBOSE( fprintf(gpx->log, "Unable to load custom machine definition errno = %d\n", errno) );
                }
                else {
                    VERBOSE( fprintf(gpx->log, "Unable to access: %s" EOL, machineIni) );
                }
            }
        }
    }
    else {
        VERBOSE( fputs("Ignoring duplicate machine definition: -m ", gpx->log) );
        VERBOSE( fputs(machine_type, gpx->log) );
        VERBOSE( fputs(EOL, gpx->log) );
    }

    // update known position mask
    gpx->axis.mask = gpx->machine.extruder_count == 1 ? (XYZ_BIT_MASK | A_IS_SET) : AXES_BIT_MASK;;
    return SUCCESS;
}

// PRIVATE FUNCTION PROTOTYPES

static double get_home_feedrate(Gpx *gpx, int flag);
static int pause_at_zpos(Gpx *gpx, float z_positon);

// initialization of global variables

// 02 - Get available buffer size
char buffer_size_query[] = {
    0xD5,   // start byte
    1,      // length
    2,      // query command
    0       // crc
};
static unsigned char calculate_crc(unsigned char *addr, long len);

void gpx_initialize(Gpx *gpx, int firstTime)
{
    int i;

    if(!gpx) return;

    // Delay to wait after opening a serial I/O connection
    gpx->open_delay = 2;

    gpx->buffer.ptr = gpx->buffer.out;
    // we default to using pipes

    // initialise machine
    if(firstTime) gpx_set_machine(gpx, "r2", 1);

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
    gpx->current.speed_factor = 100;

    // actually, gpx doesn't know A and B except that at the start of an SD
    // print they're always reset to zero.  The first absolute move already
    // assumed any unspecified extruder was 0.  So while this isn't yet correct
    // it makes the output most like the old GPX prior to fixing issue markwal/GPX#1
    gpx->axis.positionKnown = gpx->machine.extruder_count == 1 ? A_IS_SET : (A_IS_SET|B_IS_SET);
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
        gpx->override[i].extrusion_factor = 100;
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
        gpx->iniPath = NULL;
        gpx->buildName = NULL;
        gpx->selectedFilename = NULL;
	gpx->preamble = NULL;
	gpx->nostart = 0;
	gpx->noend = 0;
        gpx->eepromMappingVector = NULL;
    }

    if(gpx->eepromMappingVector != NULL) {
        free(gpx->eepromMappingVector);
        gpx->eepromMappingVector = NULL;
    }
    gpx->eepromMap = NULL;

    gpx->flag.relativeCoordinates = 0;
    gpx->flag.extruderIsRelative = 0;

    if(firstTime) {
        gpx->flag.reprapFlavor = 1; // reprap flavor is enabled by default
        gpx->flag.dittoPrinting = 0;
        gpx->flag.buildProgress = 0;
        gpx->flag.verboseMode = 0;
        gpx->flag.logMessages = 1; // logging is enabled by default
        gpx->flag.rewrite5D = 0;
        gpx->flag.sioConnected = 0;
        gpx->flag.M106AlwaysValve = 0;
        gpx->flag.onlyExplicitToolChange = 0;
    }

    // STATE

    gpx->flag.programState = 0;
    gpx->flag.doPauseAtZPos = 0;
    gpx->flag.pausePending = 0;
    gpx->flag.macrosEnabled = 0;
    if(firstTime) {
        gpx->flag.loadMacros = 1;
        gpx->flag.runMacros = 1;
        gpx->flag.ignoreAbsoluteMoves = 0;
    }

    if(firstTime)
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
    gpx->resultHandler = NULL;
    gpx->sio = NULL;

    // LOGGING

    if(firstTime) gpx->log = stderr;

    // CANNED COMMANDS
    buffer_size_query[3] = calculate_crc((unsigned char *)buffer_size_query + 2, 1);
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

static void write_16(Gpx *gpx, uint16_t value)
{
    union {
        uint16_t s;
        unsigned char b[2];
    } u;
    u.s = htole16(value);
    *gpx->buffer.ptr++ = u.b[0];
    *gpx->buffer.ptr++ = u.b[1];
}

static uint16_t read_16(Gpx *gpx)
{
    union {
        uint16_t s;
        unsigned char b[2];
    } u;
    u.b[0] = *gpx->buffer.ptr++;
    u.b[1] = *gpx->buffer.ptr++;
    return le16toh(u.s);
}

static void write_32(Gpx *gpx, uint32_t value)
{
    union {
        uint32_t i;
        unsigned char b[4];
    } u;
    u.i = htole32(value);
    *gpx->buffer.ptr++ = u.b[0];
    *gpx->buffer.ptr++ = u.b[1];
    *gpx->buffer.ptr++ = u.b[2];
    *gpx->buffer.ptr++ = u.b[3];
}

static uint32_t read_32(Gpx *gpx)
{
    union {
        uint32_t i;
        unsigned char b[4];
    } u;
    u.b[0] = *gpx->buffer.ptr++;
    u.b[1] = *gpx->buffer.ptr++;
    u.b[2] = *gpx->buffer.ptr++;
    u.b[3] = *gpx->buffer.ptr++;
    return le32toh(u.i);
}

static void write_fixed_16(Gpx *gpx, float value)
{
    unsigned char b = (unsigned char)value;
    *gpx->buffer.ptr++ = b;
    *gpx->buffer.ptr++ = (unsigned char)(int)((value - b)*256.0);
}

static float read_fixed_16(Gpx *gpx)
{
    unsigned char b[2];
    b[0] = *gpx->buffer.ptr++;
    b[1] = *gpx->buffer.ptr++;
    gcodeResult(gpx, "(line %u) read_fixed_16 %u, %u" EOL, gpx->lineNumber, (unsigned)b[0], (unsigned)b[1]);
    return ((float)b[0]) + ((float)b[1])/256.0;
}

static void write_float(Gpx *gpx, float value)
{
    union {
        float f;
        uint32_t i;
        unsigned char b[4];
    } u;
    u.f = value;
    u.i = htole32(u.i);
    *gpx->buffer.ptr++ = u.b[0];
    *gpx->buffer.ptr++ = u.b[1];
    *gpx->buffer.ptr++ = u.b[2];
    *gpx->buffer.ptr++ = u.b[3];
}

static float read_float(Gpx *gpx)
{
    union {
        float f;
        uint32_t i;
        unsigned char b[4];
    } u;
    u.b[0] = *gpx->buffer.ptr++;
    u.b[1] = *gpx->buffer.ptr++;
    u.b[2] = *gpx->buffer.ptr++;
    u.b[3] = *gpx->buffer.ptr++;
    u.i = le32toh(u.i);
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

static long write_string(Gpx *gpx, const char *string, long length)
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
    if(gpx->callbackHandler) {
        return gpx->callbackHandler(gpx, gpx->callbackData, gpx->buffer.out, length);
    }
    return SUCCESS;
}

// no x3g to emit, but the callback might want to look at the parsed command
static int empty_frame(Gpx *gpx)
{
    if(gpx->callbackHandler) {
        return gpx->callbackHandler(gpx, gpx->callbackData, gpx->buffer.out, 0);
    }
    return SUCCESS;
}

// set the build name for start_build

static void set_build_name(Gpx *gpx, char *buildName)
{
    if(gpx->buildName)
        free(gpx->buildName);
    gpx->buildName = NULL;
    if(buildName)
        gpx->buildName = strdup(buildName);
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
    // 7 February 2015
    // The DDA here is the microseconds/step.  So a larger value is slower.
    // We want the largest value, not the smallest value.  (Bug in original GPX)

    // calculate once
    int longestDDA = gpx->longestDDA;
    if(longestDDA == 0) {
        longestDDA = (int)(60 * 1000000.0 / (gpx->machine.x.max_feedrate * gpx->machine.x.steps_per_mm));

        int axisDDA = (int)(60 * 1000000.0 / (gpx->machine.y.max_feedrate * gpx->machine.y.steps_per_mm));
        if(longestDDA > axisDDA) longestDDA = axisDDA;

        axisDDA = (int)(60 * 1000000.0 / (gpx->machine.z.max_feedrate * gpx->machine.z.steps_per_mm));
        if(longestDDA > axisDDA) longestDDA = axisDDA;
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

    double feedrate = gpx->current.feedrate * ((double)gpx->current.speed_factor / 100);
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
    // compute the relative distance traveled along each axis
    if(gpx->command.flag & X_IS_SET) deltaMM.x = gpx->target.position.x - gpx->current.position.x; else deltaMM.x = 0;
    if(gpx->command.flag & Y_IS_SET) deltaMM.y = gpx->target.position.y - gpx->current.position.y; else deltaMM.y = 0;
    if(gpx->command.flag & Z_IS_SET) deltaMM.z = gpx->target.position.z - gpx->current.position.z; else deltaMM.z = 0;
    if(gpx->command.flag & A_IS_SET) deltaMM.a = (gpx->target.position.a - gpx->current.position.a) * gpx->override[A].extrusion_factor / 100; else deltaMM.a = 0;
    if(gpx->command.flag & B_IS_SET) deltaMM.b = (gpx->target.position.b - gpx->current.position.b) * gpx->override[B].extrusion_factor / 100; else deltaMM.b = 0;
    return deltaMM;
}

static Point5d delta_steps(Gpx *gpx,Point5d deltaMM)
{
    Point5d deltaSteps;
    // Convert the relative distance traveled along each axis from units of mm to steps
    if(gpx->command.flag & X_IS_SET) deltaSteps.x = round(fabs(deltaMM.x) * gpx->machine.x.steps_per_mm); else deltaSteps.x = 0;
    if(gpx->command.flag & Y_IS_SET) deltaSteps.y = round(fabs(deltaMM.y) * gpx->machine.y.steps_per_mm); else deltaSteps.y = 0;
    if(gpx->command.flag & Z_IS_SET) deltaSteps.z = round(fabs(deltaMM.z) * gpx->machine.z.steps_per_mm); else deltaSteps.z = 0;
    if(gpx->command.flag & A_IS_SET) deltaSteps.a = round(fabs(deltaMM.a) * gpx->machine.a.steps_per_mm); else deltaSteps.a = 0;
    if(gpx->command.flag & B_IS_SET) deltaSteps.b = round(fabs(deltaMM.b) * gpx->machine.b.steps_per_mm); else deltaSteps.b = 0;
    return deltaSteps;
}

static void set_unknown_axes(Gpx *gpx, int flag)
{
    gpx->axis.positionKnown &= ~(flag & gpx->axis.mask);

    // we don't know where the bot is and most likely 0 is wrong
    // but always setting it at least makes any errors very deterministic
    if(flag & X_IS_SET)
        gpx->current.position.x = 0;
    if(flag & Y_IS_SET)
        gpx->current.position.y = 0;
    if(flag & Z_IS_SET)
        gpx->current.position.z = 0;
    if(flag & A_IS_SET)
        gpx->current.position.a = 0;
    if(flag & B_IS_SET)
        gpx->current.position.b = 0;
}

// X3G QUERIES

#define COMMAND_OFFSET 2
#define EXTRUDER_ID_OFFSET 3
#define QUERY_COMMAND_OFFSET 4
#define EEPROM_LENGTH_OFFSET 5

#ifdef FUTURE
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
#endif // FUTURE

// 03 - Clear buffer (same as 07 and 17)

int clear_buffer(Gpx *gpx)
{
    begin_frame(gpx);
    set_unknown_axes(gpx, gpx->axis.mask);
    gpx->excess.a = 0;
    gpx->excess.b = 0;

    write_8(gpx, 3);

    return end_frame(gpx);
}

// 07 - Abort immediately

int abort_immediately(Gpx *gpx)
{
    begin_frame(gpx);
    set_unknown_axes(gpx, gpx->axis.mask);
    gpx->excess.a = 0;
    gpx->excess.b = 0;

    write_8(gpx, 7);

    return end_frame(gpx);
}

// 08 - Pause/Resume

int pause_resume(Gpx *gpx)
{
    begin_frame(gpx);

    write_8(gpx, 8);

    return end_frame(gpx);
}

// 10 - Extruder Query Commands

#ifdef FUTURE
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
#endif // FUTURE

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

int is_extruder_ready(Gpx *gpx, unsigned extruder_id)
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

int is_build_platform_ready(Gpx *gpx, unsigned extruder_id)
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

#ifdef FUTURE
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
#endif // FUTURE

// 11 - Is ready

int is_ready(Gpx *gpx)
{
    begin_frame(gpx);

    write_8(gpx, 11);

    return end_frame(gpx);
}

// 12 - Read from EEPROM

int read_eeprom(Gpx *gpx, unsigned address, unsigned length)
{
    SHOW( fprintf(gpx->log, "Reading EEPROM address %u length %u\n", address, length) );

    begin_frame(gpx);

    write_8(gpx, 12);

    // uint16: EEPROM memory offset to begin reading from
    write_16(gpx, address);

    // uint8: Number of bytes to read, N.
    write_8(gpx, length);

    return end_frame(gpx);
}

// 13 - Write to EEPROM

int write_eeprom(Gpx *gpx, unsigned address, char *data, unsigned length)
{
    SHOW( fprintf(gpx->log, "Writing EEPROM address %u length %u\n", address, length) );

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

static int select_filename(Gpx *gpx, char *filename)
{
    if(gpx->selectedFilename != NULL) {
        free(gpx->selectedFilename);
        gpx->selectedFilename = NULL;
    }
    gpx->selectedFilename = strdup(filename);
    if(gpx->selectedFilename == NULL)
        return EOSERROR;
    empty_frame(gpx);
    return SUCCESS;
}

#ifdef FUTURE
// 17 - Reset

static int reset(Gpx *gpx)
{
    begin_frame(gpx);
    set_unknown_axes(gpx->axis.mask);
    gpx->excess.a = 0;
    gpx->excess.b = 0;

    write_8(gpx, 17);

    return end_frame(gpx);
}
#endif // FUTURE

// 18 - Get next filename

int get_next_filename(Gpx *gpx, unsigned restart)
{
    begin_frame(gpx);

    write_8(gpx, 18);

    // uint8: 0 if file listing should continue, 1 to restart listing.
    write_8(gpx, restart);

    return end_frame(gpx);
}

#ifdef FUTURE
// 20 - Get build name

static int get_build_name(Gpx *gpx)
{
    begin_frame(gpx);

    write_8(gpx, 20);

    return end_frame(gpx);
}
#endif // FUTURE

// 21 - Get extended position

int get_extended_position(Gpx *gpx)
{
    begin_frame(gpx);

    write_8(gpx, 21);

    return end_frame(gpx);
}

// 22 - Extended stop

int extended_stop(Gpx *gpx, unsigned halt_steppers, unsigned clear_queue)
{
    unsigned flag = 0;
    if(halt_steppers) flag |= 0x1;
    if(clear_queue) flag |= 0x2;
    set_unknown_axes(gpx, gpx->axis.mask);
    gpx->excess.a = 0;
    gpx->excess.b = 0;

    begin_frame(gpx);

    write_8(gpx, 22);

    /* uint8: Bitfield indicating which subsystems to shut down.
              If bit 0 is set, halt all stepper motion.
              If bit 1 is set, clear the command queue. */
    write_8(gpx, flag);

    return end_frame(gpx);
}

// 23 - Get motherboard status

int get_motherboard_status(Gpx *gpx)
{
    begin_frame(gpx);

    write_8(gpx, 23);

    return end_frame(gpx);
}

// 24 - Get build statistics

int get_build_statistics(Gpx *gpx)
{
    begin_frame(gpx);

    write_8(gpx, 24);

    return end_frame(gpx);
}

// 27 - Get advanced version number

int get_advanced_version_number(Gpx *gpx)
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
            gcodeResult(gpx, "(line %u) Semantic warning: X axis homing to %s endstop" EOL, gpx->lineNumber, direction ? "maximum" : "minimum");
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
            gcodeResult(gpx, "(line %u) Semantic warning: Y axis homing to %s endstop" EOL, gpx->lineNumber, direction ? "maximum" : "minimum");
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
            gcodeResult(gpx, "(line %u) Semantic warning: Z axis homing to %s endstop" EOL, gpx->lineNumber, direction ? "maximum" : "minimum");
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

int delay(Gpx *gpx, unsigned milliseconds)
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

int set_nozzle_temperature(Gpx *gpx, unsigned extruder_id, unsigned temperature)
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
    if(gpx->machine.id >= MACHINE_TYPE_REPLICATOR_1) {

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
        gcodeResult(gpx, "(line %u) Semantic warning: ignoring M126/M127 with Gen 4 extruder electronics" EOL, gpx->lineNumber);
    }
    return SUCCESS;
}

// Action 27 - Enable / Disable Automated Build Platform (ABP)
// Note: MBI usurped command code 27 when they introduced the
//       "advanced version" command.  So, this is the OLD
//       X3G command 27.

static int set_abp(Gpx *gpx, unsigned extruder_id, unsigned state)
{
    assert(extruder_id < gpx->machine.extruder_count);
    if(gpx->machine.id < MACHINE_TYPE_REPLICATOR_1) {

        begin_frame(gpx);

        write_8(gpx, 136);

        // uint8: ID of the extruder to query
        write_8(gpx, extruder_id);

        // uint8: Action command to send to the extruder
        write_8(gpx, 27);

        // uint8: Length of the extruder command payload (N)
        write_8(gpx, 1);

        // uint8: 1 to enable, 0 to disable
        write_8(gpx, state);

        return end_frame(gpx);
    }
    else if(gpx->flag.logMessages) {
	 gcodeResult(gpx, "(line %u) Semantic warning: command to toggle the Automated Build Platform's conveyor (ABP); not supported on non-Gen 3 and Gen 4 electronics" EOL, gpx->lineNumber);
    }
    return SUCCESS;
}

// Action 31 - Set build platform target temperature

int set_build_platform_temperature(Gpx *gpx, unsigned extruder_id, unsigned temperature)
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
    double feedrate;
    long longestDDA = gpx->longestDDA ? gpx->longestDDA : get_longest_dda(gpx);
    Point5d steps = mm_to_steps(gpx, &gpx->target.position, &gpx->excess);

    feedrate = gpx->current.feedrate * ((double)gpx->current.speed_factor / 100);

    // 7 February 2015
    // Fix issue #12 whereby an unaccelerated move may move far too fast

    if (feedrate > 0) {
	 // Seconds for the move is steps-needed / steps-per-mm / feedrate
	 // DDA is then 60 * 1000000 * seconds / steps-needed
	 // Thus DDA is 60 * 1000000 * steps-needed / (steps-per-mm * feedrate) / steps-needed
         //      DDA = 60 * 1000000 / (steps-per-mm * feedrate)
	 if (steps.x) {
	      long DDA = (long)(60.0 * 1000000.0 / (gpx->machine.x.steps_per_mm * feedrate));
	      if (DDA > longestDDA) longestDDA = DDA;
	 }
	 if (steps.y) {
	      long DDA = (long)(60.0 * 1000000.0 / (gpx->machine.y.steps_per_mm * feedrate));
	      if (DDA > longestDDA) longestDDA = DDA;
	 }
	 if (steps.z) {
	      long DDA = (long)(60.0 * 1000000.0 / (gpx->machine.z.steps_per_mm * feedrate));
	      if (DDA > longestDDA) longestDDA = DDA;
	 }
	 if (steps.a) {
	      long DDA = (long)(60.0 * 1000000.0 / (gpx->machine.a.steps_per_mm * feedrate));
	      if (DDA > longestDDA) longestDDA = DDA;
	 }
	 if (steps.b) {
	      long DDA = (long)(60.0 * 1000000.0 / (gpx->machine.b.steps_per_mm * feedrate));
	      if (DDA > longestDDA) longestDDA = DDA;
	 }
    }

    // Safety measure: don't send a DDA interval of 0 -- that's telling
    // the bot to step as fast as it possibly can
    if (longestDDA <= 0) longestDDA = 200;

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

int set_position(Gpx *gpx)
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
    // so set zero relative position change

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

    begin_frame(gpx);

    write_8(gpx, 145);

    // uint8: axis value (valid range 0-4) which axis pot to set
    write_8(gpx, axis);

    // uint8: value (valid range 0-255)
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

#ifdef FUTURE
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
#endif // FUTURE

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

int set_build_progress(Gpx *gpx, unsigned percent)
{
    if(percent > 100) percent = 100;
    gpx->current.percent = percent;

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

#ifdef FUTURE
// 152 - Reset to factory defaults

static int factory_defaults(Gpx *gpx)
{
    begin_frame(gpx);

    write_8(gpx, 152);

    // uint8: 0 (reserved for future use)
    write_8(gpx, 0);

    return end_frame(gpx);
}
#endif // FUTURE


// 153 - Build start notification

static int start_build(Gpx *gpx, const char * filename)
{
    size_t len;

    begin_frame(gpx);

    write_8(gpx, 153);

    // uint32: 0 (reserved for future use)
    write_32(gpx, 0);

    // 1+N bytes: Name of the build, in ASCII, null terminated
    // 32 bytes max in a payload
    //  4 bytes used for "reserved"
    //  1 byte used for NUL terminator
    // that leaves 27 bytes for the build name
    // (But the LCD actually has far less room)
    // We'll just truncate at 24
    if(!filename)
	 filename = PACKAGE_STRING;
    len = strlen(filename);
    if(len > 24) len = 24;
    write_string(gpx, filename, len);

    return end_frame(gpx);
}

// 154 - Build end notification

int end_build(Gpx *gpx)
{
    begin_frame(gpx);

    write_8(gpx, 154);

    // uint8: 0 (reserved for future use)
    write_8(gpx, 0);

    return end_frame(gpx);
}

// 155 - Queue extended point x3g

// IMPORTANT: this command updates the parser state

static int queue_ext_point(Gpx *gpx, double feedrate, Ptr5d delta, int relative)
{
    /* If we don't know our previous position on a command axis, we can't calculate the feedrate
       or distance correctly, so we use an unaccelerated command with a fixed DDA. */
    // Unless we're in relative mode in which case we'll issue a relative move
    // Or if one of the unknown axes was not specified by this command in which
    // case we don't know where to tell 139 to go, so we'll use a 0 relative move on
    // those axes here (see markwal/GPX#1)
    unsigned mask = gpx->command.flag & gpx->axis.mask;
    unsigned stillUnknown = (~(gpx->axis.positionKnown | mask)) & gpx->axis.mask;
    if((gpx->axis.positionKnown & mask) != mask && !relative && !stillUnknown) {
        return queue_absolute_point(gpx);
    }

    Point5d deltaMM;
    if (stillUnknown || relative)
        deltaMM = *delta;
    else
        deltaMM = delta_mm(gpx);

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
                deltaMM.a *= ((double)gpx->override[A].extrusion_factor / 100);
                if(gpx->axis.positionKnown & A_IS_SET)
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
                deltaMM.b *= ((double)gpx->override[B].extrusion_factor / 100);
                if(gpx->axis.positionKnown & B_IS_SET)
                    gpx->target.position.b = gpx->current.position.b + deltaMM.b;
                deltaSteps.b = round(fabs(deltaMM.b) * gpx->machine.b.steps_per_mm);
            }
        }

        Point5d target = relative ? *delta : gpx->target.position;

        if (stillUnknown) {
            if (stillUnknown & X_IS_SET)
                target.x = 0;
            if (stillUnknown & Y_IS_SET)
                target.y = 0;
            if (stillUnknown & Z_IS_SET)
                target.z = 0;
        }

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

	// Total time required for the motion in units of microseconds
        double usec = (60000000.0L * minutes);

	// Time interval between steps along the axis with the highest step count
	//   total-time / highest-step-count
	// Has units of microseconds per step
        double dda_interval = usec / largest_axis(gpx->command.flag, &deltaSteps);

        // Convert dda_interval into dda_rate (dda steps per second on the longest axis)
	// steps-per-microsecond * 1000000 us/s = 1000000 * (1 / dda_interval)
        double dda_rate = 1000000.0L / dda_interval;

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
        write_8(gpx, relative ? AXES_BIT_MASK : stillUnknown|A_IS_SET|B_IS_SET);

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

#ifdef FUTURE
// 157 - Stream Version

static int stream_version(Gpx *gpx)
{
    if(gpx->machine.id >= MACHINE_TYPE_REPLICATOR_1) {
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
        if(gpx->machine.id >= MACHINE_TYPE_REPLICATOR_2) {
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
#endif // FUTURE

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
            gcodeResult(gpx, "(line %u) Buffer overflow: too many @filament definitions (maximum = %i)" EOL, gpx->lineNumber, FILAMENT_MAX - 1);
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
        gcodeResult(gpx, "(line %u) Semantic error: @pause macro with undefined filament name '%s', use a @filament macro to define it" EOL, gpx->lineNumber, filament_id);
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
                VERBOSE( gcodeResult(gpx, "Inserted index=%d ", i) );
            }
            // append command
            else {
                gpx->commandAt[i].z = z;
                gpx->commandAt[i].filament_index = index;
                gpx->commandAt[i].nozzle_temperature = nozzle_temperature;
                gpx->commandAt[i].build_platform_temperature = build_platform_temperature;
                gpx->commandAtZ = z;
                VERBOSE( gcodeResult(gpx, "Appended index=%d ", i) );
            }
            if(gpx->flag.verboseMode && gpx->flag.logMessages) {
                gcodeResult(gpx, "Command @ %0.2lf: ", z);
                if(nozzle_temperature == 0 && build_platform_temperature == 0)
                    gcodeResult(gpx, "Pause\n");
                else
                    gcodeResult(gpx, "Set temperature; nozzle=%u, bed=%u\n", nozzle_temperature, build_platform_temperature);
            }
            // nonzero temperature signals a temperature change, not a pause @ zPos
            // so if its the first pause @ zPos que it up
            if(nozzle_temperature == 0 && build_platform_temperature == 0 && gpx->commandAtLength == 0) {
                if(gpx->flag.macrosEnabled) {
                    CALL( pause_at_zpos(gpx, z) );
                    VERBOSE( gcodeResult(gpx, "Sent pause @ %0.2lf\n", z) );
                }
                else {
                    gpx->flag.pausePending = 1;
                }
            }
            gpx->commandAtLength++;
        }
    }
    else {
        gcodeResult(gpx, "(line %u) Buffer overflow: too many @pause definitions (maximum = %i)" EOL, gpx->lineNumber, COMMAND_AT_MAX);
    }
    return SUCCESS;
}

// EEPROM MACRO FUNCTIONS

int write_eeprom_8(Gpx *gpx, Sio *sio, unsigned address, unsigned char value)
{
    int rval;
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    write_8(gpx, value);
    CALL( write_eeprom(gpx, address, sio->response.eeprom.buffer, 1) );
    return SUCCESS;
}

int read_eeprom_8(Gpx *gpx, Sio *sio, unsigned address, unsigned char *value)
{
    int rval;
    CALL( read_eeprom(gpx, address, 2) );
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    *value = read_8(gpx);
    return SUCCESS;
}

int write_eeprom_16(Gpx *gpx, Sio *sio, unsigned address, unsigned short value)
{
    int rval;
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    write_16(gpx, value);
    CALL( write_eeprom(gpx, address, sio->response.eeprom.buffer, 2) );
    return SUCCESS;
}

int read_eeprom_16(Gpx *gpx, Sio *sio, unsigned address, unsigned short *value)
{
    int rval;
    CALL( read_eeprom(gpx, address, 2) );
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    *value = read_16(gpx);
    return SUCCESS;
}

int write_eeprom_fixed_16(Gpx *gpx, Sio *sio, unsigned address, float value)
{
    int rval;
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    write_fixed_16(gpx, value);
    CALL( write_eeprom(gpx, address, sio->response.eeprom.buffer, 2) );
    return SUCCESS;
}

int read_eeprom_fixed_16(Gpx *gpx, Sio *sio, unsigned address, float *value)
{
    int rval;
    CALL( read_eeprom(gpx, address, 2) );
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    *value = read_fixed_16(gpx);
    return SUCCESS;
}

int write_eeprom_32(Gpx *gpx, Sio *sio, unsigned address, unsigned long value)
{
    int rval;
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    write_32(gpx, value);
    CALL( write_eeprom(gpx, address, sio->response.eeprom.buffer, 4) );
    return SUCCESS;
}

int read_eeprom_32(Gpx *gpx, Sio *sio, unsigned address, unsigned long *value)
{
    int rval;
    CALL( read_eeprom(gpx, address, 4) );
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    *value = read_32(gpx);
    return SUCCESS;
}

int write_eeprom_float(Gpx *gpx, Sio *sio, unsigned address, float value)
{
    int rval;
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    write_float(gpx, value);
    CALL( write_eeprom(gpx, address, sio->response.eeprom.buffer, 4) );
    return SUCCESS;
}

int read_eeprom_float(Gpx *gpx, Sio *sio, unsigned address, float *value)
{
    int rval;
    CALL( read_eeprom(gpx, address, 4) );
    gpx->buffer.ptr = sio->response.eeprom.buffer;
    *value = read_float(gpx);
    return SUCCESS;
}

const char *get_firmware_variant(unsigned int variant_id)
{
    const char *variant = "Unknown";
    switch(variant_id) {
        case 0x01:
            variant = "Makerbot";
            break;
        case 0x80:
            variant = "Sailfish";
            break;
    }
    return variant;
}

// find a built-in eeprom map based on the firmware variant and version
EepromMap *find_eeprom_map(Gpx *gpx)
{
    int rval = get_advanced_version_number(gpx);
    if(rval != SUCCESS) {
        gcodeResult(gpx, "(line %u) Unable to load eeprom map: bot didn't reply to version number query: %d.\n", gpx->lineNumber, rval);
        return NULL;
    }

    int i;
    EepromMap *pem = eepromMaps;
    for(i = 0; i < eepromMapCount; i++, pem++) {
        if(gpx->sio->response.firmware.variant == pem->variant &&
                gpx->sio->response.firmware.version >= pem->versionMin &&
                gpx->sio->response.firmware.version <= pem->versionMax) {
            return pem;
        }
    }
    return NULL;
}

// load a built-in eeprom map based on the firmware variant and version
int load_eeprom_map(Gpx *gpx)
{
    if(!gpx->flag.sioConnected || gpx->sio == NULL) {
        gcodeResult(gpx, "(line %u) Serial not connected: can't detect which eeprom map without asking the bot" EOL, gpx->lineNumber);
        return ERROR;
    }

    EepromMap *pem = find_eeprom_map(gpx);
    if(pem != NULL) {
        gpx->eepromMap = pem;
        gcodeResult(gpx, "EEPROM map loaded for firmware %s version %d.\n", get_firmware_variant(pem->variant), gpx->sio->response.firmware.version);
        return SUCCESS;
    }

    gcodeResult(gpx, "(line %u) Unable to find a matching eeprom map for firmware %s version = %u\n",
            gpx->lineNumber, get_firmware_variant(gpx->sio->response.firmware.variant),
            gpx->sio->response.firmware.version);
    return ERROR;
}

static int find_in_eeprom_map(EepromMap *map, char *name)
{
    EepromMapping *pem = map->eepromMappings;
    int iem;
    for(iem = 0; iem < map->eepromMappingCount; iem++, pem++) {
        if(strcmp(name, pem->id) == 0)
            return iem;
    }
    return -1;
}

// find an eeprom mapping entry from the builtin mapping table
static int find_builtin_eeprom_mapping(Gpx *gpx, char *name)
{
    if(gpx->eepromMap == NULL)
        return -1;

    return find_in_eeprom_map(gpx->eepromMap, name);
}

// find an existing EEPROM mapping
static int find_eeprom_mapping(Gpx *gpx, char *name)
{
    if(gpx->eepromMappingVector == NULL)
        return -1;

    int iem;
    EepromMapping *pem = vector_get(gpx->eepromMappingVector, 0);
    for(iem = 0; iem < gpx->eepromMappingVector->c; iem++, pem++) {
        if(strcmp(name, pem->id) == 0)
            return iem;
    }
    return -1;
}

static void init_eeprom_mapping(EepromMapping *pem)
{
    memset(pem, 0, sizeof(*pem));
}

// add an eeprom mapping
static int add_eeprom_mapping(Gpx *gpx, char *name, EepromType et, unsigned address, int len)
{
    if(gpx->eepromMappingVector == NULL) {
        gpx->eepromMappingVector = vector_create(sizeof(EepromMapping), 10, 10);
        if(gpx->eepromMappingVector == NULL)
            return -1;
    }

    int iem = find_eeprom_mapping(gpx, name);
    if(iem >= 0) {
        // update the existing
        EepromMapping *pem = vector_get(gpx->eepromMappingVector, iem);
        pem->address = address;
        pem->et = et;
        pem->len = len;
        return iem;
    }

    EepromMapping em;
    init_eeprom_mapping(&em);
    em.id = strdup(name);
    if(em.id == NULL)
        return -1;
    em.address = address;
    em.et = et;
    em.len = len;

    return vector_append(gpx->eepromMappingVector, &em);
}

EepromMapping *find_any_eeprom_mapping(Gpx *gpx, char *name)
{
    if(!gpx->flag.sioConnected || gpx->sio == NULL) {
        gcodeResult(gpx, "(line %u) Error: eeprom operation without serial connection\n", gpx->lineNumber);
        return NULL;
    }

    EepromMapping *pem = NULL;
    int iem = find_eeprom_mapping(gpx, name);
    if(iem >= 0) {
        pem = vector_get(gpx->eepromMappingVector, iem);
    }
    else if((iem = find_builtin_eeprom_mapping(gpx, name)) >= 0) {
        if(gpx->eepromMap == NULL) {
            gcodeResult(gpx, "(line %u) Unexpected error: find_builtin_eeprom_mapping returned an invalid index %d.\n", gpx->lineNumber, iem);
            return NULL;
        }
        pem = &gpx->eepromMap->eepromMappings[iem];
    }
    if(pem == NULL) {
        gcodeResult(gpx, "(line %u) Error: eeprom mapping '%s' not defined\n", gpx->lineNumber, name);
    }
    return pem;
}

// read an eeprom value from a defined mapping
static int read_eeprom_name(Gpx *gpx, char *name)
{
    int rval = SUCCESS;

    EepromMapping *pem = find_any_eeprom_mapping(gpx, name);
    if(pem == NULL)
        return ERROR;

    const char *unit = pem->unit != NULL ? pem->unit : "";
    switch(pem->et) {
        case et_bitfield:
        case et_boolean:
        case et_byte: {
            unsigned char b;
            CALL( read_eeprom_8(gpx, gpx->sio, pem->address, &b) );
            gcodeResult(gpx, "EEPROM byte %s @ 0x%x is %u %s (0x%x)\n", pem->id, pem->address, (unsigned)b, unit, (unsigned)b);
            break;
        }

        case et_ushort: {
            unsigned short us;
            CALL( read_eeprom_16(gpx, gpx->sio, pem->address, &us) );
            gcodeResult(gpx, "EEPROM value %s @ 0x%x is %u %s (0x%x)\n", pem->id, pem->address, us, unit, us);
            break;
        }

        case et_fixed: {
            float n;
            CALL( read_eeprom_fixed_16(gpx, gpx->sio, pem->address, &n) );
            gcodeResult(gpx, "EEPROM float %s @ 0x%x is %g %s\n", pem->id, pem->address, n, unit);
            break;
        }

        case et_long:
        case et_ulong: {
            unsigned long ul;
            CALL( read_eeprom_32(gpx, gpx->sio, pem->address, &ul) );
            if(pem->et == et_long) {
                gcodeResult(gpx, "EEPROM value %s @ 0x%x is %d %s (0x%lx)\n", pem->id, pem->address, ul, unit, ul);
            }
            else {
                gcodeResult(gpx, "EEPROM value %s @ 0x%x is %u %s (0x%lx)\n", pem->id, pem->address, ul, unit, ul);
            }
            break;
        }

        case et_float: {
            float n;
            CALL( read_eeprom_float(gpx, gpx->sio, pem->address, &n) );
            gcodeResult(gpx, "EEPROM float %s @ 0x%x is %g %s\n", pem->id, pem->address, unit, n);
            break;
        }

        case et_string:
            memset(gpx->sio->response.eeprom.buffer, 0, sizeof(gpx->sio->response.eeprom.buffer));
            int len = pem->len;
            if(len > sizeof(gpx->sio->response.eeprom.buffer))
                len = sizeof(gpx->sio->response.eeprom.buffer);
            CALL( read_eeprom(gpx, pem->address, len) );
            gcodeResult(gpx, "EEPROM string %s @ 0x%x is %s\n", pem->id, pem->address, gpx->sio->response.eeprom.buffer);
            break;

        default:
            gcodeResult(gpx, "(line %u) Error: type of %s not supported by @eread\n", gpx->lineNumber, pem->id);
            break;
    }
    return SUCCESS;
}

// write an eeprom value via a defined mapping
static int write_eeprom_name(Gpx *gpx, char *name, char *string_value, unsigned long hex, float value)
{
    int rval = SUCCESS;

    EepromMapping *pem = find_any_eeprom_mapping(gpx, name);
    if(pem == NULL)
        return ERROR;

    if((string_value != NULL) + (hex != 0) + (value != 0.0) > 1) {
        gcodeResult(gpx, "(line %u) Error: only one value expected for @ewrite macro\n", gpx->lineNumber);
        return ERROR;
    }
    if(pem->et != et_string) {
        if(string_value != NULL) {
            gcodeResult(gpx, "(line %u) Error: string value unexpected for eeprom setting %s\n", gpx->lineNumber, pem->id);
            return ERROR;
        }

        if(value != 0.0)
            hex = (unsigned long)value;

    }

    switch(pem->et) {
        case et_bitfield:
        case et_boolean:
        case et_byte: {
            if(hex > 255) {
                gcodeResult(gpx, "(line %u) Error: parameter out of range for eeprom setting %s\n", gpx->lineNumber, pem->id);
                return ERROR;
            }
            unsigned char b = (unsigned char)hex;
            CALL( write_eeprom_8(gpx, gpx->sio, pem->address, b) );
            gcodeResult(gpx, "EEPROM wrote 8-bits, %u to address 0x%x\n", (unsigned)b, pem->address);
            break;
        }

        case et_ushort: {
            if(hex > 65535) {
                gcodeResult(gpx, "(line %u) Error: parameter out of range for eeprom setting %s\n", gpx->lineNumber, pem->id);
                return ERROR;
            }
            unsigned short us = (unsigned short)hex;
            CALL( write_eeprom_16(gpx, gpx->sio, pem->address, us) );
            gcodeResult(gpx, "EEPROM wrote 16-bits, %u to address 0x%x\n", us, pem->address);
            break;
        }

        case et_fixed: {
            if(value == 0.0)
                value = (float)hex;
            CALL( write_eeprom_fixed_16(gpx, gpx->sio, pem->address, value) );
            gcodeResult(gpx, "EEPROM wrote fixed point 16-bits, %f to address 0x%x\n", value, pem->address);
            break;
        }

        case et_long:
        case et_ulong:
            CALL( write_eeprom_32(gpx, gpx->sio, pem->address, hex) );
            gcodeResult(gpx, "EEPROM wrote 32-bits, %lu to address 0x%x\n", hex, pem->address);
            break;

        case et_float:
            gcodeResult(gpx, "(line %u) Error: writing float type to eeprom not yet supported.\n", gpx->lineNumber);
            break;

        case et_string:
            if(pem->len <= 0) {
                gcodeResult(gpx, "(line %u) Error: can't write a string to zero length eeprom mapping\n", gpx->lineNumber);
                break;
            }
            if(strlen(string_value) >= pem->len)
                string_value[pem->len - 1] = 0;
            CALL( write_eeprom(gpx, pem->address, string_value, strlen(string_value) + 1) );
            gcodeResult(gpx, "EEPROM wrote %d bytes to address 0x%x\n", strlen(string_value) + 1, pem->address);
            break;

        default:
            gcodeResult(gpx, "(line %u) Error: type of %s not supported by @ewrite\n", gpx->lineNumber, pem->id);
            break;
    }
    return SUCCESS;
}

// DISPLAY TAG

static int display_tag(Gpx *gpx) {
    int rval;
    CALL( display_message(gpx, PACKAGE_STRING, 0, 0, 2, 0) );
    return SUCCESS;
}

// TARGET POSITION

// calculate target position

static int calculate_target_position(Gpx *gpx, Ptr5d delta, int *relative)
{
    int rval;
    // G10 ofset
    Point3d userOffset = gpx->offset[gpx->current.offset];
    double userScale = 1.0;

    // we try to calculate a new absolute target even if we're in relative mode
    // only resort to relative x,y,z if we don't know where we are.
    delta->x = delta->y = delta->z = delta->a = delta->b = 0;
    *relative = 0;

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
    gpx->target.position.x = gpx->current.position.x;
    if(gpx->command.flag & X_IS_SET) {
        if(gpx->flag.relativeCoordinates) {
            delta->x = gpx->command.x * userScale;
            gpx->target.position.x += delta->x;
            *relative |= !!(gpx->axis.positionKnown & X_IS_SET);
        }
        else {
            gpx->target.position.x = (gpx->command.x + userOffset.x) * userScale;
            delta->x = gpx->target.position.x - gpx->current.position.x;
        }
    }

    // y
    gpx->target.position.y = gpx->current.position.y;
    if(gpx->command.flag & Y_IS_SET) {
        if(gpx->flag.relativeCoordinates) {
            delta->y = gpx->command.y * userScale;
            gpx->target.position.y += delta->y;
            *relative |= !!(gpx->axis.positionKnown & Y_IS_SET);
        }
        else {
            gpx->target.position.y = (gpx->command.y + userOffset.y) * userScale;
            delta->y = gpx->target.position.y - gpx->current.position.y;
        }
    }

    // z
    gpx->target.position.z = gpx->current.position.z;
    if(gpx->command.flag & Z_IS_SET) {
        if(gpx->flag.relativeCoordinates) {
            delta->z = gpx->command.z * userScale;
            gpx->target.position.z += delta->z;
            *relative |= !!(gpx->axis.positionKnown & Z_IS_SET);
        }
        else {
            gpx->target.position.z = (gpx->command.z + userOffset.z) * userScale;
            delta->z = gpx->target.position.z - gpx->current.position.z;
        }
    }

    // a
    gpx->target.position.a = gpx->current.position.a;
    if(gpx->command.flag & A_IS_SET) {
        double a = (gpx->override[A].filament_scale == 1.0) ? gpx->command.a : (gpx->command.a * gpx->override[A].filament_scale);
        if(gpx->flag.relativeCoordinates || gpx->flag.extruderIsRelative) {
            delta->a = a;
            gpx->target.position.a += a;
        }
        else {
            gpx->target.position.a = a;
            delta->a = gpx->target.position.a - gpx->current.position.a;
        }
    }

    // b
    gpx->target.position.b = gpx->current.position.b;
    if(gpx->command.flag & B_IS_SET) {
        double b = (gpx->override[B].filament_scale == 1.0) ? gpx->command.b : (gpx->command.b * gpx->override[B].filament_scale);
        if(gpx->flag.relativeCoordinates || gpx->flag.extruderIsRelative) {
            delta->b = b;
            gpx->target.position.b += b;
        }
        else {
            gpx->target.position.b = b;
            delta->b = gpx->target.position.b - gpx->current.position.b;
        }
    }

    // update current feedrate
    if(gpx->command.flag & F_IS_SET) {
        gpx->current.feedrate = gpx->command.f;
    }

    // DITTO PRINTING

    if(gpx->flag.dittoPrinting) {
        if(gpx->command.flag & A_IS_SET) {
            delta->b = delta->a;
            if(gpx->axis.positionKnown & A_IS_SET) {
                gpx->target.position.b = gpx->target.position.a;
                gpx->axis.positionKnown |= A_IS_SET;
            }
            gpx->command.flag |= B_IS_SET;
        }
        else if(gpx->command.flag & B_IS_SET) {
            delta->a = delta->b;
            if(gpx->axis.positionKnown & B_IS_SET) {
                gpx->target.position.a = gpx->target.position.b;
                gpx->axis.positionKnown |= A_IS_SET;
            }
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
    if(!gpx->flag.relativeCoordinates) gpx->axis.positionKnown |= (gpx->command.flag & gpx->axis.mask);
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

    // MBI's firmware and Sailfish 7.7 and earlier effect a tool change
    // by adding the tool offset to the next move command.  That has two
    // undesirable effects:
    //
    //  1. It changes the slope in the XY plane of the move, and
    //  2. Since the firmwares do not recompute the distance, the acceleration
    //       behavior is wrong.
    //
    // Item 2 is particularly foul in some instances causing thumps,
    // lurches, or other odd behaviors as things move at the wrong speed
    // (either too fast or too slow).
    //
    // Chow Loong Jin's simple solution solves both of these by queuing
    // an *unaccelerated* move to the current position.  Since it is
    // unaccelerated, there's no need for proper distance calcs and the
    // firmware's failure to re-calc that info has no impact.  And since
    // the motion is to the current position all that occurs is a simple
    // travel move that does the tool offset.  The next useful motion
    // command does not have its slope perturbed and will occur with the
    // proper acceleration profile.
    //
    // Only gotcha here is that we may only do this when the position is
    // well defined.  For example, we cannot do this for a tool change
    // immediately after a 'recall home offsets' command.

    if(gpx->axis.mask == (gpx->axis.positionKnown & gpx->axis.mask)) {
        gpx->target.position = gpx->current.position;
        VERBOSE( gcodeResult(gpx, "(line %u) queuing an absolute point to ", gpx->lineNumber) );
        VERBOSE( show_current_pos(gpx) );
        CALL( queue_absolute_point(gpx) );
    }

    // set current extruder so changes in E are expressed as changes to A or B
    gpx->current.extruder = gpx->target.extruder;

    return SUCCESS;
}

// FUTURE allow long names for types? like 'bitfield'
EepromType eepromTypeFromTypeName(char *type_name)
{
    if(strlen(type_name) != 1)
        return et_null;
    switch(type_name[0]) {
        case 'B': return et_byte;
        case 'H': return et_ushort;
        case 'i': return et_long;
        case 'I': return et_ulong;
        case 'f': return et_fixed;
        case 's': return et_string;
    }
    return et_null;
}

// PARSER PRE-PROCESSOR

#ifdef FUTURE
// return the length of the given file in bytes

static long get_filesize(FILE *file)
{
    long filesize = -1;
    fseek(file, 0L, SEEK_END);
    filesize = ftell(file);
    fseek(file, 0L, SEEK_SET);
    return filesize;
}
#endif // FUTURE

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
 COMMAND:= PRINTER | ENABLE | FILAMENT | EXTRUDER | SLICER | START | PAUSE | FLAVOR | BUILD
 COMMENT:= S+ '(' [^)]* ')' S+
 PRINTER:= ('printer' | 'machine' | 'slicer') (TYPE | PACKING_DENSITY | DIAMETER | TEMP | RGB )+
 TYPE:=  S+ ('c3' | 'c4' | 'cp4' | 'cpp' | 'cxy' | 'cxysz' | 't6' | 't7' | 't7d' | 'r1' | 'r1d' | 'r2' | 'r2h' | 'r2x' | 'z' | 'zd' )
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
 BUILD:= 'build' BUILD_NAME
 BUILD_NAME:= S+ ALPHA ALPHA_NUMERIC*
 FLAVOR:= 'flavor' GCODE_FLAVOR
 GCODE_FLAVOR:= S+ ('makerbot' | 'reprap')
 */

#define MACRO_IS(token) (strcasecmp(token, macro) == 0)
#define NAME_IS(n) (strcasecmp(name, n) == 0)

static int parse_macro(Gpx *gpx, const char* macro, char *p)
{
    int rval;
    char *name = NULL;
    char *string_param = NULL;
    double z = 0.0;
    double diameter = 0.0;
    unsigned nozzle_temperature = 0;
    unsigned build_platform_temperature = 0;
    unsigned LED = 0;

    while(*p != 0) {
        // trim any leading white space
        while(isspace(*p)) p++;
        if(isalpha(*p)) {
            if(name == NULL || (!MACRO_IS("eeprom") && !MACRO_IS("ewrite")))
                name = p;
            else
                string_param = p;
            while(*p && (isalnum(*p) || *p == '_')) p++;
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
        else if(*p == '"') {
            string_param = ++p;
            while(*p && *p != '"') p++;
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
            gcodeResult(gpx, "(line %u) Syntax error: unrecognised macro parameter" EOL, gpx->lineNumber);
            break;
        }
    }
    // ;@printer <TYPE> <PACKING_DENSITY> <DIAMETER>mm <HBP-TEMP>c #<LED-COLOUR>
    if(MACRO_IS("machine") || MACRO_IS("printer") || MACRO_IS("slicer")) {
        if(name) {
            if(gpx_set_machine(gpx, name, 0)) {
                gcodeResult(gpx, "(line %u) Semantic error: @%s macro with unrecognised type '%s'" EOL, gpx->lineNumber, macro, name);
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
                gcodeResult(gpx, "(line %u) Semantic warning: @%s macro cannot override non-existant heated build platform" EOL, gpx->lineNumber, macro);
            }
        }
        if(LED) {
            CALL( set_LED_RGB(gpx, LED, 0) );
        }
    }
    // ;@enable ditto
    // ;@enable progress
    // ;@enable explicit_tool_change
    else if(MACRO_IS("enable")) {
        if(name) {
            if(NAME_IS("ditto")) {
                if(gpx->machine.extruder_count == 1) {
                    gcodeResult(gpx, "(line %u) Semantic warning: ditto printing cannot access non-existant second extruder" EOL, gpx->lineNumber);
                    gpx->flag.dittoPrinting = 0;
                }
                else {
                    gpx->flag.dittoPrinting = 1;
                }
            }
            else if(NAME_IS("progress")) gpx->flag.buildProgress = 1;
            else if(NAME_IS("explicit_tool_change")) gpx->flag.onlyExplicitToolChange = 1;
            else {
                gcodeResult(gpx, "(line %u) Semantic error: @enable macro with unrecognised parameter '%s'" EOL, gpx->lineNumber, name);
            }
        }
        else {
            gcodeResult(gpx, "(line %u) Syntax error: @enable macro with missing parameter" EOL, gpx->lineNumber);
        }
    }
    // ;@disable ditto
    // ;@disable progress
    // ;@disable explicit_tool_change
    else if(MACRO_IS("disable")) {
        if(name) {
            if(NAME_IS("ditto")) gpx->flag.dittoPrinting = 0;
            else if(NAME_IS("progress")) gpx->flag.buildProgress = 0;
            else if(NAME_IS("explicit_tool_change")) gpx->flag.onlyExplicitToolChange = 0;
            else {
                gcodeResult(gpx, "(line %u) Semantic error: @disable macro with unrecognised parameter '%s'" EOL, gpx->lineNumber, name);
            }
        }
        else {
            gcodeResult(gpx, "(line %u) Syntax error: @disable macro with missing parameter" EOL, gpx->lineNumber);
        }
    }
    // ;@filament <NAME> <DIAMETER>mm <TEMP>c #<LED-COLOUR>
    else if(MACRO_IS("filament")) {
        if(name) {
            add_filament(gpx, name, diameter, nozzle_temperature, LED);
        }
        else {
            gcodeResult(gpx, "(line %u) Semantic error: @filament macro with missing name" EOL, gpx->lineNumber);
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
            gcodeResult(gpx, "(line %u) Semantic error: @pause macro with missing zPos" EOL, gpx->lineNumber);
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
                gcodeResult(gpx, "(line %u) Semantic error: @%s macro with missing zPos" EOL, gpx->lineNumber, macro);
            }
        }
        else {
            gcodeResult(gpx, "(line %u) Semantic error: @%s macro with missing temperature" EOL, gpx->lineNumber, macro);
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
                gcodeResult(gpx, "(line %u) Semantic error: @start with undefined filament name '%s', use a @filament macro to define it" EOL, gpx->lineNumber, name ? name : "");
            }
        }
    }
    // ;@build <NAME>
    else if(MACRO_IS("build")) {
        set_build_name(gpx, string_param ? string_param : name);
    }
    // ;@flavor <FLAVOR>
    else if(MACRO_IS("flavor")) {
        if(NAME_IS("reprap")) gpx->flag.reprapFlavor = 1;
        else if(NAME_IS("makerbot")) gpx->flag.reprapFlavor = 0;
        else {
            gcodeResult(gpx, "(line %u) Macro error: unrecognised GCODE flavor '%s'" EOL, gpx->lineNumber, name);
        }
    }
    // ;@body
    else if(MACRO_IS("body")) {
        if(gpx->flag.pausePending && gpx->flag.runMacros) {
            CALL( pause_at_zpos(gpx, gpx->commandAt[0].z) );
            gpx->flag.pausePending = 0;
            VERBOSE( gcodeResult(gpx, "Issued next pause @ %0.2lf\n", z) );
        }
        gpx->flag.macrosEnabled = 1;
    }
    // ;@clear_cancel
    else if(MACRO_IS("clear_cancel")) {
        // allow caller to sync driver with gcode stream
        gcodeResult(gpx, "@clear_cancel");
    }
    // ;@header
    // ;@footer
    else if(MACRO_IS("header") && MACRO_IS("footer")) {
        // REVIEW can't be both header and footer at the same time, so I don't
        // think this can ever be executed, what was it supposed to be for?
        gpx->flag.macrosEnabled = 0;
    }
    else if(MACRO_IS("open_start_gcode") || MACRO_IS("open_end_gcode")) {
        gpx->flag.macrosEnabled = 0;
    }
    else if(MACRO_IS("close_start_gcode") || MACRO_IS("close_end_gcode")) {
        gpx->flag.macrosEnabled = 1;
    }
    // ;@load_eeprom_map
    // Load the appropriate built-in eeprom map for the firmware flavor and version
    else if(MACRO_IS("load_eeprom_map")) {
        if(name) {
            // FUTURE <NAME> parameter to allow run-time loading of non-built-in maps
            gcodeResult(gpx, "(line %u) Error: custom eeprommap's not supported by this version of gpx" EOL, gpx->lineNumber);
        }
        else {
            load_eeprom_map(gpx);
        }
    }
    // ;@eeprom <NAME> <TYPENAME> #<HEX> <LEN>
    // Add a single eeprom mapping of <NAME> to <HEX> address of type <TYPENAME> with length <LEN>
    // <LEN> only applies to et_string
    else if(MACRO_IS("eeprom")) {
        if(name && string_param) {
            EepromType et = eepromTypeFromTypeName(string_param);
            if(et == et_null) {
                gcodeResult(gpx, "(line %u) Error: @eeprom macro unknown type name %s\n", gpx->lineNumber, string_param);
            }
            else {
                int len = 0;
                if(et == et_string)
                    len = (int)z;
                add_eeprom_mapping(gpx, name, et, LED, len);
            }
        }
        else {
            gcodeResult(gpx, "(line %u) Semantic error: @eeprom macro with missing name or typename" EOL, gpx->lineNumber);
        }
    }
    // ;@eread <NAME>
    // Read eeprom setting <NAME> defined by earlier @eeprom macro
    else if(MACRO_IS("eread")) {
        if(name) {
            read_eeprom_name(gpx, name);
        }
        else {
            gcodeResult(gpx, "(line %u) Semantic error: @eread macro with missing name" EOL, gpx->lineNumber);
        }
    }
    // ;@ewrite <NAME> <VALUE>
    // Write eeprom setting <NAME> defined by earlier @eeprom macro
    else if(MACRO_IS("ewrite")) {
        if(name) {
            write_eeprom_name(gpx, name, string_param, LED, z);
        }
        else {
            gcodeResult(gpx, "(line %u) Error: @ewrite macro with missing name" EOL, gpx->lineNumber);
        }
    }
    // ;@debug <COMMAND>
    else if(MACRO_IS("debug")) {
        // ;@debug pos
        // Output current position
        if(NAME_IS("pos")) {
            gcodeResult(gpx, "gpx position ");
            show_current_pos(gpx);

            char axes_names[] = "XYZAB";
            char s[sizeof(axes_names)], *p = s;
            int i;
            for (i = 0; i < sizeof(axes_names); i++) {
                if(gpx->axis.positionKnown & (1 << i))
                    *p++ = axes_names[i];
            }
            *p++ = 0;
            gcodeResult(gpx, "positions known: %s\n", s);
        }
        // @debug axes
        // Output current machine settings for each axxes
        else if(NAME_IS("axes")) {
            gcodeResult(gpx, "steps_per_mm, max_feedrate, max_acceleration, max_speed_change, home_feedrate, length, endstop\n");
            Axis *a = &gpx->machine.x;
            char *s = "X";
            gcodeResult(gpx, "%s: %.10g, %g, %g, %g, %g, %g, %u\n", s, a->steps_per_mm, a->max_feedrate, a->max_accel, a->max_speed_change, a->home_feedrate, a->length, a->endstop);
            a = &gpx->machine.y;
            s = "Y";
            gcodeResult(gpx, "%s: %.10g, %g, %g, %g, %g, %g, %u\n", s, a->steps_per_mm, a->max_feedrate, a->max_accel, a->max_speed_change, a->home_feedrate, a->length, a->endstop);
            a = &gpx->machine.z;
            s = "Z";
            gcodeResult(gpx, "%s: %.10g, %g, %g, %g, %g, %g, %u\n", s, a->steps_per_mm, a->max_feedrate, a->max_accel, a->max_speed_change, a->home_feedrate, a->length, a->endstop);

            gcodeResult(gpx, "steps_per_mm, max_feedrate, max_acceleration, max_speed_change, motor_steps, has_heated_build_platform\n");
            Extruder *e = &gpx->machine.a;
            s = "A";
            gcodeResult(gpx, "%s: %.10g, %g, %g, %g, %g, %u\n", s, e->steps_per_mm, e->max_feedrate, e->max_accel, e->max_speed_change, e->motor_steps, e->has_heated_build_platform);
            e = &gpx->machine.b;
            s = "B";
            gcodeResult(gpx, "%s: %.10g, %g, %g, %g, %g, %u\n", s, e->steps_per_mm, e->max_feedrate, e->max_accel, e->max_speed_change, e->motor_steps, e->has_heated_build_platform);
        }
        // @debug progress
        // Output internal state relating to progress reporting
        else if(NAME_IS("progress")) {
            char *s;
            switch (gpx->flag.programState) {
                case READY_STATE:   s = "READY_STATE"; break;
                case RUNNING_STATE: s = "RUNNING_STATE"; break;
                case ENDED_STATE:   s = "ENDED_STATE"; break;
                default:            s = "UNKNOWN"; break;
            }
            gcodeResult(gpx, "buildName: %s\n", gpx->buildName);
            gcodeResult(gpx, "buildProgress: %s\n", gpx->flag.buildProgress ? "True" : "False");
            gcodeResult(gpx, "programState: %s\n", s);
            gcodeResult(gpx, "macrosEnabled: %s\n", gpx->flag.macrosEnabled ? "True" : "False");
            gcodeResult(gpx, "runMacros: %s\n", gpx->flag.runMacros ? "True" : "False");
            gcodeResult(gpx, "current.percent: %d\%\n", gpx->current.percent);
            gcodeResult(gpx, "total.time: %lf\n", gpx->total.time);
        }
        // @debug overheat
        // generate an overheat failure result
        else if(NAME_IS("overheat")) {
            return 0x8B;
        }
        // @debug verboseon
        // turn on verboseMode
        else if(NAME_IS("verboseon")) {
            gpx->flag.verboseMode = 1;
        }
        // @debug verboseoff
        // turn off verboseMode
        else if(NAME_IS("verboseoff")) {
            gpx->flag.verboseMode = 0;
        }
        // @debug iostatus
        // pass to io callback to output i/o state
        else if(NAME_IS("iostatus")) {
            gcodeResult(gpx, "@iostatus");
        }
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
            if(handler(gpx, section, prev_name, start) && !error)
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
                if(*end == ';')
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

int gpx_set_property_inner(Gpx *gpx, const char* section, const char* property, char* value);

int gpx_set_property(Gpx *gpx, const char* section, const char* property, char* value)
{
     char c, *inptr, *outptr, *ptr, *tmp, *tmp0, *tmpend;
     int iret;

     // If there is no section name or the section name has no comma, then
     // just set the property

     if(!section || !(ptr = strchr(section, ',')))
	  return gpx_set_property_inner(gpx, section, property, value);

     // Section name has a comma
     // Strip LWSP and call gpx_set_property_inner() once for each section
     tmp0 = strdup(section);
     if(!tmp0)
     {
	  SHOW( fprintf(gpx->log, "Configuration error: insufficient virtual memory" EOL) )
	  return gpx->lineNumber;
     }

     // Remove all LWSP
     outptr = inptr = tmp0;
     while((c = *inptr++))
	  if(!isspace(c))
	       *outptr++ = c;
     *outptr = '\0';

     // Note the end of the resulting string
     tmpend = tmp0 + strlen(tmp0);
     tmp = tmp0;

     iret = SUCCESS;
loop:
     // Find the next token.  We could use strtok(_r) but it's not on all systems
     ptr = strchr(tmp, ',');
     if(ptr)
	  *ptr = '\0';
     if((iret = gpx_set_property_inner(gpx, tmp, property, value)))
	  goto done;

     // Advance to the next section
     tmp = ptr ? ptr + 1 : tmpend;

     // If there's more left, then repeat
     if(tmp < tmpend)
	  goto loop;

done:
     if(tmp0)
	  free(tmp0);

     return iret;
}

int gpx_parse_steps_per_mm_all_axes(Gpx *gpx, char *parm);

int gpx_set_property_inner(Gpx *gpx, const char* section, const char* property, char* value)
{
    int rval;
    if(strcasecmp(value, "None") == 0) {
        gcodeResult(gpx, "(line %u) Configuration error: Ignoring configuration value '%s'" EOL, gpx->lineNumber, value);
        return gpx->lineNumber;
    }
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
            if(gpx_set_machine(gpx, value, 0)) {
                gcodeResult(gpx, "(line %u) Configuration error: unrecognised machine type '%s'" EOL, gpx->lineNumber, value);
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
                gcodeResult(gpx, "(line %u) Configuration error: unrecognised GCODE flavor '%s'" EOL, gpx->lineNumber, value);
                return gpx->lineNumber;
            }
        }
        else if(PROPERTY_IS("build_platform_temperature")) {
            if(gpx->machine.a.has_heated_build_platform) gpx->override[A].build_platform_temperature = atoi(value);
            if(gpx->machine.b.has_heated_build_platform) gpx->override[B].build_platform_temperature = atoi(value);
        }
        else if(PROPERTY_IS("sd_card_path")) {
            gpx->sdCardPath = strdup(value);
        }
        else if(PROPERTY_IS("verbose")) {
            gpx->flag.verboseMode = atoi(value);
        }
		else if(PROPERTY_IS("machine_description") ||
				PROPERTY_IS("nozzle_diameter") ||
				PROPERTY_IS("toolhead_offset_x") ||
				PROPERTY_IS("toolhead_offset_y") ||
				PROPERTY_IS("toolhead_offset_z") ||
				PROPERTY_IS("jkn_k") ||
				PROPERTY_IS("jkn_k2") ||
				PROPERTY_IS("nozzle_diameter") ||
				PROPERTY_IS("extruder_count") ||
				PROPERTY_IS("timeout")) { }
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("x")) {
        if(PROPERTY_IS("max_feedrate")) gpx->machine.x.max_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("home_feedrate")) gpx->machine.x.home_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("steps_per_mm")) gpx->machine.x.steps_per_mm = strtod(value, NULL);
        else if(PROPERTY_IS("endstop")) gpx->machine.x.endstop = atoi(value);
		else if(PROPERTY_IS("max_acceleration") ||
				PROPERTY_IS("max_speed_change") ||
				PROPERTY_IS("length")) { }
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("y")) {
        if(PROPERTY_IS("max_feedrate")) gpx->machine.y.max_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("home_feedrate")) gpx->machine.y.home_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("steps_per_mm")) gpx->machine.y.steps_per_mm = strtod(value, NULL);
        else if(PROPERTY_IS("endstop")) gpx->machine.y.endstop = atoi(value);
		else if(PROPERTY_IS("max_acceleration") ||
				PROPERTY_IS("max_speed_change") ||
				PROPERTY_IS("length")) { }
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("z")) {
        if(PROPERTY_IS("max_feedrate")) gpx->machine.z.max_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("home_feedrate")) gpx->machine.z.home_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("steps_per_mm")) gpx->machine.z.steps_per_mm = strtod(value, NULL);
        else if(PROPERTY_IS("endstop")) gpx->machine.z.endstop = atoi(value);
		else if(PROPERTY_IS("max_acceleration") ||
				PROPERTY_IS("max_speed_change") ||
				PROPERTY_IS("length")) { }
        else goto SECTION_ERROR;
    }
    else if(SECTION_IS("a")) {
        if(PROPERTY_IS("max_feedrate")) gpx->machine.a.max_feedrate = strtod(value, NULL);
        else if(PROPERTY_IS("steps_per_mm")) gpx->machine.a.steps_per_mm = strtod(value, NULL);
        else if(PROPERTY_IS("motor_steps")) gpx->machine.a.motor_steps = strtod(value, NULL);
        else if(PROPERTY_IS("has_heated_build_platform")) gpx->machine.a.has_heated_build_platform = atoi(value);
		else if(PROPERTY_IS("max_acceleration") ||
				PROPERTY_IS("max_speed_change")) { }
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
		else if(PROPERTY_IS("max_acceleration") ||
				PROPERTY_IS("max_speed_change")) { }
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
        else if(PROPERTY_IS("steps_per_mm")) {
            gpx_parse_steps_per_mm_all_axes(gpx, value);
        }
        else goto SECTION_ERROR;
    }
    else {
        gcodeResult(gpx, "(line %u) Configuration error: unrecognised section [%s]" EOL, gpx->lineNumber, section);
        return gpx->lineNumber;
    }
    return SUCCESS;

SECTION_ERROR:
    gcodeResult(gpx, "(line %u) Configuration error: [%s] section contains unrecognised property %s = %s" EOL, gpx->lineNumber, section, property, value);
    return gpx->lineNumber;
}

// parse a steps per mm parameter of the form x88.9y88.9z94.5a102.4b105.7
int gpx_parse_steps_per_mm_all_axes(Gpx *gpx, char *parm)
{
    if(parm == NULL)
        return SUCCESS;

    char *s = parm;
    char axis = *s;

    while(*s++) {
        if (isdigit(*s)) {
            double steps = strtod(s, &s);
            switch(tolower(axis)) {
                case 'x': gpx->machine.x.steps_per_mm = steps; break;
                case 'y': gpx->machine.y.steps_per_mm = steps; break;
                case 'z': gpx->machine.z.steps_per_mm = steps; break;
                case 'a': gpx->machine.a.steps_per_mm = steps; break;
                case 'b': gpx->machine.b.steps_per_mm = steps; break;
                default:
                    gcodeResult(gpx, "(line %u) Configuration error: steps per mm parameter (%s) contains unrecognized axis '%c'\n", gpx->lineNumber, parm, axis);
            }
        }
        axis = *s;
    }
    return SUCCESS;
}

int gpx_load_config(Gpx *gpx, const char *filename)
{
    VERBOSE( fprintf(gpx->log, "Loading config: %s\n", filename) );
    if(gpx->iniPath != NULL) {
        free(gpx->iniPath);
        gpx->iniPath = NULL;
    }
    char *t = strdup(filename);
    if(t != NULL) {
        gpx->iniPath = strdup(dirname(t));
        free(t);
    }

    int rval = ini_parse(gpx, filename, gpx_set_property);
    if(rval == 0)
        VERBOSE( fprintf(gpx->log, "Loaded config: %s\n", filename) );
    return rval;
}

void gpx_register_callback(Gpx *gpx, int (*callbackHandler)(Gpx*, void*, char*, size_t), void *callbackData)
{
    gpx->callbackHandler = callbackHandler;
    gpx->callbackData = callbackData;
}

static int process_options(Gpx *gpx, int item_code, va_list ap)
{
     if(!gpx) {
	  SHOW( fprintf(gpx->log, "GPX programming error; NULL context "
			"passed to process_options(); aborting " EOL) );
	  return ERROR;
     }

     while (item_code)
     {
	  switch (item_code) {

	  case ITEM_FRAMING_ENABLE :
	       gpx->flag.framingEnabled = 1;
	       break;

	  case ITEM_FRAMING_DISABLE :
	       gpx->flag.framingEnabled = 0;
	       break;

	  default :
	       // Unrecognized item code; error
	       SHOW( fprintf(gpx->log, "GPX programming error; invalid "
			     "item code %d used; aborting " EOL, item_code) );
	       return ERROR;
	  }
	  item_code = va_arg(ap, int);
     }

     return SUCCESS;
}

void gpx_start_convert(Gpx *gpx, char *buildName, int item_code, ...)
{
    if(item_code) {
	 va_list ap;
	 int retstat;

	 va_start(ap, item_code);
	 retstat = process_options(gpx, item_code, ap);
	 va_end(ap);
	 if(retstat != SUCCESS)
	      return;
    }

    if(buildName)
        set_build_name(gpx, buildName);

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

// M105: Get Extruder Temperature
static int get_extruder_temperature_extended(Gpx *gpx)
{
    int rval;

    // Warning: The tio callback handler depends on this call order
    CALL(get_build_statistics(gpx));
    CALL(get_extruder_temperature(gpx, 0));
    CALL(get_extruder_target_temperature(gpx, 0));
    if(gpx->machine.extruder_count > 1) {
        CALL(get_extruder_temperature(gpx, 1));
        CALL(get_extruder_target_temperature(gpx, 1));
    }
    if(gpx->machine.a.has_heated_build_platform) {
        CALL(get_build_platform_temperature(gpx, 0));
        CALL(get_build_platform_target_temperature(gpx, 0));
    }
    else if(gpx->machine.b.has_heated_build_platform) {
        CALL(get_build_platform_temperature(gpx, 1));
        CALL(get_build_platform_target_temperature(gpx, 1));
    }
    empty_frame(gpx);
    return SUCCESS;
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
    VERBOSESIO( if(gpx->flag.sioConnected) fprintf(gpx->log, "gcode_line: %s\n", gcode_line); )
    // check for line number
    if(*p == 'n' || *p == 'N') {
        digits = p;
        p = normalize_word(p);
        if(*p == 0) {
            gcodeResult(gpx, "(line %u) Syntax error: line number command word 'N' is missing digits" EOL, gpx->lineNumber);
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
                    if(gpx->command.m == 23 || gpx->command.m == 28) {
                        char *s = p + 1;
                        while(*s && *s != '*') s++;
                        if(*s) *s++ = 0;
                        gpx->command.arg = normalize_comment(p + 1);
                        gpx->command.flag |= ARG_IS_SET;
                        p = s;
                    }
                    break;
                    // Tnnn	 Select extruder nnn.
                case 't':
                case 'T':
                    gpx->command.t = atoi(digits);
                    gpx->command.flag |= T_IS_SET;
                    break;
                    // Nnnn      Line number
                case 'n':
                case 'N':
                    // this line's number was already stripped off, so this should
                    // be a parameter to an M110 which GPX does not currently implement
                    // so we'll silently ignore
                    if((gpx->command.flag & M_IS_SET) && gpx->command.m == 110)
                        break;
                    // fallthrough

                default:
                    gcodeResult(gpx, "(line %u) Syntax warning: unrecognised command word '%c'" EOL, gpx->lineNumber, c);
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
                gcodeResult(gpx, "(line %u) Syntax warning: nested comment detected" EOL, gpx->lineNumber);
                e = strrchr(p + 1, ')');
            }
            if(e) {
                *e = 0;
                gpx->command.comment = normalize_comment(p + 1);
                gpx->command.flag |= COMMENT_IS_SET;
                p = e + 1;
            }
            else {
                gcodeResult(gpx, "(line %u) Syntax warning: comment is missing closing ')'" EOL, gpx->lineNumber);
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
            gcodeResult(gpx, "(line %u) Syntax error: unrecognised gcode '%s'" EOL, gpx->lineNumber, p);
            break;
        }
    }

    // revert tool selection to current extruder (Makerbot Tn is not sticky)
    if(!gpx->flag.reprapFlavor || gpx->flag.onlyExplicitToolChange) gpx->target.extruder = gpx->current.extruder;

    // change the extruder selection (in the virtual tool carosel)
    if(gpx->command.flag & T_IS_SET && !gpx->flag.dittoPrinting) {
        unsigned tool_id = (unsigned)gpx->command.t;
        if(tool_id < gpx->machine.extruder_count) {
            gpx->target.extruder = tool_id;
        }
        else {
            gcodeResult(gpx, "(line %u) Semantic warning: T%u cannot select non-existant extruder" EOL, gpx->lineNumber, tool_id);
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

    Point5d delta;
    int relative;

    if(gpx->command.flag & G_IS_SET) {
        switch(gpx->command.g) {
                // G0 - Rapid Positioning
            case 0: {
                double feedrate = 0.0;

                if(!gpx->flag.relativeCoordinates && gpx->flag.ignoreAbsoluteMoves)
                    break;

                CALL( calculate_target_position(gpx, &delta, &relative) );
                if(!(gpx->command.flag & F_IS_SET)) {
                    if(gpx->command.flag & X_IS_SET) delta.x = fabs(delta.x);
                    if(gpx->command.flag & Y_IS_SET) delta.y = fabs(delta.y);
                    if(gpx->command.flag & Z_IS_SET) delta.z = fabs(delta.z);
                    double length = magnitude(gpx->command.flag & XYZ_BIT_MASK, (Ptr5d)&delta);
                    double candidate;
                    feedrate = DBL_MAX;
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
                }
                CALL( queue_ext_point(gpx, feedrate, &delta, relative) );
                update_current_position(gpx);
                command_emitted++;
                break;
            }

                // G1 - Coordinated Motion
            case 1:
                if(!gpx->flag.relativeCoordinates && gpx->flag.ignoreAbsoluteMoves)
                    break;
                CALL( calculate_target_position(gpx, &delta, &relative) );
                CALL( queue_ext_point(gpx, 0.0, &delta, relative) );
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
                        CALL( calculate_target_position(gpx, &delta, &relative) );
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
                    gcodeResult(gpx, "(line %u) Syntax error: G4 is missing delay parameter, use Pn where n is milliseconds" EOL, gpx->lineNumber);
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
                    gcodeResult(gpx, "(line %u) Syntax error: G10 is missing coordiante system, use Pn where n is 1-6" EOL, gpx->lineNumber);
                }
                break;

                // G15 - Use cartesian coordinates
            case 15:
                break;  // yep, it's all we do, currently

                // G21 - Use Millimeters as Units (IGNORED)
                // G71 - Use Millimeters as Units (IGNORED)
            case 21:
            case 71:
                break;

                // G28 - Home given axes to machine defined endstop
            case 28: {
                unsigned endstop_max = 0;
                unsigned endstop_min = 0;
                // none means all
                if((gpx->command.flag & gpx->axis.mask) == 0) gpx->command.flag |= XYZ_BIT_MASK;
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
                        CALL( home_axes(gpx, endstop_min, ENDSTOP_IS_MIN) );
                    }
                    if(endstop_max) {
                        CALL( home_axes(gpx, endstop_max, ENDSTOP_IS_MAX) );
                    }
                }
                command_emitted++;
                set_unknown_axes(gpx, gpx->command.flag);
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
                gpx->flag.relativeCoordinates = 1;
                break;

                // G92 - Define current position on axes
            case 92: {
                if(gpx->command.flag & XYZ_BIT_MASK) {
                    gpx->flag.ignoreAbsoluteMoves = 0;
                }
                double userScale = gpx->flag.macrosEnabled ? gpx->user.scale : 1.0;
                if(gpx->command.flag & X_IS_SET) gpx->current.position.x = gpx->command.x * userScale;
                if(gpx->command.flag & Y_IS_SET) gpx->current.position.y = gpx->command.y * userScale;
                if(gpx->command.flag & Z_IS_SET) gpx->current.position.z = gpx->command.z * userScale;
                if(gpx->command.flag & A_IS_SET) gpx->current.position.a = gpx->command.a;
                if(gpx->command.flag & B_IS_SET) gpx->current.position.b = gpx->command.b;
                if(((gpx->axis.positionKnown | gpx->command.flag) & gpx->axis.mask) != gpx->axis.mask) {
                    // there's a problem with MakerBot's version of G92 (140 set extended position) in that
                    // it must take all coordinates, so if a G92 specifies a subset, the other coordinates
                    // are set as a side effect to whatever GPX thinks is "current" whether it knows or not
                    gcodeResult(gpx, "(line %u) warning G92 emulation unable to determine all coordinates to set via x3g:140 set extended position\n", gpx->lineNumber);
                    gcodeResult(gpx, "current position defined as ");
                    show_current_pos(gpx);
                }
                CALL( set_position(gpx) );
                command_emitted++;
                // flag axes that are known
                gpx->axis.positionKnown |= (gpx->command.flag & gpx->axis.mask);
                break;
            }

                // G130 - Set given axes potentiometer Value
            case 130:
                if(gpx->command.flag & X_IS_SET) {
                    CALL( set_pot_value(gpx, 0, gpx->command.x < 0 ? 0 : gpx->command.x > 255 ? 255 : (unsigned)gpx->command.x) );
                }

                if(gpx->command.flag & Y_IS_SET) {
                    CALL( set_pot_value(gpx, 1, gpx->command.y < 0 ? 0 : gpx->command.y > 255 ? 255 : (unsigned)gpx->command.y) );
                }

                if(gpx->command.flag & Z_IS_SET) {
                    CALL( set_pot_value(gpx, 2, gpx->command.z < 0 ? 0 : gpx->command.z > 255 ? 255 : (unsigned)gpx->command.z) );
                }

                if(gpx->command.flag & A_IS_SET) {
                    CALL( set_pot_value(gpx, 3, gpx->command.a < 0 ? 0 : gpx->command.a > 255 ? 255 : (unsigned)gpx->command.a) );
                }

                if(gpx->command.flag & B_IS_SET) {
                    CALL( set_pot_value(gpx, 4, gpx->command.b < 0 ? 0 : gpx->command.b > 255 ? 255 : (unsigned)gpx->command.b) );
                }
                break;

                // G161 - Home given axes to minimum
            case 161:
                if(gpx->command.flag & F_IS_SET) gpx->current.feedrate = gpx->command.f;
                CALL( home_axes(gpx, gpx->command.flag & XYZ_BIT_MASK, ENDSTOP_IS_MIN) );
                command_emitted++;
                // clear homed axes
                set_unknown_axes(gpx, gpx->command.flag);
                gpx->excess.a = 0;
                gpx->excess.b = 0;
                break;
                // G162 - Home given axes to maximum
            case 162:
                if(gpx->command.flag & F_IS_SET) gpx->current.feedrate = gpx->command.f;
                CALL( home_axes(gpx, gpx->command.flag & XYZ_BIT_MASK, ENDSTOP_IS_MAX) );
                command_emitted++;
                // clear homed axes
                set_unknown_axes(gpx, gpx->command.flag);
                gpx->excess.a = 0;
                gpx->excess.b = 0;
                break;
            default:
                gcodeResult(gpx, "(line %u) Syntax warning: unsupported gcode command 'G%u'" EOL, gpx->lineNumber, gpx->command.g);
        }
    }
    else if(gpx->command.flag & M_IS_SET) {
        switch(gpx->command.m) {
                // M0 - Marlin pause
            case 0:
				break;

                // M1 - ANSI pause, Marlin pause
            case 1:
				pause_resume(gpx);
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
                if(gpx->flag.dittoPrinting || (gpx->command.m == 116 && !(gpx->command.flag & T_IS_SET))) {
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
                // fallthrough

                // M21 - Init SD card
            case 21:
                CALL( get_next_filename(gpx, 1) );
                break;

                // M22 - Release SD card
            case 22:
                break;

                // M23 - Select SD file
            case 23:
                if(gpx->command.flag & ARG_IS_SET)
                    CALL( select_filename(gpx, gpx->command.arg) );
                break;

                // M24 - Start/resume SD print
            case 24:
                if(gpx->flag.sd_paused) {
                    CALL( pause_resume(gpx) );
                    gpx->flag.sd_paused = 0;
                }
                else if(gpx->selectedFilename != NULL) {
                    CALL( play_back_capture(gpx, gpx->selectedFilename) );
                }
                break;

                // M25 - Pause SD print
            case 25:
                if(!gpx->flag.sd_paused) {
                    CALL( pause_resume(gpx) );
                    gpx->flag.sd_paused = 1;
                }
                break;

                // M26 - Set SD position
            case 26:
                // s3g doesn't have a set SD position
                // looks like software uses the sequence M25\nM26 S0 to cancel
                // the print so that the next M24 will start over 
                VERBOSE( {if(gpx->command.flag & S_IS_SET && gpx->command.s > 0)
                    fprintf(gpx->log, "Only reset to sd position 0 is supported: M26 S0" EOL);} )
                break;

                // M27 - Report SD print status
            case 27:
                CALL( get_build_statistics(gpx) );
                CALL( get_extended_position(gpx) );
                break;

                // M28 - Begin write to SD card
            case 28:
                if(gpx->command.flag & ARG_IS_SET) {
                    CALL( capture_to_file(gpx, gpx->command.arg) );
                }
                break;

                // M29 - Stop writing to SD card
            case 29:
                CALL( end_capture_to_file(gpx) );
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
                    gcodeResult(gpx, "(line %u) Syntax error: M70 is missing message text, use (text) where text is message" EOL, gpx->lineNumber);
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
                    gcodeResult(gpx, "(line %u) Syntax warning: M72 is missing song number, use Pn where n is 0-2" EOL, gpx->lineNumber);
                }
                break;

                // M73 - Manual set build percentage
            case 73:
                if(gpx->command.flag & P_IS_SET) {
                    unsigned percent = (unsigned)gpx->command.p;
                    if(percent > 100) percent = 100;
                    if(program_is_ready() && percent < 100) {
                        start_program();
                        if(!gpx->nostart) {
			     CALL( start_build(gpx, gpx->buildName) );
			}
                        CALL( set_build_progress(gpx, percent) );
                        // start extruder in a known state
                        CALL( change_extruder_offset(gpx, gpx->current.extruder) );
                    }
                    else if(program_is_running()) {
                        if(percent == 100) {
                            // disable macros in footer
                            gpx->flag.macrosEnabled = 0;
                            end_program();
                            CALL( set_build_progress(gpx, 100) );
			    if(!gpx->noend) {
				 CALL( end_build(gpx) );
			    }
                        }
                        else {
                            // enable macros in object body
                            if(!gpx->flag.macrosEnabled && percent > 0) {
                                if(gpx->flag.pausePending && gpx->flag.runMacros) {
                                    CALL( pause_at_zpos(gpx, gpx->commandAt[0].z) );
                                    gpx->flag.pausePending = 0;
                                    VERBOSE( gcodeResult(gpx, "Issued next pause @ %0.2lf\n", gpx->commandAt[0].z) );
                                }
                                gpx->flag.macrosEnabled = 1;
                            }
                            if(gpx->current.percent < percent && (percent == 1 || gpx->total.time == 0.0 || gpx->flag.buildProgress == 0)) {
                                CALL( set_build_progress(gpx, percent) );
                            }
                        }
                    }
                }
                else {
                    gcodeResult(gpx, "(line %u) Syntax warning: M73 is missing build percentage, use Pn where n is 0-100" EOL, gpx->lineNumber);
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
                    CALL( set_steppers(gpx, A_IS_SET|B_IS_SET, 0) );
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
                    gcodeResult(gpx, "(line %u) Syntax error: M104 is missing temperature, use Sn where n is 0-280" EOL, gpx->lineNumber);
                }
                break;

                // M105 - Get extruder temperature
            case 105:
                CALL(get_extruder_temperature_extended(gpx));
                break;

                // M106 - Turn heatsink cooling fan on
		// M106 - Turn ABP conveyor on

		// 3 November 2014
		//
		// In Gen 4 electronics, turn the ABP conveyor on
		// In MightyBoard electronics, turn the heatsink fan on

            case 106:
                if(gpx->machine.id >= MACHINE_TYPE_REPLICATOR_1) {
		    int state = (gpx->command.flag & S_IS_SET) ? ((unsigned)gpx->command.s ? 1 : 0) : 1;
		    if(gpx->flag.reprapFlavor || gpx->flag.M106AlwaysValve) {
			 // Toggle valve
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
			 // Toggle heatsink fan
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
                }
                else {
		     // Enable ABP
		     CALL( set_abp(gpx, gpx->target.extruder, 1) );
		     command_emitted++;
                }
                break;

                // M107 - Turn cooling fan off
		// M107 - Turn ABP conveyor off

		// 3 November 2014
		//
		// In Gen 4 electronics, turn the ABP conveyor off
		// In MightyBoard electronics, turn the heatsink fan off

            case 107:
                if(gpx->machine.id >= MACHINE_TYPE_REPLICATOR_1) {
		    int state = (gpx->command.flag & S_IS_SET) ? ((unsigned)gpx->command.s ? 1 : 0) : 0;
		    if(gpx->flag.reprapFlavor || gpx->flag.M106AlwaysValve) {
			 // Toggle valve
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
			 // Set the heatsink fan off
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
                }
                else {
		     // Disable ABP
		     CALL( set_abp(gpx, gpx->target.extruder, 0) );
		     command_emitted++;
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
                else
#endif
                if(gpx->command.flag & T_IS_SET) {
                    // M108 - toolchange for ReplicatorG
                    if(!gpx->flag.dittoPrinting && gpx->target.extruder != gpx->current.extruder) {
                        int timeout = gpx->command.flag & P_IS_SET ? (int)gpx->command.p : MAX_TIMEOUT;
                        CALL( do_tool_change(gpx, timeout) );
                        command_emitted++;
                    }
                }
                else if(gpx->flag.sioConnected) {
                    // M108 - cancel heating for Marlin
                    CALL( abort_immediately(gpx) );
                }
#if ENABLE_SIMULATED_RPM
                else {
                    gcodeResult(gpx, "(line %u) Syntax error: M108 is missing motor RPM, use Rn where n is 0-5" EOL, gpx->lineNumber);
                }
#endif
                break;

                // M112 - Emergency stop
            case 112:
                // In Marlin this appears to be emergency!, emergency!, which
                // according to reprap.org/wiki/G-code is supposed to shutdown
                // the bot.  Closest thing we have in s3g/x3g is "abort immediately"
                // which is the same as "reset" and "clear command buffer".
                // In SailFish, it clears the command buffer, cancels SD printing
                // if any, and turns off the steppers and heaters.  So at the moment
                // this is the closest thing gcode has for controlled cancel.
                // weird.
                // We only pay attention to this when connected to the bot, why would
                // you just put it in an offline file?
                if(gpx->flag.sioConnected)
                    CALL( abort_immediately(gpx) );
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
                        gcodeResult(gpx, "(line %u) Syntax error: M109 is missing temperature, use Sn where n is 0-280" EOL, gpx->lineNumber);
                    }
                    break;
                }
                // fall through to M140 for Makerbot/ReplicatorG flavor

                // M140 - Set build platform temperature
            case 140:
                if(gpx->machine.a.has_heated_build_platform || gpx->machine.b.has_heated_build_platform) {
                    if(gpx->command.flag & S_IS_SET) {
                        unsigned temperature = (unsigned)gpx->command.s;
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
                            gcodeResult(gpx, "(line %u) Semantic warning: M%u cannot select non-existant heated build platform T%u" EOL, gpx->lineNumber, gpx->command.m, tool_id);
                        }
                    }
                    else {
                        gcodeResult(gpx, "(line %u) Syntax error: M%u is missing temperature, use Sn where n is 0-130" EOL, gpx->lineNumber, gpx->command.m);
                    }
                }
                else {
                    gcodeResult(gpx, "(line %u) Semantic warning: M%u cannot select non-existant heated build platform" EOL, gpx->lineNumber, gpx->command.m);
                }
                break;

                // M110 - Set current line number
            case 110:
                break;

                // M111 - Set debug level
            case 111:
                break;

                // M114 - Get current position
            case 114:
                CALL( get_extended_position(gpx) );
                break;

                // M115 - Get firmware version and capabilities
            case 115:
                CALL( get_advanced_version_number(gpx) );
                break;

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
                    gcodeResult(gpx, "(line %u) Syntax error: M131 is missing axes, use X Y Z A B" EOL, gpx->lineNumber);
                }
                break;

                // M132 - Load Current Position from EEPROM
            case 132:
                if(gpx->command.flag & AXES_BIT_MASK) {
                    if(gpx->command.flag & XYZ_BIT_MASK) {
                        gpx->flag.ignoreAbsoluteMoves = 0;
                    }
                    CALL( recall_home_positions(gpx) );
                    command_emitted++;
                    // clear loaded axes
                    set_unknown_axes(gpx, gpx->command.flag);
                    gpx->excess.a = 0;
                    gpx->excess.b = 0;

                    // since we always emit relative extruder moves, let's pretend
                    // that M132 always returns 0 for the extruders, this will prevent
                    // confusion later when the gcode only uses E and we continue
                    // to think we don't know where the other extruder is
                    if(gpx->command.flag & A_IS_SET) {
                        gpx->axis.positionKnown |= A_IS_SET;
                        gpx->current.position.a = 0;
                    }
                    if(gpx->command.flag & B_IS_SET) {
                        gpx->axis.positionKnown |= B_IS_SET;
                        gpx->current.position.b = 0;
                    }
                }
                else {
                    gcodeResult(gpx, "(line %u) Syntax error: M132 is missing axes, use X Y Z A B" EOL, gpx->lineNumber);
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
                // Cura intends M190 to mean *set* and wait
            case 134:
            case 190: {
                if(gpx->machine.a.has_heated_build_platform || gpx->machine.b.has_heated_build_platform) {
                    if(gpx->command.flag & S_IS_SET) {
                        unsigned temperature = (unsigned)gpx->command.s;
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
                            gcodeResult(gpx, "(line %u) Semantic warning: M%u cannot select non-existant heated build platform T%u" EOL, gpx->lineNumber, gpx->command.m, tool_id);
                        }
                    }

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
                        gcodeResult(gpx, "(line %u) Semantic warning: M%u cannot select non-existant heated build platform T%u" EOL, gpx->lineNumber, gpx->command.m, tool_id);
                    }
                }
                else {
                    gcodeResult(gpx, "(line %u) Semantic warning: M%u cannot select non-existant heated build platform" EOL, gpx->lineNumber, gpx->command.m);
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
		    if(!gpx->nostart) {
			 CALL( start_build(gpx, gpx->buildName) );
		    }
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
                }
                break;

                // M220 - Set speed factor override percentage
            case 220:
                if((gpx->command.flag & S_IS_SET) && gpx->command.s > 0)
                    gpx->current.speed_factor = (unsigned)gpx->command.s;
                break;

                // M221 - Set extrude factor override percentage
            case 221:
                if((gpx->command.flag & S_IS_SET) && gpx->command.s > 0) {
                    unsigned tool_id = gpx->current.extruder;
                    if(gpx->command.flag & T_IS_SET)
                        tool_id = gpx->target.extruder;
                    gpx->override[tool_id].extrusion_factor = (unsigned)gpx->command.s;
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

                    if(gpx->flag.relativeCoordinates && !(gpx->axis.positionKnown & Z_IS_SET)) {
                        gcodeResult(gpx, "(line %u) Pause at zPos ignored because relative positioning is set and current Z position is unknown.\n", gpx->lineNumber);
                    }
                    else {
                        double z = gpx->flag.relativeCoordinates ? (gpx->current.position.z + gpx->command.z) : (gpx->command.z + conditional_z);
                        CALL( pause_at_zpos(gpx, z) );
                        VERBOSE( gcodeResult(gpx, "Issued pause @ %0.2lf\n", z) );
                    }
                }
                else {
                    gcodeResult(gpx, "(line %u) Syntax warning: M322 is missing Z axis" EOL, gpx->lineNumber);
                }
                command_emitted++;
                break;

                // M400 - Wait for current moves to finish
            case 400:
                empty_frame(gpx);
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
                gcodeResult(gpx, "(line %u) Syntax warning: unsupported mcode command 'M%u'" EOL, gpx->lineNumber, gpx->command.m);
        }
    }
    // X,Y,Z,A,B,E,F
    else if(gpx->command.flag & (AXES_BIT_MASK | F_IS_SET)) {
        if(!(gpx->command.flag & COMMENT_IS_SET) && (gpx->flag.relativeCoordinates || !gpx->flag.ignoreAbsoluteMoves)) {
            CALL( calculate_target_position(gpx, &delta, &relative) );
            CALL( queue_ext_point(gpx, 0.0, &delta, relative) );
            update_current_position(gpx);
            command_emitted++;
        }
    }
    // Tn
    else if(gpx->command.flag & T_IS_SET && !gpx->flag.dittoPrinting && gpx->target.extruder != gpx->current.extruder) {
        int timeout = gpx->command.flag & P_IS_SET ? (int)gpx->command.p : MAX_TIMEOUT;
        CALL( do_tool_change(gpx, timeout) );
        command_emitted++;
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
		if(!gpx->nostart) {
		     CALL( start_build(gpx, gpx->buildName) );
		}
                CALL( set_build_progress(gpx, 0) );
                // start extruder in a known state
                CALL( change_extruder_offset(gpx, gpx->current.extruder) );
            }
            else if(percent < 100 && program_is_running()) {
                if(gpx->current.percent) {
                    CALL( set_build_progress(gpx, percent) );
                }
                // force 1%
                else {
                    CALL( set_build_progress(gpx, 1) );
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

	if(gpx->preamble)
	     start_build(gpx, gpx->preamble);

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
                // ignore run-on comments, this is actually a little too permissive
                // since technically we should ignore ';' contained within a
                // parenthetical comment
                if(!strchr(gpx->buffer.in, ';'))
                    gcodeResult(gpx, "(line %u) Buffer overflow: input exceeds %u character limit, remaining characters in line will be ignored" EOL, gpx->lineNumber, BUFFER_MAX);
            }

            rval = gpx_convert_line(gpx, gpx->buffer.in);
            // normal exit
            if(rval == END_OF_FILE) break;
            // error
            if(rval < 0) return rval;
        }

        if(program_is_running()) {
            end_program();
	    if(!gpx->noend) {
		 CALL( set_build_progress(gpx, 100) );
		 CALL( end_build(gpx) );
	    }
        }

        // Ending gcode should disable the heaters and stepper motors
	// This line of code here in GPX was making it such that people
	// could not convert gcode utility scripts to x3g with GPX.  For
	// instance, a script for build plate leveling which wanted to
	// home the axes and then leave Z enabled

        // CALL( set_steppers(gpx, AXES_BIT_MASK, 0) );

        gpx->total.length = gpx->accumulated.a + gpx->accumulated.b;
        gpx->total.time = gpx->accumulated.time;
        gpx->total.bytes = gpx->accumulated.bytes;

        if(++i > 1) break;

        // rewind for second pass
        fseek(file.in, 0L, SEEK_SET);
        gpx_initialize(gpx, 0);
        gpx->flag.loadMacros = 0;
        gpx->flag.runMacros = 1;
        gpx->flag.pausePending = (gpx->commandAtLength > 0);
        //gpx->flag.logMessages = 0;
        gpx->callbackHandler = (int (*)(Gpx*, void*, char*, size_t))file_handler;
        gpx->callbackData = &file;
    }
    gpx->flag.logMessages = logMessages;;
    return SUCCESS;
}

char *sd_status[] = {
    "operation successful",
    "SD Card not present",
    "SD Card initialization failed",
    "partition table could not be read",
    "filesystem could not be opened",
    "root directory could not be opened",
    "SD Card is locked",
    "file not found",
    "general error",
    "changed working dir",
    "volume too big",
    "CRC failure",
    "SD Card read error",
    "SD operating at low speeds",
    "unknown status"
};

char *get_sd_status(unsigned int status)
{
    return sd_status[status < 14 ? status : 14];
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

char *get_build_status(unsigned int status)
{
    return build_status[status < 6 ? status : 6];
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

void hexdump(FILE *out, char *p, size_t len)
{
    while (len--)
        fprintf(out, "%02x ", (unsigned)(unsigned char)*p++);
    fflush(out);
}

static void read_query_response(Gpx *gpx, Sio *sio, unsigned command, char *buffer)
{
    gpx->buffer.ptr = gpx->buffer.in + 3;
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
            VERBOSE( fprintf(gpx->log, "Play back captured file: %d, %s" EOL,
                        sio->response.sd.status, get_sd_status(sio->response.sd.status)) );
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
                fprintf(gpx->log, "%s firmware v%u.%u" EOL, get_firmware_variant(sio->response.firmware.variant),
                        sio->response.firmware.version / 100, sio->response.firmware.version % 100);
            }
            break;
    }
}


#if defined(_WIN32) || defined(_WIN64)
// windows has more simultaneous timeout values, so we don't need select
// for the first byte
#define readport read
#else
size_t readport(int port, char *buffer, size_t bytes)
{
    fd_set fds;
    struct timeval tv;
    int rval;

    FD_ZERO(&fds);
    FD_SET(port, &fds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    // wait up to one second for the first byte
    rval = select(port + 1, &fds, NULL, NULL, &tv);
    if(rval <= 0)
        return rval;

    // wait up to 1/10th intercharacter (from VTIME)
    return read(port, buffer, bytes);
}
#endif

int port_handler(Gpx *gpx, Sio *sio, char *buffer, size_t length)
{
    int rval = SUCCESS;
    if(length) {
        size_t bytes;
        int retry_count = 0;
        do {
            VERBOSESIO( fprintf(gpx->log, "port_handler write: %lu" EOL, (unsigned long)length) );
            VERBOSESIO( hexdump(gpx->log, buffer, length) );
            // send the packet
            if((bytes = write(sio->port, buffer, length)) == -1) {
                return EOSERROR;
            }
            else if(bytes != length) {
                return ESIOWRITE;
            }
            sio->bytes_out += length;

            VERBOSESIO( fprintf(gpx->log, EOL "port_handler read:" EOL) );
            for(;;) {
                // read start byte
                if((bytes = readport(sio->port, gpx->buffer.in, 1)) == -1) {
                    return EOSERROR;
                }
                else if(bytes != 1) {
                    VERBOSESIO( fprintf(gpx->log, EOL "want 1 bytes = %u" EOL, (unsigned)bytes) );
                    if(bytes == 0)
                        return ESIOTIMEOUT;
                    return ESIOREAD;
                }
                VERBOSESIO( hexdump(gpx->log, gpx->buffer.in, bytes) );
                // loop until we get a valid start byte
                if((unsigned char)gpx->buffer.in[0] == 0xD5) break;
            }
            size_t payload_length = 0;
            do {
                // read length
                if((bytes = readport(sio->port, gpx->buffer.in + 1, 1)) == -1) {
                    return EOSERROR;
                }
                else if(bytes != 1) {
                    VERBOSESIO( fprintf(gpx->log, EOL "want 1 bytes = %u" EOL, (unsigned)bytes) );
                    return ESIOREAD;
                }
                VERBOSESIO( hexdump(gpx->log, gpx->buffer.in, bytes) );
                payload_length = gpx->buffer.in[1];
            } while ((unsigned char)gpx->buffer.in[1] == 0xd5);
            // recieve payload
            if((bytes = readport(sio->port, gpx->buffer.in + 2, payload_length + 1)) == -1) {
                return EOSERROR;
            }
            VERBOSESIO( hexdump(gpx->log, gpx->buffer.in + 2, bytes) );
            VERBOSESIO( fprintf(gpx->log, EOL) );
            if(bytes != payload_length + 1) {
                VERBOSESIO( fprintf(gpx->log, EOL "want %u bytes = %u" EOL, (unsigned)payload_length + 1, (unsigned)bytes) );
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
            rval = (int)(unsigned char)gpx->buffer.in[2];
            switch(rval) {
                    // 0x80 - Generic Packet error, packet discarded (retry)
                case 0x80:
                    VERBOSE( fprintf(gpx->log, "(retry %u) Generic Packet error: packet discarded" EOL, retry_count) );
                    break;

                    // 0x81 - Success
                case 0x81: {
                    unsigned command = (unsigned)buffer[COMMAND_OFFSET];
                    if((command & 0x80) == 0) {
                        read_query_response(gpx, sio, command, buffer);
                    }
                    return SUCCESS;
                }

                    // 0x82 - Action buffer overflow, entire packet discarded
                case 0x82:
                    VERBOSE( fprintf(gpx->log, "(retry %u) Action buffer overflow\n", retry_count) );
                    if(!sio->flag.retryBufferOverflow)
                        goto L_ABORT;

                    // first, harass the bot in a tight loop in case we're
                    // doing lots of short movements. On the one hand, we're
                    // making it worse by making the bot take time on serial
                    // i/o. On the other hand we want to make sure we send the
                    // next command into the buffer as soon as possible so we
                    // don't get a zit because of a sleep on our side
                    //
                    // twenty times, check for room every 10ms
                    int i;
                    for(i = 0; i < 20; i++) {
                        short_sleep(NS_10MS);

                        // query buffer size
                        CALL( port_handler(gpx, sio, buffer_size_query, 4) );

                        // if we now have room, let's go again
                        if (sio->response.bufferSize >= length)
                            goto L_REPEATSEND;
                    }

                    if(sio->flag.shortRetryBufferOverflowOnly) {
                        rval = 0x82; // recursion cleared it, put it back
                        goto L_ABORT;
                    }

                    // now, wait until we've got room for the command, checking
                    // every 1/10 second
                    do {
                        short_sleep(NS_100MS);
                        // query buffer size
                        buffer_size_query[3] = calculate_crc((unsigned char *)buffer_size_query + 2, 1);
                        CALL( port_handler(gpx, sio, buffer_size_query, 4) );
                        i++;
                        // loop until buffer has space for the next command
                    } while(sio->response.bufferSize < length);
L_REPEATSEND:
                    VERBOSE( fprintf(gpx->log, "(%u) Query buffer size: %u\n", i, sio->response.bufferSize) );
                    // we just did all the waiting we needed, skip the 2 second timeout
                    continue;

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
                    // I think this means the build was cancelled from the LCD
                    // panel or the bot overheated, we should bail out.  Is
                    // there a way to confirm the cancel?
                    goto L_ABORT;

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
            long_sleep(2);
        } while(++retry_count < 5);
    }

L_ABORT:
    return rval;
}

int gpx_convert_and_send(Gpx *gpx, FILE *file_in, int sio_port,
			 int item_code, ...)
{
    int i, rval;
    Sio sio;
    sio.in = stdin;
    sio.port = -1;
    sio.bytes_out = 0;
    sio.bytes_in = 0;
    sio.flag.retryBufferOverflow = 1;
    sio.flag.shortRetryBufferOverflowOnly = 0;
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
        gpx->flag.sioConnected = 1;
        gpx->callbackHandler = (int (*)(Gpx*, void*, char*, size_t))port_handler;;
        gpx->callbackData = &sio;
        gpx->sio = &sio;
    }

    if(item_code) {
	 va_list ap;
	 int retstat;

	 va_start(ap, item_code);
	 retstat = process_options(gpx, item_code, ap);
	 va_end(ap);
	 if(retstat != SUCCESS)
	      return retstat;
    }

    if(sio_port >= 0) {
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
                gcodeResult(gpx, "(line %u) Buffer overflow: input exceeds %u character limit, remaining characters in line will be ignored" EOL, gpx->lineNumber, BUFFER_MAX);
            }

            rval = gpx_convert_line(gpx, gpx->buffer.in);
            // normal exit
            if(rval > 0) break;
            // error
            if(rval < 0) return rval;
        }

        if(program_is_running()) {
            end_program();
	    if(!gpx->noend) {
		 CALL( set_build_progress(gpx, 100) );
		 CALL( end_build(gpx) );
	    }
        }

        // Ending gcode should disable the heaters and stepper motors
	// This line of code here in GPX was making it such that people
	// could not convert gcode utility scripts to x3g with GPX.  For
	// instance, a script for build plate leveling which wanted to
	// home the axes and then leave Z enabled

        // CALL( set_steppers(gpx, AXES_BIT_MASK, 0) );

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
        gpx->sio = &sio;
        gpx->flag.sioConnected = 1;
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

static int eeprom_set_property(Gpx *gpx, const char* section, const char* property, char* value)
{
    int rval;
    unsigned int address = (unsigned int)strtol(property, NULL, 0);

    if(gpx->sio == NULL) {
        gcodeResult(gpx, "(line %u) Error: eeprom_set_property only supported when connected via serial" EOL, gpx->lineNumber);
        return ERROR;
    }

    if(SECTION_IS("byte")) {
        unsigned char b = (unsigned char)strtol(value, NULL, 0);
        CALL( write_eeprom_8(gpx, gpx->sio, address, b) );
    }
    else if(SECTION_IS("integer")) {
        unsigned int i = (unsigned int)strtol(value, NULL, 0);
        CALL( write_eeprom_32(gpx, gpx->sio, address, i) );
    }
    else if(SECTION_IS("hex") || SECTION_IS("hexadecimal")) {
        unsigned int h = (unsigned int)strtol(value, NULL, 16);
        unsigned length = (unsigned)strlen(value) / 2;
        if(length > 4) length = 4;
        gpx->buffer.ptr = gpx->sio->response.eeprom.buffer;
        write_32(gpx, h);
        CALL( write_eeprom(gpx, address, gpx->sio->response.eeprom.buffer, length) );
    }
    else if(SECTION_IS("float")) {
        float f = strtof(value, NULL);
        CALL( write_eeprom_float(gpx, gpx->sio, address, f) );
    }
    else if(SECTION_IS("string")) {
        unsigned length = (unsigned)strlen(value);
        CALL( write_eeprom(gpx, address, value, length) );
    }
    else {
        gcodeResult(gpx, "(line %u) Configuration error: unrecognised section [%s]" EOL, gpx->lineNumber, section);
        return gpx->lineNumber;
    }
    return SUCCESS;
}

int eeprom_load_config(Gpx *gpx, const char *filename)
{
    return ini_parse(gpx, filename, eeprom_set_property);
}

void gpx_set_preamble(Gpx *gpx, const char *preamble)
{
     if(gpx)
	  gpx->preamble = preamble;
}

void gpx_set_start(Gpx *gpx, int head)
{
     if(gpx)
	  gpx->nostart = head ? 0 : 1;
}

void gpx_set_end(Gpx *gpx, int tail)
{
     if(gpx)
	  gpx->noend = tail ? 0 : 1;
}
