import gcodex3g as gpx
gpx.connect("/dev/ttyACM0", 115200, "/home/pi/gpx.ini")
gpx.write("M72 P1")
gpx.disconnect()

