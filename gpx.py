#Name: GPX
#Info: GCode to x3g conversion post processor
#Help: GPX
#Depend: GCode
#Type: postprocess
#Param: machineType(str:r2) Machine type
#Param: gpxPath(str:/Applications/GPX) GPX path

##  gpx.py - cura plugin
##
##  install in ~/.cura/plugins
##
##  Created by WHPThomas <me(at)henri(dot)net> on 9/25/13.
##
##  Copyright (c) 2013 WHPThomas, All rights reserved.
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) any later version.
##
##  This program is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with this program; if not, write to the Free Software Foundation,
##  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


import platform
import os
from Cura.util import profile
from subprocess import call

def getGpxAppName():
	if platform.system() == 'Windows':
		if os.path.exists(gpxPath + '/gpx.exe'):
			return gpxPath + '/gpx.exe'
		return gpxPath + 'gpx.exe'
	if os.path.isfile(gpxPath + '/' + configFile):
		return gpxPath + '/' + configFile
	return gpxPath + configFile

x3gFile = profile.getPreference('lastFile')
x3gFile = x3gFile[0:x3gFile.rfind('.')] + '.x3g'

commandList = [getGpxAppName(), '-p', '-r']
commandList += ['-m', machineType]
commandList += ['"' + filename + '"', '"' + x3gFile + '"']
call(commandList)