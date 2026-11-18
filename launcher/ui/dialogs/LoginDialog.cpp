#include "LoginDialog.h"
#include "ui_LoginDialog.h"

#include "minecraft/auth/AuthFlow.h"

#include <QtWidgets/QPushButton>

LoginDialog::LoginDialog(QWidget* parent, AccountType type) : QDialog(parent), ui(new Ui::LoginDialog), m_accountType{type}
{
    ui->setupUi(this);
    ui->authlibInjectorBaseTextBox->setVisible(m_accountType == AccountType::Yggdrasil);
    ui->progressBar->setVisible(false);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Keep OK/Cancel consistent with the other account actions: no mnemonics/shortcuts.
    if (auto* okButton = ui->buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setText(tr("OK"));
    }
    if (auto* cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelButton->setText(tr("Cancel"));
    }
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::accept()
{
    setUserInputsEnabled(false);
    ui->progressBar->setVisible(true);

    if (m_accountType == AccountType::Yggdrasil) {
        m_account = MinecraftAccount::createYggdrasil(ui->authlibInjectorBaseTextBox->text().trimmed());
    } else {
        m_account = MinecraftAccount::createOffline(ui->userTextBox->text().trimmed());
    }

    if (m_account) {
        if (m_accountType == AccountType::Yggdrasil) {
            m_account->accountData()->yggdrasilToken.extra["username"] = ui->userTextBox->text().trimmed();
            m_account->accountData()->yggdrasilToken.extra["password"] = ui->passTextBox->text();
            m_account->accountData()->yggdrasilToken.extra["userName"] = ui->userTextBox->text().trimmed();
        }
    }

    m_loginTask = m_account->login();
    connect(m_loginTask.get(), &Task::failed, this, &LoginDialog::onTaskFailed);
    connect(m_loginTask.get(), &Task::succeeded, this, &LoginDialog::onTaskSucceeded);
    connect(m_loginTask.get(), &Task::status, this, &LoginDialog::onTaskStatus);
    connect(m_loginTask.get(), &Task::progress, this, &LoginDialog::onTaskProgress);
    m_loginTask->start();
}

void LoginDialog::setUserInputsEnabled(bool enable)
{
    ui->authlibInjectorBaseTextBox->setEnabled(enable);
    ui->userTextBox->setEnabled(enable);
    ui->passTextBox->setEnabled(enable);
    ui->buttonBox->setEnabled(enable);
}

void LoginDialog::on_userTextBox_textEdited(const QString& newText)
{
    ui->buttonBox->button(QDialogButtonBox::Ok)
        ->setEnabled(!newText.isEmpty() && !ui->passTextBox->text().isEmpty());
}

void LoginDialog::on_passTextBox_textEdited(const QString& newText)
{
    ui->buttonBox->button(QDialogButtonBox::Ok)
        ->setEnabled(!newText.isEmpty() && !ui->userTextBox->text().isEmpty());
}

void LoginDialog::onTaskFailed(const QString& reason)
{
    auto lines = reason.split('\n');
    QString processed;
    for (const auto& line : lines) {
        if (line.size()) {
            processed += "<font color='red'>" + line + "</font><br />";
        } else {
            processed += "<br />";
        }
    }
    ui->label->setText(processed);

    setUserInputsEnabled(true);
    ui->progressBar->setVisible(false);
}

void LoginDialog::onTaskSucceeded()
{
    QDialog::accept();
}

void LoginDialog::onTaskStatus(const QString& status)
{
    ui->label->setText(status);
}

void LoginDialog::onTaskProgress(qint64 current, qint64 total)
{
    ui->progressBar->setMaximum(total);
    ui->progressBar->setValue(current);
}

MinecraftAccountPtr LoginDialog::newAccount(QWidget* parent, QString msg, AccountType type)
{
    LoginDialog dlg(parent, type);
    dlg.ui->label->setText(msg);
    if (dlg.exec() == QDialog::Accepted) {
        return dlg.m_account;
    }
    return nullptr;
}