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

/*  opt.c
 *
 *  ".ini"-style file parser for option=value pairs.
 *
 *   And, YES, gpx already has a different .ini file parser.  It is an event-driven,
 *   callback-style parser.  That is suitable to gpx and the macros which can be
 *   put into a gpx .ini file.  For other uses of the same .ini file -- uses where
 *   it's a fixed, rigid set of option names -- this style of parser is easier to use.
 *   Thus, we have two .ini file parsers.
 *
 *  About this parser...
 *
 *  0. Blank lines are ignored.
 *
 *  1. Comments are introduced on a line with either ';' or '#'.  A line with
 *     a comment may only have a comment.  Do not attempt to follow an option=value
 *     with a comment.
 *
 *  2. Presently there is no character escape mechanism to literalize a
 *     character.  Easy enough to add should it ever prove needed.
 *
 *  3. Presently there is no mechanism to enclose in quotes an option value
 *     and then have the quotes stripped by the parser.  This can be added
 *     should it be needed.
 *
 *  4. Leading and trailing linear whitespace (LWSP) is ignored.  Combined
 *     with 3 above, this means that if LWSP at the end of an option value
 *     is significant, then 3 above needs to be rectified.
 *
 *  5. Groups are started with a line of the form
 *
 *       [group]
 *
 *     All subsequent options are grouped under that group until such
 *     time that a new group is started.
 *
 *  6. A group containing commas in the name, creates multiple groups,
 *
 *       [group1, group2, group3]
 *
 *     Each subsequent option will be filed under each named group.
 *
 *  7. Option/value pairs are expressed one per line in the format
 *
 *        option-name=value
 *
 *     There can be any amount of LWSP before the option-name, between
 *     the option-name and the '=', after the '=', and after the value.
 *
 *  8. Any option/value pair specified before any group declarations are
 *     considered to be in the group whose name is the empty-string.
 *
 *  9. As the file is parsed, a linked list of option=value pairs is built.
 *     Options can subsequently be retrieved by specifying the group name
 *     and the option name.  The group and option names are considered to
 *     be case insensitive.
 *
 * 10. When retrieving an option value of type "double", strtod() is used
 *     to parse the value.
 *
 * 11. When retrieving an option value of type "int", strtol(, 0) is used
 *     to parse the value.  As such, the value may be expressed in octal,
 *     decimal, or hexadecimal.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "opt.h"

// Structure to store an option=value pair
//   The structure is always allocated such that its length is
//
//     sizeof(option_t) + strlen(name) + 1 + strlen(value)
//
//   Then name and value point into the buf[] field and store
//   their values in buf[].  Note that since sizeof(option_t)
//   includes 1 byte of buf, the allocated structure size
//   includes 1 byte for the NUL terminator of the value field.
//   Hence the length computation above including 1 byte for the
//   name field's NUL terminator but not the value field's.

typedef struct option_s {
     struct option_s *prev;
     struct option_s *next;
     char            *option;
     char            *value;
     char             buf[1];
} option_t;

static option_t *root = NULL;

// Forward declarations
static int opt_add(const char *group, const char *option, const char *value);
static int opt_add_inner(const char *group, size_t glen, const char *option, const char *value);
static const char *opt_find(const char *group, const char *option);
static int opt_parse_line(char *line, char **group);


// Dispose of the linked list of option=value pairs

void opt_dispose(void)
{
     option_t *tmp;

     while (root)
     {
	  tmp = root->next;
	  free(root);
	  root = tmp;
     }
}


// Add an option=value pair to the linked list of option=value pairs
// Group names are handled by prefixing the option name with the
// string
//
//     <group> 0x01
//
// The 0x01 is a sentinel to guard against
//
//    [foo]
//    bar=2
//
// being confused with
//
//    foobar=2

static int opt_add(const char *group, const char *option, const char *value)
{
     int iret;
     char *ptr, *tmp, *tmp0, *tmpend;

     ptr = group ? strchr(group, ',') : NULL;

     if (!ptr)
	  return(opt_add_inner(group, group ? strlen(group) : (size_t)0, option, value));

     // group has commas
     //   treat as multiple groups
     //   LWSP was already removed

     // Make a copy we can manipulate
     tmp0 = strdup(group);

     // Note the end of the string
     tmpend = tmp0 + strlen(tmp0);

     // Figure out where the "," is in our copy
     // We could do this a bit more simply with strtok(_r) but strtok(_r)
     // is not found on all platforms.
     tmp = tmp0;
     ptr = tmp + (ptr - group);
     *ptr = '\0';

     iret = 0;
loop:
     if ((iret = opt_add_inner(tmp, strlen(tmp), option, value)))
	  goto done;

     // Move to the next segment
     tmp = ptr + 1;

     // Finish up if the string has been exhausted
     if (tmp >= tmpend)
	  goto done;

     // Another comma?
     if ((ptr = strchr(tmp, ',')))
	  // Yes, another comma
	  *ptr = '\0';
     else
	  // Process remainder of the string and set a termination condition
	  ptr = tmpend;

     // Process this section
     goto loop;

done:
     if (tmp0)
	  free(tmp0);

     return(iret);
}


static int opt_add_inner(const char *group, size_t lg, const char *option, const char *value)
{
     size_t i, lo, lv;
     char *ptr;
     option_t *tmp;

     // Determine the lengths of each of the fields
     lo = option ? strlen(option) : 0;
     lv = value ? strlen(value) : 0;

     // Allocate an option_t structure
     tmp = (option_t *)calloc(1, sizeof(option_t) + lg + 1 + lo + 1 + lv);
     if (!tmp)
	  return(-1);

     // Option field receives <tolower(group)> 0x01 <tolower(option)> NUL
     tmp->option = tmp->buf;
     ptr = tmp->buf;
     for (i = 0; i < lg; i++) *ptr++ = tolower(*group++);
     *ptr++ = (char)0x01;
     for (i = 0; i < lo; i++) *ptr++ = tolower(*option++);
     *ptr++ = '\0';

     // Value field receives <value> NUL
     tmp->value = ptr;  // tmp->buf + lg + 1 + lo + 1;
     if (value) memcpy(tmp->value, value, lv);
     tmp->value[lv] = '\0';

     // Add this option to the head of the list
     // This will effectively make the last seen of a duplicate option prevail
     tmp->next = root;

     // Back link things
     // This is only used when dumping the options back to a file.  It facilitates
     // listing the options in the order in which they appeared in the file.
     if (root)
	  root->prev = tmp;

     // Make this new option the head of the list
     root = tmp;

     return(0);
}


// Find the indicated option in the linked list of options.
// The first match is returned, starting from the head of the list.

static const char *opt_find(const char *group, const char *option)
{
     char buf[64], *bufptr, *ptr;
     size_t i, lg, lo;
     const char *value;
     option_t *tmp;

     // If there are no options, then just return a no-match
     if (!root)
	  return((const char *)NULL);

     // Field lengths
     lg = group ? strlen(group) : 0;
     lo = option ? strlen(option) : 0;

     // Use our static buffer if possible; otherwise, allocate a buffer
     if ((lg + 1 + lo + 1) < sizeof(buf))
	  bufptr = buf;
     else
     {
	  bufptr = malloc(lg + 1 + lo + 1);
	  if (!bufptr)
	       return(NULL);
     }

     // Build our search string <tolower(group)> 0x01 <tolower>(option-name)
     ptr = bufptr;
     for (i = 0; i < lg; i++) *ptr++ = tolower(*group++);
     *ptr++ = (char)0x01;
     for (i = 0; i < lo; i++) *ptr++ = tolower(*option++);
     *ptr = '\0';

     // Now walk the list looking for this option
     value = NULL;
     tmp = root;
     while (tmp)
     {
	  if (0 == strcmp(tmp->option, bufptr))
	       break;
	  tmp = tmp->next;
     }

     // If tmp != NULL, then we found a match
     if (tmp)
	  value = tmp->value;

     // Release any memory we allocated
     if (bufptr && bufptr != buf)
	  free(bufptr);

     // Return the match or NULL if no match was found
     return(value);
}




// Parse a line of the configuration file, updating the linked
// list of option=value pairs

#define STATE_BEGIN			0
#define STATE_GROUP			1
#define STATE_LEFT_HAND_SIDE		2
#define STATE_FIND_DELIM		3
#define STATE_FIND_RIGHT_HAND_SIDE	4
#define STATE_RIGHT_HAND_SIDE		5

static int opt_parse_line(char *line, char **group)
{
     char c;
     char *inptr, *left, *outptr, *right, *ptr;
     const char *grp;
     int state;
     static const char empty_string[] = "";

     if (!line)
	  return(OPT_ERR_BADARGS);

     // ignore leading LWSP
     while (isspace(*line))
	  ++line;

     // Return now if the line was exhausted
     if (!(*line))
	  return(0);

     // And chew off any trailing LWSP
     //   strlen(line) >= 1 owing to prior test

     ptr = line + strlen(line) - 1;
     while ((ptr >= line) && isspace(*ptr))
	  *ptr-- = '\0';

     // Return now if the line was exhausted
     if (!(*line))
	  return(0);

     // Return now the line is a comment
     if (*line == ';' || *line == '#')
	  return(0);

     // We're ready to parse
     //
     // Case 1:  ';' comment
     // Case 2:  '#' comment
     // Case 3:  option-name LWSP '=' LWSP option-value
     // Case 4:  '[' LWSP group-name LWSP ']'

     state   = STATE_BEGIN;
     left    = NULL;
     right   = NULL;
     inptr   = line;
     outptr  = line;

     while ((c = *inptr++))
     {
	  switch(state)
	  {
	  case STATE_BEGIN:
	       if (c == '[')
	       {
		    state = STATE_GROUP;
		    left = inptr;
		    outptr = left;
	       }
	       else
	       {
		    state = STATE_LEFT_HAND_SIDE;
		    left = outptr;
		    // Lower case the option name
		    *outptr++ = tolower(c);
	       }
	       break;

	  case STATE_GROUP:
	       if (c == ']')
	       {
		    // End of group name

		    // NUL terminate
		    *outptr = '\0';

		    // watch out for trailing non-LWSP after the ']'
		    if (*inptr)
			 // There's non-LWSP after the ']'
			 return(OPT_ERR_NONLWSP_AFTER_GROUPNAME);
		    else if (group)
		    {
			 // Strip LWSP
			 inptr  = left;
			 outptr = left;
			 while ((c = *inptr++))
			      if (!isspace(c))
				   *outptr++ = c;
			 *outptr = '\0';
			 char *tmp = strdup(left);
			 if (!tmp)
			      return(OPT_ERR_NOMEM);
			 *group = tmp;
			 break;
		    }
	       }
	       else
		    // Lower case the group name
		    *outptr++ = tolower(c);
	       break;

	  case STATE_LEFT_HAND_SIDE:
	       if ((c == '=') || isspace(c))
	       {
		    // End of option name

		    // NUL terminated
		    *outptr++ = '\0';

		    // Look for the '='
		    //   There may be intervening LWSP
		    state = (c == '=') ? STATE_FIND_RIGHT_HAND_SIDE : STATE_FIND_DELIM;
		    right  = inptr;
		    outptr = right;
	       }
	       else
		    // Lower case the option name
		    *outptr++ = tolower(c);
	       break;

	  case STATE_FIND_DELIM:
	       if (c == '=')
	       {
		    state = STATE_FIND_RIGHT_HAND_SIDE;

		    // If EOL is next, then right[0] = NUL and so we're already NUL terminated
		    right  = inptr;
		    outptr = right;
	       }
	       else if (!isspace(c))
		    // Error -- non-LWSP seen before the '='
		    return(OPT_ERR_NONLWSP_AFTER_OPTNAME);
	       break;

	  case STATE_FIND_RIGHT_HAND_SIDE:
	       if (isspace(c))
	       {
		    // If EOL is next, then right[0] = NUL and so we're already NUL terminated
		    right  = inptr;
		    outptr = right;
		    break;
	       }
	       else
		    // We've started to see the start of the right hand side
		    state = STATE_RIGHT_HAND_SIDE;
	       // Fall through

	  case STATE_RIGHT_HAND_SIDE:
	       *outptr++ = c;
	       break;
	  }
     }

     if (state == STATE_RIGHT_HAND_SIDE)
	  *outptr = '\0';

     // Successful end states
     if ((state != STATE_RIGHT_HAND_SIDE) && (state != STATE_GROUP))
	  return(OPT_ERR_CANNOT_PARSE);

     // If we parsed a group name then there's nothing to do for now
     if (state == STATE_GROUP)
	  return(0);

     // Push this option into the list
     if ((group == NULL) || (*group == NULL))
	  grp = empty_string;
     else
	  grp = *group;

     return((opt_add(grp, left, right)) ? OPT_ERR_NOMEM : OPT_OK);
}


// Build a linked list of option=value pairs from a ".ini" style
// configuration file.

int opt_loadfile(const char *fname, int *lineno)
{
     char buffer[4096], *group;
     FILE *fp;
     int istat;

     if (lineno)
	  *lineno = 0;

     if (!fname)
	  return(OPT_ERR_BADARGS);

     fp = fopen(fname, "r");
     if (!fp)
	  return(OPT_ERR_FOPEN);

     group = NULL;

     while (fgets(buffer, sizeof(buffer), fp) != NULL)
     {
	  if (lineno)
	       *lineno += 1;
	  if ((istat = opt_parse_line(buffer, &group)))
	       break;
     }

     // See why we've finished the loop
     if (ferror(fp))
	  istat = OPT_ERR_FREAD;

     // otherwise, we exited because
     //   istat != 0 -- parsing error
     //   istat == 0 -- EOF

     if (group)
	  free(group);

     if (fp != NULL)
	  fclose(fp);

     return(istat);
}


// Return as a string the matching option's value

const char *opt_get_str(const char *group, const char *option)
{
     return(opt_find(group, option));
}


// Return by reference the double precision value of the matching option.
// The returned value is passed via the (option) d call argument.
//
// In the event of a parsing error, -1 is returned.  Otherwise, 0 is returned

int opt_get_double(double *d, const char *group, const char *option)
{
     double dval;
     const char *val;
     char *endptr = NULL;

     val = opt_find(group, option);
     if (!val)
	  return(0);

     endptr = NULL;
     dval = strtod(val, &endptr);
     if (endptr == val)
	  // Parsing error!
	  return(OPT_ERR_BADFLOAT);

     if (d)
	  *d = dval;

     return(0);
}


// Return by reference the integer value of the matching option.
// The returned value is passed via the (option) i call argument.
//
// In the event of a parsing error, -1 is returned.  Otherwise, 0 is returned

int opt_get_int(int *i, const char *group, const char *option)
{
     long lval;
     const char *val;
     char *endptr = NULL;

     val = opt_find(group, option);
     if (!val)
	  return(0);

     endptr = NULL;
     lval = strtol(val, &endptr, 0);
     if (endptr == val)
	  // Parsing error!
	  return(OPT_ERR_BADINT);

     if (i)
	  *i = (int)lval;

     return(0);
}


#if defined(OPT_DEBUG)

void opt_dump(void)
{
     option_t *opt;

     if (!root)
	  return;

     // Run to the end of the list
     //   We do this so that we can dump the options
     //   in the order they were read from the file.
     opt = root;
     while(opt->next)
	  opt = opt->next;

     while(opt)
     {
	  char *ptr = opt->option;
	  while (*ptr)
	  {
	       if (*ptr == (char)0x01)
		    fputc('/', stdout);
	       else
		    fputc(*ptr, stdout);
	       ptr++;
	  }
	  fprintf(stdout, " = %s\n", opt->value);
	  opt = opt->prev;
     }
}

#endif

static const char *errmsg[] = {
     /*  0 */  "success",
     /*  1 */  "programming error; invalid call arguments",
     /*  2 */  "incorrectly specified real number",
     /*  3 */  "incorrectly specified integer value",
     /*  4 */  "syntactically invalid line",
     /*  5 */  "unable to open the configuration file",
     /*  6 */  "unable to read the configuration file",
     /*  7 */  "insufficient virtual memory",
     /*  8 */  "non space characters following the group name",
     /*  9 */  "non space characters following the option name and preceeding '='",
};

#define ERROR_COUNT (sizeof(errmsg)/sizeof(char *))

// Provide an English languag) text string for a given error condition

const char *opt_strerror(int err)
{
     static const char *unknown = "programming error; unknown error code";

     if (err < 0 || err >= ERROR_COUNT)
	  return(unknown);
     else
	  return(errmsg[err]);	  
}
