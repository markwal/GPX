python-gpx is a python package that wraps GPX (Dr. Henry Thomas's g-code
translator).  You can use it to talk gcode to MakerBot's (pre gen 5) and to
clones.

#Work in progress
Currently this is just barely wired up.  So, for example, it really only
supports the g-codes that would normally come out of a slicer and doesn't
really do anything useful with g-codes that are intended to return useful
information like temperature, etc.

#How to install
via sources:
```
python setup.py install
```

#Examples
For fun, you can boot up python and type this at the python command prompt:
```
import gpx
# reads config from gpx.ini in current dir and writes log entries to stderr
gpx.connect("/dev/ttyACM0", 115200, "gpx.ini")
# play a song
gpx.write("M72 P1")
# cleanup
gpx.disconnect()
```
