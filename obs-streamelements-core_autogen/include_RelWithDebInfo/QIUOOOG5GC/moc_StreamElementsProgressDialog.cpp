/****************************************************************************
** Meta object code from reading C++ file 'StreamElementsProgressDialog.hpp'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "streamelements/StreamElementsProgressDialog.hpp"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'StreamElementsProgressDialog.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_StreamElementsProgressDialog_t {
    QByteArrayData data[11];
    char stringdata0[106];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_StreamElementsProgressDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_StreamElementsProgressDialog_t qt_meta_stringdata_StreamElementsProgressDialog = {
    {
QT_MOC_LITERAL(0, 0, 28), // "StreamElementsProgressDialog"
QT_MOC_LITERAL(1, 29, 15), // "setEnableCancel"
QT_MOC_LITERAL(2, 45, 0), // ""
QT_MOC_LITERAL(3, 46, 6), // "enable"
QT_MOC_LITERAL(4, 53, 10), // "setMessage"
QT_MOC_LITERAL(5, 64, 11), // "std::string"
QT_MOC_LITERAL(6, 76, 3), // "msg"
QT_MOC_LITERAL(7, 80, 11), // "setProgress"
QT_MOC_LITERAL(8, 92, 3), // "min"
QT_MOC_LITERAL(9, 96, 3), // "max"
QT_MOC_LITERAL(10, 100, 5) // "value"

    },
    "StreamElementsProgressDialog\0"
    "setEnableCancel\0\0enable\0setMessage\0"
    "std::string\0msg\0setProgress\0min\0max\0"
    "value"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_StreamElementsProgressDialog[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   29,    2, 0x0a /* Public */,
       4,    1,   32,    2, 0x0a /* Public */,
       7,    3,   35,    2, 0x0a /* Public */,

 // slots: parameters
    QMetaType::Void, QMetaType::Bool,    3,
    QMetaType::Void, 0x80000000 | 5,    6,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::Int,    8,    9,   10,

       0        // eod
};

void StreamElementsProgressDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<StreamElementsProgressDialog *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->setEnableCancel((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 1: _t->setMessage((*reinterpret_cast< std::string(*)>(_a[1]))); break;
        case 2: _t->setProgress((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3]))); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject StreamElementsProgressDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_StreamElementsProgressDialog.data,
    qt_meta_data_StreamElementsProgressDialog,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *StreamElementsProgressDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *StreamElementsProgressDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_StreamElementsProgressDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int StreamElementsProgressDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 3;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
