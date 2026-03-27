#include "add_subscription_dialog.h"
#include "ui_add_subscription_dialog.h"

#include "subscription_manager.h"
#include "subscription_downloader.h"

#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>

AddSubscriptionDialog::AddSubscriptionDialog(SubscriptionManager *manager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddSubscriptionDialog)
    , m_manager(manager)
{
    ui->setupUi(this);
    m_testNetwork = new QNetworkAccessManager(this);

    connect(ui->testButton, &QPushButton::clicked, this, &AddSubscriptionDialog::onTestButtonClicked);
    connect(ui->downloadButton, &QPushButton::clicked, this, &AddSubscriptionDialog::onDownloadButtonClicked);
    connect(m_testNetwork, &QNetworkAccessManager::finished,
            this, &AddSubscriptionDialog::onTestFinished);

    // Enable interval spinbox only when auto update is checked
    connect(ui->autoUpdateCheckBox, &QCheckBox::toggled, ui->intervalSpinBox, &QSpinBox::setEnabled);
    ui->intervalSpinBox->setEnabled(false);
}

AddSubscriptionDialog::~AddSubscriptionDialog()
{
    delete ui;
}

QString AddSubscriptionDialog::name() const
{
    return ui->nameEdit->text().trimmed();
}

QString AddSubscriptionDialog::url() const
{
    return ui->urlEdit->text().trimmed();
}

bool AddSubscriptionDialog::autoUpdate() const
{
    return ui->autoUpdateCheckBox->isChecked();
}

int AddSubscriptionDialog::updateInterval() const
{
    return ui->intervalSpinBox->value();
}

void AddSubscriptionDialog::onTestButtonClicked()
{
    QString error;
    if (!validateInput(error)) {
        showValidationError(error);
        return;
    }

    setUiEnabled(false);
    ui->statusLabel->setText(tr("Testing connection..."));

    QUrl url(ui->urlEdit->text().trimmed());
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    m_testNetwork->get(request);
}

void AddSubscriptionDialog::onDownloadButtonClicked()
{
    QString error;
    if (!validateInput(error)) {
        showValidationError(error);
        return;
    }

    // Check for duplicates
    if (m_manager->isNameExists(name())) {
        QMessageBox::warning(this, tr("Error"), tr("Subscription name already exists"));
        return;
    }

    if (m_manager->isUrlExists(url())) {
        QMessageBox::warning(this, tr("Error"), tr("This URL has already been added"));
        return;
    }

    accept();
}

void AddSubscriptionDialog::onTestFinished(QNetworkReply *reply)
{
    setUiEnabled(true);

    if (reply->error() != QNetworkReply::NoError) {
        ui->statusLabel->setText(tr("Connection failed: %1").arg(reply->errorString()));
        m_urlValid = false;
    } else {
        QByteArray data = reply->readAll();

        QString validationError;
        if (SubscriptionDownloader::isValidSingBoxConfig(data, validationError)) {
            ui->statusLabel->setText(tr("✓ Valid sing-box config"));
            m_urlValid = true;
        } else {
            ui->statusLabel->setText(tr("✗ %1").arg(validationError));
            m_urlValid = false;
        }
    }

    reply->deleteLater();
}

void AddSubscriptionDialog::setUiEnabled(bool enabled)
{
    ui->nameEdit->setEnabled(enabled);
    ui->urlEdit->setEnabled(enabled);
    ui->autoUpdateCheckBox->setEnabled(enabled);
    ui->testButton->setEnabled(enabled);
    ui->downloadButton->setEnabled(enabled);
    ui->buttonBox->setEnabled(enabled);
}

bool AddSubscriptionDialog::validateInput(QString &error)
{
    QString subName = name();
    if (subName.isEmpty()) {
        error = tr("Subscription name cannot be empty");
        return false;
    }

    QString subUrl = url();
    if (subUrl.isEmpty()) {
        error = tr("Subscription URL cannot be empty");
        return false;
    }

    QUrl urlObj(subUrl);
    if (!urlObj.isValid()) {
        error = tr("Invalid URL format");
        return false;
    }

    QString scheme = urlObj.scheme().toLower();
    if (scheme != "http" && scheme != "https") {
        error = tr("Only HTTP/HTTPS protocols are supported");
        return false;
    }

    if (urlObj.host().isEmpty()) {
        error = tr("Invalid host name");
        return false;
    }

    return true;
}

void AddSubscriptionDialog::showValidationError(const QString &error)
{
    ui->statusLabel->setText(tr("✗ %1").arg(error));
}
