/****************************************************************************
** Meta object code from reading C++ file 'faceauthscreen.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.2.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../qt6_ui/faceauthscreen.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'faceauthscreen.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.2.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_FaceAuthScreen_t {
    const uint offsetsAndSize[16];
    char stringdata0[123];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(offsetof(qt_meta_stringdata_FaceAuthScreen_t, stringdata0) + ofs), len 
static const qt_meta_stringdata_FaceAuthScreen_t qt_meta_stringdata_FaceAuthScreen = {
    {
QT_MOC_LITERAL(0, 14), // "FaceAuthScreen"
QT_MOC_LITERAL(15, 24), // "faceSuccessTestRequested"
QT_MOC_LITERAL(40, 0), // ""
QT_MOC_LITERAL(41, 8), // "driverId"
QT_MOC_LITERAL(50, 21), // "faceFailTestRequested"
QT_MOC_LITERAL(72, 11), // "resetStatus"
QT_MOC_LITERAL(84, 20), // "showDriverRecognized"
QT_MOC_LITERAL(105, 17) // "showUnknownDriver"

    },
    "FaceAuthScreen\0faceSuccessTestRequested\0"
    "\0driverId\0faceFailTestRequested\0"
    "resetStatus\0showDriverRecognized\0"
    "showUnknownDriver"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_FaceAuthScreen[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   44,    2, 0x06,    1 /* Public */,
       4,    0,   47,    2, 0x06,    3 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       5,    0,   48,    2, 0x0a,    4 /* Public */,
       6,    1,   49,    2, 0x0a,    5 /* Public */,
       7,    0,   52,    2, 0x0a,    7 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void,

       0        // eod
};

void FaceAuthScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<FaceAuthScreen *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->faceSuccessTestRequested((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->faceFailTestRequested(); break;
        case 2: _t->resetStatus(); break;
        case 3: _t->showDriverRecognized((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->showUnknownDriver(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (FaceAuthScreen::*)(int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&FaceAuthScreen::faceSuccessTestRequested)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (FaceAuthScreen::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&FaceAuthScreen::faceFailTestRequested)) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject FaceAuthScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_FaceAuthScreen.offsetsAndSize,
    qt_meta_data_FaceAuthScreen,
    qt_static_metacall,
    nullptr,
qt_incomplete_metaTypeArray<qt_meta_stringdata_FaceAuthScreen_t
, QtPrivate::TypeAndForceComplete<FaceAuthScreen, std::true_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>
, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>


>,
    nullptr
} };


const QMetaObject *FaceAuthScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *FaceAuthScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_FaceAuthScreen.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int FaceAuthScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void FaceAuthScreen::faceSuccessTestRequested(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void FaceAuthScreen::faceFailTestRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
