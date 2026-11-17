#pragma once

#include "minecraft/auth/AuthStep.h"
#include "net/NetJob.h"
#include "net/Request.h"

class MinecraftProfileStepMojang : public AuthStep {
    Q_OBJECT
public:
    explicit MinecraftProfileStepMojang(AccountData* data);
    virtual ~MinecraftProfileStepMojang() noexcept = default;

    QString describe() override;
    void perform() override;

private slots:
    void onRequestDone(QByteArray* response);

private:
    QString getBaseUrl();
    Net::Request::Ptr m_request;
    NetJob::Ptr m_task;
};