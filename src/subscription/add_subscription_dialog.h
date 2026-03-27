#ifndef ADD_SUBSCRIPTION_DIALOG_H
#define ADD_SUBSCRIPTION_DIALOG_H

#include <QDialog>
#include <QNetworkAccessManager>

class SubscriptionManager;

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
