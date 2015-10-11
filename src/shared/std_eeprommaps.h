//
//  std_eeprommaps.h
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

#ifndef __std_eeprommaps_h__
#define __std_eeprommaps_h__

#include "eeprominfo.h"
#include "sailfish_7_7.h"

EepromMap eepromMaps[] = {
    { 707, 708, 0x80, eeprom_map_sailfish_7_7, (sizeof(eeprom_map_sailfish_7_7) / sizeof(EepromMapping)) },
};

#define eepromMapCount (sizeof(eepromMaps) / sizeof(EepromMap))

#endif // __std_eeprommaps_h__
