#Name: GPX
#Info: GCode to x3g conversion post processor
#Help: GPX
#Depend: GCode
#Type: postprocess
#Param: gpxPath(str:/Applications/GPX) GPX path
#Param: flags(str:-m r2) Flags

import platform
import os
from Cura.util import profile
from subprocess import call

def getGpxAppName():
	if platform.system() == 'Windows':
		if os.path.exists(gpxPath + '/gpx.exe'):
			return gpxPath + '/gpx.exe'
		return gpxPath + 'gpx.exe'
	if os.path.isfile(gpxPath + '/gpx'):
		return gpxPath + '/gpx'
	return gpxPath + 'gpx'

x3gFile = profile.getPreference('lastFile')
x3gFile = x3gFile[0:x3gFile.rfind('.')] + '.x3g'

commandList = [getGpxAppName(), '-p', '-r', flags, filename, x3gFile]
call(commandList)
