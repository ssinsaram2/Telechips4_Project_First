#include "mainwindow.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>



namespace {

QString stateStyle(const QString &state)
{
    if (state == "BLUE")
        return "background:#12365a; border:2px solid #38bdf8; color:#e0f2fe;";
    if (state == "RED")
        return "background:#4a111b; border:2px solid #fb7185; color:#ffe4e6;";
    return "background:#242933; border:2px solid #475569; color:#cbd5e1;";
}

int x10ToInt(int value)
{
    return value >= 0 ? (value + 5) / 10 : (value - 5) / 10;
}

}

MainWindowScreen::MainWindowScreen(QWidget *parent)
    : QWidget(parent)
{
    /*
     * OPERATIONAL/PASSENGER_TRANSFER 메인 화면 전용 위젯.
     *
     * Central이 IPC_MSG_SHOW_OPERATION_SCREEN 또는 IPC_MSG_SHOW_BOARDING_SCREEN을 보내면
     * ScenarioWindow가 이 화면을 보여주고 모드 제목만 바꾼다.
     * 좌석 색상과 센서값은 Central이 CAN 값을 해석한 뒤 IPC로 전달한 결과를 표시한다.
     */
    setStyleSheet("background:#0b1120;");

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(36, 28, 36, 28);
    root->setSpacing(18);

    m_mainTitle = makeLabel(QStringLiteral("주행중"), 34, true);
    root->addWidget(m_mainTitle);

    QGridLayout *seatGrid = new QGridLayout;
    seatGrid->setSpacing(14);
    for (int i = 0; i < 8; ++i) {
        QLabel *seat = makeLabel(QStringLiteral("%1번\n미탑승").arg(i + 1), 22, true);
        seat->setMinimumSize(180, 110);
        seat->setStyleSheet(stateStyle("GREY") + " border-radius:16px;");
        m_seatLabels.append(seat);
        seatGrid->addWidget(seat, i / 4, i % 4);
    }
    root->addLayout(seatGrid, 1);

    m_seatSummary = makeLabel(QStringLiteral("좌석: 전원 ON 직후 전체 회색 / 승하차 모드에서 체결 시 파란색"), 18);
    root->addWidget(m_seatSummary);

    QHBoxLayout *sensorRow = new QHBoxLayout;
    m_envLabel = makeLabel(QStringLiteral("온습도/조도: 수신 대기"), 16);
    m_dustLabel = makeLabel(QStringLiteral("미세먼지: 수신 대기"), 16);
    m_fwLabel = makeLabel(QStringLiteral("F/W 상태: 수신 대기"), 16);
    sensorRow->addWidget(m_envLabel);
    sensorRow->addWidget(m_dustLabel);
    sensorRow->addWidget(m_fwLabel);
    root->addLayout(sensorRow);

    QPushButton *boardingButton = makeButton(QStringLiteral("승하차"));
    connect(boardingButton, &QPushButton::clicked, this, &MainWindowScreen::boardingButtonClicked);
    root->addWidget(boardingButton);
}

void MainWindowScreen::showOperationMode()
{
    if (m_mainTitle)
        m_mainTitle->setText(QStringLiteral("주행중"));
}

void MainWindowScreen::showBoardingMode()
{
    if (m_mainTitle)
        m_mainTitle->setText(QStringLiteral("승하차 중"));
}

void MainWindowScreen::updateSeatbelt(quint32 currentBitmap, quint32 blueBitmap, quint32 redBitmap, quint32 grayBitmap)
{
    m_currentBitmap = currentBitmap;
    m_blueBitmap = blueBitmap;
    m_redBitmap = redBitmap;
    m_grayBitmap = grayBitmap;

    for (int i = 0; i < m_seatLabels.size(); ++i) {
        const quint32 bit = 1U << i;
        if (blueBitmap & bit)
            setSeatLabel(i, "BLUE");
        else if (redBitmap & bit)
            setSeatLabel(i, "RED");
        else
            setSeatLabel(i, "GREY");
    }
    refreshSeatSummary();
}

void MainWindowScreen::updateEnv(int tempX10, int humidityX10, int luxX10, int valid)
{
    if (!m_envLabel)
        return;

    m_envLabel->setText(QStringLiteral("실내 %1℃ / 습도 %2% / 조도 %3 lux / valid=0x%4")
                            .arg(x10ToInt(tempX10))
                            .arg(x10ToInt(humidityX10))
                            .arg(x10ToInt(luxX10))
                            .arg(valid, 2, 16, QLatin1Char('0')).toUpper());
}

void MainWindowScreen::updateDust(int pm1, int pm25, int pm10, int valid)
{
    if (!m_dustLabel)
        return;

    m_dustLabel->setText(QStringLiteral("PM1.0 %1 / PM2.5 %2 / PM10 %3 / valid=0x%4")
                             .arg(pm1)
                             .arg(pm25)
                             .arg(pm10)
                             .arg(valid, 2, 16, QLatin1Char('0')).toUpper());
}

void MainWindowScreen::updateFwStatus(int fanLevel, int cabinLed, int ignitionOperation, int pirValidAlert)
{
    if (!m_fwLabel)
        return;

    m_fwLabel->setText(QStringLiteral("팬 %1 / 내부등 %2 / 상태 0x%3 / PIR-valid-alert 0x%4")
                           .arg(fanLevel)
                           .arg(cabinLed)
                           .arg(ignitionOperation, 4, 16, QLatin1Char('0')).toUpper()
                           .arg(pirValidAlert, 6, 16, QLatin1Char('0')).toUpper());
}

QLabel *MainWindowScreen::makeLabel(const QString &text, int pointSize, bool bold)
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

QPushButton *MainWindowScreen::makeButton(const QString &text)
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

void MainWindowScreen::setSeatLabel(int index, const QString &state)
{
    if (index < 0 || index >= m_seatLabels.size())
        return;

    QString text;
    if (state == "BLUE")
        text = QStringLiteral("%1번\n착석 / 벨트 착용").arg(index + 1);
    else if (state == "RED")
        text = QStringLiteral("%1번\n착석 / 벨트 미착용").arg(index + 1);
    else
        text = QStringLiteral("%1번\n미탑승").arg(index + 1);

    m_seatLabels[index]->setText(text);
    m_seatLabels[index]->setStyleSheet(stateStyle(state) + " border-radius:16px;");
}

void MainWindowScreen::refreshSeatSummary()
{
    if (!m_seatSummary)
        return;

    int blue = 0;
    int red = 0;
    for (int i = 0; i < 8; ++i) {
        if (m_blueBitmap & (1U << i))
            ++blue;
        if (m_redBitmap & (1U << i))
            ++red;
    }
    m_seatSummary->setText(QStringLiteral("현재 bitmap=0x%1 / 파란좌석 %2 / 빨간좌석 %3")
                               .arg(m_currentBitmap, 2, 16, QLatin1Char('0')).toUpper()
                               .arg(blue)
                               .arg(red));
}
