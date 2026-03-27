#ifndef SUBSCRIPTION_DIALOG_H
#define SUBSCRIPTION_DIALOG_H

#include <QDialog>
#include <QProgressBar>
#include <QSet>
#include <QMap>

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
    void updateItemStatus(int row, const Subscription &sub);
    void onTableItemDoubleClicked(int row, int column);
    void onCustomContextMenu(const QPoint &pos);

private:
    void setupConnections();
    void setupContextMenu();
    void showAddSubscriptionDialog();
    void showErrorDialog(const QString &title, const QString &subscriptionName,
                        const QString &error, bool showRetry = true);
    QString formatLastUpdated(const Subscription &sub) const;
    QString getStatusIcon(const Subscription &sub) const;

    Ui::SubscriptionDialog *ui;
    SubscriptionManager *m_manager;
    QMap<QString, QProgressBar*> m_progressBars;
    QSet<QString> m_downloadingIds;
};

#endif // SUBSCRIPTION_DIALOG_H
