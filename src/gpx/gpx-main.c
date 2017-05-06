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
#include <errno.h>

#include <unistd.h>

#include "gpx.h"
#include "machine_config.h"

// Global variables

static Gpx gpx;

static FILE *file_in = NULL;
static FILE *file_out = NULL;
static FILE *file_out2 = NULL;
static int sio_port = -1;
static char temp_config_name[24];

// cleanup code in case we encounter an error that causes the program to exit

static void exit_handler(void)
{
    // close open files
    if(file_in != stdin && file_in != NULL) {
        fclose(file_in);
        file_in = NULL;
    }
    if(file_out != stdout && file_out != NULL) {
        if(ferror(file_out)) {
            perror("Error writing to output file");
        }
        fclose(file_out);
        file_out = NULL;
    }
    if(file_out2 != NULL) {
        fclose(file_out2);
        file_out2 = NULL;
    }

    // 23 February 2015
    // Assuming stdin=0, stdout=1, stderr=3 isn't always safe
    // Do not assume that sio_port > 2 means it needs to be
    // closed.  Instead, if it isn't negative, then it needs
    // to be closed.

    if(sio_port >= 0) {
        close(sio_port);
	sio_port = -1;
    }

    if(temp_config_name[0]) {
	 unlink(temp_config_name);
	 temp_config_name[0] = '\0';
    }
}

// display usage

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

    if (err)
        fputs(EOL, fp);

    fputs("GPX " PACKAGE_VERSION EOL, fp);
    fputs("Copyright (c) 2013 WHPThomas, All rights reserved." EOL, fp);

    fputs("Additional changes Copyright (c) 2014, 2015 DNewman, MWalker" EOL, fp);
	fputs("All rights reserved." EOL, fp);

    if (err) {
        fputs(EOL "For usage information: gpx -?" EOL, fp);
        return;
    }

    fputs(EOL "This program is free software; you can redistribute it and/or modify" EOL, fp);
    fputs("it under the terms of the GNU General Public License as published by" EOL, fp);
    fputs("the Free Software Foundation; either version 2 of the License, or" EOL, fp);
    fputs("(at your option) any later version." EOL, fp);

    fputs(EOL "This program is distributed in the hope that it will be useful," EOL, fp);
    fputs("but WITHOUT ANY WARRANTY; without even the implied warranty of" EOL, fp);
    fputs("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the" EOL, fp);
    fputs("GNU General Public License for more details." EOL, fp);

    fputs(EOL "Usage:" EOL, fp);
    fputs("gpx [-CFIdgilpqr" SERIAL_MSG1 "tvw] " SERIAL_MSG2 "[-L LOGFILE] [-D NEWPORT] [-E EXISTINGPORT] [-c CONFIG] [-e EEPROM] [-f DIAMETER] [-m MACHINE] [-N h|t|ht] [-n SCALE] [-x X] [-y Y] [-z Z] [-W S] IN [OUT]" EOL, fp);
    fputs(EOL "Options:" EOL, fp);
    fputs("\t-C\tcreate temporary file with a copy of the machine configuration" EOL, fp);
    fputs("\t-D\trun in daemon mode and create the named virtual port" EOL, fp);
    fputs("\t-E\trun in daemon mode and open the named psuedo-terminal" EOL, fp);
    fputs("\t-F\twrite X3G on-wire framing data to output file" EOL, fp);
    fputs("\t-I\tignore default .ini files" EOL, fp);
    fputs("\t-N\tdisable writing of the X3G header (start build notice)," EOL, fp);
    fputs("\t  \ttail (end build notice), or both" EOL, fp);
	fputs("\t-W\twait S seconds after opening the serial connection" EOL, fp);
	fputs("\t  \tbefore reading or writing (default is 2 seconds)" EOL, fp);
    fputs("\t-d\tsimulated ditto printing" EOL, fp);
    fputs("\t-g\tMakerbot/ReplicatorG GCODE flavor" EOL, fp);
    fputs("\t-i\tenable stdin and stdout support for command line pipes" EOL, fp);
    fputs("\t-l\tlog to file" EOL, fp);
    fputs("\t-L\tlog to named [LOGFILE] file" EOL, fp);
    fputs("\t-p\toverride build percentage" EOL, fp);
    fputs("\t-q\tquiet mode" EOL, fp);
    fputs("\t-r\tReprap GCODE flavor" EOL, fp);
#if defined(SERIAL_SUPPORT)
    fputs("\t-s\tenable USB serial I/O and send x3G output to 3D printer" EOL, fp);
#endif
    fputs("\t-t\ttruncate filename (DOS 8.3 format)" EOL, fp);
    fputs("\t-v\tverbose mode" EOL, fp);
    fputs("\t-w\trewrite 5d extrusion values" EOL, fp);
#if defined(SERIAL_SUPPORT)
    fputs(EOL "BAUDRATE: the baudrate for serial I/O (default is 115200)" EOL, fp);
#endif
    fputs("CONFIG: the filename of a custom machine definition (ini file)" EOL, fp);
    fputs("EEPROM: the filename of an eeprom settings definition (ini file)" EOL, fp);
    fputs("DIAMETER: the actual filament diameter in the printer" EOL, fp);
    fputs(EOL "MACHINE: the predefined machine type" EOL, fp);
    fputs("\tsome machine definitions have been updated with corrected steps per mm" EOL, fp);
    fputs("\tthe original can be selected by prefixing o to the machine id" EOL, fp);
    fputs("\t(or1, or1d, or2, or2h, orx, ot7, ot7d)" EOL, fp);
    gpx_list_machines(fp);
    fputs(EOL "SCALE: the coordinate system scale for the conversion (ABS = 1.0035)" EOL, fp);
    fputs("X,Y & Z: the coordinate system offsets for the conversion" EOL, fp);
    fputs("\tX = the x axis offset" EOL, fp);
    fputs("\tY = the y axis offset" EOL, fp);
    fputs("\tZ = the z axis offset" EOL, fp);
    fputs(EOL "IN: the name of the sliced gcode input filename" EOL, fp);
    fputs("OUT: the name of the X3G output filename"
#if defined(SERIAL_SUPPORT)
	  "or the serial I/O port"
#endif
	  EOL, fp);
    fputs("       specify '--' to write to stdout" EOL, fp);
    fputs(EOL "Examples:" EOL, fp);
    fputs("\tgpx -p -m r2 my-sliced-model.gcode" EOL, fp);
    fputs("\tgpx -c custom-tom.ini example.gcode /volumes/things/example.x3g" EOL, fp);
    fputs("\tgpx -x 3 -y -3 offset-model.gcode" EOL, fp);
#if defined(SERIAL_SUPPORT)
    fputs("\tgpx -m c4 -s sio-example.gcode /dev/tty.usbmodem" EOL EOL, fp);
#endif
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

#if !defined(_WIN32) && !defined(_WIN64)
int gpx_sio_open(Gpx *gpx, const char *filename, speed_t baud_rate,
		 int *sio_port)
{
    struct termios tp;
    int port;

    if(sio_port)
	 *sio_port = -1;

    fprintf(gpx->log, "Opening port: %s.\n", filename);
    // open and configure the serial port
    if((port = open(filename, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0) {
        perror("Error opening port");
	return 0;
    }

    if(fcntl(port, F_SETFL, O_RDWR) < 0) {
        perror("Setting port descriptor flags");
	return 0;
    }

    if(tcgetattr(port, &tp) < 0) {
        fprintf(stderr, "errno = %d", errno);
        perror("Error getting port attributes");
	return 0;
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

    // let's ask the i/o system to block for up to a tenth of a second
    // waiting for at least 255 bytes or whatever we asked for (whichever
    // is least).
    tp.c_cc[VMIN] = 255;
    tp.c_cc[VTIME] = 1;

    if(tcsetattr(port, TCSANOW, &tp) < 0) {
        perror("Error setting port attributes");
	return 0;
    }

    if(gpx->open_delay > 0) {
		sleep(gpx->open_delay);
	}
    if(tcflush(port, TCIOFLUSH) < 0) {
        perror("Error flushing port");
	return 0;
    }

    if(gpx->flag.verboseMode) fprintf(gpx->log, "Communicating via: %s" EOL, filename);
    if(sio_port)
	 *sio_port = port;

    return 1;
}
#endif

void sio_open(const char *filename, speed_t baud_rate)
{
    if (!gpx_sio_open(&gpx, filename, baud_rate, &sio_port))
        exit(-1);
}

#endif // SERIAL_SUPPORT

int gpx_find_ini(Gpx *gpx, char *argv0)
{
    char fbuf[1024];
    char *home;
    int i;

#if defined(_WIN32) || defined(_WIN64)
    // if present, read the %appdata%/local/GpxUi/gpx.ini
    home = getenv("LOCALAPPDATA");
    if(home && home[0]) {
        snprintf(fbuf, sizeof(fbuf), "%s/GpxUi/gpx.ini", home);
        if(!access(fbuf, R_OK)) {
            i = gpx_load_config(gpx, fbuf);
            if(i >= 0)
                return i;
        }
    }
#endif

    home = getenv("HOME");
    if(home && home[0]) {
        snprintf(fbuf, sizeof(fbuf), "%s/.gpx.ini", home);
        if (!access(fbuf, R_OK)) {
            i = gpx_load_config(gpx, fbuf);
            if(i >= 0)
                return i;
        }
    }

    // if present, read the gpx.ini file from the program directory
    // TODO this doesn't really work on Windows because argv0 doesn't necessarily
    // have the full path
    // check for .exe extension
    const char *dot = strrchr(argv0, '.');
    size_t len = 0;
    if(dot && !strcasecmp(dot, ".exe")) {
        len = dot - argv0;
    } else {
        len = strlen(argv0);
    }

    if(len + 5 > sizeof(fbuf))
        return -1;
    memcpy(fbuf, argv0, len);
    strcpy(fbuf + len, ".ini");
    return gpx_load_config(gpx, fbuf);
}

// GPX program entry point

int main(int argc, char * const argv[])
{
    int c, i, rval = 1;
    int force_framing = 0;
    int ignore_default_ini = 0;
    int log_to_file = 0;
    int standard_io = 0;
    int serial_io = 0;
    int truncate_filename = 0;
    char *daemon_port = NULL;
    char *config = NULL;
    char *eeprom = NULL;
    double filament_diameter = 0;
    char *buildname = PACKAGE_STRING;
    char *logname = NULL;
    char *filename;
    speed_t baud_rate = B115200;
    int make_temp_config = 0;
    int create_daemon_port = 0;

    // Blank the temporary config file name.  If it isn't blank
    //   on exit and an error has occurred, then it is deleted
    temp_config_name[0] = '\0';

    // default to standard I/O
    file_in = stdin;
    file_out = stdout;

    // register cleanup function
    atexit(exit_handler);

    gpx_initialize(&gpx, 1);
    gpx.log = stderr;

    // we run through getopt twice, the first time is to figure out whether to load
    // the ini file from the default locations and whether to be verbose about it
    // we need to load the ini file before parsing the rest so that the command line
    // overrides the default ini in the standard case
    while ((c = getopt(argc, argv, "CD:E:FIL:N:W:b:c:de:gf:ilm:n:pqrstu:vwx:y:z:?")) != -1) {
        switch (c) {
            case 'I':
                ignore_default_ini = 1;
                break;
            case 'l':
                gpx.flag.verboseMode = 1;
                // fallthrough
            case 'L': // does not imply verbose unlike -l
                log_to_file = 1;
                break;
            case 'v':
                if(gpx.flag.verboseMode)
                    gpx.flag.verboseSioMode = 1;
                gpx.flag.verboseMode = 1;
                break;
        }
    }
    optind = 1;

    if(!ignore_default_ini) {
        // READ GPX.INI
        i = gpx_find_ini(&gpx, argv[0]);
        if (i > 0) {
            fprintf(stderr, "(line %u) Configuration syntax error: unrecognised parameters" EOL, i);
            usage(1);
            goto done;
        }
    }

    // READ COMMAND LINE

    // get the command line options
    // Allow -s and -b so that we can give a more targetted
    // error message should they be attempted when the code
    // is compiled without serial I/O support.

    while ((c = getopt(argc, argv, "CD:E:FIL:N:W:b:c:de:gf:ilm:n:pqrstu:vwx:y:z:?")) != -1) {
        switch (c) {
	    case 'C':
		 // Write config data to a temp file
		 // Write output to stdout
		 make_temp_config = 1;
		 break;
            case 'D':
                 create_daemon_port = 1;
                 // fallthrough
            case 'E':
                 //
                 // Run in daemon mode - implies serial mode to the printer and
                 // then gcode input and reprap responses to other processes are
                 // via a two-way pipe to emulate a RepRap printer on the specified
                 // port
                 daemon_port = optarg;
#if !defined(SERIAL_SUPPORT)
                 fprintf(stderr, NO_SERIAL_SUPPORT_MSG EOL);
                 usage(1);
                 goto done;
#else
                 serial_io = 1;
                 gpx.flag.framingEnabled = 1;
#endif
                break;
	    case 'F':
		 force_framing = ITEM_FRAMING_ENABLE;
		 break;
            case 'I':
                 break; // handled in first getopt loop
            case 'L':
                logname = optarg;
                break;
	    case 'N':
		 if(optarg[0] == 'h' || optarg[1] == 'h')
		      gpx_set_start(&gpx, 0);
		 if(optarg[0] == 't' || optarg[1] == 't')
		      gpx_set_end(&gpx, 0);
		 break;
	    case 'b':
#if !defined(SERIAL_SUPPORT)
		fprintf(stderr, NO_SERIAL_SUPPORT_MSG EOL);
		usage(1);
		goto done;
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
			goto done;
                }
                if(gpx.flag.verboseMode) fprintf(stderr, "Setting baud rate to: %i bps" EOL, i);
#endif
                // fall through
            case 's':
#if !defined(SERIAL_SUPPORT)
		fprintf(stderr, NO_SERIAL_SUPPORT_MSG EOL);
		usage(1);
		goto done;
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
                break; // handled in first getopt loop
            case 'm':
                if(gpx_set_property(&gpx, "printer", "machine_type", optarg)) {
                    usage(1);
		    goto done;
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
            case 'u':
                if(gpx_set_property(&gpx, "machine", "steps_per_mm", optarg)) {
                    usage(1);
                    goto done;
                }
                break;
            case 'v':
                break; // handled in first getopt loop
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
            case 'W':
                gpx.open_delay = strtod(optarg, NULL);
                break;
            case '?':
		usage(0);
		rval = SUCCESS;
		goto done;
            default:
                usage(1);
		goto done;
        }
    }

    argc -= optind;
    argv += optind;

    // LOG TO FILE

    if(log_to_file && logname == NULL && argc > 0) {
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
        logname = filename;
    }

    if(logname != NULL) {
        if((gpx.log = fopen(logname, "w+")) == NULL) {
            gpx.log = stderr;
            perror("Error opening log");
        }
        fprintf(gpx.log, "GPX started.\n");
    }

    // READ CONFIGURATION

    if(config) {
        i = gpx_load_config(&gpx, config);
        if (i < 0) {
            fprintf(stderr, "Command line error: cannot load configuration file '%s'" EOL, config);
            usage(1);
	    goto done;
        }
        else if (i > 0) {
            fprintf(stderr, "(line %u) Configuration syntax error in %s: unrecognised paremeters" EOL, i, config);
            usage(1);
	    goto done;
        }
    }

    if(make_temp_config) {
	 FILE *temp_config = NULL;
#if !defined(_WIN32) && !defined(_WIN64)
	 int fd;
	 strncpy(temp_config_name, "/tmp/gpx-XXXXXX.ini",
		 sizeof(temp_config_name) - 1);
	 fd = mkstemps(temp_config_name, 4);
	 temp_config = (fd >= 0) ? fdopen(fd, "w") : NULL;
#else
	 strcpy(temp_config_name, "gpx-XXXXXX");
	 _mktemp(temp_config_name);
	 strncat(temp_config_name, ".ini", sizeof(temp_config_name)-1);
	 temp_config = fopen(temp_config_name, "w");
#endif
	 if(temp_config) {
	      config_dump(temp_config, &gpx.machine);
	      fclose(temp_config);
	 }
	 else {
	      perror("Error writing a temporary configuration file for "
		     "the -C option");
	      goto done;
	 }
    }

    if(baud_rate == B57600 && gpx.machine.id >= MACHINE_TYPE_REPLICATOR_1) {
        if(gpx.flag.verboseMode) fputs("WARNING: a 57600 bps baud rate will cause problems with Repicator 2/2X Mightyboards" EOL, gpx.log);
    }

    // OPEN FILES AND PORTS FOR INPUT AND OUTPUT

    if(daemon_port != NULL) {
        if(standard_io) {
            fprintf(stderr, "Command line error: daemon mode incompatible with standard i/o\n");
            usage(1);
            goto done;
        }
        if(argc == 0) {
            fprintf(stderr, "Command line error: port required for serial I/O in daemon mode\n");
            usage(1);
            goto done;
        }

        // create the bi-directional virtual port for other processes
        // and read and write from there until somebody tells us to quit
        gpx_daemon(&gpx, create_daemon_port, daemon_port, argv[0], baud_rate);
        goto done;
    }
    else if(standard_io) {
        if(daemon_port != NULL) {
            fprintf(stderr, "Using standard in/out is incompatible with daemon mode\n");
            usage(1);
            goto done;
        }
        if(serial_io) {
            if(argc > 0) {
                filename = argv[0];
                sio_open(filename, baud_rate);
            }
            else {
                fputs("Command line error: port required for serial I/O" EOL, stderr);
                usage(1);
		goto done;
            }
        }
#ifdef _WIN32
        else {
            setmode(fileno(stdout), O_BINARY);
        }
#endif
    }
    // open the input filename if one is provided
    else if(argc > 0) {
        filename = argv[0];
        if(gpx.flag.verboseMode) fprintf(gpx.log, "Reading from: %s" EOL, filename);
        if((file_in = fopen(filename, "rw")) == NULL) {
            perror("Error opening input");
	    goto done;
        }
        // assign build name
        buildname = strrchr(filename, PATH_DELIM);
#ifdef _WIN32
        char *otherdelim = strrchr(filename, '/');
        if (otherdelim > buildname)
        {
            buildname = otherdelim;
        }
#endif
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
            // prefer output filename over input for the buildname
            char *s = strrchr(filename, PATH_DELIM);
#ifdef _WIN32
            otherdelim = strrchr(filename, '/');
            if (otherdelim > s)
                s = otherdelim;
#endif
            s = strdup(s ? s+1 : filename);
            if (s)
                buildname = s;
        }
        else {
            if(serial_io) {
                fputs("Command line error: port required for serial I/O" EOL, stderr);
                usage(1);
		goto done;
            }

            // or use the input filename with a .x3g extension
            char *ext = strrchr(filename, '.');
            size_t l = ext ? ext - filename : strlen(filename);
            memcpy(gpx.buffer.out, filename, l);
            filename = gpx.buffer.out;
            ext = filename + l;

            if(truncate_filename) {
                // truncate, replace all non alnum with '_' and uppercase
                char *s = gpx.buffer.out;
                for(i = 0; s < ext && i < 8; i++) {
                    char c = *s;
                    if(isalnum(c)) {
                        *s++ = toupper(c);
                    }
                    else {
                        *s++ = '_';
                    }
                }
                strcpy(s, ".X3G");
            }
            else {
                strcpy(ext, ".x3g");
            }
        }

        // trim build name extension
        char *dot = strrchr(buildname, '.');
        if(dot) *dot = 0;

        if(serial_io) {
            sio_open(filename, baud_rate);
        }
        else {
	    if(filename[0] != '-' || filename[1] != '-' || filename[2] != '\0') {
              if((file_out = fopen(filename, "wb")) == NULL) {
                  perror("Error creating output");
		  goto done;
              }
              if(gpx.flag.verboseMode) fprintf(gpx.log, "Writing to: %s" EOL, filename);
              // write a second copy to the SD Card
              if(gpx.sdCardPath) {
                  long sl = strlen(gpx.sdCardPath);
                  if(sl > 0 && gpx.sdCardPath[sl - 1] == PATH_DELIM) {
                      gpx.sdCardPath[--sl] = 0;
                  }

                  char *leaf = strrchr(filename, PATH_DELIM);
                  if (!leaf)
                      leaf = filename;
                  else
                      leaf++;
                  // strdup because we could be pointing into gpx.buffer.out
                  // and we're about to use that to prepend the sdCardPath
                  leaf = strdup(leaf);
                  if(leaf == NULL) {
                      fputs("Insufficient memory" EOL, stderr);
                      goto done;
                  }

                  memcpy(gpx.buffer.out, gpx.sdCardPath, sl);
                  gpx.buffer.out[sl++] = PATH_DELIM;
                  strcpy(gpx.buffer.out + sl, filename);

                  free(leaf);

                  file_out2 = fopen(gpx.buffer.out, "wb");
                  if(file_out2 && gpx.flag.verboseMode) fprintf(gpx.log, "Writing to: %s" EOL, gpx.buffer.out);
              }
	   }
        }
    }
    else {
        fputs("Command line error: provide an input file or enable standard I/O" EOL, stderr);
        usage(1);
	goto done;
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

    if(make_temp_config)
	 gpx_set_preamble(&gpx, temp_config_name);

    if(serial_io) {
        // READ CONFIG AND WRITE EEPROM SETTINGS
        if(eeprom) {
            if(gpx.flag.verboseMode) fprintf(gpx.log, "Loading eeprom config: %s" EOL, eeprom);
            i = eeprom_load_config(&gpx, eeprom);
            if (i < 0) {
                fprintf(stderr, "Command line error: cannot load eeprom configuration file '%s'" EOL, eeprom);
                usage(1);
		goto done;
            }
            else if (i > 0) {
                fprintf(stderr, "(line %u) Eeprom configuration syntax error in %s: unrecognised paremeters" EOL, i, eeprom);
                usage(1);
		goto done;
            }
            rval = SUCCESS;
	    goto done;
        }
        else {
            // READ INPUT AND SEND OUTPUT TO PRINTER

	    gpx_start_convert(&gpx, buildname, force_framing, 0);
            rval = gpx_convert_and_send(&gpx, file_in, sio_port, force_framing, 0);
            gpx_end_convert(&gpx);
        }
    }
    else {
        // READ INPUT AND CONVERT TO OUTPUT

	gpx_start_convert(&gpx, buildname, force_framing, 0);
        rval = gpx_convert(&gpx, file_in, file_out, file_out2);
        gpx_end_convert(&gpx);
    }

done:
    if (temp_config_name[0])
    {
	 if (rval != SUCCESS)
	      unlink(temp_config_name);
	 temp_config_name[0] = '\0';
    }
    return(rval);
}
