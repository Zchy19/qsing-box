#include "main_window.h"

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLocale>
#include <QMessageBox>
#include <QSharedMemory>
#include <QTimer>
#include <QTranslator>

#include <Windows.h>

#include "privilege_manager.h"
#include "settings_manager.h"

static const char* SINGLE_INSTANCE_SERVER_NAME = "qsing-box-single-instance";

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setQuitOnLastWindowClosed(false);
    QCoreApplication::setOrganizationName("NextIn");
    QCoreApplication::setApplicationName("qsing-box");

    // Check if another instance is already running
    QSharedMemory sharedMemory("qsing-box");
    if (!sharedMemory.create(1)) {
        // Another instance is running, try to activate it
        QLocalSocket socket;
        socket.connectToServer(SINGLE_INSTANCE_SERVER_NAME);
        if (socket.waitForConnected(500)) {
            socket.write("SHOW");
            socket.flush();
            socket.waitForBytesWritten(500);
        }
        // Exit silently without showing warning
        return 0;
    }

    PrivilegeManager privilegeManager;
    SettingsManager settingsManager;
    if (settingsManager.runAsAdmin() && !privilegeManager.isRunningAsAdmin()) {
        QString appPath = QCoreApplication::applicationFilePath();

        if (!privilegeManager.runAsAdmin(appPath)) {
            if (privilegeManager.getLastError() == ERROR_CANCELLED) {
                // Rejected UAC prompt
                qDebug() << "Administrator permissions were denied.\n";
            } else {
                // Other errors cause privilege elevation to fail
                qDebug() << "Failed to launch as administrator.\n";
            }
        } else {
            // Successfully started administrator mode
            // and exited the current instance
            return 0;
        }
    }

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "qsing-box_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }
    app.installTranslator(&translator);

    // Clean up any stale server socket from previous crash
    QLocalServer::removeServer(SINGLE_INSTANCE_SERVER_NAME);

    // Create local server to listen for activation requests from other instances
    QLocalServer server;
    if (!server.listen(SINGLE_INSTANCE_SERVER_NAME)) {
        qWarning() << "Failed to create single instance server:" << server.errorString();
    }

    MainWindow mainWindow;
    if (privilegeManager.isRunningAsAdmin()) {
        QString title = mainWindow.windowTitle() + QObject::tr(" (Administrator)");
        mainWindow.setWindowTitle(title);
    }

    // Handle activation requests from other instances
    QObject::connect(&server, &QLocalServer::newConnection, [&]() {
        QLocalSocket* client = server.nextPendingConnection();
        if (client) {
            if (client->waitForReadyRead(500)) {
                QByteArray data = client->readAll();
                if (data == "SHOW") {
                    // Restore window from minimized state and bring to front
                    mainWindow.show();
                    mainWindow.setWindowState(mainWindow.windowState() & ~Qt::WindowMinimized);
                    mainWindow.raise();
                    mainWindow.activateWindow();
                }
            }
            client->disconnectFromServer();
            delete client;
        }
    });

    bool isAutorun = false;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "/autorun") {
            isAutorun = true;
            break;
        }
    }
    if (isAutorun) {
        QTimer::singleShot(3000, &mainWindow, &MainWindow::startProxy);
    } else {
        mainWindow.show();
    }

    return app.exec();
}
