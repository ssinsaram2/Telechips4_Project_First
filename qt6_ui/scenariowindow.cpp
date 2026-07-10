#include "scenariowindow.h"

#include "faceauthscreen.h"
#include "mainwindow.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString sourceText(int source)
{
    if (source == 2)
        return QStringLiteral("카메라 객체감지");
    if (source == 1)
        return QStringLiteral("PIR 센서");
    return QStringLiteral("잔류 감지 센서");
}

QString levelText(int level)
{
    if (level >= 2)
        return QStringLiteral("2차 경고: 텔레그램 알림 전송");
    return QStringLiteral("1차 경고: 현장 경고 출력 중");
}

}

ScenarioWindow::ScenarioWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("어린이 통학차량 안전관리 시스템"));
    resize(1280, 720);

    m_stack = new QStackedWidget(this);
    m_authScreen = new FaceAuthScreen;
    m_mainScreen = new MainWindowScreen;

    connect(m_authScreen, &FaceAuthScreen::faceSuccessTestRequested,
            this, &ScenarioWindow::faceSuccessTestRequested);
    connect(m_authScreen, &FaceAuthScreen::faceFailTestRequested,
            this, &ScenarioWindow::faceFailTestRequested);
    connect(m_mainScreen, &MainWindowScreen::boardingButtonClicked,
            this, &ScenarioWindow::boardingButtonClicked);

    m_stack->addWidget(makeBlackScreen());
    m_stack->addWidget(m_authScreen);
    m_stack->addWidget(makePinScreen());
    m_stack->addWidget(makeReadyScreen());
    m_stack->addWidget(m_mainScreen);
    m_stack->addWidget(makeWarningScreen());
    m_stack->addWidget(makeLockedScreen());
    setCentralWidget(m_stack);

    showBlackScreen(false);
}

QLabel *ScenarioWindow::makeLabel(const QString &text, int pointSize, bool bold)
{
    QLabel *label = new QLabel(text);
    QFont font = label->font();
    font.setPointSize(pointSize);
    font.setBold(bold);
    label->setFont(font);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setStyleSheet("color:#f8fafc;");
    return label;
}

QPushButton *ScenarioWindow::makeButton(const QString &text)
{
    QPushButton *button = new QPushButton(text);
    QFont font = button->font();
    font.setPointSize(18);
    font.setBold(true);
    button->setFont(font);
    button->setMinimumHeight(64);
    button->setStyleSheet(
        "QPushButton { background:#155e75; color:white; border:2px solid #67e8f9; border-radius:14px; padding:12px; }"
        "QPushButton:pressed { background:#0e7490; }");
    return button;
}

QWidget *ScenarioWindow::makeBlackScreen()
{
    QWidget *screen = new QWidget;
    screen->setStyleSheet("background:#000000;");
    QVBoxLayout *layout = new QVBoxLayout(screen);
    m_blackLabel = makeLabel(QString(), 34, true);
    layout->addStretch();
    layout->addWidget(m_blackLabel);
    layout->addStretch();
    return screen;
}

QWidget *ScenarioWindow::makePinScreen()
{
    QWidget *screen = new QWidget;
    screen->setStyleSheet("background:#111827;");
    QVBoxLayout *layout = new QVBoxLayout(screen);
    layout->setContentsMargins(160, 80, 160, 80);
    layout->addStretch();
    layout->addWidget(makeLabel(QStringLiteral("PIN 대체 로그인"), 40, true));
    layout->addWidget(makeLabel(QStringLiteral("얼굴 인식 실패 시 관리자 PIN을 입력합니다. 기본 PIN: 1234"), 20));
    m_pinEdit = new QLineEdit;
    m_pinEdit->setEchoMode(QLineEdit::Password);
    m_pinEdit->setAlignment(Qt::AlignCenter);
    m_pinEdit->setMaxLength(12);
    QFont font = m_pinEdit->font();
    font.setPointSize(28);
    m_pinEdit->setFont(font);
    m_pinEdit->setStyleSheet("background:#020617; color:white; border:2px solid #38bdf8; border-radius:12px; padding:12px;");
    connect(m_pinEdit, &QLineEdit::returnPressed, this, &ScenarioWindow::submitPin);
    layout->addWidget(m_pinEdit);
    QPushButton *submitButton = makeButton(QStringLiteral("확인"));
    connect(submitButton, &QPushButton::clicked, this, &ScenarioWindow::submitPin);
    layout->addWidget(submitButton);
    layout->addStretch();
    return screen;
}

QWidget *ScenarioWindow::makeReadyScreen()
{
    QWidget *screen = new QWidget;
    screen->setStyleSheet("background:#0f172a;");
    QVBoxLayout *layout = new QVBoxLayout(screen);
    layout->setContentsMargins(80, 60, 80, 60);
    layout->addStretch();
    m_readyTitle = makeLabel(QStringLiteral("인증 완료"), 44, true);
    m_readyDetail = makeLabel(QStringLiteral("전원 버튼을 한 번 더 누르면 시동 ON"), 28);
    layout->addWidget(m_readyTitle);
    layout->addWidget(m_readyDetail);
    layout->addStretch();
    return screen;
}

QWidget *ScenarioWindow::makeWarningScreen()
{
    QWidget *screen = new QWidget;
    screen->setStyleSheet("background:#3b0a12;");
    QVBoxLayout *layout = new QVBoxLayout(screen);
    layout->setContentsMargins(80, 60, 80, 60);
    layout->addStretch();
    m_warningTitle = makeLabel(QStringLiteral("잔류 아동 감지"), 46, true);
    m_warningDetail = makeLabel(QStringLiteral("1차 경고 출력 중"), 24);
    layout->addWidget(m_warningTitle);
    layout->addWidget(m_warningDetail);
    QPushButton *ackButton = makeButton(QStringLiteral("경고 해제"));
    ackButton->setStyleSheet(
        "QPushButton { background:#f8fafc; color:#7f1d1d; border:3px solid #fecaca; border-radius:16px; padding:16px; }"
        "QPushButton:pressed { background:#fee2e2; }");
    connect(ackButton, &QPushButton::clicked, this, &ScenarioWindow::alarmAckClicked);
    layout->addWidget(ackButton);
    layout->addStretch();
    return screen;
}

QWidget *ScenarioWindow::makeLockedScreen()
{
    QWidget *screen = new QWidget;
    screen->setStyleSheet("background:#111111;");
    QVBoxLayout *layout = new QVBoxLayout(screen);
    layout->addStretch();
    layout->addWidget(makeLabel(QStringLiteral("인증 차단"), 44, true));
    layout->addWidget(makeLabel(QStringLiteral("관리자에게 문의 하시오"), 30, true));
    layout->addStretch();
    return screen;
}

void ScenarioWindow::showBlackScreen(bool residualTransition)
{
    m_stack->setCurrentIndex(ScreenBlack);
    if (residualTransition) {
        m_blackLabel->setText(QStringLiteral("잔류감지 모드로 전환"));
        QTimer::singleShot(1800, this, &ScenarioWindow::resetToPureBlack);
    } else {
        resetToPureBlack();
    }
}

void ScenarioWindow::resetToPureBlack()
{
    if (m_stack->currentIndex() == ScreenBlack)
        m_blackLabel->clear();
}

void ScenarioWindow::showAuthScreen()
{
    if (m_authScreen)
        m_authScreen->resetStatus();
    m_stack->setCurrentIndex(ScreenAuth);
}

void ScenarioWindow::showPinScreen()
{
    m_pinFailCount = 0;
    if (m_pinEdit)
        m_pinEdit->clear();
    m_stack->setCurrentIndex(ScreenPin);
}

void ScenarioWindow::showReadyScreen(int driverId, int seatPosition, int temperatureTarget)
{
    m_lastDriverId = driverId;
    m_readyTitle->setText(QStringLiteral("운전자 인증 완료"));
    m_readyDetail->setText(QStringLiteral("Driver %1 / 좌석 %2 / 목표온도 %3℃\n전원 버튼을 한 번 더 누르면 시동 ON")
                               .arg(driverId)
                               .arg(seatPosition)
                               .arg(temperatureTarget));
    m_stack->setCurrentIndex(ScreenReady);
}

void ScenarioWindow::showOperationScreen()
{
    if (m_mainScreen)
        m_mainScreen->showOperationMode();
    m_stack->setCurrentIndex(ScreenMain);
}

void ScenarioWindow::showBoardingScreen()
{
    if (m_mainScreen)
        m_mainScreen->showBoardingMode();
    m_stack->setCurrentIndex(ScreenMain);
}

void ScenarioWindow::showChildWarning(int source, int level)
{
    m_warningTitle->setText(QStringLiteral("잔류 아동 감지"));
    m_warningDetail->setText(QStringLiteral("%1\n감지 경로: %2\n30초 이내 해제하지 않으면 텔레그램 알림")
                                 .arg(levelText(level))
                                 .arg(sourceText(source)));
    m_stack->setCurrentIndex(ScreenWarning);
}

void ScenarioWindow::showAuthFailScreen()
{
    showPinScreen();
}

void ScenarioWindow::showIgnitionBlocked(quint32 seatbeltBitmap)
{
    QMessageBox::warning(this,
                         QStringLiteral("시동 OFF 보류"),
                         QStringLiteral("아직 체결된 좌석이 있습니다. bitmap=0x%1")
                             .arg(seatbeltBitmap, 2, 16, QLatin1Char('0')).toUpper());
}

void ScenarioWindow::applyProfile(int driverId, int seatPosition, int temperatureTarget)
{
    m_lastDriverId = driverId;
    m_readyDetail->setText(QStringLiteral("개인화 적용: 좌석 %1 / 팬 목표 %2℃")
                               .arg(seatPosition)
                               .arg(temperatureTarget));
}

void ScenarioWindow::updateSeatbelt(quint32 currentBitmap, quint32 blueBitmap, quint32 redBitmap, quint32 grayBitmap)
{
    if (m_mainScreen)
        m_mainScreen->updateSeatbelt(currentBitmap, blueBitmap, redBitmap, grayBitmap);
}

void ScenarioWindow::updateEnv(int tempX10, int humidityX10, int luxX10, int valid)
{
    if (m_mainScreen)
        m_mainScreen->updateEnv(tempX10, humidityX10, luxX10, valid);
}

void ScenarioWindow::updateDust(int pm1, int pm25, int pm10, int valid)
{
    if (m_mainScreen)
        m_mainScreen->updateDust(pm1, pm25, pm10, valid);
}

void ScenarioWindow::updateFwStatus(int fanLevel, int cabinLed, int ignitionOperation, int pirValidAlert)
{
    if (m_mainScreen)
        m_mainScreen->updateFwStatus(fanLevel, cabinLed, ignitionOperation, pirValidAlert);
}

void ScenarioWindow::handleFaceResultSuccess(int driverId)
{
    if (m_authScreen)
        m_authScreen->showDriverRecognized(driverId);
}

void ScenarioWindow::handleFaceResultFail()
{
    if (m_authScreen)
        m_authScreen->showUnknownDriver();
    QTimer::singleShot(800, this, &ScenarioWindow::showPinScreen);
}

void ScenarioWindow::submitPin()
{
    const QString pin = m_pinEdit ? m_pinEdit->text().trimmed() : QString();
    if (pin == QStringLiteral("1234")) {
        m_pinFailCount = 0;
        emit faceSuccessTestRequested(1);
        return;
    }

    ++m_pinFailCount;
    if (m_pinEdit)
        m_pinEdit->clear();

    if (m_pinFailCount >= 3) {
        m_stack->setCurrentIndex(ScreenLocked);
    } else {
        QMessageBox::warning(this,
                             QStringLiteral("PIN 오류"),
                             QStringLiteral("PIN이 올바르지 않습니다. 남은 횟수: %1")
                                 .arg(3 - m_pinFailCount));
    }
}
