# 订阅管理系统设计方案

## 1. 功能概述

为 qsing-box 添加订阅链接管理功能，支持从网络 URL 导入 sing-box JSON 配置文件。

### 核心功能

| 功能 | 描述 |
|------|------|
| 添加订阅 | 输入订阅名称和 URL，下载并保存配置 |
| 更新订阅 | 重新下载订阅配置，覆盖本地文件 |
| 删除订阅 | 删除订阅记录和对应的配置文件 |
| 全部更新 | 一键更新所有订阅（并发下载） |
| 自动更新 | 启动时自动检查更新（可配置间隔） |

---

## 2. 系统架构

### 2.1 模块关系图

```
┌─────────────────────────────────────────────────────────────────┐
│                         MainWindow                               │
│                                                                  │
│  ┌──────────┐   ┌──────────────┐   ┌──────────────────────┐     │
│  │ Control  │   │  Profiles    │   │       Logs           │     │
│  │          │   │              │   │                      │     │
│  │ [Start]  │   │ 订阅列表     │   │                      │     │
│  │ [Stop]   │   │ ├─ 赔钱机场  │   │                      │     │
│  │ [Setting]│   │ ├─ 机场2     │   │                      │     │
│  │ [About]  │   │ └─ 机场3     │   │                      │     │
│  │          │   │              │   │                      │     │
│  │ [订阅]   │   │ [New][Import]│   │                      │     │
│  └──────────┘   └──────────────┘   └──────────────────────┘     │
│       │                                     ▲                    │
│       │                                     │                    │
│       ▼                                     │                    │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                  SubscriptionDialog                      │    │
│  └─────────────────────────────────────────────────────────┘    │
│       │                                                          │
│       ▼                                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                SubscriptionManager                       │    │
│  │  - 管理订阅列表                                          │    │
│  │  - 存储订阅信息到 JSON 文件                              │    │
│  │  - 信号驱动，与 ConfigManager 解耦                       │    │
│  └─────────────────────────────────────────────────────────┘    │
│       │                                                          │
│       │ 信号: configAdded(configPath, name)                      │
│       │ 信号: configRemoved(configPath)                          │
│       ▼                                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                SubscriptionDownloader                     │    │
│  │  - QNetworkAccessManager 下载配置                        │    │
│  │  - JSON 格式验证                                         │    │
│  │  - 大小限制、超时控制、自动重定向                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│       │                                                          │
│       │ 下载完成后发射信号                                       │
│       ▼                                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                  ConfigManager (现有)                    │    │
│  │  - 管理配置列表                                          │    │
│  │  - 监听 SubscriptionManager 信号自动更新列表              │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 数据流（信号驱动）

```
用户点击"订阅"按钮
        │
        ▼
SubscriptionDialog.show()
        │
        ├── 添加订阅 ──► 输入名称+URL
        │                      │
        │                      ▼
        │              ┌─────────────────────────────────┐
        │              │ 检查重名/重URL                   │
        │              │ SubscriptionManager::isNameExists()│
        │              │ SubscriptionManager::isUrlExists()│
        │              └─────────────────────────────────┘
        │                      │
        │                      ▼ 通过验证
        │              SubscriptionDownloader::download()
        │                      │
        │                      ▼
        │              QNetworkAccessManager::get(URL)
        │              - 设置超时 30s
        │              - 设置大小限制 10MB
        │              - 启用自动重定向
        │                      │
        │                      ▼
        │              验证 JSON 格式
        │                      │
        │                      ▼
        │              保存到 config/profiles/订阅名_uuid.json
        │                      │
        │                      ▼
        │              SubscriptionManager::addSubscription()
        │                      │
        │                      ├── saveToJson() 写入 subscriptions.json
        │                      │
        │                      └── emit configAdded(configPath, name)
        │                              │
        │                              ▼
        │                      ConfigManager::onConfigAdded()
        │                              │
        │                              ▼
        │                      UI 自动刷新配置列表
        │
        ├── 更新订阅 ──► SubscriptionDownloader::download()
        │                      │
        │                      ▼
        │                覆盖现有配置文件
        │                      │
        │                      ▼
        │                更新 subscriptions.json 时间戳
        │                      │
        │                      └── emit configUpdated(configPath)
        │
        ├── 删除订阅 ──► 删除本地配置文件
        │                      │
        │                      ▼
        │              从 subscriptions.json 移除记录
        │                      │
        │                      └── emit configRemoved(configPath)
        │
        └── 更新全部 ──► 遍历所有订阅
                              │
                              ├──► 并发下载订阅 1
                              ├──► 并发下载订阅 2
                              └──► 并发下载订阅 N
                                      │
                                      ▼ (计数器跟踪)
                              所有下载完成后:
                              emit allSubscriptionsUpdated(successCount, failCount)
```

---

## 3. 数据结构设计

### 3.1 Subscription 类

```cpp
// src/subscription/subscription.h

#ifndef SUBSCRIPTION_H
#define SUBSCRIPTION_H

#include <QString>
#include <QDateTime>
#include <QFile>

class Subscription
{
public:
    Subscription();
    Subscription(const QString &name, const QString &url);

    // Getters
    QString id() const;             // 唯一标识符（UUID）
    QString name() const;           // 订阅名称，如 "赔钱机场"
    QString url() const;            // 订阅 URL
    QString configPath() const;     // 本地配置文件路径（相对路径）
    QDateTime lastUpdated() const;  // 最后更新时间
    bool autoUpdate() const;        // 是否自动更新
    int updateInterval() const;     // 更新间隔（小时），0 表示禁用

    // 动态计算本地缓存状态（非存储字段）
    bool isCached() const {
        return !m_configPath.isEmpty() && QFile::exists(fullConfigPath());
    }

    // 获取完整配置路径
    QString fullConfigPath() const;

    // Setters
    void setId(const QString &id);
    void setName(const QString &name);
    void setUrl(const QString &url);
    void setConfigPath(const QString &path);
    void setLastUpdated(const QDateTime &time);
    void setAutoUpdate(bool enabled);
    void setUpdateInterval(int hours);

    // JSON 序列化
    QJsonObject toJson() const;
    static Subscription fromJson(const QJsonObject &json);

    // 生成唯一 ID
    static QString generateId();

private:
    QString m_id;
    QString m_name;
    QString m_url;
    QString m_configPath;           // 相对于 profiles 目录
    QDateTime m_lastUpdated;
    bool m_autoUpdate = false;
    int m_updateInterval = 24;      // 默认 24 小时
};

#endif // SUBSCRIPTION_H
```

**设计变更说明**：
- `isCached()` 改为动态计算，不存储 `m_cached` 字段
- `configPath` 存储相对路径，通过 `fullConfigPath()` 获取完整路径
- 移除 `setCached()` 方法

### 3.2 存储结构（JSON 文件）

采用 JSON 文件存储订阅元数据，便于调试、备份和迁移。

**文件位置**：`config/subscriptions.json`

```json
{
  "version": 1,
  "subscriptions": [
    {
      "id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
      "name": "赔钱机场",
      "url": "http://server.199658.xyz:5000/output/windows/赔钱机场.json",
      "configPath": "赔钱机场_a1b2c3d4.json",
      "lastUpdated": "2024-01-01T12:00:00",
      "autoUpdate": true,
      "updateInterval": 24
    }
  ]
}
```

**目录结构**：
```
config/
├── subscriptions.json           # 订阅元数据
└── profiles/                    # 订阅配置文件目录
    ├── 赔钱机场_a1b2c3d4.json
    └── 机场2_b2c3d4e5.json
```

---

## 4. 类设计

### 4.1 SubscriptionManager

管理订阅列表，从 JSON 文件读写，支持批量更新，通过信号与 ConfigManager 解耦。

```cpp
// src/subscription/subscription_manager.h

#ifndef SUBSCRIPTION_MANAGER_H
#define SUBSCRIPTION_MANAGER_H

#include <QObject>
#include <QList>
#include <QSet>
#include "subscription.h"
#include "subscription_downloader.h"

class SubscriptionManager : public QObject
{
    Q_OBJECT

public:
    explicit SubscriptionManager(QObject *parent = nullptr);
    ~SubscriptionManager();

    // 订阅列表操作
    bool addSubscription(const Subscription &subscription);  // 返回是否成功
    void removeSubscription(const QString &id);
    bool updateSubscription(const QString &id, const Subscription &subscription);
    void updateAllSubscriptions();

    // 重命名订阅（同步更新配置文件名）
    bool renameSubscription(const QString &id, const QString &newName);

    // 获取订阅信息
    Subscription subscription(const QString &id) const;
    Subscription subscriptionByName(const QString &name) const;
    Subscription subscriptionByUrl(const QString &url) const;
    QList<Subscription> subscriptions() const;
    int subscriptionCount() const;
    QString idOfConfigPath(const QString &configPath) const;

    // 去重检查
    bool isNameExists(const QString &name, const QString &excludeId = QString()) const;
    bool isUrlExists(const QString &url, const QString &excludeId = QString()) const;

    // 检查需要自动更新的订阅
    QList<Subscription> subscriptionsNeedingUpdate() const;

    // 获取下载器（供 UI 使用）
    SubscriptionDownloader* downloader() const;

    // 配置文件目录
    static QString profilesDir();
    static QString subscriptionsFilePath();

signals:
    // 列表变化信号
    void subscriptionListChanged();

    // 下载状态信号
    void subscriptionDownloadStarted(const QString &id, const QString &name);
    void subscriptionDownloadFinished(const QString &id, bool success, const QString &error);

    // 批量更新完成信号
    void allSubscriptionsUpdated(int successCount, int failCount);

    // 与 ConfigManager 解耦的信号
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
    QString generateConfigFileName(const QString &name, const QString &id) const;
    bool validateSubscription(const Subscription &sub, QString &error) const;

    QList<Subscription> m_subscriptions;
    SubscriptionDownloader *m_downloader;

    // 并发下载跟踪
    int m_activeDownloadCount = 0;
    int m_batchTotalCount = 0;
    int m_batchSuccessCount = 0;
    int m_batchFailCount = 0;
    QSet<QString> m_pendingSubscriptionIds;  // 待完成的订阅 ID
};

#endif // SUBSCRIPTION_MANAGER_H
```

**新增功能**：
- `isNameExists()` / `isUrlExists()`：去重检查
- `renameSubscription()`：重命名并同步文件
- `configAdded` / `configUpdated` / `configRemoved`：与 ConfigManager 解耦
- 并发下载计数器：跟踪批量更新进度

### 4.2 SubscriptionDownloader

支持并发下载，内置安全限制。

```cpp
// src/subscription/subscription_downloader.h

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
    // 下载配置常量
    static constexpr qint64 DEFAULT_MAX_SIZE = 10 * 1024 * 1024;  // 10MB
    static constexpr int DEFAULT_TIMEOUT = 30000;  // 30秒

    explicit SubscriptionDownloader(QObject *parent = nullptr);

    // 下载订阅配置，返回下载任务 ID
    QString download(const QUrl &url, const QString &savePath,
                     qint64 maxSize = DEFAULT_MAX_SIZE,
                     int timeoutMs = DEFAULT_TIMEOUT);

    // 取消指定下载任务
    void cancelDownload(const QString &downloadId);

    // 取消所有下载任务
    void cancelAllDownloads();

    // 验证 JSON 格式
    static bool isValidSingBoxConfig(const QByteArray &data, QString &error);

    // 获取当前活跃下载数量
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
    bool isPathSafe(const QString &path) const;  // 防止路径穿越
    QString sanitizeUrlForLog(const QString &url) const;  // URL 脱敏

    struct DownloadTask {
        QString id;
        QString savePath;
        QNetworkReply *reply;
        qint64 maxSize;
        qint64 bytesReceived = 0;
        QTimer *timeoutTimer;
    };

    QNetworkAccessManager *m_networkManager;
    QMap<QString, DownloadTask> m_activeDownloads;
};

#endif // SUBSCRIPTION_DOWNLOADER_H
```

**安全增强**：
- `maxSize` 参数：限制下载大小
- `timeoutMs` 参数：超时控制
- `validateUrl()`：URL 协议验证（仅 HTTP/HTTPS）
- `isPathSafe()`：防止路径穿越攻击
- `sanitizeUrlForLog()`：日志脱敏（隐藏 URL 中的 token）

### 4.3 SubscriptionDialog

订阅管理对话框，支持多选和批量操作。

```cpp
// src/subscription/subscription_dialog.h

#ifndef SUBSCRIPTION_DIALOG_H
#define SUBSCRIPTION_DIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QProgressBar>
#include <QSet>

class SubscriptionManager;
class Subscription;

namespace Ui {
class SubscriptionDialog;
}

class SubscriptionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SubscriptionDialog(SubscriptionManager *manager, QWidget *parent = nullptr);
    ~SubscriptionDialog();

private slots:
    void onAddButtonClicked();
    void onUpdateSelectedButtonClicked();
    void onUpdateAllButtonClicked();
    void onDeleteSelectedButtonClicked();
    void onRenameActionTriggered();

    void onSubscriptionDownloadStarted(const QString &id, const QString &name);
    void onSubscriptionDownloadFinished(const QString &id, bool success, const QString &error);
    void onAllSubscriptionsUpdated(int successCount, int failCount);
    void onDownloadProgress(const QString &downloadId, qint64 received, qint64 total);

    void refreshSubscriptionList();
    void updateItemStatus(QListWidgetItem *item, const Subscription &sub);

private:
    void setupConnections();
    void setupContextMenu();
    void showAddSubscriptionDialog();
    void showErrorDialog(const QString &title, const QString &subscriptionName,
                        const QString &error, bool showRetry = true);

    Ui::SubscriptionDialog *ui;
    SubscriptionManager *m_manager;
    QMap<QString, QProgressBar*> m_progressBars;
    QSet<QString> m_downloadingIds;  // 正在下载的订阅 ID
};

#endif // SUBSCRIPTION_DIALOG_H
```

### 4.4 AddSubscriptionDialog

添加订阅对话框，支持 URL 验证和预检。

```cpp
// src/subscription/add_subscription_dialog.h

#ifndef ADD_SUBSCRIPTION_DIALOG_H
#define ADD_SUBSCRIPTION_DIALOG_H

#include <QDialog>
#include <QNetworkAccessManager>

namespace Ui {
class AddSubscriptionDialog;
}

class AddSubscriptionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddSubscriptionDialog(SubscriptionManager *manager, QWidget *parent = nullptr);
    ~AddSubscriptionDialog();

    QString name() const;
    QString url() const;
    bool autoUpdate() const;
    int updateInterval() const;

private slots:
    void onTestButtonClicked();
    void onDownloadButtonClicked();
    void onTestFinished(QNetworkReply *reply);

private:
    void setUiEnabled(bool enabled);
    bool validateInput(QString &error);
    void showValidationError(const QString &error);

    Ui::AddSubscriptionDialog *ui;
    SubscriptionManager *m_manager;
    QNetworkAccessManager *m_testNetwork;
    bool m_urlValid = false;
};

#endif // ADD_SUBSCRIPTION_DIALOG_H
```

---

## 5. UI 设计

### 5.1 订阅管理对话框

```
┌──────────────────────────────────────────────────────────────────┐
│  订阅管理                                              [X]      │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ [✓] 状态  订阅名称          订阅地址                  更新时间│  │
│  ├────────────────────────────────────────────────────────────┤  │
│  │ [ ] 🟢   赔钱机场    http://server.../赔钱机场.json  2024-01-01│  │
│  │ [ ] 🔄   机场2       http://server.../机场2.json     正在更新...│  │
│  │ [ ] 🔴   机场3       http://server.../机场3.json     下载失败  │  │
│  │ [ ] ⚪   机场4       http://server.../机场4.json     -        │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
│  状态说明: 🟢 已缓存可用  🔄 正在更新  🔴 下载失败  ⚪ 无本地缓存   │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ [更新选中]  [更新全部]  [删除选中]  [重命名]                │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
│                                        [添加订阅]  [关闭]       │
└──────────────────────────────────────────────────────────────────┘
```

**交互特性**：
- 复选框支持多选
- 右键菜单：更新、删除、重命名、复制 URL
- 双击项目可编辑名称

### 5.2 添加订阅对话框

```
┌──────────────────────────────────────────────────────────────────┐
│  添加订阅                                              [X]      │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  订阅名称: *                                                      │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ 赔钱机场                                                  │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
│  订阅地址: *                                                      │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ http://server.199658.xyz:5000/output/windows/赔钱机场.json│   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ [✓] 自动更新    间隔: [24] 小时                           │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
│                              [取消]  [测试连接]  [下载并添加]    │
└──────────────────────────────────────────────────────────────────┘
```

### 5.3 错误处理对话框

```
┌──────────────────────────────────────────────────────────────────┐
│  ⚠️ 下载失败                                           [X]      │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  订阅: 赔钱机场                                                   │
│                                                                  │
│  错误详情:                                                        │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ Connection timeout (30s)                                  │   │
│  │                                                           │   │
│  │ 请检查:                                                    │   │
│  │ 1. 网络连接是否正常                                        │   │
│  │ 2. 订阅地址是否正确                                        │   │
│  │ 3. 服务器是否可访问                                        │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
│                              [关闭]  [重试]  [编辑URL]           │
└──────────────────────────────────────────────────────────────────┘
```

### 5.4 批量更新进度对话框

```
┌──────────────────────────────────────────────────────────────────┐
│  批量更新订阅                                          [X]      │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  总进度: 2 / 5                                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
│  当前任务:                                                        │
│  ✅ 赔钱机场 - 完成                                               │
│  ✅ 机场2 - 完成                                                  │
│  🔄 机场3 - 正在下载...  [████████░░░░░░░░] 65%                  │
│  ⏳ 机场4 - 等待中                                                │
│  ⏳ 机场5 - 等待中                                                │
│                                                                  │
│                                            [取消]               │
└──────────────────────────────────────────────────────────────────┘
```

---

## 6. 安全设计

### 6.1 URL 安全验证

```cpp
bool SubscriptionDownloader::validateUrl(const QUrl &url, QString &error) const
{
    // 仅允许 HTTP/HTTPS 协议
    QString scheme = url.scheme().toLower();
    if (scheme != "http" && scheme != "https") {
        error = QStringLiteral("仅支持 HTTP/HTTPS 协议");
        return false;
    }

    // 必须有有效的主机名
    if (url.host().isEmpty()) {
        error = QStringLiteral("无效的主机名");
        return false;
    }

    return true;
}
```

### 6.2 路径穿越防护

```cpp
bool SubscriptionDownloader::isPathSafe(const QString &path) const
{
    // 禁止路径穿越
    if (path.contains("..") || path.contains("\\")) {
        return false;
    }

    // 必须是相对路径
    if (QFileInfo(path).isAbsolute()) {
        return false;
    }

    return true;
}
```

### 6.3 URL 日志脱敏

```cpp
QString SubscriptionDownloader::sanitizeUrlForLog(const QString &url) const
{
    QUrl parsed(url);
    if (parsed.hasQuery()) {
        // 移除查询参数中的敏感信息
        QUrlQuery query(parsed);
        query.removeAllQueryItems("token");
        query.removeAllQueryItems("key");
        query.removeAllQueryItems("secret");
        parsed.setQuery(query);
    }
    return parsed.toString(QUrl::RemoveUserInfo);
}
```

### 6.4 下载安全限制

| 限制项 | 值 | 说明 |
|--------|-----|------|
| 最大文件大小 | 10MB | 防止大文件攻击 |
| 下载超时 | 30秒 | 防止慢速攻击 |
| 最大重定向次数 | 10 | Qt 默认值 |
| SSL 证书验证 | 默认开启 | 可配置关闭（不推荐） |

---

## 7. 错误处理

### 7.1 网络错误

| 错误类型 | 错误代码 | 处理方式 |
|----------|----------|----------|
| 网络超时 | QNetworkReply::TimeoutError | 显示超时提示，建议检查网络 |
| DNS 解析失败 | QNetworkReply::HostNotFoundError | 显示错误提示，检查 URL |
| SSL 证书错误 | QNetworkReply::SslHandshakeFailedError | 显示警告，可选择忽略（需用户确认） |
| HTTP 4xx | QNetworkReply::ContentNotFoundError 等 | 显示具体 HTTP 错误信息 |
| HTTP 5xx | QNetworkReply::InternalServerError 等 | 显示服务器错误，建议稍后重试 |
| 文件过大 | 自定义 | 显示大小限制提示 |

### 7.2 数据错误

| 错误类型 | 处理方式 |
|----------|----------|
| 非 JSON 格式 | 显示"配置文件格式错误，请检查订阅地址" |
| JSON 缺少必要字段 | 显示具体缺失字段 |
| 文件保存失败 | 显示保存路径和错误原因 |

### 7.3 业务错误

| 错误类型 | 处理方式 |
|----------|----------|
| 订阅名称已存在 | 提示用户更换名称 |
| 订阅 URL 已存在 | 提示"该订阅已添加" |
| 本地缓存损坏 | 提示重新下载 |

---

## 8. 文件结构

### 8.1 新增文件

```
src/
├── subscription/                    # 新增订阅模块
│   ├── CMakeLists.txt              # 模块构建文件
│   ├── subscription.h              # 订阅数据类
│   ├── subscription.cpp
│   ├── subscription_manager.h      # 订阅管理器
│   ├── subscription_manager.cpp
│   ├── subscription_downloader.h   # 下载器
│   ├── subscription_downloader.cpp
│   ├── subscription_dialog.h       # 订阅管理对话框
│   ├── subscription_dialog.cpp
│   ├── subscription_dialog.ui
│   ├── add_subscription_dialog.h   # 添加订阅对话框
│   ├── add_subscription_dialog.cpp
│   └── add_subscription_dialog.ui
│
└── main_window.cpp                 # 修改：添加订阅按钮处理

config/
├── subscriptions.json              # 订阅元数据（运行时生成）
└── profiles/                       # 订阅配置文件目录（运行时生成）
```

### 8.2 修改文件

| 文件 | 修改内容 |
|------|----------|
| `CMakeLists.txt` | 添加 Qt6::Network，添加 subscription 子目录 |
| `src/CMakeLists.txt` | 添加 subscription 子目录 |
| `src/main_window.ui` | 添加订阅按钮 |
| `src/main_window.h` | 添加 SubscriptionManager 成员，连接信号槽 |
| `src/main_window.cpp` | 连接 configAdded/configRemoved 信号到 ConfigManager |
| `src/config/config_manager.h` | 添加 onConfigAdded/onConfigRemoved 槽函数 |
| `src/config/config_manager.cpp` | 实现槽函数 |
| `resources/styles/style.qss` | 添加订阅相关控件样式 |

---

## 9. 实现步骤

### Phase 1: 基础架构

1. 创建 `src/subscription/` 目录
2. 实现 `Subscription` 数据类（含 JSON 序列化）
3. 实现 `SubscriptionManager` 基础 CRUD
4. 创建 `config/profiles/` 目录结构
5. 实现 JSON 文件读写

### Phase 2: 下载功能

1. 实现 `SubscriptionDownloader` 单任务下载
2. 添加 URL 验证、路径穿越检查
3. 添加大小限制、超时控制
4. 实现 JSON 格式验证
5. 添加错误处理

### Phase 3: UI 实现

1. 创建 `SubscriptionDialog` UI（含多选支持）
2. 创建 `AddSubscriptionDialog` UI（含验证）
3. 实现错误对话框
4. 实现批量更新进度显示

### Phase 4: 集成

1. 修改主窗口添加订阅按钮
2. 连接 `SubscriptionManager` 信号到 `ConfigManager`
3. 实现订阅配置自动添加/移除
4. 测试完整流程

### Phase 5: 增强

1. 自动更新功能（启动时检查）
2. 订阅重命名
3. 右键菜单
4. 导入/导出订阅列表

---

## 10. 测试计划

### 10.1 单元测试

- [ ] Subscription 数据类测试
- [ ] SubscriptionManager CRUD 测试
- [ ] SubscriptionManager 去重测试
- [ ] SubscriptionDownloader URL 验证测试
- [ ] SubscriptionDownloader 路径穿越测试
- [ ] JSON 验证测试

### 10.2 集成测试

- [ ] 添加订阅流程测试
- [ ] 更新订阅流程测试
- [ ] 删除订阅流程测试（验证文件删除）
- [ ] 全部更新测试
- [ ] 重命名订阅测试（验证文件重命名）

### 10.3 边界测试

- [ ] 空订阅列表
- [ ] 无效 URL 格式
- [ ] 超大文件下载（>10MB）
- [ ] 网络断开
- [ ] 磁盘空间不足
- [ ] 重复添加同名/同URL订阅

---

## 11. 后续扩展

1. **订阅解析**: 支持解析 base64 编码的订阅链接
2. **多协议支持**: 支持 SS/V2Ray/Trojan 等协议的订阅链接
3. **节点选择**: 从订阅中选择特定节点
4. **延迟测试**: 测试订阅节点延迟
5. **流量统计**: 显示订阅流量使用情况
6. **订阅分组**: 支持文件夹分组管理

---

## 12. 修订记录

| 版本 | 日期 | 修改内容 |
|------|------|----------|
| 1.0 | 2024-01-01 | 初始设计方案 |
| 1.1 | 2024-01-02 | 存储方式从注册表改为 JSON 文件；支持并发下载；添加 UUID 标识；增加离线缓存状态显示 |
| 1.2 | 2026-03-23 | **架构改进**：信号驱动解耦 ConfigManager；**安全增强**：URL验证、路径穿越防护、大小限制、超时控制、日志脱敏；**功能完善**：去重检查、重命名、批量操作、改进错误处理；**UI改进**：多选支持、错误对话框、批量进度显示 |
