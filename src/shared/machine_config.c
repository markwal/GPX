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

/* config.c
 *
 * Load a machine definiton (configuration) from a .ini file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "opt.h"
#include "machine_config.h"
#include "machine.h"

#define MACHINE_ARRAY
#include "std_machines.h"

// Forward declarations
static int config_axis(Axis *axis, const char *name);
static int config_extruder(Extruder *axis, const char *name);


const Machine *config_get_machine(const char *type)
{
     Machine **ptr = machines;

     if (!type)
	  return(NULL);

     while (*ptr)
     {
	  if (0 == strcasecmp((*ptr)->type, type))
	       return(*ptr);
	  ++ptr;
     }
     return(NULL);
}

#define GET_UNSIGNED(f, s)						\
     opt = s;								\
     if ((iret = opt_get_int(&i, name, opt)))				\
	  goto error;							\
     axis->f = (unsigned)(0x7fffffff & i)

#define GET_DOUBLE(f, s)						\
     opt = s;								\
     if ((iret = opt_get_double(&axis->f, name, opt)))			\
	  goto error

static int config_axis(Axis *axis, const char *name)
{
     int i, iret;
     const char *opt;

     if (!axis || !name)
	  return(OPT_ERR_BADARGS);

     GET_UNSIGNED(endstop,        "endstop");
     axis->endstop = axis->endstop ? 1 : 0;

     GET_DOUBLE(max_feedrate,     "max_feedrate");
     GET_DOUBLE(max_accel,        "max_acceleration");
     GET_DOUBLE(max_speed_change, "max_speed_change");
     GET_DOUBLE(home_feedrate,    "home_feedrate");
     GET_DOUBLE(length,           "length");
     GET_DOUBLE(steps_per_mm,     "steps_per_mm");

     return(0);

error:
     fprintf(stderr, "Cannot parse the value for the \"%s\" option for "
	     "the \"%s\" axis; %s\n", opt ? opt : "???",
	     name ? name : "???", opt_strerror(iret));
     return(iret);
}


static void config_dump_axis(FILE *fp, const Axis *a, const char *name)
{
     if (!fp || !a)
	  return;

     if (name)
	  fprintf(fp, "[%s]\n", name);

     fprintf(fp, "steps_per_mm = %.10g\n",     a->steps_per_mm);
     fprintf(fp, "max_feedrate = %g\n",     a->max_feedrate);
     fprintf(fp, "max_acceleration = %g\n", a->max_accel);
     fprintf(fp, "max_speed_change = %g\n", a->max_speed_change);
     fprintf(fp, "home_feedrate = %g\n",    a->home_feedrate);
     fprintf(fp, "length = %g\n",           a->length);
     fprintf(fp, "endstop = %u\n",         a->endstop);
}

static int config_extruder(Extruder *axis, const char *name)
{
     int i, iret;
     const char *opt = NULL;

     if (!axis || !name)
	  return(OPT_ERR_BADARGS);

     GET_DOUBLE(max_feedrate,     "max_feedrate");
     GET_DOUBLE(max_accel,        "max_acceleration");
     GET_DOUBLE(max_speed_change, "max_speed_change");
     GET_DOUBLE(steps_per_mm,     "steps_per_mm");
     GET_DOUBLE(motor_steps,      "motor_steps");
     GET_UNSIGNED(has_heated_build_platform, "has_heated_build_platform");
     axis->has_heated_build_platform = axis->has_heated_build_platform ? 1 : 0;

     return(0);

error:
     fprintf(stderr, "Cannot parse the value for the \"%s\" option for "
	     "the \"%s\" axis; %s\n", opt ? opt : "???",
	     name ? name : "???", opt_strerror(iret));
     return(iret);
}

static void config_dump_extruder(FILE *fp, const Extruder *a, const char *name)
{
     if (!fp || !a)
	  return;

     if (name)
	  fprintf(fp, "[%s]\n", name);

     fprintf(fp, "steps_per_mm = %g\n",              a->steps_per_mm);
     fprintf(fp, "max_feedrate = %g\n",              a->max_feedrate);
     fprintf(fp, "max_acceleration = %g\n",          a->max_accel);
     fprintf(fp, "max_speed_change = %g\n",          a->max_speed_change);
     fprintf(fp, "motor_steps = %g\n",               a->motor_steps);
     fprintf(fp, "has_heated_build_platform = %u\n", a->has_heated_build_platform);
}

#undef GET_UNSIGNED
#undef GET_DOUBLE

#define GET_STRING(f, t, s)						\
     tmp = (char *)opt_get_str("printer", s);				\
     if (tmp) tmp = strdup(tmp);					\
     if (tmp) {								\
	  if (m->f && m->t) free((char *)m->f);				\
	  m->f = tmp;							\
	  m->t = 1; }

#define GET_UNSIGNED(f, s)						\
     opt = s;								\
     if ((iret = opt_get_int(&i, "printer", opt)))       		\
	  goto error;							\
     m->f = (unsigned)(0x7fffffff & i)

#define GET_DOUBLE(f, s)						\
     opt = s;								\
     if ((iret = opt_get_double(&m->f, "printer", opt)))		\
	  goto error

int config_machine(Machine *m, const Machine *def, const char *mtype)
{
     int i, iret;
     const char *opt;
     char *tmp;

     if (!m)
	  return(OPT_ERR_BADARGS);

     if (!def)
	  def = &replicator_2;

     // See if a machine type was specified in the config file
     //   If one was, then attempt to use it as the source of defaults
     //   If one wasn't, then use the passed in mtype if not NULL
     //   Finally, use def if all else fails.
     opt = opt_get_str("printer", "machine_type");
     if (!opt)
	  opt = mtype;

     i = 0;
     if (opt)
     {
	  const Machine *d = config_get_machine(opt);
	  if (d)
	  {
	       memcpy(m, d, sizeof(Machine));
	       i = 1;
	  }
     }

     // Fall back to using the passed in default machine as our defaults
     // We are assured of def being non-NULL: if it was NULL upon entry,
     // it was set to &replicator_2.
     if ((i == 0) && (m != def))
	  memcpy(m, def, sizeof(Machine));

     if ((iret = config_axis(&m->x,     "x"))) return(iret);
     if ((iret = config_axis(&m->y,     "y"))) return(iret);
     if ((iret = config_axis(&m->z,     "z"))) return(iret);
     if ((iret = config_extruder(&m->a, "a"))) return(iret);
     if ((iret = config_extruder(&m->b, "b"))) return(iret);

     GET_DOUBLE(nominal_filament_diameter, "filament_diameter");
     GET_DOUBLE(nominal_packing_density,   "packing_density");
     GET_DOUBLE(nozzle_diameter,           "nozzle_diameter");
     GET_DOUBLE(toolhead_offsets[0],       "toolhead_offset_x");
     GET_DOUBLE(toolhead_offsets[1],       "toolhead_offset_y");
     GET_DOUBLE(toolhead_offsets[2],       "toolhead_offset_z");
     GET_DOUBLE(jkn[0],                    "jkn_k");
     GET_DOUBLE(jkn[1],                    "jkn_k2");
     GET_UNSIGNED(extruder_count,          "extruder_count");
     GET_UNSIGNED(timeout,                 "timeout");
     // GET_STRING(type, free_type,           "machine_type");
     GET_STRING(desc, free_desc,           "machine_description");

     return(OPT_OK);

error:
     fprintf(stderr, "Cannot parse the value for the \"%s\" option; "
	     "%s\n", opt ? opt : "???", opt_strerror(iret));
     return(iret);
}

void config_dump(FILE *fp, const Machine *m)
{
     int dumped;

     if (!fp || !m)
	  return;

     fprintf(fp, "[printer]\n");

     if (m->type)
	  fprintf(fp, "machine_type = %s\n", m->type);
     if (m->desc)
	  fprintf(fp, "machine_description = %s\n", m->desc);

     fprintf(fp, "filament_diameter = %g\n", m->nominal_filament_diameter);
     fprintf(fp, "packing_density = %g\n",   m->nominal_packing_density);
     fprintf(fp, "nozzle_diameter = %g\n",   m->nozzle_diameter);
     fprintf(fp, "toolhead_offset_x = %g\n", m->toolhead_offsets[0]);
     fprintf(fp, "toolhead_offset_y = %g\n", m->toolhead_offsets[1]);
     fprintf(fp, "toolhead_offset_z = %g\n", m->toolhead_offsets[2]);
     fprintf(fp, "jkn_k = %g\n",             m->jkn[0]);
     fprintf(fp, "jkn_k2 = %g\n",            m->jkn[1]);
     fprintf(fp, "extruder_count = %u\n",    m->extruder_count);
     fprintf(fp, "timeout = %u\n",           m->timeout);

     fprintf(fp, "\n");

     dumped = 0x00;
     if (0 == memcmp(&m->x, &m->y, sizeof(Axis)))
     {
	  if (0 == memcmp(&m->x, &m->z, sizeof(Axis)))
	  {
	       dumped |= 0x7;
	       config_dump_axis(fp, &m->x, "x, y, z");
	  }
	  else
	  {
	       dumped |= 0x3;
	       config_dump_axis(fp, &m->x, "x, y");
	  }
     }
     else
     {
	  dumped |= 0x1;
	  config_dump_axis(fp, &m->x, "x");
     }

     fprintf(fp, "\n");

     if (0 == (dumped & 0x2))
     {
	  if (0 == memcmp(&m->y, &m->z, sizeof(Axis)))
	  {
	       dumped |= 0x06;
	       config_dump_axis(fp, &m->y, "y, z");
	  }
	  else
	  {
	       dumped |= 0x02;
	       config_dump_axis(fp, &m->y, "y");
	  }
	  fprintf(fp, "\n");
     }

     if (0 == (dumped & 0x04))
     {
	  config_dump_axis(fp, &m->z, "z"); 
	  fprintf(fp, "\n");
     }

     if (0 == memcmp(&m->a, &m->b, sizeof(Extruder)))
	  config_dump_extruder(fp, &m->a, "a, b");
     else
     {
	  config_dump_extruder(fp, &m->a, "a");
	  fprintf(fp, "\n");
	  config_dump_extruder(fp, &m->b, "b");
     }
}
