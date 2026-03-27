#ifndef SUBSCRIPTION_MANAGER_H
#define SUBSCRIPTION_MANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QSet>
#include "subscription.h"
#include "subscription_downloader.h"

class SubscriptionManager : public QObject
{
    Q_OBJECT

public:
    explicit SubscriptionManager(QObject *parent = nullptr);
    ~SubscriptionManager();

    // Subscription list operations
    bool addSubscription(const Subscription &subscription);  // Returns success
    void addSubscriptionWithDownload(const Subscription &subscription);  // Add and start download
    void removeSubscription(const QString &id);
    bool updateSubscription(const QString &id, const Subscription &subscription);
    void updateAllSubscriptions();
    void updateSubscriptionById(const QString &id);  // Update single subscription

    // Rename subscription (sync update config file name)
    bool renameSubscription(const QString &id, const QString &newName);

    // Get subscription info
    Subscription subscription(const QString &id) const;
    Subscription subscriptionByName(const QString &name) const;
    Subscription subscriptionByUrl(const QString &url) const;
    QList<Subscription> subscriptions() const;
    int subscriptionCount() const;
    QString idOfConfigPath(const QString &configPath) const;

    // Deduplication check
    bool isNameExists(const QString &name, const QString &excludeId = QString()) const;
    bool isUrlExists(const QString &url, const QString &excludeId = QString()) const;

    // Check subscriptions that need auto update
    QList<Subscription> subscriptionsNeedingUpdate() const;

    // Get downloader (for UI use)
    SubscriptionDownloader* downloader() const;

    // Config file directory
    static QString profilesDir();
    static QString subscriptionsFilePath();

    // Generate config file name (public for UI use)
    QString generateConfigFileName(const QString &name, const QString &id) const;

signals:
    // List change signal
    void subscriptionListChanged();

    // Download status signal
    void subscriptionDownloadStarted(const QString &id, const QString &name);
    void subscriptionDownloadFinished(const QString &id, bool success, const QString &error);

    // Batch update completed signal
    void allSubscriptionsUpdated(int successCount, int failCount);

    // Decoupled signals for ConfigManager
    void configAdded(const QString &configPath, const QString &name);
    void configUpdated(const QString &configPath);
    void configRemoved(const QString &configPath);

private slots:
    void onDownloadFinished(const QString &downloadId, const QString &savePath,
                            bool success, const QString &error);

private:
    void loadFromJson();
    void saveToJson();
    void ensureProfilesDirExists();
    bool validateSubscription(const Subscription &sub, QString &error) const;

    QList<Subscription> m_subscriptions;
    SubscriptionDownloader *m_downloader;

    // Concurrent download tracking
    int m_activeDownloadCount = 0;
    int m_batchTotalCount = 0;
    int m_batchSuccessCount = 0;
    int m_batchFailCount = 0;
    QSet<QString> m_pendingSubscriptionIds;  // Pending subscription IDs
    QMap<QString, QString> m_downloadToSubscriptionId;  // downloadId -> subscriptionId
    QSet<QString> m_newSubscriptionDownloads;  // Subscription IDs that are new (need configAdded signal)
};

#endif // SUBSCRIPTION_MANAGER_H
