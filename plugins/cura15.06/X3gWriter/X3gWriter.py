# Copyright (c) 2015 Ultimaker B.V.
# Cura is released under the terms of the AGPLv3 or higher.

from UM.Mesh.MeshWriter import MeshWriter
from UM.Logger import Logger
from UM.Application import Application
from PyQt5.QtCore import QProcess
import io


class X3gWriter(MeshWriter):
    def __init__(self):
        super().__init__()
        self._gcode = None

    def write(self, file_name, storage_device, mesh_data):
        Logger.log("i", "In X3gWriter.write")
        if "x3g" in file_name:
            scene = Application.getInstance().getController().getScene()
            gcode_list = getattr(scene, "gcode_list")
            if gcode_list:
                # f = storage_device.openFile(file_name, "wt")
                Logger.log("i", "Writing X3g to file %s", file_name)
                p = QProcess()
                p.setReadChannel(1)
                p.setStandardOutputFile(file_name)
                p.start("gpx", ["-i"])
                p.waitForStarted()

                for gcode in gcode_list:
                    p.write(gcode)
                    if (p.canReadLine()):
                        Logger.log("i", "gpx: %s", p.readLine().data().decode('utf-8'))

                p.closeWriteChannel()
                if p.waitForFinished():
                    Logger.log("i", "gpx: %s", p.readAll().data().decode('utf-8'))
                p.close()
                # storage_device.closeFile(f)
                return True

        return False
