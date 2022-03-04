/****************************************************************************
** Meta object code from reading C++ file 'StreamElementsCefClient.hpp'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "streamelements/StreamElementsCefClient.hpp"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'StreamElementsCefClient.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_StreamElementsCefClient_t {
    QByteArrayData data[1];
    char stringdata0[24];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_StreamElementsCefClient_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_StreamElementsCefClient_t qt_meta_stringdata_StreamElementsCefClient = {
    {
QT_MOC_LITERAL(0, 0, 23) // "StreamElementsCefClient"

    },
    "StreamElementsCefClient"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_StreamElementsCefClient[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

       0        // eod
};

void StreamElementsCefClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_UNUSED(_o);
    Q_UNUSED(_id);
    Q_UNUSED(_c);
    Q_UNUSED(_a);
}

QT_INIT_METAOBJECT const QMetaObject StreamElementsCefClient::staticMetaObject = { {
    QMetaObject::SuperData::link<CefClient::staticMetaObject>(),
    qt_meta_stringdata_StreamElementsCefClient.data,
    qt_meta_data_StreamElementsCefClient,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *StreamElementsCefClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *StreamElementsCefClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_StreamElementsCefClient.stringdata0))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "CefLifeSpanHandler"))
        return static_cast< CefLifeSpanHandler*>(this);
    if (!strcmp(_clname, "CefContextMenuHandler"))
        return static_cast< CefContextMenuHandler*>(this);
    if (!strcmp(_clname, "CefLoadHandler"))
        return static_cast< CefLoadHandler*>(this);
    if (!strcmp(_clname, "CefDisplayHandler"))
        return static_cast< CefDisplayHandler*>(this);
    if (!strcmp(_clname, "CefKeyboardHandler"))
        return static_cast< CefKeyboardHandler*>(this);
    if (!strcmp(_clname, "CefRequestHandler"))
        return static_cast< CefRequestHandler*>(this);
    if (!strcmp(_clname, "CefRenderHandler"))
        return static_cast< CefRenderHandler*>(this);
    if (!strcmp(_clname, "CefJSDialogHandler"))
        return static_cast< CefJSDialogHandler*>(this);
    if (!strcmp(_clname, "CefFocusHandler"))
        return static_cast< CefFocusHandler*>(this);
    if (!strcmp(_clname, "CefDragHandler"))
        return static_cast< CefDragHandler*>(this);
    if (!strcmp(_clname, "QObject"))
        return static_cast< QObject*>(this);
    return CefClient::qt_metacast(_clname);
}

int StreamElementsCefClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = CefClient::qt_metacall(_c, _id, _a);
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
