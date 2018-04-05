#include <Python.h>

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#include <termios.h>
#else
#include "winsio.h"
#endif

#include "eeprominfo.h"
#include "gpx.h"

// TODO at the moment the module is taking twice as much memory for Gpx as it
// should to because gpx-main also has one. We can merge them via extern or pass
// it one way or the other, but perhaps the best way is to drop gpx-main from
// the module and reserve that for the CLI.  We'll need to refactor a couple of
// things from it however
static Gpx gpx;
static Tio *tio;

static int connected = 0;

// Some custom python exceptions
static PyObject *pyerrCancelBuild;
static PyObject *pyerrBufferOverflow;
static PyObject *pyerrTimeout;
static PyObject *pyerrUnknownFirmware;

static void clear_state_for_cancel(void)
{
    tio_clear_state_for_cancel(tio);
    tio->cur = 0;
    tio->translation[0] = 0;
}

// wrap port_handler and translate to the expect gcode response
#define COMMAND_OFFSET 2
#define EXTRUDER_ID_OFFSET 3
#define QUERY_COMMAND_OFFSET 4
#define EEPROM_LENGTH_OFFSET 8

// return the translation or set the error context and return NULL if failure
static PyObject *py_return_translation(int rval)
{
    rval = gpx_return_translation(&gpx, rval);

    switch (rval) {
        case SUCCESS:
        case END_OF_FILE:
            break;

        case EOSERROR:
            return PyErr_SetFromErrno(PyExc_IOError);
        case ERROR:
            PyErr_SetString(PyExc_IOError, "GPX error");
            return NULL;
        case ESIOWRITE:
        case ESIOREAD:
        case ESIOFRAME:
        case ESIOCRC:
            PyErr_SetString(PyExc_IOError, "Serial communication error");
            return NULL;
        case ESIOTIMEOUT:
            PyErr_SetString(pyerrTimeout, "Timeout");
            return NULL;
        case 0x80:
            PyErr_SetString(PyExc_IOError, "Generic Packet error");
            return NULL;
        case 0x82: // Action buffer overflow
            PyErr_SetString(pyerrBufferOverflow, "Buffer overflow");
            return NULL;
        case 0x83:
            break;
        case 0x84:
            PyErr_SetString(PyExc_IOError, "Query packet too big");
            return NULL;
        case 0x85:
            PyErr_SetString(PyExc_IOError, "Command not supported or recognized");
            return NULL;
        case 0x87:
        case 0x88:
            break;
        case 0x89:
            PyErr_SetString(pyerrCancelBuild, "Cancel build");
            return NULL;
        case 0x8A:
            break;
        case 0x8B:
            break;
        case 0x8C:
            break;

        default:
            PyErr_SetString(PyExc_IOError, "Unknown error.");
            return NULL;
    }
    return Py_BuildValue("s", tio->translation);
}

static PyObject *py_write_string(const char *s)
{
    return py_return_translation(gpx_write_string_core(&gpx, s));
}

// def connect(port, baudrate, inipath, logpath)
static PyObject *py_connect(PyObject *self, PyObject *args)
{
    const char *port = NULL;
    long baudrate = 0;
    const char *inipath = NULL;
    const char *logpath = NULL;
    int verbose = 0;

    if (!PyArg_ParseTuple(args, "s|lssi", &port, &baudrate, &inipath, &logpath, &verbose))
        return NULL;

    tio_cleanup(tio);
    connected = 1;
    gpx_initialize(&gpx, 0);
    gpx.axis.positionKnown = 0;
    gpx.flag.M106AlwaysValve = 1;
    gpx.flag.verboseSioMode = gpx.flag.verboseMode = verbose;
    gpx.flag.logMessages = 1;

    // open the log file
    if (logpath != NULL && (gpx.log = fopen(logpath, "a")) == NULL) {
        fprintf(stderr, "Unable to open logfile (%s) for writing\n", logpath);
    }
    if (gpx.log == NULL)
        gpx.log = stderr;
#ifdef ALWAYS_USE_STDERR
    else if (gpx.log != stderr)
    {
        fclose(gpx.log);
        gpx.log = stderr;
    }
#endif

    // load the config
    if (inipath != NULL)
    {
        int lineno = gpx_load_config(&gpx, inipath);
        if (lineno < 0) {
            fprintf(gpx.log, "Unable to load configuration file (%s)\n", inipath);
            tio_printf(tio, "Error: Unable to load configuration file (%s)\n", inipath);
        }
        if (lineno > 0) {
            tio_log_printf(tio, "(line %u) Configuration syntax error in %s: unrecognized parameters\n", lineno, inipath);
        }
    }

    int rval = gpx_connect(&gpx, port, baudrate);
    tio->sio.flag.shortRetryBufferOverflowOnly = 1;
    switch (rval) {
        case ESIOBADBAUD:
            PyErr_SetString(PyExc_ValueError, "Unsupported baudrate");
            return NULL;
        case EOSERROR:
            return PyErr_SetFromErrnoWithFilename(PyExc_OSError, port);
    }

    return py_return_translation(rval);
}

static PyObject *PyErr_NotConnected(void)
{
    PyErr_SetString(PyExc_IOError, "Not connected");
    return NULL;
}

// def start()
//  Intended to be called after connect to have the first conversation with the
//  bot, connect merely opens the port. Separating the two allows for a pause
//  between the calls at the python level so multithreading works.
static PyObject *py_start(PyObject *self, PyObject *args)
{
    if (!connected)
        return PyErr_NotConnected();

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    tio->cur = 0;
    tio->translation[0] = 0;
    int rval = get_advanced_version_number(&gpx);
    if (rval >= 0) {
        tio->waitflag.waitForEmptyQueue = 1;
        tio_printf(tio, "\necho: gcode to x3g translation by GPX");
        rval = gpx_write_string(&gpx, "M21");
    }
    return py_return_translation(rval);
}

// def write(data)
static PyObject *py_write(PyObject *self, PyObject *args)
{
    char *line;

    if (!connected)
        return PyErr_NotConnected();

    if (!PyArg_ParseTuple(args, "s", &line))
        return NULL;

    tio->cur = 0;
    tio->translation[0] = 0;
    tio->waitflag.waitForBuffer = 0; // maybe clear this every time?
    tio->flag.okPending = !tio->waiting;
    PyObject *rval = py_write_string(line);
    tio->flag.okPending = 0;
    return rval;
}

// def readnext()
static PyObject *py_readnext(PyObject *self, PyObject *args)
{
    int rval = SUCCESS;

    if (!connected)
        return PyErr_NotConnected();

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    if (gpx.flag.verboseMode)
        fprintf(gpx.log, "i");
    tio->cur = 0;
    tio->translation[0] = 0;

    if (tio->flag.listingFiles) {
        rval = get_next_filename(&gpx, 0);
    }
    else if (tio->waiting) {
        if (gpx.flag.verboseMode)
            fprintf(gpx.log, "tio->waiting = %u\n", tio->waiting);
        if (!tio->waitflag.waitForCancelSync) {
            if (tio->waitflag.waitForUnpause)
                rval = get_build_statistics(&gpx);
            // if we're waiting for the queue to drain, do that before checking on
            // anything else
            if (rval == SUCCESS && (tio->waitflag.waitForEmptyQueue || tio->waitflag.waitForButton))
                rval = is_ready(&gpx);
            if (rval == SUCCESS && !tio->waitflag.waitForEmptyQueue) {
                if (tio->waitflag.waitForStart || tio->waitflag.waitForBotCancel)
                    rval = get_build_statistics(&gpx);
                if (rval == SUCCESS && tio->waitflag.waitForPlatform)
                    rval = is_build_platform_ready(&gpx, 0);
                if (rval == SUCCESS && tio->waitflag.waitForExtruderA)
                    rval = is_extruder_ready(&gpx, 0);
                if (rval == SUCCESS && tio->waitflag.waitForExtruderB)
                    rval = is_extruder_ready(&gpx, 1);
            }
        }
        if (gpx.flag.verboseMode)
            fprintf(gpx.log, "tio->waiting = %u and rval = %d\n", tio->waiting, rval);
        if (rval == SUCCESS) {
            if (tio->waiting) {
                if (gpx.flag.verboseMode) {
                    tio_printf(tio, "// echo: tio->waiting = 0x%x\n", tio->waiting);
                    fprintf(gpx.log, "o");
                }
                return py_write_string("M105");
            }
            tio->cur = 0;
            tio_printf(tio, "ok");
        }
    }
    else if (tio->flag.waitClearedByCancel) {
        if(gpx.flag.verboseMode)
            fprintf(gpx.log, "adding ok for wait cleared by cancel\n");
        tio->flag.waitClearedByCancel = 0;
        tio_printf(tio, "ok");
    }
    if (gpx.flag.verboseMode)
        fprintf(gpx.log, "o");
    return py_return_translation(rval);
}

#if !defined(_WIN32) && !defined(_WIN64)
// def baudrate(long)
static PyObject *py_set_baudrate(PyObject *self, PyObject *args)
{
    struct termios tp;
    long baudrate;
    speed_t speed;

    if (!connected)
        return PyErr_NotConnected();

    if (!PyArg_ParseTuple(args, "l", &baudrate))
        return NULL;

    if(tcgetattr(tio->sio.port, &tp) < 0)
        return PyErr_SetFromErrno(PyExc_IOError);
    speed = speed_from_long(&baudrate);
    if (speed == B0)
        return NULL;
    cfsetspeed(&tp, speed);
    if(tcsetattr(tio->sio.port, TCSANOW, &tp) < 0)
        return PyErr_SetFromErrno(PyExc_IOError);

    return Py_BuildValue("i", 0);
}
#else
static PyObject *py_set_baudrate(PyObject *self, PyObject *args)
{
    long baudrate;

    if (!connected)
        return PyErr_NotConnected();

    if (!PyArg_ParseTuple(args, "l", &baudrate))
        return NULL;

    // TODO Windows set_baudrate

    return NULL;
}
#endif // _WIN32 || _WIN64

// def disconnect()
static PyObject *py_disconnect(PyObject *self, PyObject *args)
{
    tio_cleanup(tio);
    connected = 0;
    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    return Py_BuildValue("i", 0);
}

// def get_machine_defaults(machine_type_id)
// for example: machine_info = get_machine_defaults("r1d")
static PyObject *py_get_machine_defaults(PyObject *self, PyObject *args)
{
    char *machine_type_id;

    if (!PyArg_ParseTuple(args, "s", &machine_type_id))
        return NULL;

    Machine *machine = gpx_find_machine(machine_type_id);
    fflush(gpx.log);
    if (machine == NULL) {
        PyErr_SetString(PyExc_ValueError, "Machine id not found");
        return NULL;
    }

    return Py_BuildValue("{s{sd,sd,sd,si}s{sd,sd,sd,si}s{sd,sd,sd,si}s{sd,sd,sd,si}s{sd,sd,sd,si}s{sdsdsdsisi}}",
        "x",
            "max_feedrate", machine->x.max_feedrate,
            "home_feedrate", machine->x.home_feedrate,
            "steps_per_mm", machine->x.steps_per_mm,
            "endstop", machine->x.endstop,
        "y",
            "max_feedrate", machine->y.max_feedrate,
            "home_feedrate", machine->y.home_feedrate,
            "steps_per_mm", machine->y.steps_per_mm,
            "endstop", machine->y.endstop,
        "z",
            "max_feedrate", machine->z.max_feedrate,
            "home_feedrate", machine->z.home_feedrate,
            "steps_per_mm", machine->z.steps_per_mm,
            "endstop", machine->z.endstop,
        "a",
            "max_feedrate", machine->a.max_feedrate,
            "steps_per_mm", machine->a.steps_per_mm,
            "motor_steps", machine->a.motor_steps,
            "has_heated_build_platform", machine->a.has_heated_build_platform,
        "b",
            "max_feedrate", machine->b.max_feedrate,
            "steps_per_mm", machine->b.steps_per_mm,
            "motor_steps", machine->b.motor_steps,
            "has_heated_build_platform", machine->b.has_heated_build_platform,
        "machine",
            "slicer_filament_diameter", machine->nominal_filament_diameter,
            "packing_density", machine->nominal_packing_density,
            "nozzle_diameter", machine->nozzle_diameter,
            "extruder_count", machine->extruder_count,
            "timeout", machine->timeout
    );

}

// def read_ini(ini_filepath)
static PyObject *py_read_ini(PyObject *self, PyObject *args)
{
    const char *inipath = NULL;

    if (!PyArg_ParseTuple(args, "s", &inipath))
        return NULL;

    int lineno = gpx_load_config(&gpx, inipath);
    if (lineno == 0)
        return Py_BuildValue("i", 0); // success

    if (lineno < 0)
        fprintf(gpx.log, "Unable to load configuration file (%s)\n", inipath);
    if (lineno > 0)
        fprintf(gpx.log, "(line %u) Configuration syntax error in %s: unrecognized parameters\n", lineno, inipath);
    fflush(gpx.log);

    PyErr_SetString(PyExc_ValueError, "Unable to load ini file");
    return Py_BuildValue("i", 0);
}

// def reset_ini()
// reset settings to defaults
static PyObject *py_reset_ini(PyObject *self, PyObject *args)
{
    // some state survives reset_ini
    void *callbackHandler = gpx.callbackHandler;
    void *resultHandler = gpx.resultHandler;
    void *callbackData = gpx.callbackData;
    FILE *log = gpx.log;
    unsigned verbose = gpx.flag.verboseMode;

    // nuke it all
    gpx_initialize(&gpx, 1);
    gpx.axis.positionKnown = 0;
    gpx.flag.M106AlwaysValve = 1;

    // restore some stuff, plus we're still in pymodule mode
    gpx.callbackHandler = callbackHandler;
    gpx.resultHandler = resultHandler;
    gpx.callbackData = callbackData;
    gpx.log = log;
    gpx.flag.framingEnabled = 1;
    gpx.flag.sioConnected = 1;
    gpx.sio = &tio->sio;
    gpx.flag.verboseMode = verbose;
    gpx.flag.logMessages = 1;

    return Py_BuildValue("i", 0);
}

// def waiting()
// is the bot waiting for something?
static PyObject *py_waiting(PyObject *self, PyObject *args)
{
    if (!connected)
        return PyErr_NotConnected();

    if (tio->waiting || tio->flag.waitClearedByCancel)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

// def build_started()
// are we printing a build?
static PyObject *py_build_started(PyObject *self, PyObject *args)
{
    if (gpx.flag.programState == RUNNING_STATE)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

// def build_paused()
// is the build paused on the LCD?
static PyObject *py_build_paused(PyObject *self, PyObject *args)
{
    if (!connected)
        return PyErr_NotConnected();

    int rval = get_build_statistics(&gpx);
    // if we fail, is that a yes or a no?
    if (rval != SUCCESS) {
        PyErr_SetString(PyExc_IOError, "Unable to get build statistics.");
        return NULL;
    }

    if (tio->waitflag.waitForUnpause)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

// def listing_files()
// are we in the middle of listing files from the SD card?
static PyObject *py_listing_files(PyObject *self, PyObject *args)
{
    if (!connected)
        return PyErr_NotConnected();

    if (tio->flag.listingFiles)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

// def reprap_flavor(turn_on_reprap)
static PyObject *py_reprap_flavor(PyObject *self, PyObject *args)
{
    if (!connected)
        return PyErr_NotConnected();

    int reprap = 1;
    if (!PyArg_ParseTuple(args, "i", &reprap))
        return NULL;

    int rval = gpx.flag.reprapFlavor;
    gpx.flag.reprapFlavor = !!reprap;
    if (rval)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

// ----- immediate mode commands ----
// There isn't a gcode equivalent of these since the gcode "standard" always waits
// for the buffer to drain. Thus, these entry points are provided to interrupt
// the queue handling

// helper for py_stop and py_abort for post abort state
static PyObject *set_build_aborted_state(Gpx *gpx)
{
    int rval = SUCCESS;

    VERBOSE( fprintf(gpx->log, "set_build_aborted_state\n") );
    if (gpx->flag.programState == RUNNING_STATE) {
        VERBOSE( fprintf(gpx->log, "currently RUNNING_STATE calling end_build\n") );
        gpx->flag.programState = READY_STATE;

        // wait for the bot to be back up after the abort
        int retries = 5;
        while (retries--) {
            rval = set_build_progress(gpx, 100);
            if (rval == 0x8B)
                return py_return_translation(rval);
            if (rval == SUCCESS || rval != ESIOTIMEOUT)
                break;
        }
        rval = end_build(gpx);
    }
    return py_return_translation(rval);
}

// def stop(halt_steppers = True, clear_queue = True)
static PyObject *py_stop(PyObject *self, PyObject *args)
{
    if (!connected)
        return PyErr_NotConnected();

    int halt_steppers = 1;
    int clear_queue = 1;
    if (!PyArg_ParseTuple(args, "|ii", &halt_steppers, &clear_queue))
        return NULL;

    if (gpx.flag.verboseMode) fprintf(gpx.log, "py_stop\n");
    if (!tio->waitflag.waitForCancelSync) {
        if (gpx.flag.verboseMode) fprintf(gpx.log, "py_stop now waiting for @clear_cancel\n");
        tio->flag.cancelPending = 1;
    }

    clear_state_for_cancel();

    int rval = SUCCESS;

    // first, ask if we are SD printing
    // delay 1ms is a queuable command that will fail if SD printing
    int sdprinting = 0;
    rval = delay(&gpx, 1);
    if (rval == 0x8A) // SD printing
        sdprinting = 1;
    // ignore any other response

    if (sdprinting && !gpx.flag.sd_paused) {
        rval = pause_resume(&gpx);
        if (rval != SUCCESS)
            return py_return_translation(rval);
        gpx.flag.sd_paused = 1;
    }

    rval = extended_stop(&gpx, halt_steppers, clear_queue);

    if (rval != 0x89)
        tio->waitflag.waitForCancelSync = 0;

    if (rval != SUCCESS)
        return py_return_translation(rval);
    gpx.flag.sd_paused = 0;
    return set_build_aborted_state(&gpx);
}

// def abort()
static PyObject *py_abort(PyObject *self, PyObject *args)
{
    if (!connected)
        return PyErr_NotConnected();

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    if (gpx.flag.verboseMode) fprintf(gpx.log, "py_abort\n");
    if (!tio->waitflag.waitForCancelSync) {
        if (gpx.flag.verboseMode) fprintf(gpx.log, "py_abort now waiting for @clear_cancel\n");
        tio->flag.cancelPending = 1;
    }

    clear_state_for_cancel();

    int rval = abort_immediately(&gpx);

    // ESIOTIMEOUT is only returned if the write succeeded, but no bytes returned
    // I think this can happen if the bot resets immediately and doesn't respond
    // via the serial port before hand
    // let's eat it
    if (rval == ESIOTIMEOUT)
        rval = SUCCESS;

    if (rval != 0x89)
        tio->waitflag.waitForCancelSync = 0;

    if (rval != SUCCESS) {
        if (gpx.flag.verboseMode) fprintf(gpx.log, "abort_immediately rval = %d\n", rval);
        return py_return_translation(rval);
    }
    gpx.flag.sd_paused = 0;
    return set_build_aborted_state(&gpx);
}

// def read_eeprom(id)
static PyObject *py_read_eeprom(PyObject *self, PyObject *args)
{
    if (!connected)
        return PyErr_NotConnected();

    tio->cur = 0;
    tio->translation[0] = 0;

    if (gpx.eepromMap == NULL && load_eeprom_map(&gpx) != SUCCESS) {
        PyErr_SetString(pyerrUnknownFirmware, "No EEPROM map found for firmware type and/or version");
        return NULL;
    }

    char *id;
    if (!PyArg_ParseTuple(args, "s", &id))
        return NULL;

    if (gpx.flag.verboseMode) fprintf(gpx.log, "py_read_eeprom %s\n", id);
    EepromMapping *pem = find_any_eeprom_mapping(&gpx, id);
    if (pem == NULL) {
        PyErr_SetString(PyExc_ValueError, "EEPROM id mapping not found");
        return NULL;
    }

    unsigned char b;
    unsigned short us;
    unsigned long ul;
    float n;
    switch (pem->et) {
        case et_boolean:
            if (read_eeprom_8(&gpx, gpx.sio, pem->address, &b) == SUCCESS)
                return Py_BuildValue("O", b ? Py_True : Py_False);
            break;

        case et_bitfield:
        case et_byte:
            if (read_eeprom_8(&gpx, gpx.sio, pem->address, &b) == SUCCESS)
                return Py_BuildValue("B", b);
            break;

        case et_ushort:
            if (read_eeprom_16(&gpx, gpx.sio, pem->address, &us) == SUCCESS)
                return Py_BuildValue("H", us);
            break;

        case et_fixed:
            if (read_eeprom_fixed_16(&gpx, gpx.sio, pem->address, &n) == SUCCESS)
                return Py_BuildValue("f", n);
            break;

        case et_long:
        case et_ulong:
            if (read_eeprom_32(&gpx, gpx.sio, pem->address, &ul) == SUCCESS)
                return Py_BuildValue(pem->et == et_long ? "l" : "k", ul);
            break;

        case et_float:
            if (read_eeprom_float(&gpx, gpx.sio, pem->address, &n) == SUCCESS)
                return Py_BuildValue("f", n);
            break;

        case et_string:
            memset(gpx.sio->response.eeprom.buffer, 0, sizeof(gpx.sio->response.eeprom.buffer));
            int len = pem->len;
            if (len > sizeof(gpx.sio->response.eeprom.buffer))
                len = sizeof(gpx.sio->response.eeprom.buffer);
            if (read_eeprom(&gpx, pem->address, len) == SUCCESS)
                return Py_BuildValue("s", gpx.sio->response.eeprom.buffer);
            break;

        default:
            PyErr_SetString(PyExc_ValueError, "EEPROM type not supported");
            return NULL;
    }

    return Py_BuildValue("i", 0);
}

// def write_eeprom(id, value)
static PyObject *py_write_eeprom(PyObject *self, PyObject *args)
{
    if (!connected)
        return PyErr_NotConnected();

    tio->cur = 0;
    tio->translation[0] = 0;

    char *id;
    PyObject *value;

    if (!PyArg_ParseTuple(args, "sO", &id, &value))
        return NULL;
    PyObject_Print(value, gpx.log, 0);
    fprintf(gpx.log, " <- \n");

    if (gpx.flag.verboseMode) fprintf(gpx.log, "py_write_eeprom\n");
    if (gpx.eepromMap == NULL && load_eeprom_map(&gpx) != SUCCESS) {
        PyErr_SetString(pyerrUnknownFirmware, "No EEPROM map found for firmware type and/or version");
        return NULL;
    }

    EepromMapping *pem = find_any_eeprom_mapping(&gpx, id);
    if (pem == NULL) {
        PyErr_SetString(PyExc_ValueError, "EEPROM id mapping not found");
        return NULL;
    }

    int rval = SUCCESS;
    int len = 0;
    unsigned char b = 0;
    unsigned short us = 0;
    unsigned long ul = 0;
    float n = 0.0;
    char *s = NULL;
    int f = 0;
    switch (pem->et) {
        case et_boolean:
            if (!PyArg_Parse(value, "B", &b))
                return NULL;
            gcodeResult(&gpx, "write_eeprom_8(%u) to address %u", (unsigned)!!b, pem->address);
            rval = write_eeprom_8(&gpx, gpx.sio, pem->address, !!b);
            break;

        case et_bitfield:
        case et_byte:
            value = PyNumber_Int(value);
            if (value == NULL)
                return NULL;
            f = PyArg_Parse(value, "B", &b);
            Py_DECREF(value);
            if (!f)
                return NULL;
            gcodeResult(&gpx, "write_eeprom_8(%u) to address %u", (unsigned)b, pem->address);
            rval = write_eeprom_8(&gpx, gpx.sio, pem->address, b);
            break;

        case et_ushort:
            value = PyNumber_Int(value);
            if (value == NULL)
                return NULL;
            f = PyArg_Parse(value, "H", &us);
            Py_DECREF(value);
            if (!f)
                return NULL;
            gcodeResult(&gpx, "write_eeprom_16(%u) to address %u", us, pem->address);
            rval = write_eeprom_16(&gpx, gpx.sio, pem->address, us);
            break;

        case et_fixed:
            value = PyNumber_Float(value);
            if (value == NULL)
                return NULL;
            f = PyArg_Parse(value, "f", &n);
            Py_DECREF(value);
            if (!f)
                return NULL;
            rval = write_eeprom_fixed_16(&gpx, gpx.sio, pem->address, n);
            gcodeResult(&gpx, "write_eeprom_fixed_16(%f) to address %u", n, pem->address);
            break;

        case et_long:
            value = PyNumber_Long(value);
            if (value == NULL)
                return NULL;
            f = PyArg_Parse(value, "l", &ul);
            Py_DECREF(value);
            if (!f)
                return NULL;
            rval = write_eeprom_32(&gpx, gpx.sio, pem->address, ul);
            gcodeResult(&gpx, "write_eeprom_32(%lu) to address %u", ul, pem->address);
            break;

        case et_ulong:
            value = PyNumber_Long(value);
            if (value == NULL)
                return NULL;
            f = PyArg_Parse(value, "L", &ul);
            Py_DECREF(value);
            if (!f)
                return NULL;
            rval = write_eeprom_32(&gpx, gpx.sio, pem->address, ul);
            gcodeResult(&gpx, "write_eeprom_32(%lu) to address %u", ul, pem->address);
            break;

        case et_float:
            value = PyNumber_Float(value);
            if (value == NULL)
                return NULL;
            f = PyArg_Parse(value, "f", &n);
            Py_DECREF(value);
            if (!f)
                return NULL;
            rval = write_eeprom_float(&gpx, gpx.sio, pem->address, n);
            gcodeResult(&gpx, "write_eeprom_float(%f) to address %u", n, pem->address);
            break;

        case et_string:
            if (!PyArg_Parse(value, "s", &s))
                return NULL;
            len = strlen(s);
            if (len >= pem->len) {
                PyErr_SetString(PyExc_ValueError, "String value too long for indicated EEPROM entry");
                return NULL;
            }
            rval = write_eeprom(&gpx, pem->address, s, len + 1);
            gcodeResult(&gpx, "write_eeprom(%s) to address %u", s, pem->address);
            break;

        default:
            PyErr_SetString(PyExc_ValueError, "EEPROM type not supported");
            return NULL;
    }

    return py_return_translation(rval);
}


// method table describes what is exposed to python
static PyMethodDef GpxMethods[] = {
    {"connect", py_connect, METH_VARARGS, "connect(port, baud = 0, inifilepath = None, logfilepath = None) Open the serial port to the printer and initialize the channel"},
    {"disconnect", py_disconnect, METH_VARARGS, "disconnect() Close the serial port and clean up."},
    {"write", py_write, METH_VARARGS, "write(string) Translate g-code into x3g and send."},
    {"readnext", py_readnext, METH_VARARGS, "readnext() read next response if any"},
    {"set_baudrate", py_set_baudrate, METH_VARARGS, "set_baudrate(long) Set the current baudrate for the connection to the printer."},
    {"get_machine_defaults", py_get_machine_defaults, METH_VARARGS, "get_machine_defaults(string) Return a dict with the default settings for the indicated machine type."},
    {"read_ini", py_read_ini, METH_VARARGS, "read_ini(string) Parse indicated ini file for gpx settings and macros and update current converter state. Loading ini files is additive. They just build on the ini's that have been read before. Use reset_ini to start from a clean state again."},
    {"reset_ini", py_reset_ini, METH_VARARGS, "reset_ini() Reset configuration state to default"},
    {"waiting", py_waiting, METH_VARARGS, "waiting() Returns True if the bot reports it is waiting for a temperature, pause or prompt"},
    {"reprap_flavor", py_reprap_flavor, METH_VARARGS, "reprap_flavor(boolean) Sets the expected gcode flavor (true = reprap, false = makerbot), returns the previous setting"},
    {"start", py_start, METH_VARARGS, "start() Call after connect and a printer specific pause (2 seconds for most) to start the serial communication"},
    {"stop", py_stop, METH_VARARGS, "stop(halt_steppers, clear_queue) Tells the bot to either stop the steppers, clear the queue or both"},
    {"abort", py_abort, METH_VARARGS, "abort() Tells the bot to clear the queue and stop all motors and heaters"},
    {"read_eeprom", py_read_eeprom, METH_VARARGS, "read_eeprom(id) Read the value identified by id from the eeprom"},
    {"write_eeprom", py_write_eeprom, METH_VARARGS, "write_eeprom(id, value) Write 'value' to the eeprom location identified by 'id'"},
    {"build_started", py_build_started, METH_VARARGS, "build_started() Returns True if a build has been started, but not yet ended"},
    {"build_paused", py_build_paused, METH_VARARGS, "build_paused() Returns true if build is paused"},
    {"listing_files", py_listing_files, METH_VARARGS, "listing_files() Returns true if there are still filenames to be returned of an SD card enumeration"},
    {NULL, NULL, 0, NULL} // sentinel
};

__attribute__ ((visibility ("default"))) PyMODINIT_FUNC initgpx(void);

// python calls init<modulename> when the module is loaded
PyMODINIT_FUNC initgpx(void)
{
    PyObject *m = Py_InitModule("gpx", GpxMethods);
    if (m == NULL)
        return;

    pyerrCancelBuild = PyErr_NewException("gpx.CancelBuild", NULL, NULL);
    Py_INCREF(pyerrCancelBuild);
    PyModule_AddObject(m, "CancelBuild", pyerrCancelBuild);

    pyerrBufferOverflow = PyErr_NewException("gpx.BufferOverflow", NULL, NULL);
    Py_INCREF(pyerrBufferOverflow);
    PyModule_AddObject(m, "BufferOverflow", pyerrBufferOverflow);

    pyerrTimeout = PyErr_NewException("gpx.Timeout", NULL, NULL);
    Py_INCREF(pyerrTimeout);
    PyModule_AddObject(m, "Timeout", pyerrTimeout);

    pyerrUnknownFirmware = PyErr_NewException("gpx.UnknownFirmware", NULL, NULL);
    Py_INCREF(pyerrUnknownFirmware);
    PyModule_AddObject(m, "UnknownFirmware", pyerrUnknownFirmware);

    gpx_initialize(&gpx, 1);
    tio = tio_initialize(&gpx);
}
