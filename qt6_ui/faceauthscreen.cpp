#include "faceauthscreen.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

FaceAuthScreen::FaceAuthScreen(QWidget *parent)
    : QWidget(parent)
{
    /*
     * 얼굴 인증 화면 전용 위젯.
     *
     * Central이 IPC_MSG_SHOW_AUTH_SCREEN을 보내면 ScenarioWindow가 이 화면을 보여준다.
     * 실제 얼굴 인식 결과는 CentralClient가 UDP 5000번에서 받고,
     * 성공/실패 결과만 이 화면의 상태 문구로 반영한다.
     */
    setStyleSheet("background:#101827;");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(80, 60, 80, 60);
    layout->addStretch();
    layout->addWidget(makeLabel(QStringLiteral("운전자 얼굴 인식 중"), 44, true));

    m_statusLabel = makeLabel(QStringLiteral("카메라를 바라봐 주세요"), 24);
    layout->addWidget(m_statusLabel);

    QHBoxLayout *testRow = new QHBoxLayout;
    QPushButton *successButton = makeButton(QStringLiteral("테스트: 등록자 1"));
    QPushButton *failButton = makeButton(QStringLiteral("테스트: 미등록"));
    connect(successButton, &QPushButton::clicked, this, [this]() {
        emit faceSuccessTestRequested(1);
    });
    connect(failButton, &QPushButton::clicked, this, &FaceAuthScreen::faceFailTestRequested);
    testRow->addWidget(successButton);
    testRow->addWidget(failButton);
    layout->addLayout(testRow);

    layout->addStretch();
}

void FaceAuthScreen::resetStatus()
{
    if (m_statusLabel)
        m_statusLabel->setText(QStringLiteral("카메라를 바라봐 주세요"));
}

void FaceAuthScreen::showDriverRecognized(int driverId)
{
    if (m_statusLabel)
        m_statusLabel->setText(QStringLiteral("등록 운전자 인식: %1").arg(driverId));
}

void FaceAuthScreen::showUnknownDriver()
{
    if (m_statusLabel)
        m_statusLabel->setText(QStringLiteral("미등록자입니다. PIN 화면으로 전환합니다."));
}

QLabel *FaceAuthScreen::makeLabel(const QString &text, int pointSize, bool bold)
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

QPushButton *FaceAuthScreen::makeButton(const QString &text)
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
