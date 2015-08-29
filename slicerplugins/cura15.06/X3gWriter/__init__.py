# Copyright (c) 2015 Ultimaker B.V.
# Cura is released under the terms of the AGPLv3 or higher.

from . import X3gWriter

from UM.i18n import i18nCatalog
catalog = i18nCatalog("plugins")

def getMetaData():
    return {
        "type": "mesh_writer",
        "plugin": {
            "name": "X3g Writer",
            "author": "MarkWal",
            "version": "1.0",
            "description": catalog.i18nc("X3g Writer Plugin Description", "Writes X3g to files")
        },

        "mesh_writer": {
            "extension": "x3g",
            "description": catalog.i18nc("X3g Writer File Description", "X3g File")
        }
    }

def register(app):
    return { "mesh_writer": X3gWriter.X3gWriter() }
