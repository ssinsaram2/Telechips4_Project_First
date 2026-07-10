#ifndef FACEAUTHSCREEN_H
#define FACEAUTHSCREEN_H

#include <QWidget>

class QLabel;

class FaceAuthScreen : public QWidget
{
    Q_OBJECT

public:
    explicit FaceAuthScreen(QWidget *parent = nullptr);

public slots:
    void resetStatus();
    void showDriverRecognized(int driverId);
    void showUnknownDriver();

signals:
    void faceSuccessTestRequested(int driverId);
    void faceFailTestRequested();

private:
    QLabel *makeLabel(const QString &text, int pointSize, bool bold = false);
    class QPushButton *makeButton(const QString &text);

    QLabel *m_statusLabel = nullptr;
};

#endif
