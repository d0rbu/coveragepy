/* Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0 */
/* For details: https://github.com/coveragepy/coveragepy/blob/main/NOTICE.txt */

#include "util.h"
#include "tracer.h"
#include "filedisp.h"

/* Module definition */

#define MODULE_DOC PyDoc_STR("Fast coverage tracer.")

/* Resolve existing paths without walking each component in Python.  None
 * asks the caller to preserve os.path.realpath's non-strict/error behavior. */
static PyObject *
realpath_existing(PyObject *self, PyObject *path)
{
#if defined(HAVE_REALPATH) && !defined(MS_WINDOWS)
    PyObject *encoded;
    PyObject *result;
    char *resolved;
    const char *input;

    if (!PyUnicode_CheckExact(path)) {
        Py_RETURN_NONE;
    }
    encoded = PyUnicode_EncodeFSDefault(path);
    if (encoded == NULL) {
        if (PyErr_ExceptionMatches(PyExc_UnicodeError)) {
            PyErr_Clear();
            Py_RETURN_NONE;
        }
        return NULL;
    }
    if (memchr(PyBytes_AS_STRING(encoded), '\0', PyBytes_GET_SIZE(encoded))) {
        Py_DECREF(encoded);
        Py_RETURN_NONE;
    }
    input = PyBytes_AS_STRING(encoded);
    Py_BEGIN_ALLOW_THREADS
    resolved = realpath(input, NULL);
    Py_END_ALLOW_THREADS
    Py_DECREF(encoded);
    if (resolved == NULL) {
        Py_RETURN_NONE;
    }
    result = PyUnicode_DecodeFSDefault(resolved);
    free(resolved);
    return result;
#else
    Py_RETURN_NONE;
#endif
}

static PyMethodDef tracer_methods[] = {
    {"_realpath_existing", realpath_existing, METH_O,
        PyDoc_STR("Resolve an existing path, or return None for Python fallback.")},
    {NULL, NULL, 0, NULL}
};

static BOOL module_inited = FALSE;

static int
tracer_exec(PyObject *mod)
{
    if (module_inited) {
        return 0;
    }

    if (CTracer_intern_strings() < 0) {
        return -1;
    }

    /* Initialize CTracer */
    CTracerType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&CTracerType) < 0) {
        return -1;
    }

    Py_INCREF(&CTracerType);
    if (PyModule_AddObject(mod, "CTracer", (PyObject *)&CTracerType) < 0) {
        Py_DECREF(&CTracerType);
        return -1;
    }

    /* Initialize CFileDisposition */
    CFileDispositionType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&CFileDispositionType) < 0) {
        Py_DECREF(&CTracerType);
        return -1;
    }

    Py_INCREF(&CFileDispositionType);
    if (PyModule_AddObject(mod, "CFileDisposition", (PyObject *)&CFileDispositionType) < 0) {
        Py_DECREF(&CTracerType);
        Py_DECREF(&CFileDispositionType);
        return -1;
    }

    module_inited = TRUE;
    return 0;
}

static PyModuleDef_Slot tracer_slots[] = {
    {Py_mod_exec, tracer_exec},
#if PY_VERSION_HEX >= 0x030c00f0  // Python 3.12+
    {Py_mod_multiple_interpreters, Py_MOD_MULTIPLE_INTERPRETERS_NOT_SUPPORTED},
#endif
#if PY_VERSION_HEX >= 0x030d00f0  // Python 3.13+
    // signal that this module supports running without an active GIL
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
#endif
    {0, NULL}
};

static PyModuleDef moduledef = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "coverage.tracer",
    .m_doc = MODULE_DOC,
    .m_size = 0,
    .m_methods = tracer_methods,
    .m_slots = tracer_slots,
};

PyMODINIT_FUNC
PyInit_tracer(void)
{
    return PyModuleDef_Init(&moduledef);
}
