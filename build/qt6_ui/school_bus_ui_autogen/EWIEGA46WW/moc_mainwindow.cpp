/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.2.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../qt6_ui/mainwindow.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.2.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_MainWindowScreen_t {
    const uint offsetsAndSize[48];
    char stringdata0[269];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(offsetof(qt_meta_stringdata_MainWindowScreen_t, stringdata0) + ofs), len 
static const qt_meta_stringdata_MainWindowScreen_t qt_meta_stringdata_MainWindowScreen = {
    {
QT_MOC_LITERAL(0, 16), // "MainWindowScreen"
QT_MOC_LITERAL(17, 21), // "boardingButtonClicked"
QT_MOC_LITERAL(39, 0), // ""
QT_MOC_LITERAL(40, 17), // "showOperationMode"
QT_MOC_LITERAL(58, 16), // "showBoardingMode"
QT_MOC_LITERAL(75, 14), // "updateSeatbelt"
QT_MOC_LITERAL(90, 13), // "currentBitmap"
QT_MOC_LITERAL(104, 10), // "blueBitmap"
QT_MOC_LITERAL(115, 9), // "redBitmap"
QT_MOC_LITERAL(125, 10), // "grayBitmap"
QT_MOC_LITERAL(136, 9), // "updateEnv"
QT_MOC_LITERAL(146, 7), // "tempX10"
QT_MOC_LITERAL(154, 11), // "humidityX10"
QT_MOC_LITERAL(166, 6), // "luxX10"
QT_MOC_LITERAL(173, 5), // "valid"
QT_MOC_LITERAL(179, 10), // "updateDust"
QT_MOC_LITERAL(190, 3), // "pm1"
QT_MOC_LITERAL(194, 4), // "pm25"
QT_MOC_LITERAL(199, 4), // "pm10"
QT_MOC_LITERAL(204, 14), // "updateFwStatus"
QT_MOC_LITERAL(219, 8), // "fanLevel"
QT_MOC_LITERAL(228, 8), // "cabinLed"
QT_MOC_LITERAL(237, 17), // "ignitionOperation"
QT_MOC_LITERAL(255, 13) // "pirValidAlert"

    },
    "MainWindowScreen\0boardingButtonClicked\0"
    "\0showOperationMode\0showBoardingMode\0"
    "updateSeatbelt\0currentBitmap\0blueBitmap\0"
    "redBitmap\0grayBitmap\0updateEnv\0tempX10\0"
    "humidityX10\0luxX10\0valid\0updateDust\0"
    "pm1\0pm25\0pm10\0updateFwStatus\0fanLevel\0"
    "cabinLed\0ignitionOperation\0pirValidAlert"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MainWindowScreen[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   56,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       3,    0,   57,    2, 0x0a,    2 /* Public */,
       4,    0,   58,    2, 0x0a,    3 /* Public */,
       5,    4,   59,    2, 0x0a,    4 /* Public */,
      10,    4,   68,    2, 0x0a,    9 /* Public */,
      15,    4,   77,    2, 0x0a,   14 /* Public */,
      19,    4,   86,    2, 0x0a,   19 /* Public */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::UInt, QMetaType::UInt, QMetaType::UInt, QMetaType::UInt,    6,    7,    8,    9,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::Int, QMetaType::Int,   11,   12,   13,   14,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::Int, QMetaType::Int,   16,   17,   18,   14,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::Int, QMetaType::Int,   20,   21,   22,   23,

       0        // eod
};

void MainWindowScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainWindowScreen *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->boardingButtonClicked(); break;
        case 1: _t->showOperationMode(); break;
        case 2: _t->showBoardingMode(); break;
        case 3: _t->updateSeatbelt((*reinterpret_cast< std::add_pointer_t<quint32>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint32>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<quint32>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<quint32>>(_a[4]))); break;
        case 4: _t->updateEnv((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 5: _t->updateDust((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 6: _t->updateFwStatus((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (MainWindowScreen::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&MainWindowScreen::boardingButtonClicked)) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject MainWindowScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_MainWindowScreen.offsetsAndSize,
    qt_meta_data_MainWindowScreen,
    qt_static_metacall,
    nullptr,
qt_incomplete_metaTypeArray<qt_meta_stringdata_MainWindowScreen_t
, QtPrivate::TypeAndForceComplete<MainWindowScreen, std::true_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>
, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<quint32, std::false_type>, QtPrivate::TypeAndForceComplete<quint32, std::false_type>, QtPrivate::TypeAndForceComplete<quint32, std::false_type>, QtPrivate::TypeAndForceComplete<quint32, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>


>,
    nullptr
} };


const QMetaObject *MainWindowScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindowScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MainWindowScreen.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int MainWindowScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void MainWindowScreen::boardingButtonClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
