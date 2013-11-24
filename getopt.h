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

Added documentation to this header, so know what everything does.

*/

#ifndef __getopt_h__
#define __getopt_h__

#ifdef __cplusplus
extern "C" {
#endif

    
/* If the value of the 'opterr' variable is nonzero, then getopt prints an
error message to the standard error stream if it encounters an unknown option
character or an option with a missing required argument. The windows version
reports the error using a message box. This is the default behavior. If you
set this variable to zero, getopt does not print any messages, but it still
returns the character ? to indicate an error. */

extern int opterr;
    

/* When getopt encounters an unknown option character or an option with a
missing required argument, it stores that option character the 'optopt'
variable. You can use this for providing your own diagnostic messages. */

extern int optopt;
    

/* The 'optind' variable is set by getopt to the index of the next element of
the argv array to be processed. Once getopt has found all of the option
arguments, you can use this variable to determine where the remaining
non-option arguments begin. The initial value of this variable is 1. */

extern int optind;

    
/* The 'optarg' variable is set by getopt to point at the value of the
option argument, for those options that accept arguments. */

extern char *optarg;
    

/* The getopt function gets the next option argument from the argument list
specified by the argv and argc arguments. Normally these values come directly
from the arguments received by main.

The options argument is a string that specifies the option characters that are
valid for this program. An option character in this string can be followed
by a colon (':') to indicate that it takes a required argument. */

int getopt(int argc, char **argv, char *opts);


#ifdef __cplusplus
}
#endif

#endif /* __getopt_h__ */
