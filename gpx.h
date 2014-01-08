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

#ifndef __gpx_h__
#define __gpx_h__

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>
#include <stdio.h>
    
#define GPX_VERSION "2.0-alpha"
#define HOST_VERSION 50

#define END_OF_FILE 1
#define SUCCESS 0
#define ERROR -1

#define ESIOWRITE -2
#define ESIOREAD -3
#define ESIOFRAME -4
#define ESIOCRC -5

#define STREAM_VERSION_HIGH 0
#define STREAM_VERSION_LOW 0
    
#define COMMAND_QUE_MAX 20
    
// Nonzero to 'simulate' RPM using 5D, zero to disable
    
#define ENABLE_SIMULATED_RPM 1
    
// Nonzero to trigger tool changes on wait, zero to disable
    
#define ENABLE_TOOL_CHANGE_ON_WAIT 0
    
// BOUNDS CHECKING VARIABLES
    
#define NOZZLE_MAX 280
#define NOZZLE_TIME 0.6
#define HBP_MAX 120
#define HBP_TIME 6
#define AMBIENT_TEMP 24
#define ACCELERATION_TIME 1.15

#define MAX_TIMEOUT 0xFFFF
    
#ifdef _WIN32
#   define PATH_DELIM '\\'
#   define EOL "\r\n"
#else
#   define PATH_DELIM '/'
#   define EOL "\n"
#endif
    
// x3g axes bitfields
    
#define X_IS_SET 0x1
#define Y_IS_SET 0x2
#define Z_IS_SET 0x4
#define A_IS_SET 0x8
#define B_IS_SET 0x10
    
#define XYZ_BIT_MASK 0x7
#define AXES_BIT_MASK 0x1F
    
#define E_IS_SET 0x20
#define F_IS_SET 0x40
#define P_IS_SET 0x100
#define R_IS_SET 0x400
#define S_IS_SET 0x800
    
// commands
    
#define G_IS_SET 0x1000
#define M_IS_SET 0x2000
#define T_IS_SET 0x4000
    
#define COMMENT_IS_SET 0x8000
    
    typedef struct tPoint2d {
        double a;
        double b;
    } Point2d, *Ptr2d;
    
    typedef struct tPoint3d {
        double x;
        double y;
        double z;
    } Point3d, *Ptr3d;
    
    typedef struct tPoint5d {
        double x;
        double y;
        double z;
        double a;
        double b;
    } Point5d, *Ptr5d;
    
    typedef struct tCommand {
        // parameters
        double x;
        double y;
        double z;
        double a;
        double b;
        
        double e;
        double f;
        
        double p;
        double r;
        double s;
        
        // commands
        unsigned g;
        unsigned m;
        unsigned t;
        
        // comments
        char *comment;
        
        // state
        int flag;
    } Command, *PtrCommand;
    
// endstop flags
    
#define ENDSTOP_IS_MIN 0
#define ENDSTOP_IS_MAX 1
    
// tool id
    
#define MAX_TOOL_ID 1
#define BUILD_PLATE_ID 2
    
// state
    
#define READY_STATE 0
#define RUNNING_STATE 1
#define ENDED_STATE 2
    
    typedef struct tAxis {
        double max_feedrate;
        double home_feedrate;
        double steps_per_mm;
        unsigned endstop;
    } Axis;
    
    typedef struct tExtruder {
        double max_feedrate;
        double steps_per_mm;
        double motor_steps;
        unsigned has_heated_build_platform;
    } Extruder;

    
#define MACHINE_TYPE_REPLICATOR_1 7
#define MACHINE_TYPE_REPLICATOR_2 9

    typedef struct tMachine {
        Axis x;
        Axis y;
        Axis z;
        Extruder a;
        Extruder b;
        double nominal_filament_diameter;
        double nominal_packing_density;
        double nozzle_diameter;
        unsigned extruder_count;
        unsigned timeout;
        unsigned type;
    } Machine;
    
    typedef struct tTool {
        unsigned motor_enabled;
#if ENABLE_SIMULATED_RPM
        unsigned rpm;
#endif
        unsigned nozzle_temperature;
        unsigned build_platform_temperature;
    } Tool;
    
    typedef struct tOverride {
        double actual_filament_diameter;
        double filament_scale;
        double packing_density;
        unsigned standby_temperature;
        unsigned active_temperature;
        unsigned build_platform_temperature;
    } Override;
    
    typedef struct tFilament {
        char *colour;
        double diameter;
        unsigned temperature;
        unsigned LED;
    } Filament;
    
#define FILAMENT_MAX 32
    
    typedef struct tCommandAt {
        double z;
        unsigned filament_index;
        unsigned nozzle_temperature;
        unsigned build_platform_temperature;
    } CommandAt;
    
#define COMMAND_AT_MAX 128
    
#define BUFFER_MAX 1023
    
    // GPX CONTEXT
    
    typedef struct tGpx Gpx;
    
    struct tGpx {
        
        // IO
        
        struct {
            char in[BUFFER_MAX + 1];
            char out[BUFFER_MAX + 1];
            char *ptr;            
        } buffer;
        
        // DATA
        
        Machine machine;        // machine definition
        
        Command command;        // the gcode command line
        
        struct {
            Point5d position;   // the target position the extruder will move to (including G10 offsets)
            int extruder;       // the target extruder (on the virtual tool carosel)
        } target;
        
        struct {
            Point5d position;   // the current position of the extruder in 5D space
            double feedrate;    // the current feed rate
            int extruder;       // the currently selected extruder being used by the bot
            int offset;         // current G10 offset
            unsigned percent;   // current percent progress
        } current;
        
        struct {
            unsigned int positionKnown;  // axis bitfields for known positions of the extruder
            unsigned int mask;
        } axis;
        
        Point2d excess;         // the accumulated rounding error in mm to step conversion
        Point3d offset[7];      // G10 offsets
        struct {
            Point3d offset;     // command line offset
            double scale;       // command line scale
        } user;
        Tool tool[2];           // tool state
        Override override[2];   // gcode override
        
        Filament filament[FILAMENT_MAX];
        int filamentLength;
        
        CommandAt commandAt[COMMAND_AT_MAX];
        int commandAtIndex;
        int commandAtLength;
        double commandAtZ;
        
        // SETTINGS
        
        char *sdCardPath;
        char *buildName;

        struct {
            unsigned relativeCoordinates:1; // signals relitive or absolute coordinates
            unsigned extruderIsRelative:1;  // signals relitive or absolute coordinates for extruder
            unsigned reprapFlavor:1;    // reprap gcode flavor
            unsigned dittoPrinting:1;   // enable ditto printing
            unsigned buildProgress:1;   // override build percent
            unsigned verboseMode:1;     // verbose output
            unsigned logMessages:1;     // enable stderr message logging
            unsigned rewrite5D:1;       // calculate 5D E values rather than scaling them
        
        // STATE
            unsigned programState:8;    // gcode program state used to trigger start and end code sequences
            unsigned doPauseAtZPos:8;   // signals that a pause is ready to be
            unsigned pausePending:1;    // signals a pause is pending before the macro script has started
            unsigned macrosEnabled:1;   // M73 P1 or ;@body encountered signalling body start (so we don't pause during homing)
            unsigned loadMacros:1;      // used by the multi-pass converter to maintain state
            unsigned runMacros:1;       // used by the multi-pass converter to maintain state
            unsigned framingEnabled:1;  // enable framming of packets with header and crc
        } flag;


        double layerHeight;     // the current layer height
        unsigned lineNumber;    // the current line number
        int longestDDA;
        
        // STATISTICS
        
        struct {
            double a;
            double b;
            double time;
            unsigned long bytes;
        } accumulated;
        
        struct {
            double length;
            double time;
            unsigned long bytes;
        } total;

        // CALLBACK
        
        int (*callbackHandler)(Gpx *gpx, void *callbackData, char *buffer, size_t length);
        void *callbackData;
        
        // LOGGING
        
        FILE *log;
    };

    void gpx_initialize(Gpx *gpx, int firstTime);
    int gpx_set_machine(Gpx *gpx, char *machine);
   
    int gpx_set_property(Gpx *gpx, const char* section, const char* property, char* value);
    int gpx_load_config(Gpx *gpx, const char *filename);

    void gpx_register_callback(Gpx *gpx, int (*callbackHandler)(Gpx *gpx, void *callbackData, char *buffer, size_t length), void *callbackData);
    
    void gpx_start_convert(Gpx *gpx, char *buildName);

    int gpx_convert_line(Gpx *gpx, char *gcode_line);
    int gpx_convert(Gpx *gpx, FILE *file_in, FILE *file_out, FILE *file_out2);
    int gpx_convert_and_send(Gpx *gpx, FILE *file_in, int sio_port);

    void gpx_end_convert(Gpx *gpx);
    
    int eeprom_load_config(Gpx *gpx, const char *filename);
    
#ifdef __cplusplus
}
#endif

#endif /* __gpx_h__ */
