#ifndef SCENARIOWINDOW_H
#define SCENARIOWINDOW_H

#include <QMainWindow>

class FaceAuthScreen;
class QLabel;
class QLineEdit;
class MainWindowScreen;
class QNetworkAccessManager;
class QPushButton;
class QStackedWidget;
class QTimer;

class ScenarioWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ScenarioWindow(QWidget *parent = nullptr);

signals:
    void boardingButtonClicked();
    void alarmAckClicked();
    void faceSuccessTestRequested(int driverId);
    void faceFailTestRequested();

public slots:
    void showBlackScreen(bool residualTransition);
    void showAuthScreen();
    void showPinScreen();
    void showReadyScreen(int driverId, int seatPosition, int temperatureTarget);
    void showOperationScreen();
    void showBoardingScreen();
    void showChildWarning(int source, int level);
    void showAuthFailScreen();
    void showIgnitionBlocked(quint32 seatbeltBitmap);
    void applyProfile(int driverId, int seatPosition, int temperatureTarget);
    void updateSeatbelt(quint32 currentBitmap, quint32 blueBitmap, quint32 redBitmap, quint32 grayBitmap);
    void updateEnv(int tempX10, int humidityX10, int luxX10, int valid);
    void updateDust(int pm1, int pm25, int pm10, int valid);
    void updateFwStatus(int fanLevel, int cabinLed, int ignitionOperation, int pirValidAlert);
    void handleFaceResultSuccess(int driverId);
    void handleFaceResultFail();

private slots:
    void submitPin();
    void erasePinDigit();
    void handleAuthTimeout();
    void resetToPureBlack();

private:
    enum ScreenIndex
    {
        ScreenBlack = 0,
        ScreenAuth,
        ScreenPin,
        ScreenReady,
        ScreenMain,
        ScreenWarning,
        ScreenLocked
    };

    QWidget *makeBlackScreen();
    QWidget *makePinScreen();
    QWidget *makeReadyScreen();
    QWidget *makeWarningScreen();
    QWidget *makeLockedScreen();
    QLabel *makeLabel(const QString &text, int pointSize, bool bold = false);
    QPushButton *makeButton(const QString &text);
    QPushButton *makePinDialButton(const QString &text);
    void appendPinDigit(const QString &digit);
    QString generatePinCode() const;
    void issuePinCode();
    bool loadTelegramConfig(QString *groupId, QString *apiToken) const;
    void sendTelegramMessage(const QString &messageText,
                             const QString &logPrefix,
                             const QString &configErrorStatus = QString(),
                             const QString &networkErrorStatus = QString(),
                             const QString &successStatus = QString(),
                             const QString &failurePrefix = QString());
    void sendPinCodeToTelegram(const QString &pinCode);
    void sendChildWarningToTelegram(int source, int level);
    void setPinStatus(const QString &text);
    void startAuthTimeout();
    void stopAuthTimeout();

    QStackedWidget *m_stack = nullptr;
    FaceAuthScreen *m_authScreen = nullptr;
    MainWindowScreen *m_mainScreen = nullptr;
    QNetworkAccessManager *m_telegramNetwork = nullptr;
    QTimer *m_authTimeoutTimer = nullptr;
    QLabel *m_blackLabel = nullptr;
    QLabel *m_readyTitle = nullptr;
    QLabel *m_readyDetail = nullptr;
    QLabel *m_warningTitle = nullptr;
    QLabel *m_warningDetail = nullptr;
    QLabel *m_pinStatusLabel = nullptr;
    QLineEdit *m_pinEdit = nullptr;

    QString m_activePinCode;
    int m_pinFailCount = 0;
    int m_lastDriverId = 0;
};

#endif
