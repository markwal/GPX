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
#include "gpx.h"

// TODO at the moment the module is taking twice as much memory for Gpx as it
// should to because gpx-main also has one. We can merge them via extern or pass
// it one way or the other, but perhaps the best way is to drop gpx-main from
// the module and reserve that for the CLI.  We'll need to refactor a couple of
// things from it however
static Gpx gpx;

#define SHOW(FN) if(gpx->flag.logMessages) {FN;}
#define VERBOSE(FN) if(gpx->flag.verboseMode && gpx->flag.logMessages) {FN;}

// Tio - translated serial io
// wraps Sio and adds translation output buffer
// translation is reprap style response
typedef struct tTio
{
    char translation[BUFFER_MAX + 1];
    size_t cur;
    Sio sio;
    struct {
        unsigned listingFiles:1;    // in the middle of an M20 response
    } flag;
} Tio;
static Tio tio;
int connected = 0;

static void tio_printf(Tio *tio, char const* fmt, ...)
{
    size_t result;
    va_list args;

    if (tio->cur >= sizeof(tio->translation))
        return;

    va_start(args, fmt);
    result = vsnprintf(tio->translation + tio->cur,
            sizeof(tio->translation) - tio->cur, fmt, args);
    if (result > 0)
        tio->cur += result;
    va_end(args);
}

// clean up anything left over from before
static void gpx_cleanup(void)
{
    connected = 0;
    if (gpx.log != NULL && gpx.log != stderr)
    {
        fclose(gpx.log);
        gpx.log = stderr;
    }
    if (tio.sio.port >= -1)
    {
        close(tio.sio.port);
        tio.sio.port = -1;
    }
}

// wrap port_handler and translate to the expect gcode response
#define COMMAND_OFFSET 2
#define EXTRUDER_ID_OFFSET 3
#define QUERY_COMMAND_OFFSET 4
#define EEPROM_LENGTH_OFFSET 8

static void translate_extruder_query_response(Gpx *gpx, Tio *tio, unsigned query_command, char *buffer)
{
    unsigned extruder_id = buffer[EXTRUDER_ID_OFFSET];

    switch (query_command) {
            // Query 00 - Query firmware version information
        case 0:
            // sio->response.firmware.version = read_16(gpx);
            break;

            // Query 02 - Get extruder temperature
        case 2:
            // like T0:170
            tio_printf(tio, " T");
            if (gpx->machine.extruder_count > 1)
                tio_printf(tio, "%u", extruder_id);
            tio_printf(tio, ":%u", tio->sio.response.temperature);
            break;

            // Query 22 - Is extruder ready
        case 22:
            // sio->response.isReady = read_8(gpx);
            break;

            // Query 30 - Get build platform temperature
        case 30:
            tio_printf(tio, " B:%u", tio->sio.response.temperature);
            break;

            // Query 32 - Get extruder target temperature
        case 32:
            // fallthrough
            // Query 33 - Get build platform target temperature
        case 33:
            tio_printf(tio, " /%u", tio->sio.response.temperature);
            break;

            // Query 35 - Is build platform ready?
        case 35:
            // sio->response.isReady = read_8(gpx);
            break;

            // Query 36 - Get extruder status
        case 36: /*
            if(gpx->flag.verboseMode && gpx->flag.logMessages) {
                fprintf(gpx->log, "Extruder T%u status" EOL, extruder_id);
                if(sio->response.extruder.flag.ready) fputs("Target temperature reached" EOL, gpx->log);
                if(sio->response.extruder.flag.notPluggedIn) fputs("The extruder or build plate is not plugged in" EOL, gpx->log);
                if(sio->response.extruder.flag.softwareCutoff) fputs("Above maximum allowable temperature recorded: heater shutdown for safety" EOL, gpx->log);
                if(sio->response.extruder.flag.temperatureDropping) fputs("Heater temperature dropped below target temperature" EOL, gpx->log);
                if(sio->response.extruder.flag.buildPlateError) fputs("An error was detected with the build plate heater or sensor" EOL, gpx->log);
                if(sio->response.extruder.flag.extruderError) fputs("An error was detected with the extruder heater or sensor" EOL, gpx->log);
            } */
            break;

            // Query 37 - Get PID state
        case 37: /*
            sio->response.pid.extruderError = read_16(gpx);
            sio->response.pid.extruderDelta = read_16(gpx);
            sio->response.pid.extruderOutput = read_16(gpx);
            sio->response.pid.buildPlateError = read_16(gpx);
            sio->response.pid.buildPlateDelta = read_16(gpx);
            sio->response.pid.buildPlateOutput = read_16(gpx); */
            break;
    }
}

// translate_handler
// Callback function for gpx_convert_and_send.  It's where we translate the
// s3g/x3g response into a text response that mimics a reprap printer.
static int translate_handler(Gpx *gpx, Tio *tio, char *buffer, size_t length)
{
    unsigned command = buffer[COMMAND_OFFSET];

    int rval = port_handler(gpx, &tio->sio, buffer, length);
    if (rval != SUCCESS) {
        VERBOSE(fprintf(gpx->log, "port_handler returned: rval = %d\n", rval);)
        return errno = rval;
    }

    switch (command) {
            // 10 - Extruder (tool) query response
        case 10: {
            unsigned query_command = buffer[QUERY_COMMAND_OFFSET];
            translate_extruder_query_response(gpx, tio, query_command, buffer);
            break;
        }

            // 18 - Get next filename
        case 18:
            if (!tio->flag.listingFiles && (gpx->command.flag & M_IS_SET) && gpx->command.m == 21) {
                // we used "get_next_filename(1)" to emulate M21
                if (tio->sio.response.sd.status == 0)
                    tio_printf(tio, "\nSD card ok");
                else
                    tio_printf(tio, "\nSD init fail");
            }
            else {
                // otherwise generate the M20 response
                if (!tio->flag.listingFiles) {
                    tio_printf(tio, "\nBegin file list\n");
                    tio->flag.listingFiles = 1;
                }
                if (!tio->sio.response.sd.filename[0]) {
                    tio_printf(tio, "End file list");
                    tio->flag.listingFiles = 0;
                }
                else
                    tio_printf(tio, "%s", tio->sio.response.sd.filename);
            }
            break;

            // 21 - Get extended position
        case 21:
            tio_printf(tio, " X:%0.2f Y:%0.2f Z:%0.2f A:%0.2f B:%0.2f",
                (double)tio->sio.response.position.x / gpx->machine.x.steps_per_mm,
                (double)tio->sio.response.position.y / gpx->machine.y.steps_per_mm,
                (double)tio->sio.response.position.z / gpx->machine.z.steps_per_mm,
                (double)tio->sio.response.position.a / gpx->machine.a.steps_per_mm,
                (double)tio->sio.response.position.b / gpx->machine.b.steps_per_mm);
            break;

            // 27 - Get advanced version number
        case 27: {
            char *variant = "Unknown";
            switch(tio->sio.response.firmware.variant) {
                case 0x01:
                    variant = "Makerbot";
                    break;
                case 0x80:
                    variant = "Sailfish";
                    break;
            }
            tio_printf(tio, "%s v%u.%u", variant, tio->sio.response.firmware.version / 100, tio->sio.response.firmware.version % 100);
            break;
        }
    }

    return rval;
}

// return the translation or set the error context and return NULL if failure
static PyObject *gpx_return_translation(int rval)
{
    if (rval < 0) {
        switch (rval) {
            case EOSERROR:
                return PyErr_SetFromErrno(PyExc_IOError);
            case ERROR:
                PyErr_SetString(PyExc_IOError, "GPX error.");
                break;
            case ESIOWRITE:
            case ESIOREAD:
            case ESIOFRAME:
            case ESIOCRC:
                PyErr_SetString(PyExc_IOError, "Serial communication error.");
                break;
        }
        return NULL;
    }

    fprintf(gpx.log, "tio.translation = %s\n", tio.translation);
    return Py_BuildValue("s", tio.translation);
}

static PyObject *gpx_write_string(const char *s)
{
    strncpy(gpx.buffer.in, s, sizeof(gpx.buffer.in));

    tio.cur = 0;
    tio_printf(&tio, "ok");

    return gpx_return_translation(gpx_convert_line(&gpx, gpx.buffer.in));
}

// convert from a long int value to a speed_t constant
// on error, sets python error state and returns B0
speed_t speed_from_long(long *baudrate)
{
    speed_t speed = B0;

    // TODO Baudrate warning for Replicator 2/2X with 57600?  Throw maybe?
    switch(*baudrate) {
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
            *baudrate=115200;
        case 115200:
            speed=B115200;
            break;
        default:
            sprintf(gpx.buffer.out, "Unsupported baud rate '%ld'\n", *baudrate);
            fprintf(gpx.log, gpx.buffer.out);
            PyErr_SetString(PyExc_ValueError, gpx.buffer.out);
            break;
    }
    return speed;
}

// def connect(port, baudrate, inipath, logpath)
static PyObject *gpx_connect(PyObject *self, PyObject *args)
{
    const char *port = NULL;
    long baudrate = 0;
    const char *inipath = NULL;
    const char *logpath = NULL;

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

    gpx.flag.verboseMode = 1;
    gpx.flag.logMessages = 1;
#ifdef ALWAYS_USE_STDERR
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

    // open the port
    speed_t speed = speed_from_long(&baudrate);
    if (speed == B0)
        return NULL;
    if (!gpx_sio_open(&gpx, port, speed, &tio.sio.port)) {
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, port);
    }

    // TODO build_name and framing
    gpx_start_convert(&gpx, "", 0);

    gpx.flag.framingEnabled = 1;
    gpx.flag.sioConnected = 1;
    gpx_register_callback(&gpx, (int (*)(Gpx*, void*, char*, size_t))translate_handler, &tio);

    tio.sio.in = NULL;
    tio.sio.bytes_out = tio.sio.bytes_in = 0;
    connected = 1;

    fprintf(gpx.log, "gpx connected to %s at %ld using %s and %s\n", port, baudrate, inipath, logpath);

    tio.cur = 0;
    tio_printf(&tio, "start\n");
    int rval = get_advanced_version_number(&gpx);
    if (rval >= 0) {
        tio_printf(&tio, "echo: gcode to x3g translation by GPX");
        return gpx_write_string("M21");
    }
    return gpx_return_translation(rval);
}

static PyObject *PyErr_NotConnected(void)
{
    PyErr_SetString(PyExc_IOError, "Not connected");
    return NULL;
}

// def write(data)
static PyObject *gpx_write(PyObject *self, PyObject *args)
{
    char *line;

    if (!connected)
        return PyErr_NotConnected();

    if (!PyArg_ParseTuple(args, "s", &line))
        return NULL;

    return gpx_write_string(line);
}

// def readnext()
static PyObject *gpx_readnext(PyObject *self, PyObject *args)
{
    int rval = SUCCESS;

    if (!connected)
        return PyErr_NotConnected();

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    tio.cur = 0;
    tio.translation[0] = 0;

    if (tio.flag.listingFiles) {
        rval = get_next_filename(&gpx, 0);
    }
    return gpx_return_translation(rval);
}

// def baudrate(long)
static PyObject *gpx_set_baudrate(PyObject *self, PyObject *args)
{
    struct termios tp;
    long baudrate;
    speed_t speed;

    if (!connected)
        return PyErr_NotConnected();

    if (!PyArg_ParseTuple(args, "l", &baudrate))
        return NULL;

    if(tcgetattr(tio.sio.port, &tp) < 0)
        return PyErr_SetFromErrno(PyExc_IOError);
    speed = speed_from_long(&baudrate);
    if (speed == B0)
        return NULL;
    cfsetspeed(&tp, speed);
    if(tcsetattr(tio.sio.port, TCSANOW, &tp) < 0)
        return PyErr_SetFromErrno(PyExc_IOError);

    return Py_BuildValue("i", 0);
}

// def disconnect()
static PyObject *gpx_disconnect(PyObject *self, PyObject *args)
{
    gpx_cleanup();
    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    return Py_BuildValue("i", 0);
}

// method table describes what is exposed to python
static PyMethodDef GpxMethods[] = {
    {"connect", gpx_connect, METH_VARARGS, "connect(port, baud = 0, inifilepath = None, logfilepath = None) Open the serial port to the printer and initialize the channel"},
    {"disconnect", gpx_disconnect, METH_VARARGS, "disconnect() Close the serial port and clean up."},
    {"write", gpx_write, METH_VARARGS, "write(string) Translate g-code into x3g and send."},
    {"readnext", gpx_readnext, METH_VARARGS, "readnext() read next response if any"},
    {"set_baudrate", gpx_set_baudrate, METH_VARARGS, "set_baudrate(long) Set the current baudrate for the connection to the printer."},
    {NULL, NULL, 0, NULL} // sentinel
};

__attribute__ ((visibility ("default"))) PyMODINIT_FUNC initgpx(void);

// python calls init<modulename> when the module is loaded
PyMODINIT_FUNC initgpx(void)
{
    PyObject *m = Py_InitModule("gpx", GpxMethods);
    if (m == NULL)
        return;
    tio.sio.port = -1;
    tio.flag.listingFiles = 0;
    gpx_initialize(&gpx, 1);
}
