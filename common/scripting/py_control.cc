#include "hooks_py.h"
#include "simulator.h"
#include "magic_server.h"
#include "sim_api.h"

static PyObject *
setROI(PyObject *self, PyObject *args)
{
   bool inRoi = false;

   if (!PyArg_ParseTuple(args, "b", &inRoi))
      return NULL;

   Sim()->getMagicServer()->Magic_unlocked(INVALID_CORE_ID, INVALID_THREAD_ID, inRoi ? SIM_CMD_ROI_START : SIM_CMD_ROI_END, 0, 0);

    Py_RETURN_NONE;
}

static PyObject *
setInstrumentationMode(PyObject *self, PyObject *args)
{
   long int mode = 999;

   if (!PyArg_ParseTuple(args, "l", &mode))
      return NULL;

   switch (mode)
   {
      case SIM_OPT_INSTRUMENT_DETAILED:
      case SIM_OPT_INSTRUMENT_WARMUP:
      case SIM_OPT_INSTRUMENT_FASTFORWARD:
         Sim()->getMagicServer()->Magic_unlocked(INVALID_CORE_ID, INVALID_THREAD_ID, SIM_CMD_INSTRUMENT_MODE, mode, 0);
         break;
      default:
         LOG_PRINT_ERROR("Unexpected instrumentation mode from python: %lx.", mode);
         return NULL;
   }

   Py_RETURN_NONE;
}

static PyMethodDef PyControlMethods[] = {
   { "set_roi", setROI, METH_VARARGS, "Set whether or not we are in the ROI" },
   { "set_instrumentation_mode", setInstrumentationMode, METH_VARARGS, "Set instrumentation mode" },
   { NULL, NULL, 0, NULL } /* Sentinel */
};

void HooksPy::PyControl::setup(void)
{
   PyObject *pModule = Py_InitModule("sim_control", PyControlMethods);

   {
      PyObject *pGlobalConst = PyInt_FromLong(SIM_OPT_INSTRUMENT_DETAILED);
      PyObject_SetAttrString(pModule, "DETAILED", pGlobalConst);
      Py_DECREF(pGlobalConst);
   }
   {
      PyObject *pGlobalConst = PyInt_FromLong(SIM_OPT_INSTRUMENT_WARMUP);
      PyObject_SetAttrString(pModule, "WARMUP", pGlobalConst);
      Py_DECREF(pGlobalConst);
   }
   {
      PyObject *pGlobalConst = PyInt_FromLong(SIM_OPT_INSTRUMENT_FASTFORWARD);
      PyObject_SetAttrString(pModule, "FASTFORWARD", pGlobalConst);
      Py_DECREF(pGlobalConst);
   }
}
