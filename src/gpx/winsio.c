//
//  winsio.c
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

#include <windows.h>
#include <io.h>
#undef ERROR

#include "winsio.h"
#include "gpx.h"


int gpx_sio_open(Gpx *gpx, const char *filename, speed_t baud_rate, int *sio_port)
{
    HANDLE h = CreateFile(filename, GENERIC_READ | GENERIC_WRITE, 0, 0,
            OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(gpx->log, "Error opening port");
        return 0;
    }
    *sio_port = _open_osfhandle((intptr_t)h, 0);

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) {
        fprintf(gpx->log, "Error getting port attributes");
        return 0;
    }

    dcb.BaudRate = baud_rate;
    dcb.ByteSize=8;
    dcb.StopBits=ONESTOPBIT;
    dcb.Parity=NOPARITY;
    if (!SetCommState(h, &dcb)) {
        fprintf(gpx->log, "Error setting port attributes");
        return 0;
    }

    COMMTIMEOUTS timeouts={0};
    timeouts.ReadIntervalTimeout=50;
    timeouts.ReadTotalTimeoutConstant=10;
    timeouts.ReadTotalTimeoutMultiplier=50;
    timeouts.WriteTotalTimeoutMultiplier=10;
    if (!SetCommTimeouts(h, &timeouts)) {
        fprintf(gpx->log, "Error setting port timeouts");
        return 0;
    }

    // not sure best way to flush the read buffer without tcflush
	if(gpx->open_delay > 0) {
		long_sleep(gpx->open_delay);
	}
    printf("reading bytes\n");
    unsigned char buffer[128];
    size_t bytes;
    while ((bytes = read(*sio_port, buffer, sizeof(buffer))) > 0) {
        unsigned char *p = buffer;
        while (bytes--)
            printf("%02x ", (unsigned)*p++);
    }
    printf("\ngot rval = %ld\n", (long)bytes);

    if(gpx->flag.verboseMode) fprintf(gpx->log, "Communicating via: %s" EOL, filename);
    return 1;
}

int ready_to_read(int fd)
{
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;

    DWORD errors;
    COMSTAT stat;
    return (ClearCommError(h, &errors, &stat) && stat.cbInQue > 0);
}
