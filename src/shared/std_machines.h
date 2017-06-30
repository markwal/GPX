//
//  std_machines.h
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

#ifndef __std_machines_h__
#define __std_machines_h__

#include "machine.h"

static Machine cupcake_G3 = {
     "c3", "Cupcake Gen3 XYZ, Mk5/6 + Gen4 Extruder", 0, 0,
    {9600, 500, 30, 500, 100, 11.767463, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 30, 500, 100, 11.767463, ENDSTOP_IS_MIN}, // y axis
    {450, 100, 25, 450, 100, 320, ENDSTOP_IS_MIN},        // z axis
    {7200, 1000, 30, 50.235478806907409, 400, 1}, // a extruder
    {7200, 1000, 30, 50.235478806907409, 400, 1}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    {0, 0, 0}, // toolhead offsets
    {0.0085, 0.0090},  // JKN
    1,  // extruder count
    20, // timeout
    MACHINE_TYPE_CUPCAKE_G3
};

static Machine cupcake_G4 = {
    "c4", "Cupcake Gen4 XYZ, Mk5/6 + Gen4 Extruder", 0, 0,
    {9600, 500, 30, 500, 100, 47.069852, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 30, 500, 100, 47.069852, ENDSTOP_IS_MIN}, // y axis
    {450, 100, 25, 450, 100, 1280, ENDSTOP_IS_MIN},        // z axis
    {7200, 1000, 30, 50.235478806907409, 400, 1}, // a extruder
    {7200, 1000, 30, 50.235478806907409, 400, 1}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    {0, 0, 0}, // toolhead offsets
    {0.0085, 0.0090},  // JKN
    1,  // extruder count
    20, // timeout
    MACHINE_TYPE_CUPCAKE_G4
};

static Machine cupcake_P4 = {
    "cp4", "Cupcake Pololu XYZ, Mk5/6 + Gen4 Extruder", 0, 0,
    {9600, 500, 30, 500, 100, 94.13970462, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 30, 500, 100, 94.13970462, ENDSTOP_IS_MIN}, // y axis
    {450, 100, 25, 450, 100, 2560, ENDSTOP_IS_MIN},        // z axis
    {7200, 1000, 30, 50.235478806907409, 400, 1}, // a extruder
    {7200, 1000, 30, 50.235478806907409, 400, 1}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    {0, 0, 0}, // toolhead offsets
    {0.0085, 0.0090},  // JKN
    1,  // extruder count
    20, // timeout
    MACHINE_TYPE_CUPCAKE_P4
};

static Machine cupcake_PP = {
    "cpp", "Cupcake Pololu XYZ, Mk5/6 + Pololu Extruder", 0, 0,
    {9600, 500, 30, 500, 100, 47.069852, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 30, 500, 100, 47.069852, ENDSTOP_IS_MIN}, // y axis
    {450, 100, 25, 450, 100, 1280, ENDSTOP_IS_MIN},        // z axis
    {7200, 1000, 30, 100.470957613814818, 400, 1}, // a extruder
    {7200, 1000, 30, 100.470957613814818, 400, 1}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    {0, 0, 0}, // toolhead offsets
    {0.0085, 0.0090},  // JKN
    1,  // extruder count
    20, // timeout
    MACHINE_TYPE_CUPCAKE_PP
};

static Machine thing_o_matic_6 = {
    "t6", "TOM Mk6 - single extruder", 0, 0,
    {9600, 500, 30, 500, 106, 47.058824, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 30, 500, 120, 47.058824, ENDSTOP_IS_MIN}, // y axis
    {1000, 150, 10, 500, 106, 200, ENDSTOP_IS_MAX},        // z axis
    {1600, 1000, 30, 50.235478806907409, 1600, 1}, // a extruder
    {1600, 1000, 30, 50.235478806907409, 1600, 1}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    {0, 0, 0}, // toolhead offsets
    {0.0070, 0.0040},  // JKN
    1,  // extruder count
    20, // timeout
    MACHINE_TYPE_THINGOMATIC_6
};

static Machine thing_o_matic_7 = {
    "t7", "TOM Mk7 - single extruder", 0, 0,
    {9600, 500, 30, 500, 106, 47.058824, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 30, 500, 120, 47.058824, ENDSTOP_IS_MIN}, // y axis
    {1000, 150, 10, 500, 106, 200, ENDSTOP_IS_MAX},        // z axis
    {1600, 1000, 30, 50.235478806907409, 1600, 1}, // a extruder
    {1600, 1000, 30, 50.235478806907409, 1600, 1}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    {0, 0, 0}, // toolhead offsets
    {0.0070, 0.0040},  // JKN
    1,  // extruder count
    20, // timeout
    MACHINE_TYPE_THINGOMATIC_7
};

static Machine thing_o_matic_7D = {
    "t7d", "TOM Mk7 - dual extruder", 0, 0,
    {9600, 500, 30, 500, 106, 47.058824, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 30, 500, 120, 47.058824, ENDSTOP_IS_MIN}, // y axis
    {1000, 150, 10, 500, 106, 200, ENDSTOP_IS_MAX},        // z axis
    {1600, 1000, 30, 50.235478806907409, 1600, 1}, // a extruder
    {1600, 1000, 30, 50.235478806907409, 1600, 1}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    {33, 0, 0}, // toolhead offsets
    {0.0070, 0.0040},  // JKN
    2,  // extruder count
    20, // timeout
    MACHINE_TYPE_THINGOMATIC_7D
};

static Machine replicator_1 = {
    "r1", "Replicator 1 - single extruder", 0, 0,
    {18000, 1000, 15, 2500, 227, 94.117647, ENDSTOP_IS_MAX}, // x axis
    {18000, 1000, 15, 2500, 148, 94.117647, ENDSTOP_IS_MAX}, // y axis
    {1170, 150, 10, 1100, 150, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    {0, 0, 0}, // toolhead offsets
    {0.0050, 0.0550},  // JKN
    1,  // extruder count
    20, // timeout
    MACHINE_TYPE_REPLICATOR_1
};

static Machine replicator_1D = {
    "r1d", "Replicator 1 - dual extruder", 0, 0,
    {18000, 1000, 15, 2500, 227, 94.117647, ENDSTOP_IS_MAX}, // x axis
    {18000, 1000, 15, 2500, 148, 94.117647, ENDSTOP_IS_MAX}, // y axis
    {1170, 150, 10, 1100, 150, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    {33, 0, 0}, // toolhead offsets
    {0.0050, 0.0550},  // JKN
    2,  // extruder count
    20, // timeout
    MACHINE_TYPE_REPLICATOR_1D
};

static Machine replicator_2 = {
    "r2", "Replicator 2 (default)", 0, 0,
    {18000, 1000, 15, 2500, 285, 88.888889, ENDSTOP_IS_MAX}, // x axis
    {18000, 1000, 15, 2500, 152, 88.888889, ENDSTOP_IS_MAX}, // y axis
    {1170, 150, 10, 1100, 155, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 0}, // a extruder
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    1.75, // nominal filament diameter
    0.97, // nominal packing density
    0.4, // nozzle diameter
    {0, 0, 0}, // toolhead offsets
    {0.0050, 0.0550},  // JKN
    1,  // extruder count
    20, // timeout
    MACHINE_TYPE_REPLICATOR_2
};

static Machine replicator_2H = {
    "r2h", "Replicator 2 with HBP", 0, 0,
    {18000, 1000, 15, 2500, 285, 88.888889, ENDSTOP_IS_MAX}, // x axis
    {18000, 1000, 15, 2500, 152, 88.888889, ENDSTOP_IS_MAX}, // y axis
    {1170, 150, 10, 1100, 155, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // b extruder
    1.75, // nominal filament diameter
    0.97, // nominal packing density
    0.4, // nozzle diameter
    {0, 0, 0}, // toolhead offsets
    {0.0050, 0.0550},  // JKN
    1,  // extruder count
    20, // timeout
    MACHINE_TYPE_REPLICATOR_2H
};

static Machine replicator_2X = {
    "r2x", "Replicator 2X", 0, 0, 
    {18000, 1000, 15, 2500, 246, 88.888889, ENDSTOP_IS_MAX}, // x axis
    {18000, 1000, 15, 2500, 152, 88.888889, ENDSTOP_IS_MAX}, // y axis
    {1170, 150, 10, 1100, 155, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    {35, 0, 0}, // toolhead offsets
    {0.0050, 0.0550},  // JKN
    2,  // extruder count
    20, // timeout
    MACHINE_TYPE_REPLICATOR_2X
};

// Core-XY machine, 18 tooth GT2 timing pulleys for X and Y
static Machine core_xy = {
    "cxy", "Core-XY with HBP - single extruder", 0, 0, 
    {18000, 1000, 15, 2500, 200, 88.888889, ENDSTOP_IS_MAX}, // x axis
    {18000, 1000, 15, 2500, 200, 88.888889, ENDSTOP_IS_MAX}, // y axis
    {1170, 150, 10, 1100, 200, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    {0, 0, 0}, // toolhead offsets
    {0.0050, 0.0550},  // JKN
    1,  // extruder count
    20, // timeout
    MACHINE_TYPE_CORE_XY
};

// Core-XY machine with a slow Z axis ("sz"), 18T GT2 pulleys for X and Y
static Machine core_xysz = {
    "cxysz", "Core-XY with HBP - single extruder, slow Z", 0, 0, 
    {18000, 1000, 15, 2500, 200, 88.888889, ENDSTOP_IS_MAX}, // x axis
    {18000, 1000, 15, 2500, 200, 88.888889, ENDSTOP_IS_MAX}, // y axis
    {600, 150, 10, 600, 200, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // b extruder
    1.75, // nominal filament diameter
    0.85, // nominal packing density
    0.4, // nozzle diameter
    {0, 0, 0}, // toolhead offsets
    {0.0050, 0.0550},  // JKN
    1,  // extruder count
    20, // timeout
    MACHINE_TYPE_CORE_XYSZ
};

// ZYYX 3D printer, single extruder, 18T GT2 pulleys for X and Y
static Machine zyyx = {
    "z", "ZYYX - single extruder", 0, 0, 
    {18000, 850, 12, 2500, 270, 88.888889, ENDSTOP_IS_MAX}, // x axis
    {18000, 850, 12, 2500, 230, 88.888889, ENDSTOP_IS_MAX}, // y axis
    {1170, 50, 12, 1100, 195, 400, ENDSTOP_IS_MIN},        // z axis
    {5000, 5000, 100, 96.275201870333662468889989185642, 3200, 0}, // a extruder
    {5000, 5000, 100, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    1.75, // nominal filament diameter
    0.97, // nominal packing density
    0.4, // nozzle diameter
    {0, 0, 0}, // toolhead offsets
    {0.0050, 0.0550},  // JKN
    1,  // extruder count
    20, // timeout
    MACHINE_TYPE_ZYYX
};

// ZYYX 3D printer, dual extruders, 18T GT2 pulleys for X and Y
static Machine zyyx_D = {
    "zd", "ZYYX - dual extruder", 0, 0, 
    {18000, 850, 12, 2500, 270, 88.888889, ENDSTOP_IS_MAX}, // x axis
    {18000, 850, 12, 2500, 230, 88.888889, ENDSTOP_IS_MAX}, // y axis
    {1170, 50, 12, 1100, 195, 400, ENDSTOP_IS_MIN},        // z axis
    {5000, 5000, 100, 96.275201870333662468889989185642, 3200, 0}, // a extruder
    {5000, 5000, 100, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    1.75, // nominal filament diameter
    0.97, // nominal packing density
    0.4, // nozzle diameter
    {33, 0, 0}, // toolhead offsets
    {0.0050, 0.0550},  // JKN
    2,  // extruder count
    20, // timeout
    MACHINE_TYPE_ZYYX_D
};

// ZYYX 3D printer, single extruder, 18T GT2 pulleys for X and Y
static Machine zyyx_pro = {
    "zp", "ZYYX pro", 0, 0, 
    {18000, 850, 12, 2500, 270, 88.888889, ENDSTOP_IS_MAX}, // x axis
    {18000, 850, 12, 2500, 230, 88.888889, ENDSTOP_IS_MAX}, // y axis
    {1170, 50, 12, 1100, 195, 400, ENDSTOP_IS_MIN},        // z axis
    {5000, 5000, 100, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {5000, 5000, 100, 96.275201870333662468889989185642, 3200, 1}, // b extruder
    1.75, // nominal filament diameter
    0.97, // nominal packing density
    0.4, // nozzle diameter
    {0, 0, 0}, // toolhead offsets
    {0.0050, 0.0550},  // JKN
    1,  // extruder count
    20, // timeout
    MACHINE_TYPE_ZYYX_PRO
};

static Machine clone_r1 = {
    "cr1", "Clone R1 Single with HBP", 0, 0,
    {18000, 1000, 15, 2500, 300, 88.888889, ENDSTOP_IS_MAX}, // x axis
    {18000, 1000, 15, 2500, 195, 88.888889, ENDSTOP_IS_MAX}, // y axis
    {1170, 150, 10, 1100, 210, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // b extruder
    1.75, // nominal filament diameter
    0.97, // nominal packing density
    0.4, // nozzle diameter
    {0, 0, 0}, // toolhead offsets
    {0.0050, 0.0550},  // JKN
    1,  // extruder count
    20, // timeout
    MACHINE_TYPE_CLONE_R1
};


static Machine clone_r1d = {
    "cr1d", "Clone R1 Dual with HBP", 0, 0,
    {18000, 1000, 15, 2500, 280, 88.888889, ENDSTOP_IS_MAX}, // x axis
    {18000, 1000, 15, 2500, 195, 88.888889, ENDSTOP_IS_MAX}, // y axis
    {1170, 150, 10, 1100, 210, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 2000, 20, 96.275201870333662468889989185642, 3200, 1}, // b extruder
    1.75, // nominal filament diameter
    0.97, // nominal packing density
    0.4, // nozzle diameter
    {33, 0, 0}, // toolhead offsets
    {0.0050, 0.0550},  // JKN
    2,  // extruder count
    20, // timeout
    MACHINE_TYPE_CLONE_R1D
};


#if defined(MACHINE_ARRAY)

static Machine *machines[] = {
     &cupcake_G3,
     &cupcake_G4,
     &cupcake_P4,
     &cupcake_PP,
     &core_xy,
     &core_xysz,
     &clone_r1,
     &clone_r1d,
     &replicator_1,
     &replicator_1D,
     &replicator_2,
     &replicator_2H,
     &replicator_2X,
     &thing_o_matic_6,
     &thing_o_matic_7,
     &thing_o_matic_7D,
     &zyyx,
     &zyyx_D,
     &zyyx_pro,
     NULL
};

#endif

#ifdef MACHINE_ALIAS_ARRAY

// FlashForge Creator Pro
static MachineAlias fcp = {
    "fcp", "r1d",
    "FlashForge Creator Pro"
};

static MachineAlias *machine_aliases[] = {
    &fcp,
    NULL
};

#endif

#endif
