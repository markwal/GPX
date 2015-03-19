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

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "machine.h"

/* config_machine()
 *
 * Using the machine "def" as defaults, populate the settings for machine "m"
 * from a previously digested .ini file.
 *
 * opt_loadfile() must be called before calling config_machine().  Do not
 * call opt_dispose() until after calling config_machine().
 *
 * Call arguments:
 *
 *   Machine *m
 *     The machine to populate from the .ini file.
 *
 *   const Machine *def
 *     A machine to use as the default template supplying values
 *     for options not specified in the configuration file.
 *
 * Return values
 *
 *   OPT_OK           -- Success
 *   OPT_ERR_BADARGS  -- Bad call arguments supplied.  One or both of m or def is NULL.
 *   OPT_ERR_BADFLOAT -- Unable to parse as a floating point value one of the option values.
 *   OPT_ERR_BADINT   -- Unable to parse as an integer value one of the option values.
 */

int config_machine(Machine *m, const Machine *def);

/* config_dump()
 *
 * Write to the file fp a .ini file describing the supplied machine.  The
 * file written will contain all machine settings.
 *
 * Call arguments:
 *
 *   FILE *fp
 *     File pointer of the file to write to.
 *
 *   const Machine *m
 *     Machine to output.
 *
 * Return values: None
 */

void config_dump(FILE *fp, const Machine *m);

const Machine *config_get_machine(const char *type);

#endif
