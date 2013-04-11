//
//  gpx.c
//
//  Created by WHPThomas on 1/04/13.
//
//  Copyright (c) 2013 WHPThomas.
//
//  Referencing ReplicatorG sources from /src/replicatorg/drivers
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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "gpx.h"

// Machine definitions

//  Axis - maxfeedrate, stepspermm, endstop
//  Extruder - maxfeedrate, stepspermm, motorsteps

static Machine replicator_1 = {
    {18000, 2500, 94.139704, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 94.139704, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    20, // timeout
};

static Machine replicator_2 = {
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 0}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    20, // timeout
};

static Machine replicator_2X = {
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // y axis
    {1170, 1100, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 1}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    20, // timeout
};

// The default machine definition is the Replicator 2

Machine machine = {
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // x axis
    {18000, 2500, 88.573186, ENDSTOP_IS_MAX}, // y axis
    {1170, 400, ENDSTOP_IS_MIN},        // z axis
    {1600, 96.275201870333662468889989185642, 3200, 0}, // a extruder
    {1600, 96.275201870333662468889989185642, 3200, 0}, // b extruder
    20, // timeout
};

// GLOBAL VARIABLES

Command command;        // command line
Paremeter current;      // current point
Point3d machineTarget;  // machine target point
Paremeter workTarget;   // work target point
Point3d currentOffset;  // current offset
Point3d offset[6];      // G10 offsets
int currentTool;
int isRelative = 0;
int isMetric = 1;
u32 line_number = 1;
static char buffer[256];

static int fputu32(u32 value, FILE *out)
{
    if(fputc(value, out) == EOF) return EOF;
    if(fputc(value >> 8, out) == EOF) return EOF;
    if(fputc(value >> 16, out) == EOF) return EOF;
    if(fputc(value >> 24, out) == EOF) return EOF;
    return 0;
}

static int fputu16(u16 value, FILE *out)
{
    if(fputc(value, out) == EOF) return EOF;
    if(fputc(value >> 8, out) == EOF) return EOF;
    return 0;
}

static double magnitude(unsigned long flag, Ptr5d vector)
{
    double acc = 0.0;
    if(flag & X_IS_SET) {
        acc = vector->x * vector->x;
    }
    if(flag & Y_IS_SET) {
        acc += vector->y * vector->y;
    }
    if(flag & Z_IS_SET) {
        acc += vector->z * vector->z;
    }
    if(flag & A_IS_SET) {
        acc += vector->a * vector->a;
    }
    if(flag & B_IS_SET) {
        acc += vector->b * vector->b;
    }
    return sqrt(acc);
}

static double get_max_feedrate(unsigned long flag)
{
    double feedrate;
    if(flag & F_IS_SET) {
        feedrate = command.f;
    }
    else {
        feedrate = 0.0;
        if(flag & X_IS_SET && feedrate < machine.x.max_feedrate) {
            feedrate = machine.x.home_feedrate;
        }
        if(flag & Y_IS_SET && feedrate < machine.y.max_feedrate) {
            feedrate = machine.y.home_feedrate;
        }
        if(flag & Z_IS_SET && feedrate < machine.z.max_feedrate) {
            feedrate = machine.z.home_feedrate;
        }
    }
    return feedrate;
}

static double get_home_feedrate(unsigned long flag) {
    double feedrate;
    if(flag & F_IS_SET) {
        feedrate = command.f;
    }
    else {
        feedrate = 0.0;
        if(flag & X_IS_SET && feedrate < machine.x.home_feedrate) {
            feedrate = machine.x.home_feedrate;
        }
        if(flag & Y_IS_SET && feedrate < machine.y.home_feedrate) {
            feedrate = machine.y.home_feedrate;
        }
        if(flag & Z_IS_SET && feedrate < machine.z.home_feedrate) {
            feedrate = machine.z.home_feedrate;
        }
    }
    return feedrate;
}

static u32 feedrate_to_microseconds(unsigned long flag, Ptr5d origin, Ptr5d vector, double feedrate) {
    Point5d deltaMM;
    Point5d deltaSteps;
    double longestStep = 0.0;
    
    if(flag & X_IS_SET) {
        deltaMM.x = fabs(vector->x - origin->x);
        deltaSteps.x = deltaMM.x * machine.x.steps_per_mm;
        longestStep = deltaSteps.x;
    }
    
    if(flag & Y_IS_SET) {
        deltaMM.y = fabs(vector->y - origin->y);
        deltaSteps.y = deltaMM.y * machine.y.steps_per_mm;
        if(longestStep < deltaSteps.y) {
            longestStep = deltaSteps.y;
        }
    }
    
    if(flag & Z_IS_SET) {
        deltaMM.z = fabs(vector->z - origin->z);
        deltaSteps.z = deltaMM.z * machine.z.steps_per_mm;
        if(longestStep < deltaSteps.z) {
            longestStep = deltaSteps.z;
        }
    }
    
    if(flag & A_IS_SET) {
        deltaMM.a = fabs(vector->a - origin->a);
        deltaSteps.a = deltaMM.a * machine.a.steps_per_mm;
        if(longestStep < deltaSteps.a) {
            longestStep = deltaSteps.a;
        }
    }
    
    if(flag & B_IS_SET) {
        deltaMM.b = fabs(vector->b - origin->b);
        deltaSteps.b = deltaMM.b * machine.b.steps_per_mm;
        if(longestStep < deltaSteps.b) {
            longestStep = deltaSteps.b;
        }
    }
    // feedrate is in mm/min
    // distance is in mm
    double distance = magnitude(flag, &deltaMM);
    // move duration in microseconds = distance / feedrate * 60,000,000
    double microseconds = distance / feedrate * 60000000.0;
    // time between steps for longest axis = microseconds / longestStep
    double step_delay = microseconds / longestStep;
    return (u32)round(step_delay);
}

// X3G COMMANDS

// 131 - Find axes minimums
// 132 - Find axes maximums

static int find_axes(unsigned direction, FILE *out)
{
    Point3d origin, vector;
    u32 flag = command.flag & XYZ_BIT_MASK;
    double feedrate = get_home_feedrate(command.flag);
    // compute the slowest feedrate
    if(flag & X_IS_SET) {
        if(machine.x.home_feedrate < feedrate) {
            feedrate = machine.x.home_feedrate;
        }
        origin.x = 0;
        vector.x = 1;
        // confirm machine compatibility
        if(direction != machine.x.endstop) {
            fprintf(stderr, "(line %u) GCode Semantic Warning: X axis homing to %s endstop", line_number, direction ? "maximum" : "minimum");
        }
    }
    if(flag & Y_IS_SET) {
        if(machine.y.home_feedrate < feedrate) {
            feedrate = machine.y.home_feedrate;
        }
        origin.y = 0;
        vector.y = 1;
        if(direction != machine.y.endstop) {
            fprintf(stderr, "(line %u) GCode Semantic Warning: Y axis homing to %s endstop", line_number, direction ? "maximum" : "minimum");
        }
    }
    if(flag & Z_IS_SET) {
        if(machine.z.home_feedrate < feedrate) {
            feedrate = machine.z.home_feedrate;
        }
        origin.z = 0;
        vector.z = 1;
        if(direction != machine.z.endstop) {
            fprintf(stderr, "(line %u) GCode Semantic Warning: Z axis homing to %s endstop", line_number, direction ? "maximum" : "minimum");
        }
    }
    if(fputc(direction == ENDSTOP_IS_MIN ? 131 :132, out) == EOF) return FAILURE;
    // uint8: Axes bitfield. Axes whose bits are set will be moved.
    if(fputc(flag, out) == EOF) return FAILURE;
    // uint32: Feedrate, in microseconds between steps on the max delta. (DDA)
    if(fputu32(feedrate_to_microseconds(flag, (Ptr5d)&origin, (Ptr5d)&vector, feedrate), out) == EOF) return FAILURE;
    // uint16: Timeout, in seconds.
    if(fputu16(machine.timeout, out) == EOF) return FAILURE;
    return 0;
}

// 133 - delay

static int delay(u32 milliseconds, FILE *out)
{
    if(fputc(133, out) == EOF) return FAILURE;
    // uint32: delay, in milliseconds
    if(fputu32(milliseconds, out) == EOF) return FAILURE;
    return 0;
}

// 134 - Change Tool
 
// 135 - Wait for tool ready

static int wait_for_tool(u32 tool_id, u32 timeout, FILE *out)
{
    currentTool = tool_id;
    if(fputc(135, out) == EOF) return FAILURE;
    // uint8: Tool ID of the tool to wait for
    if(fputc(tool_id, out) == EOF) return FAILURE;
    // uint16: delay between query packets sent to the tool, in ms (nominally 100 ms)
    if(fputu16(100, out) == EOF) return FAILURE;
    // uint16: Timeout before continuing without tool ready, in seconds (nominally 1 minute)
    if(fputu16(timeout, out) == EOF) return FAILURE;
    return 0;
}
 
// 136 - Tool action command
 
// 137 - Enable/disable axes
 
// 139 - Queue extended point
 
// 140 - Set extended position
 
// 141 - Wait for platform ready

static int wait_for_platform(u32 tool_id, u32 timeout, FILE *out)
{
    //currentTool = tool_id;
    if(fputc(141, out) == EOF) return FAILURE;
    // uint8: Tool ID of the tool to wait for
    if(fputc(tool_id, out) == EOF) return FAILURE;
    // uint16: delay between query packets sent to the tool, in ms (nominally 100 ms)
    if(fputu16(100, out) == EOF) return FAILURE;
    // uint16: Timeout before continuing without tool ready, in seconds (nominally 1 minute)
    if(fputu16(timeout, out) == EOF) return FAILURE;
    return 0;
}

// 142 - Queue extended point, new style
 
// 143 - Store home positions
 
// 144 - Recall home positions



// 145 - Set digital potentiometer value

static int set_pot_value(int axis, int value, FILE *out)
{
    if(fputc(145, out) == EOF) return FAILURE;
    // uint8: axis value (valid range 0-4) which axis pot to set
    if(fputc(axis, out) == EOF) return FAILURE;
    // uint8: value (valid range 0-127), values over max will be capped at max
    if(fputc(value, out) == EOF) return FAILURE;
    return 0;
}
 
// 146 - Set RGB LED value
 
// 147 - Set Beep
 
// 148 - Wait for button
 
// 149 - Display message to LCD
 
// 150 - Set Build Percentage
 
// 151 - Queue Song
 
// 152 - reset to Factory
 
// 153 - Build start notification
 
// 154 - Build end notification
 
// 155 - Queue extended point x3g

static int queue_point(double feedrate, FILE *out)
{
// int32: X coordinate, in steps
// int32: Y coordinate, in steps
// int32: Z coordinate, in steps
// int32: A coordinate, in steps
// int32: B coordinate, in steps
// uint32: DDA Feedrate, in steps/s
// uint8: Axes bitfield to specify which axes are relative. Any axis with a bit set should make a relative movement.
// float (single precision, 32 bit): mm distance for this move.  normal of XYZ if any of these axes are active, and AB for extruder only moves
// uint16: feedrate in mm/s, multiplied by 64 to assist fixed point calculation on the bot
    return 0;
}

// 157 - Stream Version

// return the length of the given file in bytes

static long get_filesize(FILE *file)
{
    long filesize = -1;
    fseek(file, 0L, SEEK_END);
    filesize = ftell(file);
    fseek(file, 0L, SEEK_SET);
    return filesize;
}

static char *normalize_word(char* p)
{
    // we expect a letter followed by a digit
    // [ a-zA-Z] [ +-]? [ 0-9]+ ('.' [ 0-9]*)?
    char *s = p + 1;
    char *e = p;
    while(isspace(*s)) s++;
    if(*s == '+' || *s == '-') {
        *e++ = *s++;
    }
    while(1) {
        // skip spaces
        if(isspace(*s)) {
            s++;
        }
        // append digits
        else if(isdigit(*s)) {
            *e++ = *s++;
        }
        else {
            break;
        }
    }
    if(*s == '.') {
        *e++ = *s++;
        while(1) {
            // skip spaces
            if(isspace(*s)) {
                s++;
            }
            // append digits
            else if(isdigit(*s)) {
                *e++ = *s++;
            }
            else {
                break;
            }
        }        
    }
    *e = 0;
    return s;
}

static char *normalize_comment(char *p) {
    // strip white space from the end of comment
    char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)(*--e))) *e = '\0';
    // strip white space from the beginning of comment.
    while(isspace(*p)) p++;
    return p;
}

static void usage()
{
    puts("Usage: gpx [-m <MACHINE> | -c <CONFIG>] INPUT [OUTPUT]");
    puts("\nMACHINE is the predefined machine type");
    puts("\n\t r|r1  = Replicator 1");
    puts("\tr2 = Replicator 2 (default)");
    puts("\tr2x = Replicator 2X");
    puts("\nCONFIG is the filename of a custom machine definition (ini)");
    puts("\nINPUT is the name of the sliced gcode input filename");
    puts("\nOUTPUT is the name of the x3g output filename");
    puts("\nCopyright (c) 2013 WHPThomas, All rights reserved.");

    exit(1);
}

int main(int argc, char * argv[])
{
    long filesize = 0;
    int rval = 1;
    int c, i;
    
    // we default to using pipes
    FILE *in = stdin;
    FILE *out = stdout;
    
    // READ COMMAND LINE
    
    // get the command line options
    while ((c = getopt(argc, argv, "om:c:")) != -1) {
        switch (c) {
            case 'm':
                if(strcasecmp(optarg, "r") == 0
                   || strcasecmp(optarg, "r1") == 0) {
                    machine = replicator_1;
                }
                else if(strcasecmp(optarg, "r2") == 0) {
                    machine = replicator_2;                    
                }
                else if(strcasecmp(optarg, "r2x") == 0) {
                    machine = replicator_2X;
                }
                else {
                    usage();
                }
                break;
            case 'c':
                /*
                TODO
                if(!get_custom_definition(&machine, optarg)) {
                    usage();
                };
                break;
                */
            case '?':
            default:
                usage();
        }
    }
    argc -= optind;
    argv += optind;
    
    // OPEN FILES FOR INPUT AND OUTPUT
    
    // open the input filename if one is provided
    if(argc > 0) {
        char *filename = argv[0];
        if((in = fopen(filename, "rw")) == NULL) {
            perror("Error opening input");
            exit(1);
        }
        filesize = get_filesize(in);
        argc--;
        argv++;
        // use the output filename if one is provided
        if(argc > 0) {
            filename = argv[0];
        }
        else {
            // or use the input filename with a .x3g extension
            char *dot = strrchr(filename, '.');
            if(dot) {
                long len = dot - filename;
                memcpy(buffer, filename, len);
                filename = buffer + len;
            }
            // or just append one if no .gcode extension is present
            else {
                filename = stpncpy(buffer, filename, 256 - 5);
            }
            *filename++ = '.';
            *filename++ = 'x';
            *filename++ = '3';
            *filename++ = 'g';
            *filename++ = '\0';
            filename = buffer;
        }
        if((out = fopen(filename, "wb")) == NULL) {
            perror("Error creating output");
            goto CLEANUP_AND_EXIT;
        }
    }
    
    // READ INPUT AND CONVERT TO OUTPUT
    
    // initialize current position to zero
    
    current.x = 0.0;
    current.y = 0.0;
    current.z = 0.0;
    
    current.a = 0.0;
    current.b = 0.0;
    
    current.f = get_home_feedrate(XYZ_BIT_MASK);
    current.l = 0.0;
    current.p = 0.0;
    current.r = 0.0;
    current.s = 0.0;
    
    currentOffset.x = 0.0;
    currentOffset.y = 0.0;
    currentOffset.z = 0.0;
    
    for(i = 0; i < 6; i++) {
        offset[i].x = 0.0;
        offset[i].y = 0.0;
        offset[i].z = 0.0;
    }
    
    currentTool = 0;
    
    line_number = 1;
    
    // at this point we have read the command line, set the machine definition
    // and both the input and output files are open, so its time to parse the
    // gcode input and convert it to x3g output
    while(fgets(buffer, 256, in) != NULL) {
        // reset flag state
        command.flag = 0;
        char *digits, *p = buffer;
        while(isspace(*p)) p++;
        // check for line number
        if(*p == 'n' || *p == 'N') {
            digits = p;
            p = normalize_word(p);
            if(*p == 0) {
                fprintf(stderr, "(line %u) GCode Syntax Error: line number command word 'N' is missing digits", line_number);
                goto CLEANUP_AND_EXIT;
            }
            line_number = atoi(digits);
        }
        else {
            line_number++;
        }
        // parse command words in command line
        while(*p != 0) {
            if(isalpha(*p)) {
                int c = *p;
                digits = p;
                p = normalize_word(p);
                switch(c) {
                        
                    // PARAMETERS

                        // Xnnn	 X coordinate, usually to move to
                    case 'x':
                    case 'X':
                        command.x = strtod(digits, NULL);
                        command.flag |= X_IS_SET;
                        break;
                        
                        // Ynnn	 Y coordinate, usually to move to
                    case 'y':
                    case 'Y':
                        command.y = strtod(digits, NULL);
                        command.flag |= Y_IS_SET;
                        break;
                        
                        // Znnn	 Z coordinate, usually to move to
                    case 'z':
                    case 'Z':
                        command.z = strtod(digits, NULL);
                        command.flag |= Z_IS_SET;
                        break;

                        // Annn	 Length of extrudate in mm.
                    case 'a':
                    case 'A':
                        command.a = strtod(digits, NULL);
                        command.flag |= A_IS_SET;
                        break;

                        // Bnnn	 Length of extrudate in mm.
                    case 'b':
                    case 'B':
                        command.b = strtod(digits, NULL);
                        command.flag |= B_IS_SET;
                        break;
                                                
                        // Ennn	 Length of extrudate in mm.
                    case 'e':
                    case 'E':
                        command.e = strtod(digits, NULL);
                        command.flag |= E_IS_SET;
                        break;
                        
                        // Fnnn	 Feedrate in mm per minute.
                    case 'f':
                    case 'F':
                        command.f = strtod(digits, NULL);
                        command.flag |= F_IS_SET;
                        break;
                        
                        // Lnnn	 Parameter - not currently used
                    case 'l':
                    case 'L':
                        command.l = strtod(digits, NULL);
                        command.flag |= L_IS_SET;
                        break;
                                                
                        // Pnnn	 Command parameter, such as a time in milliseconds
                    case 'p':
                    case 'P':
                        command.p = strtod(digits, NULL);
                        command.flag |= P_IS_SET;
                        break;
                            
                        // Rnnn	 Command Parameter, such as RPM
                    case 'r':
                    case 'R':
                        command.r = strtod(digits, NULL);
                        command.flag |= R_IS_SET;
                        break;
                        
                        // Snnn	 Command parameter, such as temperature
                    case 's':
                    case 'S':
                        command.s = strtod(digits, NULL);
                        command.flag |= S_IS_SET;
                        break;
                        
                    // COMMANDS

                        // Gnnn GCode command, such as move to a point
                    case 'g':
                    case 'G':
                        command.g = atoi(digits);
                        command.flag |= G_IS_SET;
                        break;
                        // Mnnn	 RepRap-defined command
                    case 'm':
                    case 'M':
                        command.m = atoi(digits);
                        command.flag |= M_IS_SET;
                        break;
                        // Tnnn	 Select tool nnn. In RepRap, tools are extruders
                    case 't':
                    case 'T':
                        command.t = atoi(digits);
                        command.flag |= T_IS_SET;
                        break;
                    
                    default:
                        fprintf(stderr, "(line %u) GCode Syntax Warning: unrecognised command word '%c'", line_number, c);
                }
            }
            else if(*p == ';') {
                // Comment
                command.comment = normalize_comment(p + 1);
                command.flag |= COMMENT_IS_SET;
                *p = 0;
            }
            else if(*p == '(') {
                // Comment
                char *e = strchr(p + 1, ')');
                if(e) {
                    *e = 0;
                    command.comment = normalize_comment(p + 1);
                    command.flag |= COMMENT_IS_SET;
                    p = e + 1;
                }
                else {
                    fprintf(stderr, "(line %u) GCode Syntax Warning: comment is missing closing ')'", line_number);
                    command.comment = normalize_comment(p + 1);
                    command.flag |= COMMENT_IS_SET;
                    *p = 0;                   
                }
            }
            else if(*p == '*') {
                // Checksum
                *p = 0;
            }
        }
        
        // CALCULATE TARGET POINT
        
        // x
        if(command.flag & X_IS_SET) {
            machineTarget.x = isMetric ? command.x : (command.x * 25.4);
            if(isRelative) machineTarget.x += current.x;
        }
        else {
            machineTarget.x = current.x;
        }
        
        // y
        if(command.flag & Y_IS_SET) {
            machineTarget.y = isMetric ? command.y : (command.y * 25.4);
            if(isRelative) machineTarget.y += current.y;
        }
        else {
            machineTarget.y = current.y;
        }
        
        // z
        if(command.flag & Z_IS_SET) {
            machineTarget.z = isMetric ? command.z : (command.z * 25.4);
            if(isRelative) machineTarget.z += current.z;
        }
        else {
            machineTarget.z = current.z;
        }
        
        if(command.flag & E_IS_SET) {
            if(currentTool == 0) {
                // a = e
                workTarget.a = isMetric ? command.e : (command.e * 25.4);
                if(isRelative) workTarget.a += current.a;
                
                // b
                if(command.flag & B_IS_SET) {
                    workTarget.b = isMetric ? command.b : (command.b * 25.4);
                    if(isRelative) workTarget.b += current.b;
                }
                else {
                    workTarget.b = current.b;
                }
            }
            else {                
                // a
                if(command.flag & A_IS_SET) {
                    workTarget.a = isMetric ? command.a : (command.a * 25.4);
                    if(isRelative) workTarget.a += current.a;
                }
                else {
                    workTarget.a = current.a;
                }
                
                // b = e
                workTarget.b = isMetric ? command.e : (command.e * 25.4);
                if(isRelative) workTarget.b += current.b;
            }
        }
        else {        
            // a
            if(command.flag & A_IS_SET) {
                workTarget.a = isMetric ? command.a : (command.a * 25.4);
                if(isRelative) workTarget.a += current.a;
            }
            else {
                workTarget.a = current.a;
            }
            // b
            if(command.flag & B_IS_SET) {
                workTarget.b = isMetric ? command.b : (command.b * 25.4);
                if(isRelative) workTarget.b += current.b;
            }
            else {
                workTarget.b = current.b;
            }
       }
        
        // it seems that feed rates should also be converted to metric
        workTarget.f = (command.flag & F_IS_SET) ? (isMetric ? command.f : (command.f * 25.4)) : current.f;
        
        workTarget.l = (command.flag & L_IS_SET) ? command.l : current.l;
        workTarget.p = (command.flag & P_IS_SET) ? command.p : current.p;
        workTarget.r = (command.flag & R_IS_SET) ? command.r : current.r;
        workTarget.s = (command.flag & S_IS_SET) ? command.s : current.s;
        
        // CALCULATE OFFSET
        
        workTarget.x = machineTarget.x + currentOffset.x;
        workTarget.y = machineTarget.y + currentOffset.y;
        workTarget.z = machineTarget.z + currentOffset.z;

        // INTERPRET COMMAND
        
        if(command.flag & G_IS_SET) {
            switch(command.g) {
                    // G0 - Rapid Positioning
                case 0:
                    if(command.flag & F_IS_SET) {
                        if(queue_point(workTarget.f, out) != SUCCESS) goto CLEANUP_AND_EXIT;
                    }
                    else {
                        Point3d delta;
                        if(command.flag & X_IS_SET) delta.x = fabs(workTarget.x - current.x);
                        if(command.flag & Y_IS_SET) delta.y = fabs(workTarget.y - current.y);
                        if(command.flag & Z_IS_SET) delta.z = fabs(workTarget.z - current.z);
                        double length = magnitude(command.flag & XYZ_BIT_MASK, (Ptr5d)&delta);
                        double candidate, feedrate = DBL_MAX;
                        if(command.flag & X_IS_SET && delta.x != 0.0) {
                            feedrate = machine.x.max_feedrate * length / delta.x;
                        }
                        if(command.flag & Y_IS_SET && delta.y != 0.0) {
                            candidate = machine.y.max_feedrate * length / delta.y;
                            if(feedrate > candidate) {
                                feedrate = candidate;
                            }
                        }
                        if(command.flag & Z_IS_SET && delta.z != 0.0) {
                            candidate = machine.z.max_feedrate * length / delta.z;
                            if(feedrate > candidate) {
                                feedrate = candidate;
                            }
                        }
                        if(feedrate == DBL_MAX) {
                            feedrate = machine.x.max_feedrate;
                        }
                        if(queue_point(feedrate, out) != SUCCESS) goto CLEANUP_AND_EXIT;
                    }
                    break;
                    
                    // G1 - Coordinated Motion
                case 1:
                    if(queue_point(workTarget.f, out) != SUCCESS) goto CLEANUP_AND_EXIT;
                    break;
                    
                    // G2 - Clockwise Arc
                    // G3 - Counter Clockwise Arc
                    
                    // G4 - Dwell
                case 4:
                    if(delay(workTarget.p, out) != SUCCESS) goto CLEANUP_AND_EXIT;
                    break;

                    // G10 - Create Coordinate System Offset from the Absolute one
                case 10:
                    if(command.flag & P_IS_SET && command.p >= 1.0 && command.p <= 6.0) {
                        i = (int)command.p - 1;
                        if(command.flag & X_IS_SET) offset[i].x = machineTarget.x;
                        if(command.flag & Y_IS_SET) offset[i].y = machineTarget.y;
                        if(command.flag & Z_IS_SET) offset[i].z = machineTarget.z;
                    }
                    else {
                        fprintf(stderr, "(line %u) GCode Syntax Error: G10 is missing coordiante system, use Pn where n is 1-6", line_number);
                        goto CLEANUP_AND_EXIT;
                    }
                    break;
                    
                    // G20 - Use Inches as Units
                case 20:
                    // G70 - Use Inches as Units
                case 70:
                    isMetric = 0;
                    break;
                    
                    // G21 - Use Milimeters as Units
                case 21:
                    // G71 - Use Milimeters as Units
                case 71:
                    isMetric = 1;
                    break;

                    // G53 - Set absolute coordinate system
                case 53:
                    currentOffset.x = 0.0;
                    currentOffset.y = 0.0;
                    currentOffset.z = 0.0;
                    break;

                    // G54 - Use coordinate system from G10 P1
                case 54:
                    currentOffset = offset[0];
                    break;

                    // G55 - Use coordinate system from G10 P2
                case 55:
                    currentOffset = offset[1];
                    break;
                    
                    // G56 - Use coordinate system from G10 P3
                case 56:
                    currentOffset = offset[2];
                    break;
                    
                    // G57 - Use coordinate system from G10 P4
                case 57:
                    currentOffset = offset[3];
                    break;
                    
                    // G58 - Use coordinate system from G10 P5
                case 58:
                    currentOffset = offset[4];
                    break;
                    
                    // G59 - Use coordinate system from G10 P6
                case 59:
                    currentOffset = offset[5];
                    break;
                    
                    // G90 - Absolute Positioning
                case 90:
                    isRelative = 0;
                    break;

                    // G91 - Relative Positioning
                case 91:
                    isRelative = 1;
                    break;

                    // G92 - Define current position on axes
                case 92:
                    if(command.flag & X_IS_SET) current.x = machineTarget.x;
                    if(command.flag & Y_IS_SET) current.y = machineTarget.y;
                    if(command.flag & Z_IS_SET) current.z = machineTarget.z;
                    if(command.flag & A_IS_SET) current.a = workTarget.a;
                    if(command.flag & B_IS_SET) current.b = workTarget.b;

                    if(command.flag & E_IS_SET) {
                        if(currentTool == 0) {
                            current.a = workTarget.a;
                        }
                        else {
                            current.b = workTarget.b;
                        }
                    }
                    break;
                    
                    // G97 - Spindle speed rate
                    
                    // G130 - Set given axes potentiometer Value
                case 130:
                    if(command.flag & X_IS_SET) if(set_pot_value(0, (int)command.x, out) == FAILURE) goto CLEANUP_AND_EXIT;
                    if(command.flag & Y_IS_SET) if(set_pot_value(0, (int)command.y, out) == FAILURE) goto CLEANUP_AND_EXIT;
                    if(command.flag & Z_IS_SET) if(set_pot_value(0, (int)command.z, out) == FAILURE) goto CLEANUP_AND_EXIT;
                    if(command.flag & A_IS_SET) if(set_pot_value(0, (int)command.a, out) == FAILURE) goto CLEANUP_AND_EXIT;
                    if(command.flag & B_IS_SET) if(set_pot_value(0, (int)command.b, out) == FAILURE) goto CLEANUP_AND_EXIT;
                    break;
                    
                    // G161 - Home given axes to minimum
                case 161:
                    if(find_axes(ENDSTOP_IS_MIN, out) != SUCCESS) goto CLEANUP_AND_EXIT;
                    break;
                    // G28 - Home given axes to maximum
                case 28:
                    // G162 - Home given axes to maximum
                case 162:
                    if(find_axes(ENDSTOP_IS_MAX, out) != SUCCESS) goto CLEANUP_AND_EXIT;
                    break;
                default:
                    fprintf(stderr, "(line %u) GCode Syntax Error: unrecognised Gcode command word 'G%i'", line_number, command.g);
            }
        }
        else if(command.flag & M_IS_SET) {
            switch(command.m) {
            // M0 - Unconditional Halt, not supported on SD?
            // M1 - Optional Halt, not supported on SD?
            // M2 - "End program
            // M3 - Spindle On - Clockwise
            // M4 - Spindle On - Counter Clockwise
            // M5 - Spindle Off
            
                    // M6 - Wait for toolhead to come up to reach (or exceed) temperature
                case 6:
                    if(command.flag & T_IS_SET) {
                        int timeout = command.flag & P_IS_SET ? (int)command.p : 0xFFFF;
                        if(wait_for_tool((int)command.t, timeout, out) == FAILURE) goto CLEANUP_AND_EXIT;
                    }
                    else {
                        fprintf(stderr, "(line %u) GCode Syntax Error: M6 is missing tool change paremeter 'T'", line_number);
                        goto CLEANUP_AND_EXIT;                        
                    }
                    break;
                    
            // M7 - Coolant A on (flood coolant)
            // M8 - Coolant B on (mist coolant)
            // M9 - All Coolant Off
            // M10 - Close Clamp
            // M11 - Open Clamp
            // M13 - Spindle CW and Coolant A On
            // M14 - Spindle CCW and Coolant A On
            // M17 - Enable Motor(s)
            // M18 - "Disable Motor(s)
            // M21 - Open Collet
            // M22 - Close Collet
            // M30 - Program Rewind
            // M40 - Change Gear Ratio to 0
            // M41 - Change Gear Ratio to 1
            // M42 - Change Gear Ratio to 2
            // M43 - "Change Gear Ratio to 3
            // M44 - Change Gear Ratio to 4
            // M45 - Change Gear Ratio to 5
            // M46 - Change Gear Ratio to 6
            // M50 - Read Spindle Speed
            // M70 - Display Message On Machine
            // M71 - Display Message, Wait For User Button Press
            // M72 - Play a Tone or Song
            // M73 - Manual Set Build %
            // M101 - Turn Extruder On, Forward
            // M102 - Turn Extruder On, Reverse
            // M103 - Turn Extruder Off
            // M104 - Set Temperature
            // M105 - Get Temperature
            // M106 - Turn Automated Build Platform (or the Fan, on older models) On
            // M107 - Turn Automated Build Platform (or the Fan, on older models) Off
            // M108 - Set Extruder's Max Speed (R = RPM, P = PWM)
            // M109 - Set Build Platform Temperature
            // M110 - Set Build Chamber Temperature
            // M126 - Valve Open
            // M127 - Valve Close
            // M128 - "Get Position
            // M131 - Store Current Position to EEPROM
            // M132 - Load Current Position from EEPROM
            // M140 - Set Build Platform Temperature
            // M141 - Set Chamber Temperature (Ignored)
            // M142 - Set Chamber Holding Pressure (Ignored)
            // M200 - Reset driver
            // M300 - Set Servo 1 Position
            // M301 - Set Servo 2 Position
            // M310 - Start data capture
            // M311 - Stop data capture
            // M312 - Log a note to the data capture store
            // M320 - Acceleration on for subsequent instructions
            // M321 - Acceleration off for subsequent instructions
            }
        }
        else if(command.flag & T_IS_SET) {
            // T0 - Set Current Tool 0
            
            // T1 - Set Current Tool 1
        }
        else if(command.flag & COMMAND_BIT_MASK) {
            if(queue_point(workTarget.f, out) == EOF) goto CLEANUP_AND_EXIT;
        }
        else if(command.flag & F_IS_SET) {
            current.f = workTarget.f;
        }
    }
    rval = 0;
    
CLEANUP_AND_EXIT:
    // close open files
    if(in != stdin) {
        fclose(in);
        if(out != stdout) {
            fclose(out);
        }
    }    
    return rval;
}

