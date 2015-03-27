#include <Python.h>

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

#include <termios.h>

#define USE_GPX_SIO_OPEN
#include "../gpx.h"

// TODO at the moment the module is taking twice as much memory as it needs to
// because gpx-main also has one. We can merge them via extern or pass it one
// way or the other, but perhaps the best way is to drop gpx-main from the
// module and reserve that for the CLI.  We'll need to refactor a couple of
// things from it however
static Gpx gpx;
static Sio sio;
static int sio_port = -1;

// clean up anything left over from before
static void gpx_cleanup(void)
{
    if (gpx.log != NULL && gpx.log != stderr)
    {
        fclose(gpx.log);
        gpx.log = stderr;
    }
    if (sio_port >= -1)
    {
        close(sio_port);
        sio.port = sio_port = -1;
    }
}

// def connect(port, baudrate, inipath, logpath)
static PyObject *gpx_connect(PyObject *self, PyObject *args)
{
    const char *port = NULL;
    long baudrate = 0;
    const char *inipath = NULL;
    const char *logpath = NULL;
    speed_t speed;

    if (!PyArg_ParseTuple(args, "s|lss", &port, &baudrate, &inipath, &logpath))
        return NULL;

    gpx_cleanup();
    gpx_initialize(&gpx, 0);

    // open the log file
    if (logpath != NULL && (gpx.log = fopen(logpath, "w+")) == NULL) {
        fprintf(gpx.log, "Unable to open logfile (%s) for writing\n", logpath);
    }
    if (gpx.log == NULL)
        gpx.log = stderr;

#ifdef ALWAYS_USE_STDERR
    // TODO remove the else, forces debug to stderr
    if (gpx.log != NULL && gpx.log != stderr)
    {
        fclose(gpx.log);
        gpx.log = stderr;
    }
#endif

    // load the config
    if (inipath != NULL)
    {
        int lineno = gpx_load_config(&gpx, inipath);
        if (lineno < 0)
            fprintf(gpx.log, "Unable to load configuration file (%s)\n", inipath);
        if (lineno > 0) 
            fprintf(gpx.log, "(line %u) Configuration syntax error in %s: unrecognized parameters\n", lineno, inipath);
    }

    // TODO make a function out of this
    // TODO Baudrate warning for Replicator 2/2X with 57600?  Throw maybe?
    switch(baudrate) {
        case 4800:
            speed=B4800;
            break;
        case 9600:
            speed=B9600;
            break;
#ifdef B14400
        case 14400:
            speed=B14400;
            break;
#endif
        case 19200:
            speed=B19200;
            break;
#ifdef B28800
        case 28800:
            speed=B28800;
            break;
#endif
        case 38400:
            speed=B38400;
            break;
        case 57600:
            speed=B57600;
            break;
            // TODO auto detect speed when 0?
        case 0: // 0 means default of 115200
            baudrate=115200;
        case 115200:
            speed=B115200;
            break;
        default:
            sprintf(gpx.buffer.out, "Unsupported baud rate '%ld'\n", baudrate);
            fprintf(gpx.log, gpx.buffer.out);
            PyErr_SetString(PyExc_ValueError, gpx.buffer.out);
            return NULL;
    }

    // close an open port left over from a previous call
    if (sio_port >= 0) {
        close(sio_port);
        sio_port = -1;
    }

    // open the port
    if (!gpx_sio_open(&gpx, port, speed, &sio_port)) {
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, port);
    }

    gpx_start_convert(&gpx, "");

    gpx.flag.framingEnabled = 1;
    gpx.callbackHandler = (int (*)(Gpx*, void*, char*, size_t))port_handler;
    gpx.callbackData = &sio;

    sio.in = NULL;
    sio.port = sio_port;
    sio.bytes_out = sio.bytes_in = 0;

    fprintf(gpx.log, "gpx connected to %s at %ld using %s and %s\n", port, baudrate, inipath, logpath);

    return Py_BuildValue("i", 0);
}

// def disconnect()
static PyObject *gpx_disconnect(PyObject *self, PyObject *args)
{
    gpx_cleanup();
    if (!PyArg_ParseTuple(args, "")) // do we need to do this?
        return NULL;
    return Py_BuildValue("i", 0);
}

// def write(data)
static PyObject *gpx_write(PyObject *self, PyObject *args)
{
    char *line;
    int rval = 0;

    if (!PyArg_ParseTuple(args, "s", &line))
        return NULL;

    rval = gpx_convert_line(&gpx, line);
    if (rval < 0) {
        PyErr_SetString(PyExc_IOError, "Unknown error sending line.");
        return NULL;
    }
    return Py_BuildValue("i", 0);
}

static PyMethodDef GpxMethods[] = {
    {"connect", gpx_connect, METH_VARARGS, "connect(port, baud = 0, inifilepath = None, logfilepath = None) Open the serial port to the printer and initialize the channel"},
    {"disconnect", gpx_disconnect, METH_VARARGS, "disconnect() Close the serial port and clean up."},
    {"write", gpx_write, METH_VARARGS, "write(string) Translate g-code into x3g and send."},
    {NULL, NULL, 0, NULL} // sentinel
};

__attribute__ ((visibility ("default"))) PyMODINIT_FUNC initgpx(void);

PyMODINIT_FUNC initgpx(void)
{
    PyObject *m = Py_InitModule("gpx", GpxMethods);
    if (m == NULL)
        return;
    gpx_initialize(&gpx, 1);
}
