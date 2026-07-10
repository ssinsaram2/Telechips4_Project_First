#ifndef MAINWINDOWSCREEN_H
#define MAINWINDOWSCREEN_H

#include <QWidget>
#include <QVector>

class QLabel;

class MainWindowScreen : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindowScreen(QWidget *parent = nullptr);

public slots:
    void showOperationMode();
    void showBoardingMode();
    void updateSeatbelt(quint32 currentBitmap, quint32 blueBitmap, quint32 redBitmap, quint32 grayBitmap);
    void updateEnv(int tempX10, int humidityX10, int luxX10, int valid);
    void updateDust(int pm1, int pm25, int pm10, int valid);
    void updateFwStatus(int fanLevel, int cabinLed, int ignitionOperation, int pirValidAlert);

signals:
    void boardingButtonClicked();

private:
    QLabel *makeLabel(const QString &text, int pointSize, bool bold = false);
    class QPushButton *makeButton(const QString &text);
    void setSeatLabel(int index, const QString &state);
    void refreshSeatSummary();

    QLabel *m_mainTitle = nullptr;
    QLabel *m_seatSummary = nullptr;
    QLabel *m_envLabel = nullptr;
    QLabel *m_dustLabel = nullptr;
    QLabel *m_fwLabel = nullptr;
    QVector<QLabel *> m_seatLabels;

    quint32 m_currentBitmap = 0;
    quint32 m_blueBitmap = 0;
    quint32 m_redBitmap = 0;
    quint32 m_grayBitmap = 0x03;
};

#endif
