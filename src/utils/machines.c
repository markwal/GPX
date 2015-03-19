/*  Copyright (c) 2015, Dan Newman <dan(dot)newman(at)mtbaldy(dot)us>
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 * 
 *  1. Redistributions of source code must retain the above copyright notice, this
 *     list of conditions and the following disclaimer. 
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// machines.c
// Write an .ini file for each of the built in machines

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "opt.h"
#include "config.h"

#define MACHINE_ARRAY
#include "std_machines.h"

int main(int argc, const char *argv[])
{
     size_t dlen = 0;
     int iret = 0;
     Machine machine, **ptr = machines;

     // Directory path
     //   Must include "/" or "\".  You supply it; code is
     //   a tad more portable that way.
     if (argc == 2)
	  dlen = strlen(argv[1]);

     while (*ptr)
     {
	  if ((*ptr)->type)
	  {
	       char *fname;
	       size_t len = strlen((*ptr)->type);
	       fname = (char *)malloc(dlen + len + 5);
	       if (fname)
	       {
		    FILE *fp;

		    if (dlen) memcpy(fname, argv[1], dlen);
		    memcpy(fname + dlen, (*ptr)->type, len);
		    memcpy(fname + dlen + len, ".ini", 5);
		    fp = fopen(fname, "w");
		    if (fp)
		    {
			 memcpy(&machine, *ptr, sizeof(Machine));
			 config_dump(fp, &machine);
			 fclose(fp);
		    }
		    else
		    {
			 fprintf(stderr, "Unable to create the file \"%s\"\n", fname);
			 iret = 1;
		    }
		    free(fname);
	       }
	       ptr++;
	  }
     }

     return(iret);
}
