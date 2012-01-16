#include "hooks_py.h"
#include "simulator.h"
#include "mcp.h"
#include "stats.h"


//////////
// get(): retrieve a stats value
//////////

static PyObject *
getStatsValue(PyObject *self, PyObject *args)
{
   const char *objectName = NULL, *metricName = NULL;
   long int index = -1;

   if (!PyArg_ParseTuple(args, "sls", &objectName, &index, &metricName))
      return NULL;

   StatsMetricBase *metric = Sim()->getStatsManager()->getMetricObject(objectName, index, metricName);

   if (!metric) {
      PyErr_SetString(PyExc_ValueError, "Stats metric not found");
      return NULL;
   }

   // FIXME: For now, return everything as string. We may want to do some templating tricks
   // to have StatsMetric<T> define a toPython() function that returns a suitable Python data type
   return PyString_FromString(metric->recordMetric().c_str());
}


//////////
// getter(): return a statsGetterObject Python object which, when called, returns a stats value
//////////

typedef struct {
   PyObject_HEAD
   StatsMetricBase *metric;
} statsGetterObject;

static PyObject *
statsGetterGet(PyObject *self, PyObject *args, PyObject *kw)
{
   statsGetterObject *getter = (statsGetterObject *)self;
   StatsMetricBase *metric = getter->metric;

   // FIXME: For now, return everything as string. We may want to do some templating tricks
   // to have StatsMetric<T> define a toPython() function that returns a suitable Python data type
   return PyString_FromString(metric->recordMetric().c_str());
}

static PyTypeObject statsGetterType = {
   PyObject_HEAD_INIT(NULL)
   0,                         /*ob_size*/
   "statsGetter",             /*tp_name*/
   sizeof(statsGetterObject), /*tp_basicsize*/
   0,                         /*tp_itemsize*/
   0,                         /*tp_dealloc*/
   0,                         /*tp_print*/
   0,                         /*tp_getattr*/
   0,                         /*tp_setattr*/
   0,                         /*tp_compare*/
   0,                         /*tp_repr*/
   0,                         /*tp_as_number*/
   0,                         /*tp_as_sequence*/
   0,                         /*tp_as_mapping*/
   0,                         /*tp_hash */
   statsGetterGet,            /*tp_call*/
   0,                         /*tp_str*/
   0,                         /*tp_getattro*/
   0,                         /*tp_setattro*/
   0,                         /*tp_as_buffer*/
   Py_TPFLAGS_DEFAULT,        /*tp_flags*/
   "Stats getter objects",    /* tp_doc */
};

static PyObject *
getStatsGetter(PyObject *self, PyObject *args)
{
   const char *objectName = NULL, *metricName = NULL;
   long int index = -1;

   if (!PyArg_ParseTuple(args, "sls", &objectName, &index, &metricName))
      return NULL;

   StatsMetricBase *metric = Sim()->getStatsManager()->getMetricObject(objectName, index, metricName);

   if (!metric) {
      PyErr_SetString(PyExc_ValueError, "Stats metric not found");
      return NULL;
   }

   statsGetterObject *pGetter = PyObject_New(statsGetterObject, &statsGetterType);
   pGetter->metric = metric;

   return (PyObject *)pGetter;
}


//////////
// write(): write the current set of statistics out to sim.stats or our own file
//////////

static PyObject *
writeStats(PyObject *self, PyObject *args)
{
   const char *prefix = NULL, *filename = "";

   if (!PyArg_ParseTuple(args, "s|s", &prefix, &filename))
      return NULL;

   Sim()->getStatsManager()->recordStats(prefix, filename);

   Py_RETURN_NONE;
}


//////////
// register(): register a callback function that returns a statistics value
//////////

static String statsCallback(String objectName, UInt32 index, String metricName, PyObject *pFunc)
{
   PyObject *pResult = HooksPy::callPythonFunction(pFunc, Py_BuildValue("(sls)", objectName.c_str(), index, metricName.c_str()));

   if (!PyString_Check(pResult)) {
      PyObject *pAsString = PyObject_Str(pResult);
      if (!pAsString) {
         fprintf(stderr, "Stats callback: return value must be (convertable into) string\n");
         Py_XDECREF(pResult);
         return "";
      }
      Py_XDECREF(pResult);
      pResult = pAsString;
   }

   String val(PyString_AsString(pResult));
   Py_XDECREF(pResult);

   return val;
}

static PyObject *
registerStats(PyObject *self, PyObject *args)
{
   const char *objectName = NULL, *metricName = NULL;
   long int index = -1;
   PyObject *pFunc = NULL;

   if (!PyArg_ParseTuple(args, "slsO", &objectName, &index, &metricName, &pFunc))
      return NULL;

   if (!PyCallable_Check(pFunc)) {
      PyErr_SetString(PyExc_TypeError, "Fourth argument must be callable");
      return NULL;
   }
   Py_INCREF(pFunc);

   Sim()->getStatsManager()->registerMetric(new StatsMetricCallback(objectName, index, metricName, (StatsCallback)statsCallback, (void*)pFunc));

   Py_RETURN_NONE;
}

static PyObject *
getTime(PyObject *self, PyObject *args)
{
   SubsecondTime time = Sim()->getMCP()->getClockSkewMinimizationServer()->getGlobalTime();
   return PyLong_FromUnsignedLongLong(time.getFS());
}


//////////
// module definition
//////////

static PyMethodDef PyStatsMethods[] = {
   {"get",  getStatsValue, METH_VARARGS, "Retrieve current value of statistic (objectName, index, metricName)."},
   {"getter", getStatsGetter, METH_VARARGS, "Return object to retrieve statistics value."},
   {"write", writeStats, METH_VARARGS, "Write statistics (<prefix>, [<filename>])."},
   {"register", registerStats, METH_VARARGS, "Register callback that defines statistics value for (objectName, index, metricName)."},
   {"time", getTime, METH_VARARGS, "Retrieve the current global time in femtoseconds (approximate, last barrier)."},
   {NULL, NULL, 0, NULL} /* Sentinel */
};

void HooksPy::PyStats::setup(void)
{
   PyObject *pModule = Py_InitModule("sim_stats", PyStatsMethods);

   statsGetterType.tp_new = PyType_GenericNew;
   if (PyType_Ready(&statsGetterType) < 0)
      return;

   Py_INCREF(&statsGetterType);
   PyModule_AddObject(pModule, "Getter", (PyObject *)&statsGetterType);
}
