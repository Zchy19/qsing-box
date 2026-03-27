#include "file_logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QDir>

QString FileLogger::s_logFilePath;
QtMessageHandler FileLogger::s_originalHandler = nullptr;

void FileLogger::install(const QString &logFilePath)
{
    s_logFilePath = logFilePath;

    // Ensure directory exists
    QDir dir = QFileInfo(logFilePath).absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Clear previous log
    QFile file(logFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream out(&file);
        out << "=== qsing-box Log Started at " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << " ===\n";
        out << "=== Application: " << QCoreApplication::applicationName() << " v" << QCoreApplication::applicationVersion() << " ===\n\n";
        file.close();
    }

    // Install message handler
    s_originalHandler = qInstallMessageHandler(messageHandler);
}

void FileLogger::uninstall()
{
    if (s_originalHandler) {
        qInstallMessageHandler(s_originalHandler);
        s_originalHandler = nullptr;
    }
}

void FileLogger::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Write to file
    QFile file(s_logFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);

        QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
        QString typeStr;

        switch (type) {
        case QtDebugMsg:
            typeStr = "DEBUG";
            break;
        case QtInfoMsg:
            typeStr = "INFO";
            break;
        case QtWarningMsg:
            typeStr = "WARN";
            break;
        case QtCriticalMsg:
            typeStr = "CRIT";
            break;
        case QtFatalMsg:
            typeStr = "FATAL";
            break;
        }

        // Format: [timestamp] [TYPE] message
        out << "[" << timestamp << "] [" << typeStr << "] " << msg << "\n";
        file.close();
    }

    // Also call original handler (output to console)
    if (s_originalHandler) {
        s_originalHandler(type, context, msg);
    }
}
