/*
Newsgroups: mod.std.unix
Subject: public domain AT&T getopt source
Date: 3 Nov 85 19:34:15 GMT

Here's something you've all been waiting for:  the AT&T public domain
source for getopt(3).  It is the code which was given out at the 1985
UNIFORUM conference in Dallas.  I obtained it by electronic mail
directly from AT&T.  The people there assure me that it is indeed
in the public domain.

25/1/2005 Henry Thomas <me(at)henri.net>

Ported this original AT&T version of 'getopt' to windows.

Added 'getargv' to parse the 'WinMain' 'lpCmdLine' argument and convert it
into an argc/argv pair. This should improve portability between windows and
unix/linux platforms.

*/

#include "321/getopt.h"
#ifndef HAS_GETOPT_H

#include <string.h>
#if defined(_MSC_VER)
#   include <io.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#ifdef HAS_WIN_GUI

/* The windows gui version reports command line errors using MessageBox */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define ERR(s, c) if(opterr) {\
    char buffer[64];\
    _snprintf(buffer, 64, "%s%s%c", argv[0], s, c);\
	MessageBox(NULL, buffer, "Command Line Error", MB_OK | MB_ICONERROR);\
}

#else

#define ERR(s, c) if(opterr) {\
    char errbuf[2];\
	errbuf[0] = c; errbuf[1] = '\n';\
	(void) write(2, argv[0], (unsigned)strlen(argv[0]));\
	(void) write(2, s, (unsigned)strlen(s));\
	(void) write(2, errbuf, 2);\
}

#endif

int	opterr = 1;
int	optind = 1;
int	optopt;
char *optarg;

int getopt(int argc, char **argv, char *opts)
{
	static int sp = 1;
	register int c;
	register char *cp;

    if(sp == 1) {
        if(optind >= argc ||
            argv[optind][0] != '-' || argv[optind][1] == '\0') {
            return(-1);
        }
        else if(strcmp(argv[optind], "--") == 0) {
            optind++;
            return(-1);
        }
    }
	optopt = c = argv[optind][sp];
    if(c == ':' || (cp = strchr(opts, c)) == NULL) {
        ERR(": illegal option -- ", c);
        if(argv[optind][++sp] == '\0') {
            optind++;
            sp = 1;
        }
        return('?');
    }
    if(*++cp == ':') {
        if(argv[optind][sp+1] != '\0') {
            optarg = &argv[optind++][sp+1];
        }
        else if(++optind >= argc) {
            ERR(": option requires an argument -- ", c);
            sp = 1;
            return('?');
        }
        else {
            optarg = argv[optind++];
        }
        sp = 1;
    }
    else {
        if(argv[optind][++sp] == '\0') {
            sp = 1;
            optind++;
        }
        optarg = NULL;
    }
	return(c);
}

#ifdef HAS_WIN_GUI

char **getargv(char *cl, int *argc)
{
    static char *argv[MAX_ARGC];
    int i;
    for(i = 0; i < MAX_ARGC; i++) {
        argv[i] = cl;
LOOP:
        cl++;
        if(cl[0] == ' ') {
            while(cl[1] == ' ') cl++;
            *cl = 0;
            cl++;
        }
        else {
            if(cl[0]) {
                goto LOOP;
            }
            i++;
            break;
        }
    }
    *argc = i;
    return argv;
}

#endif
#endif /* HAS_GETOPT_H */
