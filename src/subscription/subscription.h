#ifndef SUBSCRIPTION_H
#define SUBSCRIPTION_H

#include <QString>
#include <QDateTime>
#include <QFile>
#include <QJsonObject>

class Subscription
{
public:
    Subscription();
    Subscription(const QString &name, const QString &url);

    // Getters
    QString id() const;             // Unique identifier (UUID)
    QString name() const;           // Subscription name, e.g. "赔钱机场"
    QString url() const;            // Subscription URL
    QString configPath() const;     // Local config file path (relative path)
    QDateTime lastUpdated() const;  // Last update time
    bool autoUpdate() const;        // Auto update enabled
    int updateInterval() const;     // Update interval (hours), 0 means disabled

    // Dynamic calculation of local cache status (non-storage field)
    bool isCached() const {
        return !m_configPath.isEmpty() && QFile::exists(fullConfigPath());
    }

    // Get full config path
    QString fullConfigPath() const;

    // Setters
    void setId(const QString &id);
    void setName(const QString &name);
    void setUrl(const QString &url);
    void setConfigPath(const QString &path);
    void setLastUpdated(const QDateTime &time);
    void setAutoUpdate(bool enabled);
    void setUpdateInterval(int hours);

    // JSON serialization
    QJsonObject toJson() const;
    static Subscription fromJson(const QJsonObject &json);

    // Generate unique ID
    static QString generateId();

private:
    QString m_id;
    QString m_name;
    QString m_url;
    QString m_configPath;           // Relative to profiles directory
    QDateTime m_lastUpdated;
    bool m_autoUpdate = false;
    int m_updateInterval = 24;      // Default 24 hours
};

#endif // SUBSCRIPTION_H
