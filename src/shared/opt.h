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

#ifndef _OPT_H_
#define _OPT_H_

#define OPT_OK				0
#define OPT_ERR_BADARGS			1
#define OPT_ERR_BADFLOAT       		2
#define OPT_ERR_BADINT			3
#define OPT_ERR_CANNOT_PARSE		4
#define OPT_ERR_FOPEN			5
#define OPT_ERR_FREAD			6
#define OPT_ERR_NOMEM			7
#define OPT_ERR_NONLWSP_AFTER_GROUPNAME	8
#define OPT_ERR_NONLWSP_AFTER_OPTNAME	9

/* opt_loadfile()
 *
 * Read and load a .ini file.
 *
 * Call arguments:
 *
 *    const char *optfile
 *      Option file to open.  File is opened read-only.
 *
 *    int *lineno
 *      Optional pointer to an integer to receive the file line number
 *      for which an error has occurred.
 *
 *  Return values:
 *
 *    OPT_OK    -- Success
 *    OPT_ERR_x -- Processing error of some sort.  Use opt_strerror()
 *                   to generate an English-language error message.
 *                   The integer pointed at with lineno will contain the
 *                   errant line in the file if a parsing error occurred.
 *                   In the event of a non-parsing error (e.g., file not
 *                   found), lineno will be zero.
 */
  
int opt_loadfile(const char *optfile, int *lineno);


/* opt_dispose()
 *
 * Release the virtual memory allocated for the option data.
 */
 
void opt_dispose(void);

/* opt_get_str()
 *
 * Return as a string pointer the value of the specified option.  This
 * pointer points within the in-memory option data parsed from the .ini
 * file.
 *
 * Call arguments:
 *
 *   const char *group
 *     Name of the group to look under.
 *
 *   const char *name
 *     Name of the option.
 * Return values:
 *
 *   Pointer to the option's value.  If the option was found, it will be
 *   a pointer to a NUL terminated string.  DO NOT free() this pointer.
 *   The pointer will be valid until opt_dispose() is called.
 */

const char *opt_get_str(const char *group, const char *name);


/* opt_get_double()
 *
 * Return the floating point value of the specified option.  The value will
 * be parsed with strtod().  In the event of a parsing error by strtod(),
 * OPT_ERR_BADFLOAT will be returned.
 *
 * Call arguments:
 *
 *   double *d
 *     Pointer to a double to receive the result.
 *
 *   const char *group
 *     Name of the group to look under.
 *
 *   const char *name
 *     Name of the option.
 *
 * Return values:
 *
 *   OPT_OK           -- Success
 *   OPT_ERR_BADFLOAT -- Unable to parse the option's value as a double
 */

int opt_get_double(double *d, const char *group, const char *name);


/* opt_get_int()
 *
 * Return the integer value of the specified option.  The value will
 * be parsed with strtol() with 0 passed for the radix.  As such, the
 * value may be expressed as an octal, decimal, or hexadecimal value
 * in the configuration file.  In the event that strtol() cannot parse
 * the value then OPT_ERR_BADINT will be returned.
 *
 * Call arguments:
 *
 *   int *d
 *     Pointer to an int to receive the result.
 *
 *   const char *group
 *     Name of the group to look under.
 *
 *   const char *name
 *     Name of the option.
 *
 * Return values:
 *
 *   OPT_OK         -- Success
 *   OPT_ERR_BADINT -- Unable to parse the option's value as an integer
 */

int opt_get_int(int *i, const char *group, const char *name);


/* opt_strerror()
 *
 * Return a pointer to a printable, English-language message describing
 * an OPT_ERR_ error code.
 */
const char *opt_strerror(int err);

#if defined(OPT_DEBUG)
void opt_dump(void);
#endif

#endif
