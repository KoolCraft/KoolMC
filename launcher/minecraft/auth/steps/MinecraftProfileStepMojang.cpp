#include "MinecraftProfileStepMojang.h"

#include <QNetworkRequest>
#include <QUrl>

#include "Application.h"
#include "minecraft/auth/Parsers.h"
#include "net/NetUtils.h"
#include "net/RawHeaderProxy.h"

MinecraftProfileStepMojang::MinecraftProfileStepMojang(AccountData* data) : AuthStep(data) {}

QString MinecraftProfileStepMojang::getBaseUrl()
{
    if (m_data->type == AccountType::Yggdrasil) {
        QString base = m_data->authlibInjectorApiLocation;
        if (base.isEmpty()) {
            base = m_data->yggdrasilServer;
        }
        while (base.endsWith('/')) {
            base.chop(1);
        }
        return base + "/sessionserver";
    }
    return "";
}

QString MinecraftProfileStepMojang::describe()
{
    return tr("Fetching the Minecraft profile.");
}

void MinecraftProfileStepMojang::perform()
{
    if (m_data->minecraftProfile.id.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_HARD, tr("A UUID is required to get the profile."));
        return;
    }

    QUrl url = getBaseUrl() + "/session/minecraft/profile/" + m_data->minecraftProfile.id;
    auto headers = QList<Net::HeaderPair>{ { "Accept", "application/json" } };

    auto [request, response] = Net::Request::makeByteArray(url);
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request->enableAutoRetry(true);

    m_task.reset(new NetJob("MinecraftProfileStepMojang", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
}

void MinecraftProfileStepMojang::onRequestDone(QByteArray* response)
{
    if (m_request->error() == QNetworkReply::ContentNotFoundError) {
        // NOTE: Succeed even if we do not have a profile. This is a valid account state.
        m_data->minecraftProfile = MinecraftProfile();
        emit finished(AccountTaskState::STATE_WORKING, tr("Account has no Minecraft profile."));
        return;
    }
    if (m_request->error() != QNetworkReply::NoError) {
        qWarning() << "Error getting profile:";
        qWarning() << " HTTP Status       :" << m_request->replyStatusCode();
        qWarning() << " Internal error no.:" << m_request->error();
        qWarning() << " Error string      :" << m_request->errorString();

        qWarning() << " Response:";
        qWarning() << QString::fromUtf8(*response);

        if (Net::isApplicationError(m_request->error()) && !Net::isServerError(m_request->error())) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT,
                          tr("Minecraft Java profile acquisition failed: %1").arg(m_request->errorString()));
        } else {
            m_data->networkError = m_request->error();
            emit finished(AccountTaskState::STATE_OFFLINE,
                          tr("Minecraft Java profile acquisition failed: %1").arg(m_request->errorString()));
        }
        return;
    }

    auto result = Parsers::parseMinecraftProfileMojang(*response, m_data->minecraftProfile);
    if (!result) {
        m_data->minecraftProfile = MinecraftProfile();
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Minecraft Java profile response could not be parsed"));
        return;
    }

    emit finished(AccountTaskState::STATE_WORKING, tr("Minecraft Java profile acquisition succeeded."));
}