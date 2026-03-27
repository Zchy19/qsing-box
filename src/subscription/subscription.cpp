#include "subscription.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonObject>
#include <QUuid>
#include <QStandardPaths>

Subscription::Subscription() = default;

Subscription::Subscription(const QString &name, const QString &url)
    : m_name(name), m_url(url)
{
    m_id = generateId();
}

QString Subscription::id() const
{
    return m_id;
}

QString Subscription::name() const
{
    return m_name;
}

QString Subscription::url() const
{
    return m_url;
}

QString Subscription::configPath() const
{
    return m_configPath;
}

QDateTime Subscription::lastUpdated() const
{
    return m_lastUpdated;
}

bool Subscription::autoUpdate() const
{
    return m_autoUpdate;
}

int Subscription::updateInterval() const
{
    return m_updateInterval;
}

QString Subscription::fullConfigPath() const
{
    if (m_configPath.isEmpty()) {
        return QString();
    }

    QString localPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString appDataPath = QString("%1/%2").arg(localPath).arg(QCoreApplication::applicationName());

    return QString("%1/profiles/%2").arg(appDataPath).arg(m_configPath);
}

void Subscription::setId(const QString &id)
{
    m_id = id;
}

void Subscription::setName(const QString &name)
{
    m_name = name;
}

void Subscription::setUrl(const QString &url)
{
    m_url = url;
}

void Subscription::setConfigPath(const QString &path)
{
    m_configPath = path;
}

void Subscription::setLastUpdated(const QDateTime &time)
{
    m_lastUpdated = time;
}

void Subscription::setAutoUpdate(bool enabled)
{
    m_autoUpdate = enabled;
}

void Subscription::setUpdateInterval(int hours)
{
    m_updateInterval = hours;
}

QJsonObject Subscription::toJson() const
{
    QJsonObject json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["url"] = m_url;
    json["configPath"] = m_configPath;
    if (m_lastUpdated.isValid()) {
        json["lastUpdated"] = m_lastUpdated.toString(Qt::ISODate);
    }
    json["autoUpdate"] = m_autoUpdate;
    json["updateInterval"] = m_updateInterval;
    return json;
}

Subscription Subscription::fromJson(const QJsonObject &json)
{
    Subscription sub;
    sub.setId(json["id"].toString());
    sub.setName(json["name"].toString());
    sub.setUrl(json["url"].toString());
    sub.setConfigPath(json["configPath"].toString());
    if (json.contains("lastUpdated")) {
        sub.setLastUpdated(QDateTime::fromString(json["lastUpdated"].toString(), Qt::ISODate));
    }
    sub.setAutoUpdate(json["autoUpdate"].toBool(false));
    sub.setUpdateInterval(json["updateInterval"].toInt(24));
    return sub;
}

QString Subscription::generateId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
