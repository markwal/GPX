//
//  gpx-main.c
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

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <strings.h>

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

#if defined(SERIAL_SUPPORT)
#include <termios.h>
#else
typedef long speed_t;
#define B115200 115200
#define B57600  57600
#define NO_SERIAL_SUPPORT_MSG "Serial I/O and USB printing is not supported " \
     "by this build of GPX"
#endif

#if defined(_WIN32) || defined(_WIN64)
#   include "getopt.h"
    // strcasecmp() is not provided on Windows
    // _stricmp() is equivalent but may require <string.h>
#   define strcasecmp _stricmp
#endif

#include "gpx.h"

// Global variables

static Gpx gpx;

static FILE *file_in = NULL;
static FILE *file_out = NULL;
static FILE *file_out2 = NULL;
static int sio_port = -1;

// cleanup code in case we encounter an error that causes the program to exit

static void exit_handler(void)
{
    // close open files
    if(file_in != stdin) {
        fclose(file_in);
        if(file_out != stdout) {
            if(ferror(file_out)) {
                perror("Error writing to output file");
            }
            fclose(file_out);
        }
        if(file_out2) {
            fclose(file_out2);
        }
    }

    // 23 February 2015
    // Assuming stdin=0, stdout=1, stderr=3 isn't always safe
    // Do not assume that sio_port > 2 means it needs to be
    // closed.  Instead, if it isn't negative, then it needs
    // to be closed.

    if(sio_port >= 0) {
        close(sio_port);
    }
}

// display usage and exit

static void usage(int err)
{
    FILE *fp = err ? stderr : stdout;
    err = err ? 1 : 0;
#if defined(SERIAL_SUPPORT)
#define SERIAL_MSG1 "s"
#define SERIAL_MSG2 "[-b BAUDRATE] "
#else
#define SERIAL_MSG1 ""
#define SERIAL_MSG2 ""
#endif

    fputs("GPX " GPX_VERSION EOL, fp);
    fputs("Copyright (c) 2013 WHPThomas, All rights reserved." EOL, fp);

    fputs("Additional changes Copyright (c) 2014, 2015 DNewman, All rights reserved." EOL, fp);

    fputs(EOL "This program is free software; you can redistribute it and/or modify" EOL, fp);
    fputs("it under the terms of the GNU General Public License as published by" EOL, fp);
    fputs("the Free Software Foundation; either version 2 of the License, or" EOL, fp);
    fputs("(at your option) any later version." EOL, fp);

    fputs(EOL "This program is distributed in the hope that it will be useful," EOL, fp);
    fputs("but WITHOUT ANY WARRANTY; without even the implied warranty of" EOL, fp);
    fputs("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the" EOL, fp);
    fputs("GNU General Public License for more details." EOL, fp);

    fputs(EOL "Usage:" EOL, fp);
    fputs("gpx [-dgilpqr" SERIAL_MSG1 "tvw] " SERIAL_MSG2 "[-c CONFIG] [-e EEPROM] [-f DIAMETER] [-m MACHINE] [-n SCALE] [-x X] [-y Y] [-z Z] IN [OUT]" EOL, fp);
    fputs(EOL "Options:" EOL, fp);
    fputs("\t-d\tsimulated ditto printing" EOL, fp);
    fputs("\t-g\tMakerbot/ReplicatorG GCODE flavor" EOL, fp);
    fputs("\t-i\tenable stdin and stdout support for command line pipes" EOL, fp);
    fputs("\t-l\tlog to file" EOL, fp);
    fputs("\t-p\toverride build percentage" EOL, fp);
    fputs("\t-q\tquiet mode" EOL, fp);
    fputs("\t-r\tReprap GCODE flavor" EOL, fp);
#if defined(SERIAL_SUPPORT)
    fputs("\t-s\tenable USB serial I/O and send x3G output to 3D printer" EOL, fp);
#endif
    fputs("\t-t\ttruncate filename (DOS 8.3 format)" EOL, fp);
    fputs("\t-v\tverose mode" EOL, fp);
    fputs("\t-w\trewrite 5d extrusion values" EOL, fp);
#if defined(SERIAL_SUPPORT)
    fputs(EOL "BAUDRATE: the baudrate for serial I/O (default is 115200)" EOL, fp);
#endif
    fputs("CONFIG: the filename of a custom machine definition (ini file)" EOL, fp);
    fputs("EEPROM: the filename of an eeprom settings definition (ini file)" EOL, fp);
    fputs("DIAMETER: the actual filament diameter in the printer" EOL, fp);
    fputs(EOL "MACHINE: the predefined machine type" EOL, fp);
    fputs("\tc3  = Cupcake Gen3 XYZ, Mk5/6 + Gen4 Extruder" EOL, fp);
    fputs("\tc4  = Cupcake Gen4 XYZ, Mk5/6 + Gen4 Extruder" EOL, fp);
    fputs("\tcp4 = Cupcake Pololu XYZ, Mk5/6 + Gen4 Extruder" EOL, fp);
    fputs("\tcpp = Cupcake Pololu XYZ, Mk5/6 + Pololu Extruder" EOL, fp);
    fputs("\tcxy = Core-XY with HBP - single extruder" EOL, fp);
    fputs("\tcxysz = Core-XY with HBP - single extruder, slow Z" EOL, fp);
    fputs("\tr1  = Replicator 1 - single extruder" EOL, fp);
    fputs("\tr1d = Replicator 1 - dual extruder" EOL, fp);
    fputs("\tr2  = Replicator 2 (default)" EOL, fp);
    fputs("\tr2h = Replicator 2 with HBP" EOL, fp);
    fputs("\tr2x = Replicator 2X" EOL, fp);
    fputs("\tt6  = TOM Mk6 - single extruder" EOL, fp);
    fputs("\tt7  = TOM Mk7 - single extruder" EOL, fp);
    fputs("\tt7d = TOM Mk7 - dual extruder" EOL, fp);
    fputs("\tz   = ZYYX - single extruder" EOL, fp);
    fputs("\tzd  = ZYYX - dual extruder" EOL, fp);
    fputs(EOL "SCALE: the coordinate system scale for the conversion (ABS = 1.0035)" EOL, fp);
    fputs("X,Y & Z: the coordinate system offsets for the conversion" EOL, fp);
    fputs("\tX = the x axis offset" EOL, fp);
    fputs("\tY = the y axis offset" EOL, fp);
    fputs("\tZ = the z axis offset" EOL, fp);
    fputs(EOL "IN: the name of the sliced gcode input filename" EOL, fp);
    fputs("OUT: the name of the x3g output filename"
#if defined(SERIAL_SUPPORT)
	  "or the serial I/O port"
#endif
	  EOL, fp);
    fputs(EOL "Examples:" EOL, fp);
    fputs("\tgpx -p -m r2 my-sliced-model.gcode" EOL, fp);
    fputs("\tgpx -c custom-tom.ini example.gcode /volumes/things/example.x3g" EOL, fp);
    fputs("\tgpx -x 3 -y -3 offset-model.gcode" EOL, fp);
#if defined(SERIAL_SUPPORT)
    fputs("\tgpx -m c4 -s sio-example.gcode /dev/tty.usbmodem" EOL EOL, fp);
#endif

    exit(err);
}

#if !defined(SERIAL_SUPPORT)

// Should never be called in practice but code is less grotty (less #ifdef's)
// if we simply provide this stub
static void sio_open(const char *filename, speed_t baud_rate)
{
     perror(NO_SERIAL_SUPPORT_MSG);
     exit(1);
}

#else

static void sio_open(const char *filename, speed_t baud_rate)
{
    struct termios tp;
    // open and configure the serial port
    if((sio_port = open(filename, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0) {
        perror("Error opening port");
        exit(-1);
    }

    if(fcntl(sio_port, F_SETFL, O_RDWR) < 0) {
        perror("Setting port descriptor flags");
        exit(-1);
    }

    if(tcgetattr(sio_port, &tp) < 0) {
        perror("Error getting port attributes");
        exit(-1);
    }

    cfmakeraw(&tp);

    /*
     // 8N1
     tp.c_cflag &= ~PARENB;
     tp.c_cflag &= ~CSTOPB;
     tp.c_cflag &= ~CSIZE;
     tp.c_cflag |= CS8;

     // no flow control
     tp.c_cflag &= ~CRTSCTS;

     // disable hang-up-on-close to avoid reset
     //tp.c_cflag &= ~HUPCL;

     // turn on READ & ignore ctrl lines
     tp.c_cflag |= CREAD | CLOCAL;

     // turn off s/w flow ctrl
     tp.c_cflag &= ~(IXON | IXOFF | IXANY);

     // make raw
     tp.c_cflag &= ~(ICANON | ECHO | ECHOE | ISIG);
     tp.c_cflag &= ~OPOST;

     // see: http://unixwiz.net/techtips/termios-vmin-vtime.html
     tp.c_cc[VMIN]  = 0;
     tp.c_cc[VTIME] = 0;
     */

    cfsetspeed(&tp, baud_rate);
    // cfsetispeed(&tp, baud_rate);
    // cfsetospeed(&tp, baud_rate);

    if(tcsetattr(sio_port, TCSANOW, &tp) < 0) {
        perror("Error setting port attributes");
        exit(-1);
    }

    sleep(2);
    if(tcflush(sio_port, TCIOFLUSH) < 0) {
        perror("Error flushing port");
        exit(-1);
    }

    if(gpx.flag.verboseMode) fprintf(gpx.log, "Communicating via: %s" EOL, filename);
}

#endif // SERIAL_SUPPORT

// GPX program entry point

int main(int argc, char * const argv[])
{
    int c, i, rval;
    int log_to_file = 0;
    int standard_io = 0;
    int serial_io = 0;
    int truncate_filename = 0;
    char *config = NULL;
    char *eeprom = NULL;
    double filament_diameter = 0;
    char *buildname = "GPX " GPX_VERSION;
    char *filename;
    int ini_loaded = 0;
    speed_t baud_rate = B115200;

    // default to standard I/O
    file_in = stdin;
    file_out = stdout;

    // register cleanup function
    atexit(exit_handler);

    gpx_initialize(&gpx, 1);

    // READ GPX.INI

#if !defined(_WIN32) && !defined(_WIN64)
    // if present, read the ~/.gpx.ini
    {
	char fbuf[1024];
        const char *home = getenv("HOME");
	if (home && home[0]) {
	     char fbuf[1024];
	     snprintf(fbuf, sizeof(fbuf), "%s/.gpx.ini", home);
	     if (!access(fbuf, R_OK))
	     {
		  i = gpx_load_config(&gpx, fbuf);
		  if(i == 0) {
		       ini_loaded = -1;
		       if(gpx.flag.verboseMode) fprintf(stderr, "Loaded config: %s" EOL, fbuf);
		  }
		  else if (i > 0) {
		       fprintf(stderr, "(line %u) Configuration syntax error in %s: unrecognised paremeters" EOL, i, fbuf);
		       usage(1);
		  }
	     }
	}
    }
#endif

    // if present, read the gpx.ini file from the program directory
    if(!ini_loaded) {
	char *appname;
        // check for .exe extension
        const char *dot = strrchr(argv[0], '.');
        if(dot && !strcasecmp(dot,".exe")) {
            long l = dot - argv[0];
            memcpy(gpx.buffer.out, argv[0], l);
            appname = gpx.buffer.out + l;
        }
        // or just append .ini if no extension is present
        else {
            size_t sl = strlen(argv[0]);
            memcpy(gpx.buffer.out, argv[0], sl);
            appname = gpx.buffer.out + sl;
        }
        *appname++ = '.';
        *appname++ = 'i';
        *appname++ = 'n';
        *appname++ = 'i';
        *appname++ = '\0';
        appname = gpx.buffer.out;
        i = gpx_load_config(&gpx, appname);
        if(i == 0) {
            if(gpx.flag.verboseMode) fprintf(stderr, "Loaded config: %s" EOL, appname);
        }
        else if (i > 0) {
	    fprintf(stderr, "(line %u) Configuration syntax error in %s: unrecognised paremeters" EOL, i, appname);
            usage(1);
        }
    }

    // READ COMMAND LINE

    // get the command line options
    // Allow -s and -b so that we can give a more targetted
    // error message should they be attempted when the code
    // is compiled without serial I/O support.

    while ((c = getopt(argc, argv, "b:c:de:gf:ilm:n:pqrstvwx:y:z:?")) != -1) {
        switch (c) {
            case 'b':
#if !defined(SERIAL_SUPPORT)
		fprintf(stderr, NO_SERIAL_SUPPORT_MSG EOL);
		usage(1);
#else
                i = atoi(optarg);
                switch(i) {
                    case 4800:
                        baud_rate=B4800;
                        break;
                    case 9600:
                        baud_rate=B9600;
                        break;
#ifdef B14400
                    case 14400:
                        baud_rate=B14400;
                        break;
#endif
                    case 19200:
                        baud_rate=B19200;
                        break;
#ifdef B28800
                    case 28800:
                        baud_rate=B28800;
                        break;
#endif
                    case 38400:
                        baud_rate=B38400;
                        break;
                    case 57600:
                        baud_rate=B57600;
                        break;
                    case 115200:
                        baud_rate=B115200;
                        break;
                    default:
                        fprintf(stderr, "Command line error: unsupported baud rate '%s'" EOL, optarg);
                        usage(1);
                }
                if(gpx.flag.verboseMode) fprintf(stderr, "Setting baud rate to: %i bps" EOL, i);
#endif
                // fall through
            case 's':
#if !defined(SERIAL_SUPPORT)
		 fprintf(stderr, NO_SERIAL_SUPPORT_MSG EOL);
		usage(1);
#else
                serial_io = 1;
                gpx.flag.framingEnabled = 1;
#endif
                break;
            case 'c':
                config = optarg;
                break;
            case 'd':
                gpx.flag.dittoPrinting = 1;
                break;
            case 'e':
                eeprom = optarg;
                break;
            case 'g':
                gpx.flag.reprapFlavor = 0;
                break;
            case 'f':
                filament_diameter = strtod(optarg, NULL);
                if(filament_diameter > 0.0001) {
                    gpx.override[0].actual_filament_diameter = filament_diameter;
                    gpx.override[1].actual_filament_diameter = filament_diameter;
                }
                break;
            case 'i':
                standard_io = 1;
                break;
            case 'l':
                gpx.flag.verboseMode = 1;
                log_to_file = 1;
                break;
            case 'm':
                if(gpx_set_property(&gpx, "printer", "machine_type", optarg)) {
                    usage(1);
                }
                break;
            case 'n':
                gpx.user.scale = strtod(optarg, NULL);
                break;
            case 'p':
                gpx.flag.buildProgress = 1;
                break;
            case 'q':
                gpx.flag.logMessages = 0;
                break;
            case 'r':
                gpx.flag.reprapFlavor = 1;
                break;
            case 't':
                truncate_filename = 1;
                break;
            case 'v':
                gpx.flag.verboseMode = 1;
                break;
            case 'w':
                gpx.flag.rewrite5D = 1;
                break;
            case 'x':
                gpx.user.offset.x = strtod(optarg, NULL);
                break;
            case 'y':
                gpx.user.offset.y = strtod(optarg, NULL);
                break;
            case 'z':
                gpx.user.offset.z = strtod(optarg, NULL);
                break;
            case '?':
		usage(0);
		// NEVER REACHED
		break;
            default:
                usage(1);
        }
    }

    argc -= optind;
    argv += optind;

    // LOG TO FILE

    if(log_to_file && argc > 0) {
        filename = (argc > 1 && !serial_io) ? argv[1] : argv[0];
        // or use the input filename with a .log extension
        char *dot = strrchr(filename, '.');
        if(dot) {
            long l = dot - filename;
            memcpy(gpx.buffer.out, filename, l);
            filename = gpx.buffer.out + l;
        }
        // or just append one if no .gcode extension is present
        else {
            size_t sl = strlen(filename);
            memcpy(gpx.buffer.out, filename, sl);
            filename = gpx.buffer.out + sl;
        }
        *filename++ = '.';
        *filename++ = 'l';
        *filename++ = 'o';
        *filename++ = 'g';
        *filename++ = '\0';
        filename = gpx.buffer.out;

        if((gpx.log = fopen(filename, "w+")) == NULL) {
            gpx.log = stderr;
            perror("Error opening log");
        }
    }

    // READ CONFIGURATION

    if(config) {
        if(gpx.flag.verboseMode) fprintf(gpx.log, "Loading custom config: %s" EOL, config);
        i = gpx_load_config(&gpx, config);
        if (i < 0) {
            fprintf(stderr, "Command line error: cannot load configuration file '%s'" EOL, config);
            usage(1);
        }
        else if (i > 0) {
            fprintf(stderr, "(line %u) Configuration syntax error in %s: unrecognised paremeters" EOL, i, config);
            usage(1);
        }
    }

    if(baud_rate == B57600 && gpx.machine.type >= MACHINE_TYPE_REPLICATOR_1) {
        if(gpx.flag.verboseMode) fputs("WARNING: a 57600 bps baud rate will cause problems with Repicator 2/2X Mightyboards" EOL, gpx.log);
    }

    // OPEN FILES AND PORTS FOR INPUT AND OUTPUT

    if(standard_io) {
        if(serial_io) {
            if(argc > 0) {
                filename = argv[0];
                sio_open(filename, baud_rate);
            }
            else {
                fputs("Command line error: port required for serial I/O" EOL, stderr);
                usage(1);
            }
        }
    }
    // open the input filename if one is provided
    else if(argc > 0) {
        filename = argv[0];
        if(gpx.flag.verboseMode) fprintf(gpx.log, "Reading from: %s" EOL, filename);
        if((file_in = fopen(filename, "rw")) == NULL) {
            perror("Error opening input");
            exit(1);
        }
        // assign build name
        buildname = strrchr(filename, PATH_DELIM);
        if(buildname) {
            buildname++;
        }
        else {
            buildname = filename;
        }

        argc--;
        argv++;
        // use the output filename if one is provided
        if(argc > 0) {
            filename = argv[0];
        }
        else {
            if(serial_io) {
                fputs("Command line error: port required for serial I/O" EOL, stderr);
                usage(1);
            }
            // or use the input filename with a .x3g extension
            char *dot = strrchr(filename, '.');
            if(dot) {
                long l = dot - filename;
                memcpy(gpx.buffer.out, filename, l);
                filename = gpx.buffer.out + l;
            }
            // or just append one if no .gcode extension is present
            else {
                size_t sl = strlen(filename);
                memcpy(gpx.buffer.out, filename, sl);
                filename = gpx.buffer.out + sl;
            }
            if(truncate_filename) {
                char *s = gpx.buffer.out;
                for(i = 0; s < filename && i < 8; i++) {
                    char c = *s;
                    if(isalnum(c)) {
                        *s++ = toupper(c);
                    }
                    else {
                        *s++ = '_';
                    }
                }
                *s++ = '.';
                *s++ = 'X';
                *s++ = '3';
                *s++ = 'G';
                *s++ = '\0';
            }
            else {
                *filename++ = '.';
                *filename++ = 'x';
                *filename++ = '3';
                *filename++ = 'g';
                *filename++ = '\0';
            }
            filename = gpx.buffer.out;
        }

        // trim build name extension
        char *dot = strrchr(buildname, '.');
        if(dot) *dot = 0;

        if(serial_io) {
            sio_open(filename, baud_rate);
        }
        else {
            if((file_out = fopen(filename, "wb")) == NULL) {
                perror("Error creating output");
                exit(-1);
            }
            if(gpx.flag.verboseMode) fprintf(gpx.log, "Writing to: %s" EOL, filename);
            // write a second copy to the SD Card
            if(gpx.sdCardPath) {
                long sl = strlen(gpx.sdCardPath);
                if(gpx.sdCardPath[sl - 1] == PATH_DELIM) {
                    gpx.sdCardPath[--sl] = 0;
                }
                char *delim = strrchr(filename, PATH_DELIM);
                if(delim) {
                    memcpy(gpx.buffer.out, gpx.sdCardPath, sl);
                    long l = strlen(delim);
                    memcpy(gpx.buffer.out + sl, delim, l);
                    gpx.buffer.out[sl + l] = 0;
                }
                else {
                    memcpy(gpx.buffer.out, gpx.sdCardPath, sl);
                    gpx.buffer.out[sl++] = PATH_DELIM;
                    long l = strlen(filename);
                    memcpy(gpx.buffer.out + sl, filename, l);
                    gpx.buffer.out[sl + l] = 0;
                }
                file_out2 = fopen(gpx.buffer.out, "wb");
                if(file_out2 && gpx.flag.verboseMode) fprintf(gpx.log, "Writing to: %s" EOL, gpx.buffer.out);
            }
        }
    }
    else {
        fputs("Command line error: provide an input file or enable standard I/O" EOL, stderr);
        usage(1);
    }

    if(log_to_file) {
        if(gpx.flag.buildProgress) fputs("Build progress: enabled" EOL, gpx.log);
        if(gpx.flag.dittoPrinting) fputs("Ditto printing: enabled" EOL, gpx.log);
        if(serial_io) fputs("Serial IO: enabled" EOL, gpx.log);
        fprintf(gpx.log, "GCode flavor: %s" EOL, gpx.flag.reprapFlavor ? "Reprap" : "Makerbot");
        if(gpx.flag.rewrite5D) fputs("Rewrite 5D: enabled" EOL, gpx.log);
    }

    /* at this point we have read the command line, set the machine definition
       and both the input and output files or ports are open, so its time to parse
       the gcode input and convert it to x3g output. */

    if(serial_io) {
        // READ CONFIG AND WRITE EEPROM SETTINGS
        if(eeprom) {
            if(gpx.flag.verboseMode) fprintf(gpx.log, "Loading eeprom config: %s" EOL, eeprom);
            i = eeprom_load_config(&gpx, eeprom);
            if (i < 0) {
                fprintf(stderr, "Command line error: cannot load eeprom configuration file '%s'" EOL, eeprom);
                usage(1);
            }
            else if (i > 0) {
                fprintf(stderr, "(line %u) Eeprom configuration syntax error in %s: unrecognised paremeters" EOL, i, eeprom);
                usage(1);
            }
            exit(SUCCESS);
        }
        else {
            // READ INPUT AND SEND OUTPUT TO PRINTER

            gpx_start_convert(&gpx, buildname);
            rval = gpx_convert_and_send(&gpx, file_in, sio_port);
            gpx_end_convert(&gpx);
        }
    }
    else {
        // READ INPUT AND CONVERT TO OUTPUT

        gpx_start_convert(&gpx, buildname);
        rval = gpx_convert(&gpx, file_in, file_out, file_out2);
        gpx_end_convert(&gpx);
    }
    exit(rval);
}
