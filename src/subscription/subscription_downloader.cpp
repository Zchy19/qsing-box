#include "subscription_downloader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>
#include <QUrlQuery>

SubscriptionDownloader::SubscriptionDownloader(QObject *parent)
    : QObject{parent}
{
    m_networkManager = new QNetworkAccessManager(this);
}

QString SubscriptionDownloader::download(const QUrl &url, const QString &savePath,
                                          qint64 maxSize, int timeoutMs)
{
    qDebug() << "[SubscriptionDownloader] Download request:" << url.toString() << "->" << savePath;

    // Validate URL
    QString urlError;
    if (!validateUrl(url, urlError)) {
        qWarning() << "[SubscriptionDownloader] URL validation failed:" << urlError;
        QString downloadId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        // Emit finished signal asynchronously
        QMetaObject::invokeMethod(this, [this, downloadId, savePath, urlError]() {
            emit downloadFinished(downloadId, savePath, false, urlError);
        }, Qt::QueuedConnection);
        return downloadId;
    }

    // Validate save path
    if (!isPathSafe(savePath)) {
        qWarning() << "[SubscriptionDownloader] Invalid save path:" << savePath;
        QString downloadId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QMetaObject::invokeMethod(this, [this, downloadId, savePath]() {
            emit downloadFinished(downloadId, savePath, false, tr("Invalid save path"));
        }, Qt::QueuedConnection);
        return downloadId;
    }

    // Create download task
    QString downloadId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    DownloadTask task;
    task.id = downloadId;
    task.savePath = savePath;
    task.maxSize = maxSize;
    task.bytesReceived = 0;

    // Create network request
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", QString("%1/%2")
                         .arg(QCoreApplication::applicationName())
                         .arg(QCoreApplication::applicationVersion())
                         .toUtf8());

    task.reply = m_networkManager->get(request);

    // Setup timeout timer
    task.timeoutTimer = new QTimer(this);
    task.timeoutTimer->setSingleShot(true);
    task.timeoutTimer->setInterval(timeoutMs);

    connect(task.reply, &QNetworkReply::finished, this, &SubscriptionDownloader::onReplyFinished);
    connect(task.reply, &QNetworkReply::downloadProgress, this, &SubscriptionDownloader::onDownloadProgress);
    connect(task.timeoutTimer, &QTimer::timeout, this, &SubscriptionDownloader::onTimeout);

    m_activeDownloads[downloadId] = task;
    task.timeoutTimer->start();

    qDebug() << "[SubscriptionDownloader] Download started, ID:" << downloadId;
    return downloadId;
}

void SubscriptionDownloader::cancelDownload(const QString &downloadId)
{
    if (m_activeDownloads.contains(downloadId)) {
        DownloadTask &task = m_activeDownloads[downloadId];
        if (task.reply) {
            task.reply->abort();
        }
        if (task.timeoutTimer) {
            task.timeoutTimer->stop();
            task.timeoutTimer->deleteLater();
        }
        m_activeDownloads.remove(downloadId);
    }
}

void SubscriptionDownloader::cancelAllDownloads()
{
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it->reply) {
            it->reply->abort();
        }
        if (it->timeoutTimer) {
            it->timeoutTimer->stop();
            it->timeoutTimer->deleteLater();
        }
    }
    m_activeDownloads.clear();
}

bool SubscriptionDownloader::isValidSingBoxConfig(const QByteArray &data, QString &error)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        error = tr("JSON parse error: %1").arg(parseError.errorString());
        return false;
    }

    if (!doc.isObject()) {
        error = tr("Config must be a JSON object");
        return false;
    }

    QJsonObject root = doc.object();

    // Check for sing-box required fields
    // sing-box configs typically have: inbounds, outbounds, route, etc.
    bool hasInbounds = root.contains("inbounds") || root.contains("inbound");
    bool hasOutbounds = root.contains("outbounds") || root.contains("outbound");

    if (!hasInbounds && !hasOutbounds) {
        error = tr("Not a valid sing-box config: missing inbounds/outbounds");
        return false;
    }

    return true;
}

int SubscriptionDownloader::activeDownloadCount() const
{
    return m_activeDownloads.size();
}

void SubscriptionDownloader::onReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    // Find the download task
    QString downloadId;
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it->reply == reply) {
            downloadId = it.key();
            break;
        }
    }

    if (downloadId.isEmpty()) {
        reply->deleteLater();
        return;
    }

    DownloadTask task = m_activeDownloads.take(downloadId);

    // Stop and cleanup timer
    if (task.timeoutTimer) {
        task.timeoutTimer->stop();
        task.timeoutTimer->deleteLater();
    }

    QString savePath = task.savePath;
    bool success = false;
    QString error;

    // Check for network errors
    if (reply->error() != QNetworkReply::NoError) {
        error = reply->errorString();
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            error = tr("Download cancelled");
        } else if (reply->error() == QNetworkReply::TimeoutError) {
            error = tr("Connection timeout");
        } else if (reply->error() == QNetworkReply::HostNotFoundError) {
            error = tr("Host not found");
        }
        qWarning() << "[SubscriptionDownloader] Network error:" << error;
    } else {
        // Get response data
        QByteArray data = reply->readAll();
        qDebug() << "[SubscriptionDownloader] Received" << data.size() << "bytes";

        // Check size limit
        if (data.size() > task.maxSize) {
            error = tr("File too large (max %1 MB)").arg(task.maxSize / (1024 * 1024));
            qWarning() << "[SubscriptionDownloader] File too large";
        } else {
            // Validate JSON format
            QString validationError;
            if (isValidSingBoxConfig(data, validationError)) {
                // Save to file
                QFile file(savePath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(data);
                    file.close();
                    success = true;
                    qDebug() << "[SubscriptionDownloader] File saved to:" << savePath;
                } else {
                    error = tr("Failed to save file: %1").arg(file.errorString());
                    qWarning() << "[SubscriptionDownloader] Failed to save:" << error;
                }
            } else {
                error = validationError;
                qWarning() << "[SubscriptionDownloader] Invalid config:" << validationError;
            }
        }
    }

    reply->deleteLater();

    qDebug() << "[SubscriptionDownloader] Download finished, ID:" << downloadId << "success:" << success;
    emit downloadFinished(downloadId, savePath, success, error);
}

void SubscriptionDownloader::onDownloadProgress(qint64 received, qint64 total)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    // Find the download task
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it->reply == reply) {
            // Check size limit during download
            if (received > it->maxSize) {
                reply->abort();
                return;
            }
            it->bytesReceived = received;
            emit downloadProgress(it.key(), received, total);
            break;
        }
    }
}

void SubscriptionDownloader::onTimeout()
{
    QTimer *timer = qobject_cast<QTimer*>(sender());
    if (!timer) {
        return;
    }

    // Find the download task
    QString downloadId;
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it->timeoutTimer == timer) {
            downloadId = it.key();
            break;
        }
    }

    if (!downloadId.isEmpty()) {
        DownloadTask &task = m_activeDownloads[downloadId];
        if (task.reply) {
            task.reply->abort();
        }
    }
}

bool SubscriptionDownloader::validateUrl(const QUrl &url, QString &error) const
{
    // Only allow HTTP/HTTPS protocols
    QString scheme = url.scheme().toLower();
    if (scheme != "http" && scheme != "https") {
        error = tr("Only HTTP/HTTPS protocols are supported");
        return false;
    }

    // Must have valid host
    if (url.host().isEmpty()) {
        error = tr("Invalid host name");
        return false;
    }

    return true;
}

bool SubscriptionDownloader::isPathSafe(const QString &path) const
{
    // Prevent path traversal
    if (path.contains("..") || path.contains("\\")) {
        return false;
    }

    // Must be an absolute path or safe relative path
    // We only allow absolute paths in this implementation
    QFileInfo info(path);
    if (!info.isAbsolute()) {
        // Allow if it's a simple filename (no directory separators)
        if (path.contains("/") || path.contains("\\")) {
            return false;
        }
    }

    return true;
}

QString SubscriptionDownloader::sanitizeUrlForLog(const QString &url) const
{
    QUrl parsed(url);
    if (parsed.hasQuery()) {
        // Remove sensitive query parameters
        QUrlQuery query(parsed);
        query.removeAllQueryItems("token");
        query.removeAllQueryItems("key");
        query.removeAllQueryItems("secret");
        query.removeAllQueryItems("password");
        parsed.setQuery(query);
    }
    return parsed.toString(QUrl::RemoveUserInfo);
}
