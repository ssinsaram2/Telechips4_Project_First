/****************************************************************************
** Meta object code from reading C++ file 'scenariowindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../qt6_ui/scenariowindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'scenariowindow.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14ScenarioWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto ScenarioWindow::qt_create_metaobjectdata<qt_meta_tag_ZN14ScenarioWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ScenarioWindow",
        "boardingButtonClicked",
        "",
        "alarmAckClicked",
        "faceSuccessTestRequested",
        "driverId",
        "faceFailTestRequested",
        "showBlackScreen",
        "residualTransition",
        "showAuthScreen",
        "showPinScreen",
        "showReadyScreen",
        "seatPosition",
        "temperatureTarget",
        "showOperationScreen",
        "showBoardingScreen",
        "showChildWarning",
        "source",
        "level",
        "showAuthFailScreen",
        "showIgnitionBlocked",
        "seatbeltBitmap",
        "applyProfile",
        "updateSeatbelt",
        "currentBitmap",
        "blueBitmap",
        "redBitmap",
        "grayBitmap",
        "updateEnv",
        "tempX10",
        "humidityX10",
        "luxX10",
        "valid",
        "updateDust",
        "pm1",
        "pm25",
        "pm10",
        "updateFwStatus",
        "fanLevel",
        "cabinLed",
        "ignitionOperation",
        "pirValidAlert",
        "handleFaceResultSuccess",
        "handleFaceResultFail",
        "submitPin",
        "erasePinDigit",
        "handleAuthTimeout",
        "resetToPureBlack"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'boardingButtonClicked'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'alarmAckClicked'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'faceSuccessTestRequested'
        QtMocHelpers::SignalData<void(int)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 },
        }}),
        // Signal 'faceFailTestRequested'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'showBlackScreen'
        QtMocHelpers::SlotData<void(bool)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 8 },
        }}),
        // Slot 'showAuthScreen'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'showPinScreen'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'showReadyScreen'
        QtMocHelpers::SlotData<void(int, int, int)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 }, { QMetaType::Int, 12 }, { QMetaType::Int, 13 },
        }}),
        // Slot 'showOperationScreen'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'showBoardingScreen'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'showChildWarning'
        QtMocHelpers::SlotData<void(int, int)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 17 }, { QMetaType::Int, 18 },
        }}),
        // Slot 'showAuthFailScreen'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'showIgnitionBlocked'
        QtMocHelpers::SlotData<void(quint32)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 21 },
        }}),
        // Slot 'applyProfile'
        QtMocHelpers::SlotData<void(int, int, int)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 }, { QMetaType::Int, 12 }, { QMetaType::Int, 13 },
        }}),
        // Slot 'updateSeatbelt'
        QtMocHelpers::SlotData<void(quint32, quint32, quint32, quint32)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 24 }, { QMetaType::UInt, 25 }, { QMetaType::UInt, 26 }, { QMetaType::UInt, 27 },
        }}),
        // Slot 'updateEnv'
        QtMocHelpers::SlotData<void(int, int, int, int)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 29 }, { QMetaType::Int, 30 }, { QMetaType::Int, 31 }, { QMetaType::Int, 32 },
        }}),
        // Slot 'updateDust'
        QtMocHelpers::SlotData<void(int, int, int, int)>(33, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 34 }, { QMetaType::Int, 35 }, { QMetaType::Int, 36 }, { QMetaType::Int, 32 },
        }}),
        // Slot 'updateFwStatus'
        QtMocHelpers::SlotData<void(int, int, int, int)>(37, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 38 }, { QMetaType::Int, 39 }, { QMetaType::Int, 40 }, { QMetaType::Int, 41 },
        }}),
        // Slot 'handleFaceResultSuccess'
        QtMocHelpers::SlotData<void(int)>(42, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 },
        }}),
        // Slot 'handleFaceResultFail'
        QtMocHelpers::SlotData<void()>(43, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'submitPin'
        QtMocHelpers::SlotData<void()>(44, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'erasePinDigit'
        QtMocHelpers::SlotData<void()>(45, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleAuthTimeout'
        QtMocHelpers::SlotData<void()>(46, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'resetToPureBlack'
        QtMocHelpers::SlotData<void()>(47, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ScenarioWindow, qt_meta_tag_ZN14ScenarioWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ScenarioWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14ScenarioWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14ScenarioWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN14ScenarioWindowE_t>.metaTypes,
    nullptr
} };

void ScenarioWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ScenarioWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->boardingButtonClicked(); break;
        case 1: _t->alarmAckClicked(); break;
        case 2: _t->faceSuccessTestRequested((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->faceFailTestRequested(); break;
        case 4: _t->showBlackScreen((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: _t->showAuthScreen(); break;
        case 6: _t->showPinScreen(); break;
        case 7: _t->showReadyScreen((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 8: _t->showOperationScreen(); break;
        case 9: _t->showBoardingScreen(); break;
        case 10: _t->showChildWarning((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 11: _t->showAuthFailScreen(); break;
        case 12: _t->showIgnitionBlocked((*reinterpret_cast< std::add_pointer_t<quint32>>(_a[1]))); break;
        case 13: _t->applyProfile((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 14: _t->updateSeatbelt((*reinterpret_cast< std::add_pointer_t<quint32>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint32>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<quint32>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<quint32>>(_a[4]))); break;
        case 15: _t->updateEnv((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 16: _t->updateDust((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 17: _t->updateFwStatus((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 18: _t->handleFaceResultSuccess((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 19: _t->handleFaceResultFail(); break;
        case 20: _t->submitPin(); break;
        case 21: _t->erasePinDigit(); break;
        case 22: _t->handleAuthTimeout(); break;
        case 23: _t->resetToPureBlack(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ScenarioWindow::*)()>(_a, &ScenarioWindow::boardingButtonClicked, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ScenarioWindow::*)()>(_a, &ScenarioWindow::alarmAckClicked, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ScenarioWindow::*)(int )>(_a, &ScenarioWindow::faceSuccessTestRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (ScenarioWindow::*)()>(_a, &ScenarioWindow::faceFailTestRequested, 3))
            return;
    }
}

const QMetaObject *ScenarioWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ScenarioWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14ScenarioWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int ScenarioWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 24)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 24;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 24)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 24;
    }
    return _id;
}

// SIGNAL 0
void ScenarioWindow::boardingButtonClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ScenarioWindow::alarmAckClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ScenarioWindow::faceSuccessTestRequested(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void ScenarioWindow::faceFailTestRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}
QT_WARNING_POP
