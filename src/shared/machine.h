//
//  machines.h
//
//  Created by WHPThomas <me(at)henri(dot)net> on 1/04/13.
//  Copyright (c) 2013 WHPThomas, All rights reserved.
//
//  Updates 2015 by Dan Newman, <dan(dot)newman(at)mtbaldy(dot)us>
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

#ifndef __machine_h__
#define __machine_h__

#define MACHINE_TYPE_NONE           0
#define MACHINE_TYPE_CUPCAKE_G3     1  // Cupcake Gen 3 electronics
#define MACHINE_TYPE_CUPCAKE_G4     2  // Cupcake Gen 4 electronics
#define MACHINE_TYPE_CUPCAKE_P4     3  // Cupcake Gen 4 + Pololu
#define MACHINE_TYPE_CUPCAKE_PP     4  // Cupcake Gen 3 + Pololu
#define MACHINE_TYPE_THINGOMATIC_6  5  // ToM Mk7 Single
#define MACHINE_TYPE_THINGOMATIC_7  6  // ToM Mk7 Single
#define MACHINE_TYPE_THINGOMATIC_7D 7  // ToM Mk7 Dual
#define MACHINE_TYPE_REPLICATOR_1   8  // Rep 1 Single
#define MACHINE_TYPE_REPLICATOR_1D  9  // Rep 1 Dual
#define MACHINE_TYPE_REPLICATOR_2  10  // Rep 2
#define MACHINE_TYPE_REPLICATOR_2H 11  // Rep 2 w/HBP
#define MACHINE_TYPE_REPLICATOR_2X 12  // Rep 2X
#define MACHINE_TYPE_CORE_XY       13  // Core XY Single w/HBP
#define MACHINE_TYPE_CORE_XYSZ     14  // Core XY Single w/HBP, slower Z
#define MACHINE_TYPE_ZYYX          15  // ZYYX Single
#define MACHINE_TYPE_ZYYX_D        16  // ZYYX Dual
#define MACHINE_TYPE_CLONE_R1      17  // Clone R1 Single w/HBP
#define MACHINE_TYPE_CLONE_R1D     18  // Clone R1 Dual w/HBP
#define MACHINE_TYPE_ZYYX_PRO      19  // ZYYX Pro

// endstop flags
#define ENDSTOP_IS_MIN 0
#define ENDSTOP_IS_MAX 1

typedef struct {
     double max_feedrate;	// mm/minute
     double max_accel;		// mm/s^2
     double max_speed_change;	// mm/s
     double home_feedrate;	// mm/minute
     double length;		// mm
     double steps_per_mm;	// steps/mm
     unsigned endstop;
} Axis;

typedef struct {
     double max_feedrate;	// mm/minute
     double max_accel;		// mm/s^2
     double max_speed_change;	// mm/s
     double steps_per_mm;	// steps/mm
     double motor_steps;        // microsteps per revolution
     unsigned has_heated_build_platform;
} Extruder;

typedef struct {
     const char *type;
     const char *desc;
     int free_type, free_desc;
     Axis x;
     Axis y;
     Axis z;
     Extruder a;
     Extruder b;
     double nominal_filament_diameter;
     double nominal_packing_density;
     double nozzle_diameter;
     double toolhead_offsets[3];
     double jkn[2];
     unsigned extruder_count;
     unsigned timeout;
     unsigned id;
} Machine;

typedef struct {
    const char *alias;
    const char *type;
    const char *desc;
} MachineAlias;

#endif
