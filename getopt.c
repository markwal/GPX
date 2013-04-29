/*
Newsgroups: mod.std.unix
Subject: public domain AT&T getopt source
Date: 3 Nov 85 19:34:15 GMT

Here's something you've all been waiting for:  the AT&T public domain
source for getopt(3).  It is the code which was given out at the 1985
UNIFORUM conference in Dallas.  I obtained it by electronic mail
directly from AT&T.  The people there assure me that it is indeed
in the public domain.

25/1/2005 Henry Thomas <me(at)henri(dot)net>

Ported this original AT&T version of 'getopt' to windows.

Added 'getargv' to parse the 'WinMain' 'lpCmdLine' argument and convert it
into an argc/argv pair. This should improve portability between windows and
unix/linux platforms.

*/

#if defined(_MSC_VER)
#include "getopt.h"

#include <string.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>

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
        fprintf(stderr, "Command line error: illegal option -- %c\r\n", c);
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
            fprintf(stderr, "Command line error: option requires an argument -- %c\r\n", c);
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

#endif

