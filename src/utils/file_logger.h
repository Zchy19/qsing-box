#ifndef FILE_LOGGER_H
#define FILE_LOGGER_H

#include <QString>

class FileLogger
{
public:
    static void install(const QString &logFilePath);
    static void uninstall();

private:
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    static QString s_logFilePath;
    static QtMessageHandler s_originalHandler;
};

#endif // FILE_LOGGER_H
