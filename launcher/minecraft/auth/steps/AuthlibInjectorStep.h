#pragma once

#include "minecraft/auth/AuthStep.h"

class QNetworkReply;

class AuthlibInjectorStep : public AuthStep {
    Q_OBJECT
public:
    explicit AuthlibInjectorStep(AccountData* data);
    virtual ~AuthlibInjectorStep() noexcept = default;

    QString describe() override;
    void perform() override;

private slots:
    void onRequestDone();

private:
    QNetworkReply* m_reply = nullptr;
};