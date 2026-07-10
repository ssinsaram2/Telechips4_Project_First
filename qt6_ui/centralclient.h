#ifndef CENTRALCLIENT_H
#define CENTRALCLIENT_H

#include <QObject>
#include <QTimer>
#include <QtGlobal>

#ifdef Q_OS_LINUX
#include <mqueue.h>
#endif

class QUdpSocket;

class CentralClient : public QObject
{
    Q_OBJECT

public:
    explicit CentralClient(QObject *parent = nullptr);
    ~CentralClient() override;

    void initialize();

public slots:
    void sendBoardingButton();
    void sendAlarmAck();
    void sendFaceSuccess(int driverId);
    void sendFaceFail();

signals:
    void authScreenRequested();
    void authFailRequested();
    void readyScreenRequested(int driverId, int seatPosition, int temperatureTarget);
    void operationScreenRequested();
    void boardingScreenRequested();
    void screenOffRequested(bool residualTransition);
    void profileApplied(int driverId, int seatPosition, int temperatureTarget);
    void seatbeltUiChanged(quint32 currentBitmap, quint32 blueBitmap, quint32 redBitmap, quint32 grayBitmap);
    void envChanged(int tempX10, int humidityX10, int luxX10, int valid);
    void dustChanged(int pm1, int pm25, int pm10, int valid);
    void fwStatusChanged(int fanLevel, int cabinLed, int ignitionOperation, int pirValidAlert);
    void childWarningRequested(int source, int level);
    void ignitionBlocked(quint32 seatbeltBitmap);
    void faceResultReceived(int driverId);
    void faceResultFailed();

private slots:
    void pollCentralQueue();
    void readFaceResultDatagrams();

private:
    void processMessage(int type, const int data[4]);
    bool sendMessage(int type, int d0 = 0, int d1 = 0, int d2 = 0, int d3 = 0);
    void setupFaceResultSocket();

#ifdef Q_OS_LINUX
    mqd_t m_centralToQt = (mqd_t)-1;
    mqd_t m_qtToCentral = (mqd_t)-1;
#endif

    QTimer m_pollTimer;
    QUdpSocket *m_faceSocket = nullptr;
};

#endif
