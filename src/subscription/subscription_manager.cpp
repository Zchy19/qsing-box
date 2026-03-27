#include "subscription_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

SubscriptionManager::SubscriptionManager(QObject *parent)
    : QObject{parent}
{
    m_downloader = new SubscriptionDownloader(this);
    connect(m_downloader, &SubscriptionDownloader::downloadFinished,
            this, &SubscriptionManager::onDownloadFinished);

    ensureProfilesDirExists();
    loadFromJson();

    qDebug() << "[SubscriptionManager] Initialized with" << m_subscriptions.size() << "subscriptions";
    qDebug() << "[SubscriptionManager] Profiles dir:" << profilesDir();
    qDebug() << "[SubscriptionManager] Subscriptions file:" << subscriptionsFilePath();
}

SubscriptionManager::~SubscriptionManager()
{
}

QString SubscriptionManager::profilesDir()
{
    QString localPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString appDataPath = QString("%1/%2").arg(localPath).arg(QCoreApplication::applicationName());
    return QString("%1/profiles").arg(appDataPath);
}

QString SubscriptionManager::subscriptionsFilePath()
{
    QString localPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString appDataPath = QString("%1/%2").arg(localPath).arg(QCoreApplication::applicationName());
    return QString("%1/subscriptions.json").arg(appDataPath);
}

bool SubscriptionManager::addSubscription(const Subscription &subscription)
{
    QString error;
    if (!validateSubscription(subscription, error)) {
        return false;
    }

    m_subscriptions.append(subscription);
    saveToJson();
    emit subscriptionListChanged();
    return true;
}

void SubscriptionManager::addSubscriptionWithDownload(const Subscription &subscription)
{
    qDebug() << "[SubscriptionManager] Adding subscription with download:" << subscription.name() << subscription.url();
    qDebug() << "[SubscriptionManager] Subscription ID:" << subscription.id();

    if (!addSubscription(subscription)) {
        qWarning() << "[SubscriptionManager] Failed to add subscription";
        return;
    }

    // Mark as new subscription download
    m_newSubscriptionDownloads.insert(subscription.id());

    // Start download
    QString savePath = QString("%1/%2").arg(profilesDir()).arg(subscription.configPath());
    qDebug() << "[SubscriptionManager] Starting download to:" << savePath;

    QString downloadId = m_downloader->download(QUrl(subscription.url()), savePath);
    qDebug() << "[SubscriptionManager] Storing mapping: downloadId=" << downloadId << "-> subscriptionId=" << subscription.id();
    m_downloadToSubscriptionId[downloadId] = subscription.id();
    emit subscriptionDownloadStarted(subscription.id(), subscription.name());
}

void SubscriptionManager::removeSubscription(const QString &id)
{
    for (int i = 0; i < m_subscriptions.size(); ++i) {
        if (m_subscriptions[i].id() == id) {
            Subscription sub = m_subscriptions[i];

            // Delete local config file
            QString configPath = sub.fullConfigPath();
            if (!configPath.isEmpty() && QFile::exists(configPath)) {
                QFile::remove(configPath);
            }

            m_subscriptions.removeAt(i);
            saveToJson();
            emit subscriptionListChanged();
            emit configRemoved(sub.configPath());
            break;
        }
    }
}

bool SubscriptionManager::updateSubscription(const QString &id, const Subscription &subscription)
{
    for (int i = 0; i < m_subscriptions.size(); ++i) {
        if (m_subscriptions[i].id() == id) {
            m_subscriptions[i] = subscription;
            saveToJson();
            emit subscriptionListChanged();
            return true;
        }
    }
    return false;
}

void SubscriptionManager::updateAllSubscriptions()
{
    if (m_subscriptions.isEmpty()) {
        emit allSubscriptionsUpdated(0, 0);
        return;
    }

    m_batchTotalCount = m_subscriptions.size();
    m_batchSuccessCount = 0;
    m_batchFailCount = 0;
    m_pendingSubscriptionIds.clear();
    m_downloadToSubscriptionId.clear();

    for (const Subscription &sub : m_subscriptions) {
        m_pendingSubscriptionIds.insert(sub.id());
        QString savePath = QString("%1/%2").arg(profilesDir()).arg(sub.configPath());
        QString downloadId = m_downloader->download(QUrl(sub.url()), savePath);
        m_downloadToSubscriptionId[downloadId] = sub.id();
        emit subscriptionDownloadStarted(sub.id(), sub.name());
    }
}

void SubscriptionManager::updateSubscriptionById(const QString &id)
{
    qDebug() << "[SubscriptionManager] updateSubscriptionById called with id:" << id;

    Subscription sub = subscription(id);
    qDebug() << "[SubscriptionManager] Found subscription:" << sub.name() << "configPath:" << sub.configPath();

    if (sub.id().isEmpty() || sub.configPath().isEmpty()) {
        qWarning() << "[SubscriptionManager] Subscription has empty id or configPath, skipping";
        return;
    }

    QString savePath = QString("%1/%2").arg(profilesDir()).arg(sub.configPath());
    qDebug() << "[SubscriptionManager] Save path:" << savePath;

    QString downloadId = m_downloader->download(QUrl(sub.url()), savePath);
    qDebug() << "[SubscriptionManager] Download ID assigned:" << downloadId;

    m_downloadToSubscriptionId[downloadId] = sub.id();
    qDebug() << "[SubscriptionManager] Mapping stored: downloadId=" << downloadId << "-> subscriptionId=" << sub.id();
    qDebug() << "[SubscriptionManager] Map size after insert:" << m_downloadToSubscriptionId.size();

    emit subscriptionDownloadStarted(sub.id(), sub.name());
}

bool SubscriptionManager::renameSubscription(const QString &id, const QString &newName)
{
    for (int i = 0; i < m_subscriptions.size(); ++i) {
        if (m_subscriptions[i].id() == id) {
            Subscription &sub = m_subscriptions[i];
            QString oldConfigPath = sub.configPath();
            QString newConfigPath = generateConfigFileName(newName, id);

            // Rename file
            QString oldFullPath = sub.fullConfigPath();
            QString newFullPath = QString("%1/%2").arg(profilesDir()).arg(newConfigPath);

            if (QFile::exists(oldFullPath)) {
                QFile::rename(oldFullPath, newFullPath);
            }

            sub.setName(newName);
            sub.setConfigPath(newConfigPath);
            saveToJson();
            emit subscriptionListChanged();
            return true;
        }
    }
    return false;
}

Subscription SubscriptionManager::subscription(const QString &id) const
{
    for (const Subscription &sub : m_subscriptions) {
        if (sub.id() == id) {
            return sub;
        }
    }
    return Subscription();
}

Subscription SubscriptionManager::subscriptionByName(const QString &name) const
{
    for (const Subscription &sub : m_subscriptions) {
        if (sub.name() == name) {
            return sub;
        }
    }
    return Subscription();
}

Subscription SubscriptionManager::subscriptionByUrl(const QString &url) const
{
    for (const Subscription &sub : m_subscriptions) {
        if (sub.url() == url) {
            return sub;
        }
    }
    return Subscription();
}

QList<Subscription> SubscriptionManager::subscriptions() const
{
    return m_subscriptions;
}

int SubscriptionManager::subscriptionCount() const
{
    return m_subscriptions.size();
}

QString SubscriptionManager::idOfConfigPath(const QString &configPath) const
{
    for (const Subscription &sub : m_subscriptions) {
        if (sub.configPath() == configPath) {
            return sub.id();
        }
    }
    return QString();
}

bool SubscriptionManager::isNameExists(const QString &name, const QString &excludeId) const
{
    for (const Subscription &sub : m_subscriptions) {
        if (sub.name() == name && sub.id() != excludeId) {
            return true;
        }
    }
    return false;
}

bool SubscriptionManager::isUrlExists(const QString &url, const QString &excludeId) const
{
    for (const Subscription &sub : m_subscriptions) {
        if (sub.url() == url && sub.id() != excludeId) {
            return true;
        }
    }
    return false;
}

QList<Subscription> SubscriptionManager::subscriptionsNeedingUpdate() const
{
    QList<Subscription> result;
    QDateTime now = QDateTime::currentDateTime();

    for (const Subscription &sub : m_subscriptions) {
        if (sub.autoUpdate() && sub.updateInterval() > 0) {
            QDateTime nextUpdate = sub.lastUpdated().addSecs(sub.updateInterval() * 3600);
            if (!sub.lastUpdated().isValid() || nextUpdate <= now) {
                result.append(sub);
            }
        }
    }
    return result;
}

SubscriptionDownloader* SubscriptionManager::downloader() const
{
    return m_downloader;
}

void SubscriptionManager::onDownloadFinished(const QString &downloadId, const QString &savePath,
                                             bool success, const QString &error)
{
    qDebug() << "[SubscriptionManager] Download finished:" << downloadId << "success:" << success;
    qDebug() << "[SubscriptionManager] Map size at lookup:" << m_downloadToSubscriptionId.size();
    qDebug() << "[SubscriptionManager] Map keys:" << m_downloadToSubscriptionId.keys();

    QString subscriptionId = m_downloadToSubscriptionId.value(downloadId);
    qDebug() << "[SubscriptionManager] Looked up subscriptionId:" << subscriptionId;

    if (subscriptionId.isEmpty()) {
        qWarning() << "[SubscriptionManager] Unknown download ID:" << downloadId;
        return;
    }

    m_downloadToSubscriptionId.remove(downloadId);

    Subscription sub = subscription(subscriptionId);
    if (sub.id().isEmpty()) {
        qWarning() << "[SubscriptionManager] Subscription not found:" << subscriptionId;
        return;
    }

    if (success) {
        qDebug() << "[SubscriptionManager] Download successful for:" << sub.name();

        // Update lastUpdated timestamp
        for (int i = 0; i < m_subscriptions.size(); ++i) {
            if (m_subscriptions[i].id() == subscriptionId) {
                m_subscriptions[i].setLastUpdated(QDateTime::currentDateTime());
                break;
            }
        }
        saveToJson();

        emit subscriptionDownloadFinished(subscriptionId, true, QString());

        // Check if this is a new subscription download
        if (m_newSubscriptionDownloads.contains(subscriptionId)) {
            m_newSubscriptionDownloads.remove(subscriptionId);
            qDebug() << "[SubscriptionManager] Emitting configAdded:" << savePath << sub.name();
            emit configAdded(savePath, sub.name());
        } else {
            qDebug() << "[SubscriptionManager] Emitting configUpdated:" << sub.configPath();
            emit configUpdated(sub.configPath());
        }
    } else {
        qWarning() << "[SubscriptionManager] Download failed for:" << sub.name() << "error:" << error;
        emit subscriptionDownloadFinished(subscriptionId, false, error);
    }

    // Track batch update progress
    if (!m_pendingSubscriptionIds.isEmpty()) {
        m_pendingSubscriptionIds.remove(subscriptionId);
        if (success) {
            m_batchSuccessCount++;
        } else {
            m_batchFailCount++;
        }

        if (m_pendingSubscriptionIds.isEmpty()) {
            emit allSubscriptionsUpdated(m_batchSuccessCount, m_batchFailCount);
        }
    }
}

void SubscriptionManager::loadFromJson()
{
    QString filePath = subscriptionsFilePath();
    QFile file(filePath);

    if (!file.exists()) {
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray subscriptionsArray = root["subscriptions"].toArray();

    m_subscriptions.clear();
    bool needsSave = false;
    for (const QJsonValue &value : subscriptionsArray) {
        if (value.isObject()) {
            Subscription sub = Subscription::fromJson(value.toObject());
            // Fix empty IDs for existing subscriptions
            if (sub.id().isEmpty()) {
                sub.setId(Subscription::generateId());
                qDebug() << "[SubscriptionManager] Fixed empty ID for subscription:" << sub.name();
                needsSave = true;
            }
            m_subscriptions.append(sub);
        }
    }

    // Save if we fixed any IDs
    if (needsSave) {
        saveToJson();
        qDebug() << "[SubscriptionManager] Saved fixed subscription IDs";
    }
}

void SubscriptionManager::saveToJson()
{
    QString filePath = subscriptionsFilePath();

    QJsonArray subscriptionsArray;
    for (const Subscription &sub : m_subscriptions) {
        subscriptionsArray.append(sub.toJson());
    }

    QJsonObject root;
    root["version"] = 1;
    root["subscriptions"] = subscriptionsArray;

    QJsonDocument doc(root);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}

void SubscriptionManager::ensureProfilesDirExists()
{
    QDir dir(profilesDir());
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Also ensure parent directory exists
    QString localPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString appDataPath = QString("%1/%2").arg(localPath).arg(QCoreApplication::applicationName());
    QDir appDir(appDataPath);
    if (!appDir.exists()) {
        appDir.mkpath(".");
    }
}

QString SubscriptionManager::generateConfigFileName(const QString &name, const QString &id) const
{
    // Format: name_id_short.json
    QString shortId = id.left(8);
    QString sanitizedName = name;
    // Remove invalid filename characters
    sanitizedName.remove(QRegularExpression(R"([<>:"/\\|?*])"));
    return QString("%1_%2.json").arg(sanitizedName).arg(shortId);
}

bool SubscriptionManager::validateSubscription(const Subscription &sub, QString &error) const
{
    if (sub.name().isEmpty()) {
        error = tr("Subscription name cannot be empty");
        return false;
    }

    if (sub.url().isEmpty()) {
        error = tr("Subscription URL cannot be empty");
        return false;
    }

    QUrl url(sub.url());
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) {
        error = tr("Invalid URL, only HTTP/HTTPS supported");
        return false;
    }

    return true;
}
