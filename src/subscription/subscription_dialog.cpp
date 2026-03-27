#include "subscription_dialog.h"
#include "ui_subscription_dialog.h"

#include "subscription_manager.h"
#include "subscription.h"
#include "add_subscription_dialog.h"

#include <QMessageBox>
#include <QMenu>
#include <QProgressBar>
#include <QInputDialog>
#include <QLineEdit>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>

SubscriptionDialog::SubscriptionDialog(SubscriptionManager *manager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SubscriptionDialog)
    , m_manager(manager)
{
    ui->setupUi(this);

    setupConnections();
    setupContextMenu();
    refreshSubscriptionList();
}

SubscriptionDialog::~SubscriptionDialog()
{
    qDeleteAll(m_progressBars);
    delete ui;
}

void SubscriptionDialog::setupConnections()
{
    connect(ui->addButton, &QPushButton::clicked, this, &SubscriptionDialog::onAddButtonClicked);
    connect(ui->updateSelectedButton, &QPushButton::clicked, this, &SubscriptionDialog::onUpdateSelectedButtonClicked);
    connect(ui->updateAllButton, &QPushButton::clicked, this, &SubscriptionDialog::onUpdateAllButtonClicked);
    connect(ui->deleteSelectedButton, &QPushButton::clicked, this, &SubscriptionDialog::onDeleteSelectedButtonClicked);
    connect(ui->renameButton, &QPushButton::clicked, this, &SubscriptionDialog::onRenameActionTriggered);

    connect(m_manager, &SubscriptionManager::subscriptionDownloadStarted,
            this, &SubscriptionDialog::onSubscriptionDownloadStarted);
    connect(m_manager, &SubscriptionManager::subscriptionDownloadFinished,
            this, &SubscriptionDialog::onSubscriptionDownloadFinished);
    connect(m_manager, &SubscriptionManager::allSubscriptionsUpdated,
            this, &SubscriptionDialog::onAllSubscriptionsUpdated);
    connect(m_manager->downloader(), &SubscriptionDownloader::downloadProgress,
            this, &SubscriptionDialog::onDownloadProgress);

    connect(ui->subscriptionTable, &QTableWidget::cellDoubleClicked,
            this, &SubscriptionDialog::onTableItemDoubleClicked);
    connect(ui->subscriptionTable, &QTableWidget::customContextMenuRequested,
            this, &SubscriptionDialog::onCustomContextMenu);

    connect(m_manager, &SubscriptionManager::subscriptionListChanged,
            this, &SubscriptionDialog::refreshSubscriptionList);
}

void SubscriptionDialog::setupContextMenu()
{
    ui->subscriptionTable->setContextMenuPolicy(Qt::CustomContextMenu);
}

void SubscriptionDialog::refreshSubscriptionList()
{
    ui->subscriptionTable->setRowCount(0);

    QList<Subscription> subs = m_manager->subscriptions();
    ui->subscriptionTable->setRowCount(subs.size());

    for (int i = 0; i < subs.size(); ++i) {
        updateItemStatus(i, subs[i]);
    }

    ui->subscriptionTable->resizeColumnsToContents();
}

void SubscriptionDialog::updateItemStatus(int row, const Subscription &sub)
{
    // Status icon
    QTableWidgetItem *statusItem = new QTableWidgetItem(getStatusIcon(sub));
    statusItem->setData(Qt::UserRole, sub.id());  // Store ID for lookup
    ui->subscriptionTable->setItem(row, 0, statusItem);

    // Name
    QTableWidgetItem *nameItem = new QTableWidgetItem(sub.name());
    ui->subscriptionTable->setItem(row, 1, nameItem);

    // URL (truncated for display)
    QString displayUrl = sub.url();
    if (displayUrl.length() > 50) {
        displayUrl = displayUrl.left(47) + "...";
    }
    QTableWidgetItem *urlItem = new QTableWidgetItem(displayUrl);
    urlItem->setToolTip(sub.url());
    ui->subscriptionTable->setItem(row, 2, urlItem);

    // Last updated
    QTableWidgetItem *updatedItem = new QTableWidgetItem(formatLastUpdated(sub));
    ui->subscriptionTable->setItem(row, 3, updatedItem);
}

QString SubscriptionDialog::getStatusIcon(const Subscription &sub) const
{
    if (m_downloadingIds.contains(sub.id())) {
        return tr("🔄 Updating");
    }
    if (!sub.isCached()) {
        return tr("⚪ Not cached");
    }
    return tr("🟢 Cached");
}

QString SubscriptionDialog::formatLastUpdated(const Subscription &sub) const
{
    if (m_downloadingIds.contains(sub.id())) {
        return tr("Updating...");
    }

    if (!sub.lastUpdated().isValid()) {
        return tr("-");
    }

    return sub.lastUpdated().toString("yyyy-MM-dd HH:mm");
}

void SubscriptionDialog::onAddButtonClicked()
{
    showAddSubscriptionDialog();
}

void SubscriptionDialog::showAddSubscriptionDialog()
{
    AddSubscriptionDialog dialog(m_manager, this);
    if (dialog.exec() == QDialog::Accepted) {
        // Use the constructor that generates ID
        Subscription sub(dialog.name(), dialog.url());
        sub.setAutoUpdate(dialog.autoUpdate());
        sub.setUpdateInterval(dialog.updateInterval());
        sub.setConfigPath(m_manager->generateConfigFileName(dialog.name(), sub.id()));

        // Add subscription with download
        m_manager->addSubscriptionWithDownload(sub);
    }
}

void SubscriptionDialog::onUpdateSelectedButtonClicked()
{
    QList<QTableWidgetItem*> selectedItems = ui->subscriptionTable->selectedItems();
    QSet<int> selectedRows;

    for (QTableWidgetItem *item : selectedItems) {
        selectedRows.insert(item->row());
    }

    if (selectedRows.isEmpty()) {
        QMessageBox::information(this, tr("Info"), tr("Please select at least one subscription"));
        return;
    }

    for (int row : selectedRows) {
        QString id = ui->subscriptionTable->item(row, 0)->data(Qt::UserRole).toString();
        m_manager->updateSubscriptionById(id);
    }
}

void SubscriptionDialog::onUpdateAllButtonClicked()
{
    if (m_manager->subscriptionCount() == 0) {
        QMessageBox::information(this, tr("Info"), tr("No subscriptions to update"));
        return;
    }

    m_manager->updateAllSubscriptions();
}

void SubscriptionDialog::onDeleteSelectedButtonClicked()
{
    QList<QTableWidgetItem*> selectedItems = ui->subscriptionTable->selectedItems();
    QSet<int> selectedRows;

    for (QTableWidgetItem *item : selectedItems) {
        selectedRows.insert(item->row());
    }

    if (selectedRows.isEmpty()) {
        QMessageBox::information(this, tr("Info"), tr("Please select at least one subscription"));
        return;
    }

    int ret = QMessageBox::question(this, tr("Confirm"),
                                    tr("Delete %1 subscription(s)?").arg(selectedRows.size()));
    if (ret != QMessageBox::Yes) {
        return;
    }

    // Get IDs before removing (since indices will shift)
    QStringList idsToDelete;
    for (int row : selectedRows) {
        QString id = ui->subscriptionTable->item(row, 0)->data(Qt::UserRole).toString();
        idsToDelete.append(id);
    }

    for (const QString &id : idsToDelete) {
        m_manager->removeSubscription(id);
    }
}

void SubscriptionDialog::onRenameActionTriggered()
{
    int row = ui->subscriptionTable->currentRow();
    if (row < 0) {
        return;
    }

    QString id = ui->subscriptionTable->item(row, 0)->data(Qt::UserRole).toString();
    Subscription sub = m_manager->subscription(id);
    if (sub.id().isEmpty()) {
        return;
    }

    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename Subscription"),
                                             tr("New name:"), QLineEdit::Normal,
                                             sub.name(), &ok);
    if (ok && !newName.isEmpty() && newName != sub.name()) {
        if (m_manager->isNameExists(newName, id)) {
            QMessageBox::warning(this, tr("Error"), tr("Subscription name already exists"));
            return;
        }
        m_manager->renameSubscription(id, newName);
    }
}

void SubscriptionDialog::onSubscriptionDownloadStarted(const QString &id, const QString &name)
{
    m_downloadingIds.insert(id);
    refreshSubscriptionList();

    // Update button states
    ui->updateSelectedButton->setEnabled(false);
    ui->updateAllButton->setEnabled(false);
}

void SubscriptionDialog::onSubscriptionDownloadFinished(const QString &id, bool success, const QString &error)
{
    m_downloadingIds.remove(id);

    // Remove progress bar if exists
    if (m_progressBars.contains(id)) {
        delete m_progressBars.take(id);
    }

    if (!success) {
        Subscription sub = m_manager->subscription(id);
        showErrorDialog(tr("Download Failed"), sub.name(), error);
    }

    refreshSubscriptionList();

    // Re-enable buttons if no more downloads
    if (m_downloadingIds.isEmpty()) {
        ui->updateSelectedButton->setEnabled(true);
        ui->updateAllButton->setEnabled(true);
    }
}

void SubscriptionDialog::onAllSubscriptionsUpdated(int successCount, int failCount)
{
    QString message = tr("Updated: %1 success, %2 failed").arg(successCount).arg(failCount);
    if (failCount > 0) {
        QMessageBox::warning(this, tr("Batch Update Complete"), message);
    } else {
        QMessageBox::information(this, tr("Batch Update Complete"), message);
    }
}

void SubscriptionDialog::onDownloadProgress(const QString &downloadId, qint64 received, qint64 total)
{
    // Find the subscription by downloadId - we'd need to track this
    // For now, we'll skip progress display
    Q_UNUSED(downloadId);
    Q_UNUSED(received);
    Q_UNUSED(total);
}

void SubscriptionDialog::onTableItemDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    QString id = ui->subscriptionTable->item(row, 0)->data(Qt::UserRole).toString();
    m_manager->updateSubscriptionById(id);
}

void SubscriptionDialog::onCustomContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    menu.addAction(tr("Update"), this, &SubscriptionDialog::onUpdateSelectedButtonClicked);
    menu.addAction(tr("Delete"), this, &SubscriptionDialog::onDeleteSelectedButtonClicked);
    menu.addAction(tr("Rename"), this, &SubscriptionDialog::onRenameActionTriggered);
    menu.addSeparator();

    QAction *copyUrlAction = menu.addAction(tr("Copy URL"));
    connect(copyUrlAction, &QAction::triggered, this, [this]() {
        int row = ui->subscriptionTable->currentRow();
        if (row >= 0) {
            QString id = ui->subscriptionTable->item(row, 0)->data(Qt::UserRole).toString();
            Subscription sub = m_manager->subscription(id);
            QApplication::clipboard()->setText(sub.url());
        }
    });

    menu.exec(ui->subscriptionTable->mapToGlobal(pos));
}

void SubscriptionDialog::showErrorDialog(const QString &title, const QString &subscriptionName,
                                         const QString &error, bool showRetry)
{
    QString message = tr("Subscription: %1\n\nError: %2").arg(subscriptionName).arg(error);

    if (showRetry) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(title);
        msgBox.setText(message);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.addButton(QMessageBox::Close);
        msgBox.addButton(tr("Retry"), QMessageBox::ActionRole);
        msgBox.addButton(tr("Edit URL"), QMessageBox::ActionRole);

        int result = msgBox.exec();
        // Handle retry or edit URL if needed
    } else {
        QMessageBox::warning(this, title, message);
    }
}
