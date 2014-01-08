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
#include <strings.h>
#include <termios.h>
#include <unistd.h>

#ifdef _WIN32
#   include "getopt.h"
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
    if(sio_port > 2) {
        close(sio_port);
    }
}

// display usage and exit

static void usage()
{
    fputs("GPX " GPX_VERSION " Copyright (c) 2013 WHPThomas, All rights reserved." EOL, stderr);
    
    fputs(EOL "This program is free software; you can redistribute it and/or modify" EOL, stderr);
    fputs("it under the terms of the GNU General Public License as published by" EOL, stderr);
    fputs("the Free Software Foundation; either version 2 of the License, or" EOL, stderr);
    fputs("(at your option) any later version." EOL, stderr);
    
    fputs(EOL "This program is distributed in the hope that it will be useful," EOL, stderr);
    fputs("but WITHOUT ANY WARRANTY; without even the implied warranty of" EOL, stderr);
    fputs("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the" EOL, stderr);
    fputs("GNU General Public License for more details." EOL, stderr);
    
    fputs(EOL "Usage:" EOL, stderr);
    fputs("gpx [-dgilpqrstvw] [-b BAUDRATE] [-c CONFIG] [-e EEPROM] [-f DIAMETER] [-m MACHINE] [-n SCALE] [-x X] [-y Y] [-z Z] IN [OUT]" EOL, stderr);
    fputs(EOL "Options:" EOL, stderr);
    fputs("\t-d\tsimulated ditto printing" EOL, stderr);
    fputs("\t-g\tMakerbot/ReplicatorG GCODE flavor" EOL, stderr);
    fputs("\t-i\tenable stdin and stdout support for command line pipes" EOL, stderr);
    fputs("\t-l\tlog to file" EOL, stderr);
    fputs("\t-p\toverride build percentage" EOL, stderr);
    fputs("\t-q\tquiet mode" EOL, stderr);
    fputs("\t-r\tReprap GCODE flavor" EOL, stderr);
    fputs("\t-s\tenable USB serial I/O and send x3G output to 3D printer" EOL, stderr);
    fputs("\t-t\ttruncate filename (DOS 8.3 format)" EOL, stderr);
    fputs("\t-v\tverose mode" EOL, stderr);
    fputs("\t-w\trewrite 5d extrusion values" EOL, stderr);
    fputs(EOL "BAUDRATE: the baudrate for serial I/O (default is 115200)" EOL, stderr);
    fputs("CONFIG: the filename of a custom machine definition (ini file)" EOL, stderr);
    fputs("EEPROM: the filename of an eeprom settings definition (ini file)" EOL, stderr);
    fputs("DIAMETER: the actual filament diameter in the printer" EOL, stderr);
    fputs(EOL "MACHINE: the predefined machine type" EOL, stderr);
    fputs("\tc3  = Cupcake Gen3 XYZ, Mk5/6 + Gen4 Extruder" EOL, stderr);
    fputs("\tc4  = Cupcake Gen4 XYZ, Mk5/6 + Gen4 Extruder" EOL, stderr);
    fputs("\tcp4 = Cupcake Pololu XYZ, Mk5/6 + Gen4 Extruder" EOL, stderr);
    fputs("\tcpp = Cupcake Pololu XYZ, Mk5/6 + Pololu Extruder" EOL, stderr);
    fputs("\tt6  = TOM Mk6 - single extruder" EOL, stderr);
    fputs("\tt7  = TOM Mk7 - single extruder" EOL, stderr);
    fputs("\tt7d = TOM Mk7 - dual extruder" EOL, stderr);
    fputs("\tr1  = Replicator 1 - single extruder" EOL, stderr);
    fputs("\tr1d = Replicator 1 - dual extruder" EOL, stderr);
    fputs("\tr2  = Replicator 2 (default)" EOL, stderr);
    fputs("\tr2h = Replicator 2 with HBP" EOL, stderr);
    fputs("\tr2x = Replicator 2X" EOL, stderr);
    fputs(EOL "SCALE: the coordinate system scale for the conversion (ABS = 1.0035)" EOL, stderr);
    fputs("X,Y & Z: the coordinate system offsets for the conversion" EOL, stderr);
    fputs("\tX = the x axis offset" EOL, stderr);
    fputs("\tY = the y axis offset" EOL, stderr);
    fputs("\tZ = the z axis offset" EOL, stderr);
    fputs(EOL "IN: the name of the sliced gcode input filename" EOL, stderr);
    fputs("OUT: the name of the x3g output filename or the serial I/O port" EOL, stderr);
    fputs(EOL "Examples:" EOL, stderr);
    fputs("\tgpx -p -m r2 my-sliced-model.gcode" EOL, stderr);
    fputs("\tgpx -c custom-tom.ini example.gcode /volumes/things/example.x3g" EOL, stderr);
    fputs("\tgpx -x 3 -y -3 offset-model.gcode" EOL, stderr);
    fputs("\tgpx -m c4 -s sio-example.gcode /dev/tty.usbmodem" EOL EOL, stderr);
    
    exit(1);
}

static void sio_open(char *filename, speed_t baud_rate)
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


// GPX program entry point

int main(int argc, char * argv[])
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
    speed_t baud_rate = B115200;
    
    // default to standard I/O
    file_in = stdin;
    file_out = stdout;

    // register cleanup function
    atexit(exit_handler);

    gpx_initialize(&gpx, 1);
    
    // READ GPX.INI
    
    // if present, read the gpx.ini file from the program directory
    {
        char *appname = argv[0];
        // check for .exe extension
        char *dot = strrchr(appname, '.');
        if(dot) {
            long l = dot - appname;
            memcpy(gpx.buffer.out, appname, l);
            appname = gpx.buffer.out + l;
        }
        // or just append .ini if no extension is present
        else {
            size_t sl = strlen(appname);
            memcpy(gpx.buffer.out, appname, sl);
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
            fprintf(stderr, "(line %u) Configuration syntax error in gpx.ini: unrecognised paremeters" EOL, i);
            usage();
        }
    }
    
    // READ COMMAND LINE
    
    // get the command line options
    while ((c = getopt(argc, argv, "b:c:de:gf:ilm:n:pqrstvwx:y:z:?")) != -1) {
        switch (c) {
            case 'b':
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
                        usage();
                }
                if(gpx.flag.verboseMode) fprintf(stderr, "Setting baud rate to: %i bps" EOL, i);
                // fall through
            case 's':
                serial_io = 1;
                gpx.flag.framingEnabled = 1;
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
                    usage();
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
            default:
                usage();
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
            usage();
        }
        else if (i > 0) {
            fprintf(stderr, "(line %u) Configuration syntax error in %s: unrecognised paremeters" EOL, i, config);
            usage();
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
                usage();
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
                usage();
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
        usage();
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
                usage();
            }
            else if (i > 0) {
                fprintf(stderr, "(line %u) Eeprom configuration syntax error in %s: unrecognised paremeters" EOL, i, eeprom);
                usage();
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
