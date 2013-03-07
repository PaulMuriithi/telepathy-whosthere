/*
 * Copyright (C) 2013 Matthias Gehre <gehre.matthias@gmail.com>
 *
 * This work is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "Python.h"
#include <QDebug>
#include "pythoninterface.h"

#include "YowsupInterface.py.h"

using namespace std;
using namespace boost::python;

const char* PYTHON_MODULE = "YowsupInterface";

YowsupInterface::YowsupInterface(QObject* parent) : QObject(parent) { //for vtable
}

/** to-python convert to QStrings */
struct QString_to_python_str
{
    static PyObject* convert(QString const& s)
      {
        return boost::python::incref(
          boost::python::object(
            s.toLatin1().constData()).ptr());
      }
};

struct QString_from_python_str
{
    QString_from_python_str()
    {
      boost::python::converter::registry::push_back(
        &convertible,
        &construct,
        boost::python::type_id<QString>());
    }

    // Determine if obj_ptr can be converted in a QString
    static void* convertible(PyObject* obj_ptr)
    {
        if (!PyString_Check(obj_ptr) && !PyUnicode_Check(obj_ptr)) return 0;
        return obj_ptr;
    }

    // Convert obj_ptr into a QString
    static void construct(
    PyObject* obj_ptr,
    boost::python::converter::rvalue_from_python_stage1_data* data)
    {
      if (PyString_Check(obj_ptr))
      {
          // Extract the character data from the python string
          const char* value = PyString_AsString(obj_ptr);

          // Verify that obj_ptr is a string (should be ensured by convertible())
          assert(value);

          // Grab pointer to memory into which to construct the new QString
          void* storage = (
            (boost::python::converter::rvalue_from_python_storage<QString>*)
            data)->storage.bytes;

          // in-place construct the new QString using the character data
          // extraced from the python object
          new (storage) QString(value);

          // Stash the memory chunk pointer for later use by boost.python
          data->convertible = storage;
      }
      else if(PyUnicode_Check(obj_ptr))
      {
          // Extract the character data from the python string
          const wchar_t* value = (const wchar_t*)PyUnicode_AS_UNICODE(obj_ptr);

          // Verify that obj_ptr is a string (should be ensured by convertible())
          assert(value);

          // Grab pointer to memory into which to construct the new QString
          void* storage = (
            (boost::python::converter::rvalue_from_python_storage<QString>*)
            data)->storage.bytes;

          QString str = QString::fromWCharArray(value);
          // in-place construct the new QString using the character data
          // extraced from the python object
          new (storage) QString(str);

          // Stash the memory chunk pointer for later use by boost.python
          data->convertible = storage;
      }
    }
};

/** to-python convert to QStrings */
struct QByteArray_to_python_str
{
    static PyObject* convert(QByteArray const& s)
      {
        return boost::python::incref(
          boost::python::object(
            s.constData()).ptr());
      }
};

#if 0
BOOST_PYTHON_MODULE(Emb)
{
    class_<YowsupInterface>("Emb")
                                .def("auth_success", &YowsupInterface::auth_success)
                                ;
}
#endif

PythonInterface::PythonInterface(YowsupInterface* handler)
{
    boost::python::to_python_converter<QString,QString_to_python_str>();
    boost::python::to_python_converter<QByteArray,QByteArray_to_python_str>();
    QString_from_python_str();
    try {
        Py_Initialize();
        PyEval_InitThreads();

        pModule = object( (handle<>(borrowed(PyImport_AddModule("__main__")))) );
        object main_namespace = pModule.attr("__dict__");

#define D(X) .def(#X,&YowsupInterface::X)
        main_namespace["Emb"] = class_<YowsupInterface, boost::noncopyable>("Emb", no_init)
                D(auth_success)
                D(auth_fail)
                D(status_dirty)
                D(message_received)
                D(disconnected)
                D(receipt_messageSent)
                D(receipt_messageDelivered)
                D(presence_updated)
                D(presence_available)
                D(presence_unavailable)
                D(message_received)
                D(group_messageReceived)
                D(group_gotInfo)
                D(group_setSubjectSuccess)
                D(group_subjectReceived)
                D(group_addParticipantsSuccess)
                D(group_removeParticipantsSuccess)
                D(group_createSuccess)
                D(group_createFail)
                D(group_endSuccess)
                D(group_gotPicture)
                D(group_infoError)
                D(group_gotParticipants)
                D(group_setPictureSuccess)
                D(group_setPictureError)
                D(profile_setStatusSuccess)
                D(profile_setPictureSuccess)
                D(profile_setPictureError)
                D(status_dirty)
                D(receipt_messageSent)
                D(receipt_messageDelivered)
                D(receipt_visible)
                D(contact_gotProfilePictureId)
                D(contact_typing)
                D(contact_paused)
                D(contact_gotProfilePicture)
                D(notification_contactProfilePictureUpdated)
                D(notification_groupParticipantAdded)
                D(notification_groupParticipantRemoved)
                D(notification_groupPictureUpdated)
                D(image_received)
                D(video_received)
                D(audio_received)
                D(location_received)
                D(vcard_received)
                D(group_imageReceived)
                D(group_videoReceived)
                D(group_audioReceived)
                D(group_locationReceived)
                D(group_vcardReceived)
                D(message_error)
                D(disconnected)
                D(ping)
                D(pong)
                ;
#undef D

        PyObject* code = Py_CompileString(YowsupInterfacePy,"YowsupInterface.py",Py_file_input);
        PyEval_EvalCode((PyCodeObject*)code,main_namespace.ptr(),main_namespace.ptr());
        object pFunc = pModule.attr("init");

        pConnectionManager = pFunc(ptr(handler));

    } catch(const error_already_set& e) {
        qDebug() << "Python error:";
        PyErr_Print();
        exit(1);
    }
    tstate = PyEval_SaveThread();
}

PythonInterface::~PythonInterface()
{
    if(readerThread.joinable()) {
        qDebug() << "PythonInterface::~PythonInterface: sending disconnect, waiting to join";
        call("disconnect");
        readerThread.join();
        qDebug() << "PythonInterface::~PythonInterface: readerThread joined";
    }
    PyEval_RestoreThread(tstate);
    //Py_Finalize();//boost::python bug
}

template object PythonInterface::call<QString,QByteArray>(const QString& method, const QString&, const QByteArray&);
template object PythonInterface::call<>(const QString& method);
template object PythonInterface::call<QString>(const QString& method, const QString&);
template object PythonInterface::call<QString,QString>(const QString& method, const QString&, const QString&);

template<typename... T>
object PythonInterface::call(const QString& method, const T&... args) {
    auto gstate = PyGILState_Ensure();
    object pRet;
    try {
        pRet = pModule.attr("call")(pConnectionManager, object(method), object(args)...);
    } catch(const error_already_set& e) {
        qDebug() << "Python error in call";
        PyErr_Print();
        exit(1);
    }
    PyGILState_Release(gstate);
    return pRet;
}

/* Do not call this more than once! */
void PythonInterface::runReaderThread()
{
    if(readerThread.joinable()) {
        qDebug() << "PythonInterface::runReaderThread may not be called multiple times";
        return;
    }
    auto lambda = [this]()
        {
            try {
                auto state = PyGILState_Ensure();
                /* this will not return until the connection to WhatsApp server
                  * is closed. But it will release the GIL lock during select() calls
                  */
                pModule.attr("runThread")(pConnectionManager);
                PyGILState_Release(state);
            } catch(const error_already_set& e) {
                qDebug() << "Python error in runReaderThread";
                PyErr_Print();
                exit(1);
            }
        };
    readerThread = thread( lambda );
}
