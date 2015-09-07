//
//  eeprominfo.h
//
//  Created by WHPThomas <me(at)henri(dot)net> on 1/04/13.
//  Copyright (c) 2013 WHPThomas, All rights reserved.
//
//  Updates 2015 by Dan Newman, <dan(dot)newman(at)mtbaldy(dot)us>
//   and Mark Walker, <markwal(at)hotmail(dot)com>
//
//  gpx references ReplicatorG sources from /src/replicatorg/drivers
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

#ifndef __eeprominfo_h__
#define __eeprominfo_h__

typedef enum { et_null, et_bitfield, et_boolean, et_byte, et_ushort, et_long, et_ulong, et_fixed, et_float, et_string } EepromType;

typedef struct tEepromMapping {
    const char *id;
    const char *label;
    const char *unit;
    unsigned address;
    EepromType et;
    int len;
    int minValue;
    int maxValue;
    const char *tooltip;
} EepromMapping;

typedef struct tEepromMap {
    unsigned short versionMin;
    unsigned short versionMax;
    unsigned char variant;
    EepromMapping *eepromMappings;
    int eepromMappingCount;
} EepromMap;

#endif // __eeprominfo_h__
