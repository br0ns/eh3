#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <Python.h>

#include "trace.h"
#include "debug.h"
#include "mem.h"
#include "consts.h"

extern word regs[];
extern bitaddr_t pc;

PyObject *pModule;

static inline
 void print_error() {
  if (PyErr_Occurred()) {
    PyErr_Print();
  }
}

static
PyObject *get_func(char *name) {
  PyObject *pFunc = PyObject_GetAttrString(pModule, name);
  if (pFunc && !PyCallable_Check(pFunc)) {
    fatal("Not a function: %s\n", name);
  }
  return pFunc;
}

static
PyObject *call_func(PyObject *pFunc) {
  PyObject *pArgs;
  PyObject *pRet;
  PyObject *pName;

  pArgs = PyTuple_New(0);
  pRet = PyObject_CallObject(pFunc, pArgs);
  Py_DECREF(pArgs);
  if (!pRet) {
    pName = PyObject_GetAttrString(pFunc, "__name__");
    if (pName) {
      error(T_NONE, "Call to '%s' failed\n", PyString_AsString(pName));
    } else {
      error(T_NONE, "Call failed\n");
    }
    print_error();
    _exit(EXIT_FAILURE);
  }

  return pRet;
}

static
PyObject *import_module(char *name) {
  PyObject *pModule = PyImport_ImportModule(name);
  if (!pModule) {
    error(T_NONE, "Could not import module '%s'.\n", name);
    print_error();
    _exit(EXIT_FAILURE);
  }
  return pModule;
}

#define str_or_none(s) ((s) ? PyString_FromString((s)) : Py_None)

static inline
void dict_set_item(PyObject *d, char *key, PyObject *val) {
  /* Reference is *not* stolen. */
  PyDict_SetItemString(d, key, val);
  Py_DECREF(val);
}

static inline
PyObject *dict_get_item(PyObject *d, char *key) {
  PyObject *val = PyDict_GetItemString(d, key);
  if (!val) {
    fatal("Name '%s' is not defined.\n", key);
  }
  return val;
}

static inline
unsigned long long ptoll(PyObject *p) {
  if (PyInt_Check(p)) {
    return PyInt_AsUnsignedLongLongMask(p);
  } else if (PyLong_Check(p)) {
    return PyLong_AsUnsignedLongLong(p);
  } else {
    fatal("Not an integer\n");
  }
}

static inline
unsigned long  ptol(PyObject *p) {
  if (PyInt_Check(p)) {
    return PyInt_AsUnsignedLongMask(p);
  } else if (PyLong_Check(p)) {
    return PyLong_AsUnsignedLong(p);
  } else {
    fatal("Not an integer\n");
  }
}

static
void set_ctx() {
  PyObject *pPc;
  PyObject *pMaps, *pMap, *pData;
  PyObject *pRegs;
  mmap_t *m;
  int i;

  /* Add program counter. */
  pPc = PyLong_FromUnsignedLongLong(pc);
  PyModule_AddObject(pModule, "pc", pPc);

  /* Add registers. */
  pRegs = PyList_New(REG_N);
  for (i = 0; i < REG_N; i++) {
    /* Reference stolen by list. */
    PyList_SET_ITEM(pRegs, i, PyLong_FromUnsignedLong(regs[i]));
  }
  PyModule_AddObject(pModule, "regs", pRegs);

  /* Add memory. */
  for (i = 0, m = mem; m; i++, m = m->next);
  pMaps = PyTuple_New(i);
  for (i = 0, m = mem; m; i++, m = m->next) {
    pMap = (PyObject *)PyDict_New();

    dict_set_item(pMap, "addr", PyLong_FromUnsignedLong(m->addr));
    dict_set_item(pMap, "prot", PyInt_FromLong(m->prot));

    pData = (PyObject *)PyObject_New(PyByteArrayObject, &PyByteArray_Type);
    Py_SIZE(pData) = mem->size;
    ((PyByteArrayObject*)pData)->ob_bytes = (char *)mem->data;
    ((PyByteArrayObject*)pData)->ob_alloc = mem->size;
    ((PyByteArrayObject*)pData)->ob_exports = 1;
    dict_set_item(pMap, "data", pData);

    PyTuple_SET_ITEM(pMaps, i, pMap);
  }

  PyModule_AddObject(pModule, "maps", pMaps);
}

static
void get_ctx() {
  PyObject *pDict;
  PyObject *pPc;
  PyObject *pMaps, *pMap, *pData;
  PyObject *pRegs, *pReg;
  int i;

  pDict = PyModule_GetDict(pModule);

  pPc = dict_get_item(pDict, "pc");
  pc = ptoll(pPc);

  pRegs = dict_get_item(pDict, "regs");
  for (i = 0; i < REG_N; i++) {
    pReg = PyList_GetItem(pRegs, i);
    regs[i] = ptol(pReg);
  }

  /* Make it possible to GC mem data. */
  pMaps = dict_get_item(pDict, "maps");
  for (i = 0; i < PyTuple_GET_SIZE(pMaps); i++) {
    pMap = PyTuple_GetItem(pMaps, i);
    if (!pMap) {
      print_error();
      _exit(EXIT_FAILURE);
    }
    pData = PyDict_GetItemString(pMap, "data");
    if (!pData) {
      print_error();
      _exit(EXIT_FAILURE);
    }
    ((PyByteArrayObject*)pData)->ob_exports = 0;
    ((PyByteArrayObject*)pData)->ob_bytes = NULL;
  }
}

void debug_init(char *exe_path, char *cmds_path) {
  PyObject *pInitFunc;
  PyObject *pSysPath, *pPath;
  PyObject *pRet;
  char py_path[PATH_MAX];

  Py_SetProgramName("debug.py");  /* optional but recommended */
  Py_Initialize();
  PySys_SetArgvEx(2, (char*[]){"emu", "debug.py"}, 0);

  /* Append Python src dir to `sys.path`. */
  if (-1 == readlink("/proc/self/exe", py_path, sizeof(py_path))) {
    perror("readlink");
    _exit(EXIT_FAILURE);
  }
  strcpy(strrchr(py_path, '/') + 1, "../../py/");
  pPath = PyString_FromString(py_path);

  pModule = import_module("sys");
  pSysPath = PyObject_GetAttrString(pModule, "path");
  PyList_Append(pSysPath, pPath);
  Py_DECREF(pPath);
  Py_DECREF(pSysPath);
  Py_DECREF(pModule);

  /* Load debugger and get callbacks. */
  pModule = import_module("debug");

  /* Set up program context. */
  PyModule_AddObject(pModule, "exe", str_or_none(exe_path));
  PyModule_AddObject(pModule, "cmds", str_or_none(cmds_path));
  set_ctx();

  pInitFunc = get_func("debug_init");

  if (pInitFunc) {
    pRet = call_func(pInitFunc);
    Py_DECREF(pRet);
    Py_DECREF(pInitFunc);

    get_ctx();
  }

}

void debug_fini() {
  PyObject *pFiniFunc;
  PyObject *pRet;

  set_ctx();

  pFiniFunc = get_func("debug_fini");
  if (pFiniFunc) {
    pRet = call_func(pFiniFunc);
    Py_DECREF(pRet);
    Py_DECREF(pFiniFunc);
  }

  get_ctx();
  Py_DECREF(pModule);

  Py_Finalize();
}

bool debug_hook() {
  PyObject *pHookFunc;
  PyObject *pRet;
  bool cont = false;

  set_ctx();

  /* Call hook. */
  pHookFunc = get_func("debug_hook");
  if (!pHookFunc) {
    fatal("Missing function: debug_hook\n");
  }
  pRet = call_func(pHookFunc);
  if (PyBool_Check(pRet)) {
    cont = Py_True == pRet;
  } else if (Py_None != pRet) {
    fatal("debug_hook must return a boolean or `None`.\n");
  }
  Py_DECREF(pRet);
  Py_DECREF(pHookFunc);

  get_ctx();

  return cont;
}
