/****************************************************************************
** Meta object code from reading C++ file 'StreamElementsNetworkDialog.hpp'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "streamelements/StreamElementsNetworkDialog.hpp"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'StreamElementsNetworkDialog.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_StreamElementsNetworkDialog_t {
    QByteArrayData data[8];
    char stringdata0[150];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_StreamElementsNetworkDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_StreamElementsNetworkDialog_t qt_meta_stringdata_StreamElementsNetworkDialog = {
    {
QT_MOC_LITERAL(0, 0, 27), // "StreamElementsNetworkDialog"
QT_MOC_LITERAL(1, 28, 6), // "reject"
QT_MOC_LITERAL(2, 35, 0), // ""
QT_MOC_LITERAL(3, 36, 36), // "DownloadFileAsyncUpdateUserIn..."
QT_MOC_LITERAL(4, 73, 7), // "dltotal"
QT_MOC_LITERAL(5, 81, 5), // "dlnow"
QT_MOC_LITERAL(6, 87, 34), // "UploadFileAsyncUpdateUserInte..."
QT_MOC_LITERAL(7, 122, 27) // "on_ctl_cancelButton_clicked"

    },
    "StreamElementsNetworkDialog\0reject\0\0"
    "DownloadFileAsyncUpdateUserInterface\0"
    "dltotal\0dlnow\0UploadFileAsyncUpdateUserInterface\0"
    "on_ctl_cancelButton_clicked"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_StreamElementsNetworkDialog[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   34,    2, 0x0a /* Public */,
       3,    2,   35,    2, 0x08 /* Private */,
       6,    2,   40,    2, 0x08 /* Private */,
       7,    0,   45,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Long, QMetaType::Long,    4,    5,
    QMetaType::Void, QMetaType::Long, QMetaType::Long,    4,    5,
    QMetaType::Void,

       0        // eod
};

void StreamElementsNetworkDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<StreamElementsNetworkDialog *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->reject(); break;
        case 1: _t->DownloadFileAsyncUpdateUserInterface((*reinterpret_cast< long(*)>(_a[1])),(*reinterpret_cast< long(*)>(_a[2]))); break;
        case 2: _t->UploadFileAsyncUpdateUserInterface((*reinterpret_cast< long(*)>(_a[1])),(*reinterpret_cast< long(*)>(_a[2]))); break;
        case 3: _t->on_ctl_cancelButton_clicked(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject StreamElementsNetworkDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_StreamElementsNetworkDialog.data,
    qt_meta_data_StreamElementsNetworkDialog,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *StreamElementsNetworkDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *StreamElementsNetworkDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_StreamElementsNetworkDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int StreamElementsNetworkDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 4;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
