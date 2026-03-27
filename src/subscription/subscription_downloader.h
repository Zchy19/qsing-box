#ifndef SUBSCRIPTION_DOWNLOADER_H
#define SUBSCRIPTION_DOWNLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QMap>
#include <QTimer>

class QNetworkReply;

class SubscriptionDownloader : public QObject
{
    Q_OBJECT

public:
    // Download config constants
    static constexpr qint64 DEFAULT_MAX_SIZE = 10 * 1024 * 1024;  // 10MB
    static constexpr int DEFAULT_TIMEOUT = 30000;  // 30 seconds

    explicit SubscriptionDownloader(QObject *parent = nullptr);

    // Download subscription config, returns download task ID
    QString download(const QUrl &url, const QString &savePath,
                     qint64 maxSize = DEFAULT_MAX_SIZE,
                     int timeoutMs = DEFAULT_TIMEOUT);

    // Cancel specified download task
    void cancelDownload(const QString &downloadId);

    // Cancel all download tasks
    void cancelAllDownloads();

    // Validate JSON format
    static bool isValidSingBoxConfig(const QByteArray &data, QString &error);

    // Get current active download count
    int activeDownloadCount() const;

signals:
    void downloadProgress(const QString &downloadId, qint64 received, qint64 total);
    void downloadFinished(const QString &downloadId, const QString &savePath,
                          bool success, const QString &error);

private slots:
    void onReplyFinished();
    void onDownloadProgress(qint64 received, qint64 total);
    void onTimeout();

private:
    bool validateUrl(const QUrl &url, QString &error) const;
    bool isPathSafe(const QString &path) const;  // Prevent path traversal
    QString sanitizeUrlForLog(const QString &url) const;  // URL sanitization for logs

    struct DownloadTask {
        QString id;
        QString savePath;
        QNetworkReply *reply;
        qint64 maxSize;
        qint64 bytesReceived = 0;
        QTimer *timeoutTimer;
        QByteArray data;
    };

    QNetworkAccessManager *m_networkManager;
    QMap<QString, DownloadTask> m_activeDownloads;
};

#endif // SUBSCRIPTION_DOWNLOADER_H
