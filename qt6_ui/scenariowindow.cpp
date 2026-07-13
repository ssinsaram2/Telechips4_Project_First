#include "scenariowindow.h"

#include "faceauthscreen.h"
#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRandomGenerator>
#include <QStackedWidget>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
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

QString unquoteValue(const QString &value)
{
    if (value.size() >= 2) {
        const QChar first = value.front();
        const QChar last = value.back();
        if ((first == QLatin1Char('"') && last == QLatin1Char('"')) ||
            (first == QLatin1Char('\'') && last == QLatin1Char('\''))) {
            return value.mid(1, value.size() - 2);
        }
    }
    return value;
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
    m_telegramNetwork = new QNetworkAccessManager(this);
    m_authTimeoutTimer = new QTimer(this);
    m_authTimeoutTimer->setSingleShot(true);
    m_authTimeoutTimer->setInterval(10000);         // 10초 간격

    connect(m_authScreen, &FaceAuthScreen::faceSuccessTestRequested,
            this, &ScenarioWindow::faceSuccessTestRequested);
    connect(m_authScreen, &FaceAuthScreen::faceFailTestRequested,
            this, &ScenarioWindow::faceFailTestRequested);
    connect(m_authScreen, &FaceAuthScreen::pinEntryRequested,
            this, &ScenarioWindow::showPinScreen);
    connect(m_mainScreen, &MainWindowScreen::boardingButtonClicked,
            this, &ScenarioWindow::boardingButtonClicked);
    connect(m_authTimeoutTimer, &QTimer::timeout,
            this, &ScenarioWindow::handleAuthTimeout);

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

QPushButton *ScenarioWindow::makePinDialButton(const QString &text)
{
    QPushButton *button = new QPushButton(text);
    QFont font = button->font();
    font.setPointSize(24);
    font.setBold(true);
    button->setFont(font);
    button->setMinimumHeight(76);
    button->setStyleSheet(
        "QPushButton { background:#1f2937; color:#f8fafc; border:2px solid #64748b; border-radius:12px; padding:10px; }"
        "QPushButton:pressed { background:#334155; }");
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
    layout->setContentsMargins(200, 52, 200, 52);
    layout->addStretch();
    layout->addWidget(makeLabel(QStringLiteral("PIN 대체 로그인"), 40, true));
    layout->addWidget(makeLabel(QStringLiteral("텔레그램으로 받은 6자리 PIN을 터치 다이얼로 입력합니다."), 20));
    m_pinEdit = new QLineEdit;
    m_pinEdit->setEchoMode(QLineEdit::Password);
    m_pinEdit->setAlignment(Qt::AlignCenter);
    m_pinEdit->setMaxLength(6);
    m_pinEdit->setReadOnly(true);
    m_pinEdit->setFocusPolicy(Qt::NoFocus);
    m_pinEdit->setInputMethodHints(Qt::ImhDigitsOnly | Qt::ImhNoPredictiveText);
    QFont font = m_pinEdit->font();
    font.setPointSize(28);
    m_pinEdit->setFont(font);
    m_pinEdit->setStyleSheet("background:#020617; color:white; border:2px solid #38bdf8; border-radius:12px; padding:12px;");
    layout->addWidget(m_pinEdit);

    m_pinStatusLabel = makeLabel(QStringLiteral("PIN 발급 대기"), 16);
    layout->addWidget(m_pinStatusLabel);

    QGridLayout *dialLayout = new QGridLayout;
    dialLayout->setSpacing(12);
    const QStringList digits = {
        QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
        QStringLiteral("4"), QStringLiteral("5"), QStringLiteral("6"),
        QStringLiteral("7"), QStringLiteral("8"), QStringLiteral("9")
    };

    for (int i = 0; i < digits.size(); ++i) {
        const QString digit = digits.at(i);
        QPushButton *digitButton = makePinDialButton(digit);
        connect(digitButton, &QPushButton::clicked, this, [this, digit]() {
            appendPinDigit(digit);
        });
        dialLayout->addWidget(digitButton, i / 3, i % 3);
    }

    QPushButton *eraseButton = makePinDialButton(QStringLiteral("지우기"));
    connect(eraseButton, &QPushButton::clicked, this, &ScenarioWindow::erasePinDigit);
    dialLayout->addWidget(eraseButton, 3, 0);

    QPushButton *zeroButton = makePinDialButton(QStringLiteral("0"));
    connect(zeroButton, &QPushButton::clicked, this, [this]() {
        appendPinDigit(QStringLiteral("0"));
    });
    dialLayout->addWidget(zeroButton, 3, 1);

    QPushButton *submitButton = makePinDialButton(QStringLiteral("확인"));
    submitButton->setStyleSheet(
        "QPushButton { background:#155e75; color:white; border:2px solid #67e8f9; border-radius:12px; padding:10px; }"
        "QPushButton:pressed { background:#0e7490; }");
    connect(submitButton, &QPushButton::clicked, this, &ScenarioWindow::submitPin);
    dialLayout->addWidget(submitButton, 3, 2);

    layout->addLayout(dialLayout);
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
    /* [MOD] 버튼 의미 명확화: 감지 기능 OFF가 아니라 "확인 완료" 입력임을 표현 */
    QPushButton *ackButton = makeButton(QStringLiteral("차량 확인 완료"));
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
    stopAuthTimeout();
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
    startAuthTimeout();
}

void ScenarioWindow::showPinScreen()
{
    stopAuthTimeout();
    m_pinFailCount = 0;
    if (m_pinEdit)
        m_pinEdit->clear();
    m_stack->setCurrentIndex(ScreenPin);
    qInfo() << "[pin] PIN screen shown; issuing Telegram PIN";
    issuePinCode();
}

void ScenarioWindow::showReadyScreen(int driverId, int seatPosition, int temperatureTarget)
{
    stopAuthTimeout();
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
    stopAuthTimeout();
    if (m_mainScreen)
        m_mainScreen->showOperationMode();
    m_stack->setCurrentIndex(ScreenMain);
}

void ScenarioWindow::showBoardingScreen()
{
    stopAuthTimeout();
    if (m_mainScreen)
        m_mainScreen->showBoardingMode();
    m_stack->setCurrentIndex(ScreenMain);
}

void ScenarioWindow::showChildWarning(int source, int level)
{
    stopAuthTimeout();
    m_warningTitle->setText(QStringLiteral("잔류 아동 감지"));
    m_warningDetail->setText(QStringLiteral("%1\n감지 경로: %2\n30초 이내 해제하지 않으면 텔레그램 알림")
                                 .arg(levelText(level))
                                 .arg(sourceText(source)));
    m_stack->setCurrentIndex(ScreenWarning);

    // 경고 화면이 실제로 표시되는 시점이 1차 잔류 아동 경고 진입점이다.
    // 2차 알림은 Central의 CHILD_ALERT_LEVEL2_CMD 흐름이 담당하므로 여기서는 1차 경고만 전송한다.
    if (level <= 1)
        sendChildWarningToTelegram(source);
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
    stopAuthTimeout();
    if (m_authScreen)
        m_authScreen->showDriverRecognized(driverId);
}

void ScenarioWindow::handleFaceResultFail()
{
    stopAuthTimeout();
    if (m_authScreen)
        m_authScreen->showUnknownDriver();
    QTimer::singleShot(800, this, &ScenarioWindow::showPinScreen);
}

void ScenarioWindow::appendPinDigit(const QString &digit)
{
    if (!m_pinEdit || m_pinEdit->text().size() >= m_pinEdit->maxLength())
        return;

    m_pinEdit->setText(m_pinEdit->text() + digit);
    qInfo() << "[pin] digit entered; length=" << m_pinEdit->text().size();
}

QString ScenarioWindow::generatePinCode() const
{
    const quint32 value = QRandomGenerator::global()->bounded(1000000U);
    return QStringLiteral("%1").arg(value, 6, 10, QLatin1Char('0'));
}

void ScenarioWindow::issuePinCode()
{
    m_activePinCode = generatePinCode();
    setPinStatus(QStringLiteral("텔레그램으로 6자리 PIN을 전송 중입니다."));
    qInfo() << "[pin] generated new 6-digit PIN";
    sendPinCodeToTelegram(m_activePinCode);
}

bool ScenarioWindow::loadTelegramConfig(QString *groupId, QString *apiToken) const
{
    const QString configuredPath = QString::fromLocal8Bit(qgetenv("TELEGRAM_CONFIG")).trimmed();
    QStringList candidates;
    if (!configuredPath.isEmpty())
        candidates.append(configuredPath);
    candidates.append(QDir::current().filePath(QStringLiteral("telegram.txt")));
    candidates.append(QCoreApplication::applicationDirPath() + QStringLiteral("/telegram.txt"));
    candidates.append(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../telegram.txt")));
    candidates.append(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../telegram.txt")));

    for (const QString &path : candidates) {
        QFile file(path);
        qInfo() << "[telegram] trying config:" << path;
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QString loadedGroupId;
        QString loadedApiToken;
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            const QString line = stream.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;

            const int separator = line.indexOf(QLatin1Char('='));
            if (separator < 0)
                continue;

            const QString key = line.left(separator).trimmed();
            const QString value = unquoteValue(line.mid(separator + 1).trimmed());
            if (key == QStringLiteral("GROUP_ID")) {
                loadedGroupId = value;
            } else if (key == QStringLiteral("API_TOKEN")) {
                loadedApiToken = value;
            }
        }

        if (!loadedGroupId.isEmpty() && !loadedApiToken.isEmpty()) {
            if (groupId)
                *groupId = loadedGroupId;
            if (apiToken)
                *apiToken = loadedApiToken;
            qInfo() << "[telegram] config loaded from" << path
                    << "group_id_length=" << loadedGroupId.size()
                    << "token_length=" << loadedApiToken.size();
            return true;
        }

        qWarning() << "[telegram] config missing GROUP_ID or API_TOKEN in" << path;
    }

    qWarning() << "[telegram] config not found";
    return false;
}

void ScenarioWindow::sendTelegramMessage(const QString &messageText,
                                         const QString &logPrefix,
                                         const QString &configErrorStatus,
                                         const QString &networkErrorStatus,
                                         const QString &successStatus,
                                         const QString &failurePrefix)
{
    QString groupId;
    QString apiToken;
    if (!loadTelegramConfig(&groupId, &apiToken)) {
        if (!configErrorStatus.isEmpty())
            setPinStatus(configErrorStatus);
        return;
    }

    if (!m_telegramNetwork) {
        qWarning() << logPrefix << "Telegram network manager is not initialized";
        if (!networkErrorStatus.isEmpty())
            setPinStatus(networkErrorStatus);
        return;
    }

    // telegram.txt에서 읽은 GROUP_ID/API_TOKEN으로 Telegram Bot API sendMessage 요청을 만든다.
    // 전송 실패가 경고 화면 표시를 막지 않도록, 네트워크 결과는 로그와 선택적 상태 문구로만 처리한다.
    QUrl url(QStringLiteral("https://api.telegram.org/bot%1/sendMessage").arg(apiToken));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("chat_id"), groupId);
    body.addQueryItem(QStringLiteral("text"), messageText);

    qInfo() << logPrefix << "sending Telegram message";
    QNetworkReply *reply = m_telegramNetwork->post(request,
                                                   body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, logPrefix, successStatus, failurePrefix]() {
        const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (reply->error() == QNetworkReply::NoError) {
            qInfo() << logPrefix << "Telegram message sent; http_status=" << status;
            if (!successStatus.isEmpty())
                setPinStatus(successStatus);
        } else {
            qWarning() << logPrefix << "Telegram message failed; http_status=" << status
                       << "error=" << reply->errorString();
            if (!failurePrefix.isEmpty())
                setPinStatus(QStringLiteral("%1%2").arg(failurePrefix, reply->errorString()));
        }
        reply->deleteLater();
    });
}

void ScenarioWindow::sendPinCodeToTelegram(const QString &pinCode)
{
    sendTelegramMessage(QStringLiteral("통학차량 PIN 로그인 인증번호: %1").arg(pinCode),
                        QStringLiteral("[pin]"),
                        QStringLiteral("telegram.txt에서 GROUP_ID/API_TOKEN을 읽지 못했습니다."),
                        QStringLiteral("텔레그램 전송 준비가 되지 않았습니다."),
                        QStringLiteral("텔레그램으로 PIN을 전송했습니다."),
                        QStringLiteral("텔레그램 전송 실패: "));
}

void ScenarioWindow::sendChildWarningToTelegram(int source)
{
    sendTelegramMessage(QStringLiteral("잔류 아동이 감지되었습니다! 차량 내부를 확인하십시오!\n감지 경로: %1")
                            .arg(sourceText(source)),
                        QStringLiteral("[child-alert]"));
}

void ScenarioWindow::setPinStatus(const QString &text)
{
    if (m_pinStatusLabel)
        m_pinStatusLabel->setText(text);
}

void ScenarioWindow::erasePinDigit()
{
    if (!m_pinEdit)
        return;

    QString pin = m_pinEdit->text();
    if (!pin.isEmpty()) {
        pin.chop(1);
        m_pinEdit->setText(pin);
        qInfo() << "[pin] digit erased; length=" << pin.size();
    }
}

void ScenarioWindow::handleAuthTimeout()
{
    if (m_stack->currentIndex() == ScreenAuth && m_authScreen)
        m_authScreen->showPinEntryPrompt();
}

void ScenarioWindow::startAuthTimeout()
{
    if (m_authTimeoutTimer)
        m_authTimeoutTimer->start();
}

void ScenarioWindow::stopAuthTimeout()
{
    if (m_authTimeoutTimer)
        m_authTimeoutTimer->stop();
}

void ScenarioWindow::submitPin()
{
    const QString pin = m_pinEdit ? m_pinEdit->text().trimmed() : QString();
    const int requiredLength = m_pinEdit ? m_pinEdit->maxLength() : 6;

    if (m_activePinCode.isEmpty()) {
        qWarning() << "[pin] submit ignored; no active PIN";
        setPinStatus(QStringLiteral("PIN이 아직 발급되지 않았습니다."));
        return;
    }

    if (pin.size() < requiredLength) {
        qWarning() << "[pin] submit ignored; incomplete PIN length=" << pin.size();
        setPinStatus(QStringLiteral("6자리 PIN을 모두 입력하세요."));
        return;
    }

    if (pin == m_activePinCode) {
        qInfo() << "[pin] PIN accepted";
        m_activePinCode.clear();
        m_pinFailCount = 0;
        // PIN 인증은 얼굴 인증과 구분해서 Central로 보낸다.
        // Central은 이 신호를 받은 뒤에만 driver_profile.json 개인화를 적용한다.
        emit pinAuthSucceeded(1);
        return;
    }

    ++m_pinFailCount;
    qWarning() << "[pin] PIN rejected; fail_count=" << m_pinFailCount;
    if (m_pinEdit)
        m_pinEdit->clear();

    if (m_pinFailCount >= 3) {
        qWarning() << "[pin] PIN locked after too many failures";
        m_activePinCode.clear();
        m_stack->setCurrentIndex(ScreenLocked);
    } else {
        setPinStatus(QStringLiteral("PIN이 올바르지 않습니다. 남은 횟수: %1")
                         .arg(3 - m_pinFailCount));
    }
}
