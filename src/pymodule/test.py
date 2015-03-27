import gpx
gpx.connect("/dev/ttyACM0")
gpx.write("M72 P1")
gpx.disconnect()

