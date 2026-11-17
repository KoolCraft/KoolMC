#pragma once
#include <QObject>

#include "minecraft/auth/AuthStep.h"
#include "net/NetJob.h"
#include "net/Request.h"

class YggdrasilAuthStep : public AuthStep {
    Q_OBJECT
public:
    explicit YggdrasilAuthStep(AccountData* data, bool refresh = false);
    virtual ~YggdrasilAuthStep() noexcept = default;

    void perform() override;

    QString describe() override;

private slots:
    void onRequestDone(QByteArray* response);

private:
    bool m_refresh;
    QString baseUrl() const;
    Net::Request::Ptr m_request;
    NetJob::Ptr m_task;
};