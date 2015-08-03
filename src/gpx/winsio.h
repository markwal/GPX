//
//  winsio.h
//
//  winsio - allow serial I/O without a cygwin dependency
//  added to gpx markwal 5/8/2015
//
//  GPX originally created by WHPThomas <me(at)henri(dot)net> on 1/04/13.
//
//  gpx is Copyright (c) 2013 WHPThomas, All rights reserved.
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
//
#define B0 0
#define B4800 4800
#define B9600 9600
#define B14400 14400
#define B19200 19200
#define B28800 28800
#define B38400 38400
#define B57600 57600
#define B115200 115200

#if !defined(SPEED_T_DEFINED)
typedef long speed_t;
#define SPEED_T_DEFINED
#endif
