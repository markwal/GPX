//
//  classic_machines.h
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

#ifndef __classic_machines_h__
#define __classic_machines_h__

// Classic Incorrect Machine definitions
//
//    From http://github.com/dcnewman/GPX 727a9fff
//    MBI has incorrectly calculated the steps/mm for the X and Y axis for all
//    their printers.  They used the pulley's effective pitch diameter to
//    arrive at the values they used.  They should instead have computed
//    (total-steps-per-revolution)/(belt-pitch * tooth-count). These are the
//    the old values before the correct calculations.  Some people may want
//    to use these incorrect values if they've tweaked packing densities etc.
//    based on them.

static Machine wrong_thing_o_matic_7 = {
    "ot7", "TOM Mk7 - single extruder - original (incorrect) MakerBot steps per mm calculation", 0, 0,
    {9600, 500, 30, 500, 106, 47.069852, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 30, 500, 120, 47.069852, ENDSTOP_IS_MIN}, // y axis
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

static Machine wrong_thing_o_matic_7D = {
    "ot7d", "TOM Mk7 - dual extruder - original (incorrect) MakerBot steps per mm calculation", 0, 0,
    {9600, 500, 30, 500, 106, 47.069852, ENDSTOP_IS_MIN}, // x axis
    {9600, 500, 30, 500, 120, 47.069852, ENDSTOP_IS_MIN}, // y axis
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

static Machine wrong_replicator_1 = {
    "or1", "Replicator 1 - single extruder - original (incorrect) MakerBot steps per mm calculation", 0, 0,
    {18000, 1000, 15, 2500, 227, 94.139704, ENDSTOP_IS_MAX}, // x axis
    {18000, 1000, 15, 2500, 148, 94.139704, ENDSTOP_IS_MAX}, // y axis
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

static Machine wrong_replicator_1D = {
    "or1d", "Replicator 1 - dual extruder - original (incorrect) MakerBot steps per mm calculation", 0, 0,
    {18000, 1000, 15, 2500, 227, 94.139704, ENDSTOP_IS_MAX}, // x axis
    {18000, 1000, 15, 2500, 148, 94.139704, ENDSTOP_IS_MAX}, // y axis
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

static Machine wrong_replicator_2 = {
    "or2", "Replicator 2 (default) - original (incorrect) MakerBot steps per mm calculation", 0, 0,
    {18000, 1000, 15, 2500, 285, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 1000, 15, 2500, 152, 88.573186, ENDSTOP_IS_MAX}, // y axis
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

static Machine wrong_replicator_2H = {
    "or2h", "Replicator 2 with HBP - original (incorrect) MakerBot steps per mm calculation", 0, 0,
    {18000, 1000, 15, 2500, 285, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 1000, 15, 2500, 152, 88.573186, ENDSTOP_IS_MAX}, // y axis
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

static Machine wrong_replicator_2X = {
    "or2x", "Replicator 2X - original (incorrect) MakerBot steps per mm calculation", 0, 0, 
    {18000, 1000, 15, 2500, 246, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 1000, 15, 2500, 152, 88.573186, ENDSTOP_IS_MAX}, // y axis
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

#if defined(MACHINE_ARRAY)
static Machine *wrong_machines[] = {
     &wrong_replicator_1,
     &wrong_replicator_1D,
     &wrong_replicator_2,
     &wrong_replicator_2H,
     &wrong_replicator_2X,
     &wrong_thing_o_matic_7,
     &wrong_thing_o_matic_7D,
     NULL
};
#endif

#endif
