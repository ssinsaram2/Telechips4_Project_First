/****************************************************************************
** Meta object code from reading C++ file 'centralclient.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../qt6_ui/centralclient.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'centralclient.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN13CentralClientE_t {};
} // unnamed namespace

template <> constexpr inline auto CentralClient::qt_create_metaobjectdata<qt_meta_tag_ZN13CentralClientE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "CentralClient",
        "authScreenRequested",
        "",
        "authFailRequested",
        "readyScreenRequested",
        "driverId",
        "seatPosition",
        "temperatureTarget",
        "operationScreenRequested",
        "boardingScreenRequested",
        "screenOffRequested",
        "residualTransition",
        "profileApplied",
        "seatbeltUiChanged",
        "currentBitmap",
        "blueBitmap",
        "redBitmap",
        "grayBitmap",
        "envChanged",
        "tempX10",
        "humidityX10",
        "luxX10",
        "valid",
        "dustChanged",
        "pm1",
        "pm25",
        "pm10",
        "fwStatusChanged",
        "fanLevel",
        "cabinLed",
        "ignitionOperation",
        "pirValidAlert",
        "childWarningRequested",
        "source",
        "level",
        "ignitionBlocked",
        "seatbeltBitmap",
        "faceResultReceived",
        "faceResultFailed",
        "sendBoardingButton",
        "sendAlarmAck",
        "sendFaceSuccess",
        "sendFaceFail",
        "pollCentralQueue",
        "readFaceResultDatagrams"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'authScreenRequested'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'authFailRequested'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'readyScreenRequested'
        QtMocHelpers::SignalData<void(int, int, int)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 }, { QMetaType::Int, 6 }, { QMetaType::Int, 7 },
        }}),
        // Signal 'operationScreenRequested'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'boardingScreenRequested'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'screenOffRequested'
        QtMocHelpers::SignalData<void(bool)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 11 },
        }}),
        // Signal 'profileApplied'
        QtMocHelpers::SignalData<void(int, int, int)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 }, { QMetaType::Int, 6 }, { QMetaType::Int, 7 },
        }}),
        // Signal 'seatbeltUiChanged'
        QtMocHelpers::SignalData<void(quint32, quint32, quint32, quint32)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 14 }, { QMetaType::UInt, 15 }, { QMetaType::UInt, 16 }, { QMetaType::UInt, 17 },
        }}),
        // Signal 'envChanged'
        QtMocHelpers::SignalData<void(int, int, int, int)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 19 }, { QMetaType::Int, 20 }, { QMetaType::Int, 21 }, { QMetaType::Int, 22 },
        }}),
        // Signal 'dustChanged'
        QtMocHelpers::SignalData<void(int, int, int, int)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 24 }, { QMetaType::Int, 25 }, { QMetaType::Int, 26 }, { QMetaType::Int, 22 },
        }}),
        // Signal 'fwStatusChanged'
        QtMocHelpers::SignalData<void(int, int, int, int)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 28 }, { QMetaType::Int, 29 }, { QMetaType::Int, 30 }, { QMetaType::Int, 31 },
        }}),
        // Signal 'childWarningRequested'
        QtMocHelpers::SignalData<void(int, int)>(32, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 33 }, { QMetaType::Int, 34 },
        }}),
        // Signal 'ignitionBlocked'
        QtMocHelpers::SignalData<void(quint32)>(35, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 36 },
        }}),
        // Signal 'faceResultReceived'
        QtMocHelpers::SignalData<void(int)>(37, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 },
        }}),
        // Signal 'faceResultFailed'
        QtMocHelpers::SignalData<void()>(38, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'sendBoardingButton'
        QtMocHelpers::SlotData<void()>(39, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'sendAlarmAck'
        QtMocHelpers::SlotData<void()>(40, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'sendFaceSuccess'
        QtMocHelpers::SlotData<void(int)>(41, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 },
        }}),
        // Slot 'sendFaceFail'
        QtMocHelpers::SlotData<void()>(42, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'pollCentralQueue'
        QtMocHelpers::SlotData<void()>(43, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'readFaceResultDatagrams'
        QtMocHelpers::SlotData<void()>(44, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<CentralClient, qt_meta_tag_ZN13CentralClientE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject CentralClient::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13CentralClientE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13CentralClientE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13CentralClientE_t>.metaTypes,
    nullptr
} };

void CentralClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<CentralClient *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->authScreenRequested(); break;
        case 1: _t->authFailRequested(); break;
        case 2: _t->readyScreenRequested((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 3: _t->operationScreenRequested(); break;
        case 4: _t->boardingScreenRequested(); break;
        case 5: _t->screenOffRequested((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 6: _t->profileApplied((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 7: _t->seatbeltUiChanged((*reinterpret_cast< std::add_pointer_t<quint32>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint32>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<quint32>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<quint32>>(_a[4]))); break;
        case 8: _t->envChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 9: _t->dustChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 10: _t->fwStatusChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 11: _t->childWarningRequested((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 12: _t->ignitionBlocked((*reinterpret_cast< std::add_pointer_t<quint32>>(_a[1]))); break;
        case 13: _t->faceResultReceived((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 14: _t->faceResultFailed(); break;
        case 15: _t->sendBoardingButton(); break;
        case 16: _t->sendAlarmAck(); break;
        case 17: _t->sendFaceSuccess((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 18: _t->sendFaceFail(); break;
        case 19: _t->pollCentralQueue(); break;
        case 20: _t->readFaceResultDatagrams(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (CentralClient::*)()>(_a, &CentralClient::authScreenRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (CentralClient::*)()>(_a, &CentralClient::authFailRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (CentralClient::*)(int , int , int )>(_a, &CentralClient::readyScreenRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (CentralClient::*)()>(_a, &CentralClient::operationScreenRequested, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (CentralClient::*)()>(_a, &CentralClient::boardingScreenRequested, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (CentralClient::*)(bool )>(_a, &CentralClient::screenOffRequested, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (CentralClient::*)(int , int , int )>(_a, &CentralClient::profileApplied, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (CentralClient::*)(quint32 , quint32 , quint32 , quint32 )>(_a, &CentralClient::seatbeltUiChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (CentralClient::*)(int , int , int , int )>(_a, &CentralClient::envChanged, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (CentralClient::*)(int , int , int , int )>(_a, &CentralClient::dustChanged, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (CentralClient::*)(int , int , int , int )>(_a, &CentralClient::fwStatusChanged, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (CentralClient::*)(int , int )>(_a, &CentralClient::childWarningRequested, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (CentralClient::*)(quint32 )>(_a, &CentralClient::ignitionBlocked, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (CentralClient::*)(int )>(_a, &CentralClient::faceResultReceived, 13))
            return;
        if (QtMocHelpers::indexOfMethod<void (CentralClient::*)()>(_a, &CentralClient::faceResultFailed, 14))
            return;
    }
}

const QMetaObject *CentralClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CentralClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13CentralClientE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int CentralClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 21)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 21;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 21)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 21;
    }
    return _id;
}

// SIGNAL 0
void CentralClient::authScreenRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void CentralClient::authFailRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void CentralClient::readyScreenRequested(int _t1, int _t2, int _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3);
}

// SIGNAL 3
void CentralClient::operationScreenRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void CentralClient::boardingScreenRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void CentralClient::screenOffRequested(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void CentralClient::profileApplied(int _t1, int _t2, int _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2, _t3);
}

// SIGNAL 7
void CentralClient::seatbeltUiChanged(quint32 _t1, quint32 _t2, quint32 _t3, quint32 _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 8
void CentralClient::envChanged(int _t1, int _t2, int _t3, int _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 9
void CentralClient::dustChanged(int _t1, int _t2, int _t3, int _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 10
void CentralClient::fwStatusChanged(int _t1, int _t2, int _t3, int _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 11
void CentralClient::childWarningRequested(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 11, nullptr, _t1, _t2);
}

// SIGNAL 12
void CentralClient::ignitionBlocked(quint32 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 12, nullptr, _t1);
}

// SIGNAL 13
void CentralClient::faceResultReceived(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 13, nullptr, _t1);
}

// SIGNAL 14
void CentralClient::faceResultFailed()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}
QT_WARNING_POP
