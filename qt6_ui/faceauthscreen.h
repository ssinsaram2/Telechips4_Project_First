#ifndef FACEAUTHSCREEN_H
#define FACEAUTHSCREEN_H

#include <QWidget>

class QLabel;
class QPushButton;      // 누름 버튼

class FaceAuthScreen : public QWidget
{
    Q_OBJECT

public:
    explicit FaceAuthScreen(QWidget *parent = nullptr);

public slots:
    void resetStatus();
    void showDriverRecognized(int driverId);
    void showUnknownDriver();
    void showPinEntryPrompt();          // 핀 입력 다이얼 프롬프트 나타내는 함수

signals:
    void faceSuccessTestRequested(int driverId);
    void faceFailTestRequested();
    void pinEntryRequested();           // 핀 다이얼이 요청되었을 때를 처리하는 함수

private:
    QLabel *makeLabel(const QString &text, int pointSize, bool bold = false);
    QPushButton *makeButton(const QString &text);

    QLabel *m_statusLabel = nullptr;
    QPushButton *m_pinEntryButton = nullptr;            // 우선은 nullptr로 초기화
};

#endif
