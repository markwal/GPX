#ifndef __machine_h__
#define __machine_h__

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

typedef struct {
     double max_feedrate;	// mm/minute
     double max_accel;		// mm/s^2
     double max_speed_change;	// mm/minute
     double home_feedrate;	// mm/minute
     double length;		// mm
     double steps_per_mm;	// steps/mm
     unsigned endstop;
} Axis;

typedef struct {
     double max_feedrate;	// mm/minute
     double max_accel;		// mm/s^2
     double max_speed_change;	// mm/minute
     double steps_per_mm;	// steps/mm
     double motor_steps;        // microsteps per revolution
     unsigned has_heated_build_platform;
} Extruder;

typedef struct {
     const char *tag;
     const char *name;
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
     unsigned type;
} Machine;

#endif
