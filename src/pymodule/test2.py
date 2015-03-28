import gpx
gpx.connect("/dev/ttyACM0", 0, "/home/pi/gpx.ini")
print gpx.write("M105")
print gpx.write("M114")
print gpx.write("G91")
gpx.disconnect()

