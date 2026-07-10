#include "centralclient.h"
#include "scenariowindow.h"

#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QFont>
#include <QProcess>
#include <QTimer>

static void startCentralIfRequested()
{
    if (qEnvironmentVariable("UI_START_CENTRAL", "1") == "0")
        return;

#ifdef Q_OS_LINUX
    QProcess check;
    check.start(QStringLiteral("pidof"), {QStringLiteral("central_app")});
    if (check.waitForFinished(500) && check.exitCode() == 0)
        return;

    QString path = qEnvironmentVariable("CENTRAL_APP_PATH").trimmed();
    if (path.isEmpty())
        path = QStringLiteral("/opt/central/central_app");

    if (!QProcess::startDetached(path, QStringList(), QFileInfo(path).absolutePath())) {
        qWarning("[UI] central_app start failed. Set CENTRAL_APP_PATH or start it manually.");
    }
#endif
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QFont font = app.font();
    font.setPointSize(13);
    app.setFont(font);

    startCentralIfRequested();

    CentralClient client;
    ScenarioWindow window;

    QObject::connect(&client, &CentralClient::authScreenRequested,
                     &window, &ScenarioWindow::showAuthScreen);
    QObject::connect(&client, &CentralClient::authFailRequested,
                     &window, &ScenarioWindow::showAuthFailScreen);
    QObject::connect(&client, &CentralClient::readyScreenRequested,
                     &window, &ScenarioWindow::showReadyScreen);
    QObject::connect(&client, &CentralClient::operationScreenRequested,
                     &window, &ScenarioWindow::showOperationScreen);
    QObject::connect(&client, &CentralClient::boardingScreenRequested,
                     &window, &ScenarioWindow::showBoardingScreen);
    QObject::connect(&client, &CentralClient::screenOffRequested,
                     &window, &ScenarioWindow::showBlackScreen);
    QObject::connect(&client, &CentralClient::profileApplied,
                     &window, &ScenarioWindow::applyProfile);
    QObject::connect(&client, &CentralClient::seatbeltUiChanged,
                     &window, &ScenarioWindow::updateSeatbelt);
    QObject::connect(&client, &CentralClient::envChanged,
                     &window, &ScenarioWindow::updateEnv);
    QObject::connect(&client, &CentralClient::dustChanged,
                     &window, &ScenarioWindow::updateDust);
    QObject::connect(&client, &CentralClient::fwStatusChanged,
                     &window, &ScenarioWindow::updateFwStatus);
    QObject::connect(&client, &CentralClient::childWarningRequested,
                     &window, &ScenarioWindow::showChildWarning);
    QObject::connect(&client, &CentralClient::ignitionBlocked,
                     &window, &ScenarioWindow::showIgnitionBlocked);
    QObject::connect(&client, &CentralClient::faceResultReceived,
                     &window, &ScenarioWindow::handleFaceResultSuccess);
    QObject::connect(&client, &CentralClient::faceResultFailed,
                     &window, &ScenarioWindow::handleFaceResultFail);

    QObject::connect(&window, &ScenarioWindow::boardingButtonClicked,
                     &client, &CentralClient::sendBoardingButton);
    QObject::connect(&window, &ScenarioWindow::alarmAckClicked,
                     &client, &CentralClient::sendAlarmAck);
    QObject::connect(&window, &ScenarioWindow::faceSuccessTestRequested,
                     &client, &CentralClient::sendFaceSuccess);
    QObject::connect(&window, &ScenarioWindow::faceFailTestRequested,
                     &client, &CentralClient::sendFaceFail);

    client.initialize();
    window.showFullScreen();
    window.showBlackScreen(false);

    return app.exec();
}
