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

// Simple tool to dump a .s3g file either from disk of from stdin
//
//     s3gdump filename
//
// or
//
//     s3gdump < filename

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "s3g.h"

#define GETOPTS_END -1

static void usage(FILE *f, const char *prog)
{
     if (f == NULL)
	  f = stderr;

     fprintf(f,
"Usage: %s -hE [file]\n"
"   file  -- The .s3g file to dump.  If not supplied then stdin is dumped\n"
"  ?, -h  -- This help message\n",
	     prog ? prog : "s3gdump");
}

int main(int argc, const char *argv[])
{
     int c;
     s3g_context_t *ctx;
     s3g_command_t cmd;
     int lineno, simple;

     simple = 0;
     while ((c = getopt(argc, (char **)argv, ":h?")) != GETOPTS_END)
     {
	  switch(c)
	  {
	  // Unknown switch
	  case ':' :
	  default :
	       usage(stderr, argv[0]);
	       return(1);

	  // Explicit help request
	  case 'h' :
	  case '?' :
	       usage(stdout, argv[0]);
	       return(0);
	  }
     }

     // Not that we care at this point
     argc -= optind;
     argv += optind;

     if (argc == 0)
	  ctx = s3g_open(0, NULL, 0, 0);
     else
	  ctx = s3g_open(0, (void *)argv[0], 0, 0);

     if (!ctx)
	  // Assume that s3g_open() has logged the problem to stderr
	  return(1);

     fprintf(stdout, "Command count: (Command ID) Command description\n");

     lineno = 0;
     while (!s3g_command_read(ctx, &cmd))
     {
	  fprintf(stdout, "%d: ", ++lineno);
	  if (simple)
	       fprintf(stdout, "(%d) %s\n", cmd.cmd_id, cmd.cmd_desc);
	  else
	       s3g_command_display(ctx, &cmd);
     }

     fprintf(stdout, "\nEOF\n");
     
     s3g_close(ctx);

     return(0);
}
