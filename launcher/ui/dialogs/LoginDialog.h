#pragma once

#include <QtWidgets/QDialog>
#include <QtCore/QEventLoop>

#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/auth/AuthFlow.h"
#include "tasks/Task.h"

namespace Ui
{
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    ~LoginDialog();

    static MinecraftAccountPtr newAccount(QWidget* parent, QString message, AccountType type = AccountType::Yggdrasil);

private:
    explicit LoginDialog(QWidget* parent = nullptr, AccountType type = AccountType::Yggdrasil);

    void setUserInputsEnabled(bool enable);

protected slots:
    void accept() override;

    void onTaskFailed(const QString& reason);
    void onTaskSucceeded();
    void onTaskStatus(const QString& status);
    void onTaskProgress(qint64 current, qint64 total);

    void on_userTextBox_textEdited(const QString& newText);
    void on_passTextBox_textEdited(const QString& newText);

private:
    Ui::LoginDialog* ui;
    MinecraftAccountPtr m_account;
    shared_qobject_ptr<AuthFlow> m_loginTask;
    AccountType m_accountType;
};