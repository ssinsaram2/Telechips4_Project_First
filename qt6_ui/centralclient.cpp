#include "centralclient.h"

extern "C" {
#include "ipc_contract.h"
}

#include <QByteArray>
#include <QDebug>
#include <QHostAddress>
#include <QUdpSocket>

#ifdef Q_OS_LINUX
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#endif

namespace {

constexpr int PollIntervalMs = 50;
constexpr quint16 DefaultFaceResultPort = 5000;

QByteArray queueName(const char *envName, const char *defaultName)
{
    const QByteArray configured = qgetenv(envName).trimmed();
    if (configured.isEmpty())
        return QByteArray(defaultName);
    return configured;
}

quint16 faceResultPort()
{
    const QByteArray configured = qgetenv("DRIVER_ID_LISTEN_PORT").trimmed();
    if (configured.isEmpty())
        return DefaultFaceResultPort;
    bool ok = false;
    const int port = QString::fromLatin1(configured).toInt(&ok);
    return ok && port > 0 && port <= 65535 ? static_cast<quint16>(port)
                                           : DefaultFaceResultPort;
}

bool parseDriverIdPayload(const QByteArray &payload, int *driverId)
{
    const QByteArray trimmed = payload.trimmed();
    const QByteArray lowered = trimmed.toLower();
    if (lowered == "fail" || lowered == "failed" || lowered == "unknown" ||
        lowered == "false" || lowered == "no") {
        if (driverId)
            *driverId = 0;
        return true;
    }

    QByteArray digits;
    for (char ch : trimmed) {
        if (ch >= '0' && ch <= '9') {
            digits.append(ch);
        } else if (!digits.isEmpty()) {
            break;
        }
    }

    if (digits.isEmpty())
        return false;

    bool ok = false;
    const int parsed = digits.toInt(&ok);
    if (!ok)
        return false;

    if (driverId)
        *driverId = parsed;
    return true;
}

}

CentralClient::CentralClient(QObject *parent)
    : QObject(parent)
{
    connect(&m_pollTimer, &QTimer::timeout, this, &CentralClient::pollCentralQueue);
}

CentralClient::~CentralClient()
{
    m_pollTimer.stop();
#ifdef Q_OS_LINUX
    if (m_centralToQt != (mqd_t)-1)
        mq_close(m_centralToQt);
    if (m_qtToCentral != (mqd_t)-1)
        mq_close(m_qtToCentral);
#endif
}

void CentralClient::initialize()
{
#ifdef Q_OS_LINUX
    mq_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg = IPC_QUEUE_MAX_MESSAGES_DEFAULT;
    attr.mq_msgsize = sizeof(IpcMessage);

    const QByteArray centralToQtName = queueName(IPC_CENTRAL_TO_QT_QUEUE_ENV,
                                                 IPC_CENTRAL_TO_QT_QUEUE_DEFAULT);
    const QByteArray qtToCentralName = queueName(IPC_QT_TO_CENTRAL_QUEUE_ENV,
                                                 IPC_QT_TO_CENTRAL_QUEUE_DEFAULT);

    m_centralToQt = mq_open(centralToQtName.constData(),
                            O_CREAT | O_RDWR | O_NONBLOCK,
                            0644,
                            &attr);
    if (m_centralToQt == (mqd_t)-1) {
        qWarning() << "[UI] mq_open central_to_qt failed:" << strerror(errno);
    }

    m_qtToCentral = mq_open(qtToCentralName.constData(),
                            O_CREAT | O_RDWR | O_NONBLOCK,
                            0644,
                            &attr);
    if (m_qtToCentral == (mqd_t)-1) {
        qWarning() << "[UI] mq_open qt_to_central failed:" << strerror(errno);
    }
#else
    qInfo() << "[UI] POSIX mqueue disabled on this platform";
#endif

    setupFaceResultSocket();
    m_pollTimer.start(PollIntervalMs);
}

void CentralClient::setupFaceResultSocket()
{
    m_faceSocket = new QUdpSocket(this);
    const quint16 port = faceResultPort();
    if (!m_faceSocket->bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress)) {
        qWarning() << "[UI] failed to bind face result UDP port" << port
                   << m_faceSocket->errorString();
        return;
    }
    connect(m_faceSocket, &QUdpSocket::readyRead,
            this, &CentralClient::readFaceResultDatagrams);
    qInfo() << "[UI] face result UDP listening on" << port;
}

bool CentralClient::sendMessage(int type, int d0, int d1, int d2, int d3)
{
#ifdef Q_OS_LINUX
    if (m_qtToCentral == (mqd_t)-1)
        return false;

    IpcMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = type;
    msg.data[0] = d0;
    msg.data[1] = d1;
    msg.data[2] = d2;
    msg.data[3] = d3;

    if (mq_send(m_qtToCentral, reinterpret_cast<const char *>(&msg), sizeof(msg), 0) != 0) {
        if (errno != EAGAIN)
            qWarning() << "[UI] mq_send failed:" << strerror(errno) << "type" << type;
        return false;
    }
    return true;
#else
    Q_UNUSED(type)
    Q_UNUSED(d0)
    Q_UNUSED(d1)
    Q_UNUSED(d2)
    Q_UNUSED(d3)
    return false;
#endif
}

void CentralClient::sendBoardingButton()
{
    sendMessage(IPC_MSG_BOARDING_BUTTON);
}

void CentralClient::sendAlarmAck()
{
    sendMessage(IPC_MSG_ALARM_ACK);
}

void CentralClient::sendFaceSuccess(int driverId)
{
    if (driverId <= 0)
        driverId = 1;
    sendMessage(IPC_MSG_FACE_AUTH_SUCCESS, driverId);
}

void CentralClient::sendFaceFail()
{
    sendMessage(IPC_MSG_FACE_AUTH_FAIL);
}

void CentralClient::pollCentralQueue()
{
#ifdef Q_OS_LINUX
    if (m_centralToQt == (mqd_t)-1)
        return;

    while (true) {
        IpcMessage msg;
        const ssize_t received = mq_receive(m_centralToQt,
                                            reinterpret_cast<char *>(&msg),
                                            sizeof(msg),
                                            nullptr);
        if (received < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                qWarning() << "[UI] mq_receive failed:" << strerror(errno);
            return;
        }
        if (received == static_cast<ssize_t>(sizeof(msg)))
            processMessage(msg.type, msg.data);
    }
#endif
}

void CentralClient::processMessage(int type, const int data[4])
{
    switch (type) {
    case IPC_MSG_SHOW_AUTH_SCREEN:
        emit authScreenRequested();
        break;
    case IPC_MSG_AUTH_FAIL:
        emit authFailRequested();
        break;
    case IPC_MSG_SHOW_READY_SCREEN:
        emit readyScreenRequested(data[0], data[1], data[2]);
        break;
    case IPC_MSG_SHOW_OPERATION_SCREEN:
        emit operationScreenRequested();
        break;
    case IPC_MSG_SHOW_BOARDING_SCREEN:
        emit boardingScreenRequested();
        break;
    case IPC_MSG_SCREEN_OFF:
        emit screenOffRequested(data[0] != 0);
        break;
    case IPC_MSG_PROFILE_APPLIED:
        emit profileApplied(data[0], data[1], data[2]);
        break;
    case IPC_MSG_SEATBELT_UPDATE:
        emit seatbeltUiChanged(static_cast<quint32>(data[0]),
                               static_cast<quint32>(data[1]),
                               static_cast<quint32>(data[2]),
                               static_cast<quint32>(data[3]));
        break;
    case IPC_MSG_ENV_UPDATE:
        emit envChanged(data[0], data[1], data[2], data[3]);
        break;
    case IPC_MSG_DUST_UPDATE:
        emit dustChanged(data[0], data[1], data[2], data[3]);
        break;
    case IPC_MSG_FW_STATUS_UPDATE:
        emit fwStatusChanged(data[0], data[1], data[2], data[3]);
        break;
    case IPC_MSG_CHILD_DETECTED_WARNING:
        emit childWarningRequested(data[0], data[1]);
        break;
    case IPC_MSG_IGNITION_BLOCKED_RESIDUAL_CHILD:
        emit ignitionBlocked(static_cast<quint32>(data[0]));
        break;
    default:
        qInfo() << "[UI] ignored IPC type" << type;
        break;
    }
}

void CentralClient::readFaceResultDatagrams()
{
    while (m_faceSocket && m_faceSocket->hasPendingDatagrams()) {
        QByteArray payload;
        payload.resize(static_cast<int>(m_faceSocket->pendingDatagramSize()));
        m_faceSocket->readDatagram(payload.data(), payload.size());

        int driverId = 0;
        if (!parseDriverIdPayload(payload, &driverId)) {
            qWarning() << "[UI] ignored face payload:" << payload;
            continue;
        }

        if (driverId > 0) {
            emit faceResultReceived(driverId);
            sendFaceSuccess(driverId);
        }
    }
}
