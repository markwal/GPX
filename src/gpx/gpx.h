//
//  gpx.h
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
#include <stdarg.h>
#include "vector.h"
#include "config.h"

#if defined(SERIAL_SUPPORT)
#if !defined(_WIN32) && !defined(_WIN64)
#include <termios.h>
#else
#include "winsio.h"
#endif
#else
typedef long speed_t;
#define B115200 115200
#define B57600  57600
#define NO_SERIAL_SUPPORT_MSG "Serial I/O and USB printing is not supported " \
     "by this build of GPX"
#endif

#define HOST_VERSION 50

#define END_OF_FILE 1
#define SUCCESS 0
#define ERROR -1

#define ESIOWRITE -2
#define ESIOREAD -3
#define ESIOFRAME -4
#define ESIOCRC -5
// EOSERROR means error code is in errno
#define EOSERROR -6
#define ESIOTIMEOUT -7
#define ESIOBADBAUD -8

// Item codes for passing control options
#define ITEM_FRAMING_ENABLE 1
#define ITEM_FRAMING_DISABLE 2

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
#define HBP_MAX 130
#define HBP_TIME 6
#define AMBIENT_TEMP 24
#define ACCELERATION_TIME 1.15

#define MAX_TIMEOUT 0xFFFF

// Define some elapsed times in terms of nanoseconds

#define NS_500MS (500000000L)
#define NS_250MS (250000000L)
#define NS_100MS (100000000L)
#define NS_10MS  (10000000L)


#if !defined(SPEED_T_DEFINED)
#if defined(_WIN32) || defined(_WIN64)
typedef long speed_t;
#define SPEED_T_DEFINED
#endif
#endif

#ifdef _WIN32
#   define PATH_DELIM '\\'
#   define EOL "\n"
#else
#   define PATH_DELIM '/'
#   define EOL "\n"
#endif

#define SHOW(FN) if(gpx->flag.logMessages) {FN;}
#define VERBOSE(FN) if(gpx->flag.verboseMode && gpx->flag.logMessages) {FN;}

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
#define ARG_IS_SET 0x10000

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

        // argument
        char *arg;
    } Command, *PtrCommand;

// tool id

#define MAX_TOOL_ID 1
#define BUILD_PLATE_ID 2

// state

#define READY_STATE 0
#define RUNNING_STATE 1
#define ENDED_STATE 2

#include "machine.h"
#include "eeprominfo.h"

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
        unsigned extrusion_factor; // set by M221
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
    typedef struct tSio Sio;

    struct tGpx {

        // IO

        struct {
            char in[BUFFER_MAX + 1];
            char out[BUFFER_MAX + 1];
            char *ptr;
        } buffer;

        int open_delay;

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
            unsigned speed_factor; // set by M220
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

        // vector (dynamic array) of eeprom mappings defined by @eeprom macro
        vector *eepromMappingVector;

        // builtin eeprom map
        EepromMap *eepromMap;

	const char *preamble;
	int nostart, noend;

        // SETTINGS

        char *sdCardPath;
        char *buildName;
        char *iniPath;

        struct {
            unsigned relativeCoordinates:1; // signals relative or absolute coordinates
            unsigned extruderIsRelative:1;  // signals relative or absolute coordinates for extruder
            unsigned reprapFlavor:1;    // reprap gcode flavor
            unsigned dittoPrinting:1;   // enable ditto printing
            unsigned buildProgress:1;   // override build percent
            unsigned verboseMode:1;     // verbose output
            unsigned logMessages:1;     // enable stderr message logging
            unsigned verboseSioMode:1;  // extra verbose for serial packets back and forth
            unsigned rewrite5D:1;       // calculate 5D E values rather than scaling them
            unsigned M106AlwaysValve:1; // force M106 to reprap flavor even in makerbot mode
            unsigned onlyExplicitToolChange:1; // no implicit tool change when Tn used as a parameter

        // STATE
            unsigned programState:8;    // gcode program state used to trigger start and end code sequences
            unsigned doPauseAtZPos:8;   // signals that a pause is ready to be
            unsigned pausePending:1;    // signals a pause is pending before the macro script has started
            unsigned macrosEnabled:1;   // M73 P1 or ;@body encountered signalling body start (so we don't pause during homing)
            unsigned loadMacros:1;      // used by the multi-pass converter to maintain state
            unsigned runMacros:1;       // used by the multi-pass converter to maintain state
            unsigned framingEnabled:1;  // enable framming of packets with header and crc
            unsigned sioConnected:1;    // connected to the bot
            unsigned sd_paused:1;       // printing from sd paused
            unsigned ignoreAbsoluteMoves:1; // until a coordinate system is defined via G92 or M132
        } flag;


        double layerHeight;     // the current layer height
        unsigned lineNumber;    // the current line number
        int longestDDA;
        char *selectedFilename; // parameter from M23 - allocated, so free before replace

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
        int (*resultHandler)(Gpx *gpx, void *callbackData, const char *fmt, va_list ap);
        struct tSio *sio;

        // LOGGING

        FILE *log;
    };

    struct tSio {
        FILE *in;
        int port;
        unsigned bytes_out;
        unsigned bytes_in;
        struct {
            unsigned retryBufferOverflow: 1;
            unsigned shortRetryBufferOverflowOnly : 1;
        } flag;

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

    };


// Note on cancellation states
// So, there are a few things that are tricky about cancellation. First, we need
// three different threads of execution to know that there is a cancel and take
// an action. Any of the three can initiate, but all need to complete before
// we're back to normal.
// 1. The host control loop needs to stop sending new commands and will send
// (@clear_cancel) when to acknowledge that it is no longer sending new commands
// from the cancelled job
// 2. The host event handler loop is asynchronous with the control loop will
// send an out of order M112 to cancel.  This is done to interrupt any long
// running operations that the control loop will just wait for (like homing or
// heating).  It should do nothing when the printer initiates a cancel.
// 3. The printer itself can initiate a cancel by returning 0x89 from a serial
// communication.  It may do this, for example, because the user used the LCD to
// cancel the print. Complicating matters is that it will also return 0x89 to
// acknowledge that it has complied with a host request to cancel and it takes
// suprisingly long time for it to do that.
//
// so we have three bits that get set to say we're still waiting for one of the
// threads, more than one may be set simultaneously
// flag.cancelPending - waiting for the control loop to send (@clear_cancel)
//      marking the end of the send stream
// waitflag.waitForCancelSync - waiting for the event loop to send the M112 via
//      gpx_abort
// waitflag.waitForBotCancel - waiting for the bot to reply with 0x89
//
// two may be set simultaneously

// In addition, on cancel we need to wait for the bot to quiesce (if clear build
// EEPROM is set) and discover our position again.

    // Sttb - string table
    // need one of these to hang onto the filename list so that we can emulate
    // case insensitivty since some host software expects that of gcode M23
    typedef struct tSttb
    {
        char **rgs;         // array of pointers
        size_t cb;          // count of bytes - allocated size of rgs
        size_t cb_expand;   // count of bytes to expand by each expansion
        long cs;            // count of strings currently stored Assert(cs * sizeof(char *) <= cb)
    } Sttb;

    // Tr - temperature reading for tool or bed
    typedef struct tTr
    {
        unsigned temperature;
        unsigned target;
    } Tr;

    // Tio - translated serial io
    // wraps Sio and adds translation output buffer
    // translation is reprap style response
    typedef struct tTio
    {
        char translation[BUFFER_MAX + 1];
        size_t cur;
        Sio sio;
        union {
            unsigned flags;
            struct {
                unsigned listingFiles:1;      // in the middle of an M20 response
                unsigned getPosWhenReady:1;   // waiting for queue to drain to get position
                unsigned cancelPending:1;     // we're eating everything until the host says it has stopped sending stuff (@clear_cancel)
                unsigned okPending:1;         // we want the ok to come at the end of the response
                unsigned waitClearedByCancel:1; // recheck wait state
                unsigned clear_on_estop_set:1;// eeprom says that the bot clears on estop, so no abs moves until G92/M132 after cancel
            } flag;
        };
        union {
            unsigned waiting;
            struct {
                unsigned waitForPlatform:1;
                unsigned waitForExtruderA:1;
                unsigned waitForExtruderB:1;
                unsigned waitForButton:1;
                unsigned waitForStart:1;
                unsigned waitForEmptyQueue:1;
                unsigned waitForCancelSync:1;  // the host came through (@clear_cancel) before the M112 so we're still waiting to send the abort
                unsigned waitForBotCancel:1;   // we told the bot to cancel, so we're waiting for an 0x89 response
                unsigned waitForBuffer:1;      // the firmware's buffer is full
                unsigned waitForUnpause:1;     // the bot is paused
            } waitflag;
        };
        Sttb sttb;
        time_t sec;
        Tr tool_tr[2];
        Tr bed_tr;
        Gpx *gpx;
        int upstream;
    } Tio;

    // 23 - Get build statistics: build state values
    // From sailfish sources (Host.hh)
    enum BuildState {
        BUILD_NONE = 0,
        BUILD_RUNNING = 1,
        BUILD_FINISHED_NORMALLY = 2,
        BUILD_PAUSED = 3,
        BUILD_CANCELED = 4,
        BUILD_CANCELLING = 5,
    };

    void gpx_initialize(Gpx *gpx, int firstTime);
    int gpx_set_machine(Gpx *gpx, const char *machine, int init);

    int gpx_set_property(Gpx *gpx, const char* section, const char* property, char* value);
    int gpx_load_config(Gpx *gpx, const char *filename);

    int gpx_sio_open(Gpx *gpx, const char *filename, speed_t baud_rate, int *sio_port);
    int ready_to_read(int fd);
    int port_handler(Gpx *gpx, Sio *sio, char *buffer, size_t length);

    void gpx_register_callback(Gpx *gpx, int (*callbackHandler)(Gpx *gpx, void *callbackData, char *buffer, size_t length), void *callbackData);

    void gpx_start_convert(Gpx *gpx, char *buildName, int item_code, ...);

    int gpx_daemon(Gpx *gpx, int create_daemon_port, const char *daemon_port, const char *printer_port, speed_t baudrate);
    int gpx_convert_line(Gpx *gpx, char *gcode_line);
    int gpx_convert(Gpx *gpx, FILE *file_in, FILE *file_out, FILE *file_out2);
    int gpx_convert_and_send(Gpx *gpx, FILE *file_in, int sio_port, int item_code, ...);

    void gpx_end_convert(Gpx *gpx);

    void gpx_list_machines(FILE *fp);

    int eeprom_load_config(Gpx *gpx, const char *filename);

    void gpx_set_preamble(Gpx *gpx, const char *preamble);

    void gpx_set_start(Gpx *gpx, int head);

    void gpx_set_end(Gpx *gpx, int tail);

    int get_next_filename(Gpx *gpx, unsigned restart);
    int get_advanced_version_number(Gpx *gpx);
    char *get_build_status(unsigned int status);
    int is_extruder_ready(Gpx *gpx, unsigned extruder_id);
    int is_build_platform_ready(Gpx *gpx, unsigned extruder_id);
    int get_build_statistics(Gpx *gpx);
    int get_motherboard_status(Gpx *gpx);
    int is_ready(Gpx *gpx);
    char *get_sd_status(unsigned int status);
    Machine *gpx_find_machine(const char *machine);
    int abort_immediately(Gpx *gpx);
    int set_build_platform_temperature(Gpx *gpx, unsigned extruder_id, unsigned temperature);
    int set_nozzle_temperature(Gpx *gpx, unsigned extruder_id, unsigned temperature);
    int delay(Gpx *gpx, unsigned milliseconds);
    int clear_buffer(Gpx *gpx);
    int get_extended_position(Gpx *gpx);
    int pause_resume(Gpx *gpx);
    int extended_stop(Gpx *gpx, unsigned halt_steppers, unsigned clear_queue);
    int set_position(Gpx *gpx);
    int set_build_progress(Gpx *gpx, unsigned percent);
    int end_build(Gpx *gpx);
    EepromMap *find_eeprom_map(Gpx *gpx);
    int load_eeprom_map(Gpx *gpx);
    int read_eeprom(Gpx *gpx, unsigned address, unsigned length);
    int write_eeprom(Gpx *gpx, unsigned address, char *data, unsigned length);
    int write_eeprom_8(Gpx *gpx, Sio *sio, unsigned address, unsigned char value);
    int read_eeprom_8(Gpx *gpx, Sio *sio, unsigned address, unsigned char *value);
    int write_eeprom_16(Gpx *gpx, Sio *sio, unsigned address, unsigned short value);
    int read_eeprom_16(Gpx *gpx, Sio *sio, unsigned address, unsigned short *value);
    int write_eeprom_fixed_16(Gpx *gpx, Sio *sio, unsigned address, float value);
    int read_eeprom_fixed_16(Gpx *gpx, Sio *sio, unsigned address, float *value);
    int write_eeprom_32(Gpx *gpx, Sio *sio, unsigned address, unsigned long value);
    int read_eeprom_32(Gpx *gpx, Sio *sio, unsigned address, unsigned long *value);
    int write_eeprom_float(Gpx *gpx, Sio *sio, unsigned address, float value);
    int read_eeprom_float(Gpx *gpx, Sio *sio, unsigned address, float *value);

    Tio *tio_initialize(Gpx *gpx);
    void tio_cleanup(Tio *tio);
    void tio_clear_state_for_cancel(Tio *tio);
    int tio_printf(Tio *tio, char const* fmt, ...);
    int tio_log_printf(Tio *tio, char const* fmt, ...);
    int gpx_connect(Gpx *gpx, const char *printer_port, speed_t speed);
    int gpx_return_translation(Gpx *gpx, int rval);
    int gpx_write_string_core(Gpx *gpx, const char *s);
    int gpx_write_string(Gpx *gpx, const char *s);
    int gcodeResult(Gpx *gpx, const char *fmt, ...);
    speed_t speed_from_long(long *baudrate);

    void short_sleep(long nsec);
    void long_sleep(time_t sec);

#ifdef __eeprominfo_h__
    EepromMapping *find_any_eeprom_mapping(Gpx *, char *name);
#endif
#ifdef __cplusplus
}
#endif

#endif /* __gpx_h__ */
