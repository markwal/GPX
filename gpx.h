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

#ifndef gpx_gpx_h
#define gpx_gpx_h

#include <limits.h>

#define GPX_VERSION "0.1a"

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
#define L_IS_SET 0x80
#define P_IS_SET 0x100
#define Q_IS_SET 0x200
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

    double l;
    double p;
    double q;
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

typedef struct tMachine {
    Axis x;
    Axis y;
    Axis z;
    Extruder a;
    Extruder b;
    double filament_diameter;
    unsigned extruder_count;
    unsigned timeout;
} Machine;

typedef struct tTool {
    unsigned motor_enabled;
    unsigned rpm;
    unsigned nozzle_temperature;
    unsigned build_platform_temperature;
} Tool;

#define EOL "\n"

#endif
