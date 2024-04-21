/*
 * This is based on the tutorial at
 *
 *     https://docs.python.org/3/extending/newtypes_tutorial.html
 *
 */
#include <Python.h>
#include <stdio.h>
#include <stddef.h> // for offsetof()
#include <structmember.h> // for PyMemberDef (COMPAT: Not required in future versions)
/*
 * PYTHON FUNCTIONS IN C
 *
 * Module level functions in C have the signature
 *
 *      PyObject *my_function(PyObject *self, PyObject *args);
 *
 * where `self` is the module object and args is a tuple of the arguments passed
 * to the function.  These arguments can be obtained with PyArg_ParseTuple()
 * or * PyArg_ParseTupleAndKeywords()
 * https://docs.python.org/3/c-api/arg.html#parsing-arguments-and-building-values
 *
 * The function must always return a PyObject *.This object can be build from
 * C values using Py_BuildValue() https://docs.python.org/3/c-api/arg.html#building-values
 *
 * Returning NULL triggers exception handling in the enterpreter.
 *
 * Functions that don't return anything must return the python None object. The
 * macro `Py_RETURN_NONE` does this for us.
 *
 * OBJECT METHODS IN C
 *
 * These functions have the same signature as module methods except that the
 * `self` argument is the object instance rather than the module.  The macro
 * `Py_UNUSED()` may be used to prevent compiler warnings about unused arguments.
 *
 * EXCEPTIONS
 *
 * To raise an exception from C, we need to do two things: set a current
 * exception and tell the interpreter that something went wrong.  When informed
 * of this, the interpreter will look at the current exception that we set and
 * do its usual thing.
 *
 * Setting the current exception is done with `PyErr_SetString(PyObject *type,
 * const char *message)` or other functions
 * (https://docs.python.org/3/c-api/exceptions.html#raising-exceptions).
 *
 * Triggering the exception process is done by returning NULL.
 *
 * In this example we do both
 *
 *      if(result == NULL){ // if something goes wrong
 *          PyErr_SetString(PyExc_RuntimeError, "Something went wrong");
 *          return NULL;
 *      }
 *
 * Something to be mindfull of is that Python C API functions may already set
 * the current exception.  For example `PyArg_ParseTuple` documentation says
 *
 *      Parse the parameters of a function that takes only positional
 *      parameters into local variables. Returns true on success; on failure,
 *      it returns false and raises the appropriate exception.
 *
 * This means that if it fails it will have already set the current exception
 *
 *      if(!PyArg_ParseTuple( ... )){ // if function failed
 *          return NULL;
 *      }
 *
 * Note that in some special cases like the `.tp_init` method, failure is
 * is indicated by returning `-1`.
 *
 * REFERENCE COUNTING:
 *
 * The CPython interpreter uses reference counting to manage memory.  This means
 * that when we write C code that is equivalent to doing Python assignments,
 * for example
 *
 *      self->first_name = first_name; // in Person_init()
 *
 * we must think of references.  If `self->first_name` pointed to a Python
 * Python string (i.e. was not NULL), then after the assignemnt, that object
 * has one less reference pointing to it.  Also, after the assignment, the
 * object that `first_name` points to has one more reference pointing to it.
 *
 * This is why we do
 *
 *      PyObject *tmp = self->first_name;
 *      self->first_name = first_name;
 *      Py_XINCREF(self->first_name);
 *      Py_XDECREF(tmp);
 *
 * >>> import mymodule
 * >>> p = mymodule.Person(first_name="Isaac", last_name="Newton", number=42)
 * tells us that 90% of our code works.
 * >>> print(p)
 * tells us that our str method works.
 * >>> print(p.name())
 * tells us that our custom method works.
 */

struct Person {
    PyObject_HEAD
    PyObject *first_name;
    PyObject *last_name;
    int number;
};

static void Person_dealloc(struct Person *self)
{
    Py_XDECREF(self->first_name);
    Py_XDECREF(self->last_name);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Person_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    struct Person *self;
    self = (struct Person *) type->tp_alloc(type, 0);
    if(self == NULL){
        return NULL;
    }

    self->first_name = PyUnicode_FromString("John");
    if(self->first_name == NULL){
        Py_DECREF(self);
        return NULL;
    }

    self->last_name = PyUnicode_FromString("Doe");
    if(self->last_name == NULL){
        Py_DECREF(self);
        return NULL;
    }

    self->number = 42;

    return (PyObject *)self;
}

static int Person_init(struct Person *self, PyObject *args, PyObject *kwds)
{
    // Initialization to NULL is important here because that is how
    // we know if PyArg_ParseTupleAndKeywords assigned something to them.
    PyObject *first_name = NULL;
    PyObject *last_name = NULL;
    static char *kwlist[] = {"first_name", "last_name", "number", NULL};

    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|OOi", kwlist, &first_name, &last_name, &self->number)){
        return -1;
    }

    if(first_name != NULL){
#if PY_MINOR_VERSION < 11
        // COMPAT: Replace this whole block with
        // Py_XSETREF(self->first_name, Py_NewRef(first_name));
        // which is a macro that basically does what these lines do
        PyObject *tmp = self->first_name;
        Py_INCREF(first_name);
        self->first_name = first_name;
        Py_XDECREF(tmp);
#else
        Py_XSETREF(self->first_name, Py_NewRef(first_name));
#endif
    }

    if(last_name != NULL){
        PyObject *tmp = self->last_name;
        Py_INCREF(last_name);
        self->last_name = last_name;
        Py_XDECREF(tmp);
    }

    return 0;
}

static PyObject *Person_str(struct Person *self, PyObject *Py_UNUSED(ignored))
{
    return PyUnicode_FromFormat("Person(first_name=%S, last_name=%S, number=%d)", self->first_name, self->last_name, self->number);
}

static PyMemberDef Person_members[] = {
    {
        .name = "first_name",
        .type = T_OBJECT_EX,
        .offset = offsetof(struct Person, first_name),
        .flags = 0,
        .doc = "First name of the person"
    },
    {
        .name = "last_name",
        .type = T_OBJECT_EX,
        .offset = offsetof(struct Person, last_name),
        .flags = 0,
        .doc = "Last name of the person"
    },
    {
        .name = "number",
        .type = T_INT,
        .offset = offsetof(struct Person, number),
        .flags = 0,
        .doc = "First name of the person"
    },
	{NULL}
};


static PyObject *Person_name(struct Person *self, PyObject *Py_UNUSED(args))
{
    if(self->first_name == NULL){
        PyErr_SetString(PyExc_AttributeError, "first_name");
        return NULL;
    }


    if(self->last_name == NULL){
        PyErr_SetString(PyExc_AttributeError, "last_name");
        return NULL;
    }

    return PyUnicode_FromFormat("%S %S", self->first_name, self->last_name);
}

static PyMethodDef Person_methods[] = {
    {
        .ml_name = "name",
        .ml_meth = (PyCFunction)Person_name,
        .ml_flags = METH_NOARGS,
        .ml_doc = "Return the name of a person combining first and last names",
    },
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyTypeObject PersonType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "mymodule.Person",
    .tp_doc = "Person object",
    .tp_basicsize = sizeof(struct Person),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Person_new,
    .tp_init = (initproc) Person_init,
    .tp_dealloc = (destructor) Person_dealloc,
    .tp_members = Person_members,
    .tp_str = (reprfunc) Person_str,
    .tp_methods = Person_methods,
};

static PyModuleDef mymodulemodule = {
    PyModuleDef_HEAD_INIT,
    .m_name = "mymodule",
    .m_doc = "Example module that creates a Person type",
    .m_size = -1,
};

enum PythonVersionDifference { DIFFERENT_MAJOR, DIFFERENT_MINOR, SAME };
static enum PythonVersionDifference version_difference(){
    PyRun_SimpleStringFlags("import sys", NULL);
    PyObject * version = PySys_GetObject("version_info");
    PyObject * major_o = PyObject_GetAttrString(version, "major");
    PyObject * minor_o = PyObject_GetAttrString(version, "minor");
    long int major = PyLong_AsLong(major_o);
    long int minor = PyLong_AsLong(minor_o);
    if(major != PY_MAJOR_VERSION){
        printf("mymodule was built for version '%d' but is being run by python '%ld'\n", \
                PY_MAJOR_VERSION, major);
        return DIFFERENT_MAJOR;
    }
    if(minor != PY_MINOR_VERSION){
        printf("mymodule was built for version '%d.%d' but is being run by python '%ld.%ld'\n",
                PY_MAJOR_VERSION, PY_MINOR_VERSION, major, minor);
        return DIFFERENT_MINOR;
    }
    return SAME;
}

PyMODINIT_FUNC PyInit_mymodule(void)
{
    switch(version_difference()){
        case DIFFERENT_MAJOR: // the dlopen call in CPython will probably miss some symbols and fail
                              // and no code from this file will run.  In case by some miracle we get
                              // here, we should signal an error and abort immediately.
                              printf("mymodule: Different major versions: For sure that won't work!\n");
                              return NULL;
        case DIFFERENT_MINOR: // As far as I know, the API is stable enough that a minor version difference
                              // should be OK and in my initial testing, I have been building with 3.8 and
                              // running with 3.6 and never knew about it.  I am adding this check in case
                              // something mysterious happens due to minor version incompatibility.
                              printf("mymodule: different python versions not sure what the implication is\n");
                              break;
        case SAME:
                              break;
    }

    if(PyType_Ready(&PersonType) < 0){
        return NULL;
    }

    PyObject *m = PyModule_Create(&mymodulemodule);
    if(m == NULL){
        return NULL;
    }

    Py_INCREF(&PersonType);
    if(PyModule_AddObject(m, "Person", (PyObject *)&PersonType) < 0){
        Py_DECREF(&PersonType);
        Py_DECREF(m);
        return NULL;
    }

    printf("PY_VERSION_HEX = %x\n", PY_VERSION_HEX);

    return m;

}
