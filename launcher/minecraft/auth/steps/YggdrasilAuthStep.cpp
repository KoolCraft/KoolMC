#include "YggdrasilAuthStep.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QUuid>

#include "Application.h"
#include "Logging.h"
#include "net/NetUtils.h"
#include "net/RawHeaderProxy.h"
#include "net/Request.h"

YggdrasilAuthStep::YggdrasilAuthStep(AccountData* data, bool refresh) : AuthStep(data), m_refresh(refresh) {}

QString YggdrasilAuthStep::describe()
{
    return m_refresh ? tr("Refreshing Yggdrasil session") : tr("Logging in to Yggdrasil server");
}

static QString normalizeServer(const QString& server)
{
    QString s = server.trimmed();
    while (s.endsWith('/')) {
        s.chop(1);
    }
    return s;
}

QString YggdrasilAuthStep::baseUrl() const
{
    if (!m_data->authlibInjectorApiLocation.isEmpty()) {
        return normalizeServer(m_data->authlibInjectorApiLocation) + "/authserver";
    }
    return normalizeServer(m_data->yggdrasilServer) + "/authserver";
}

void YggdrasilAuthStep::perform()
{
    QString server = baseUrl();
    if (server.isEmpty() || server == "/authserver") {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Yggdrasil server URL is empty."));
        return;
    }

    QString clientToken = m_data->yggdrasilToken.extra.value("clientToken").toString();
    if (clientToken.isEmpty()) {
        clientToken = QUuid::createUuid().toString(QUuid::Id128);
        m_data->yggdrasilToken.extra["clientToken"] = clientToken;
    }

    QUrl url;
    QByteArray body;
    if (m_refresh && !m_data->yggdrasilToken.token.isEmpty()) {
        url = QUrl(server + "/refresh");
        QJsonObject req;
        req.insert("clientToken", clientToken);
        req.insert("accessToken", m_data->yggdrasilToken.token);
        req.insert("requestUser", false);
        body = QJsonDocument(req).toJson();
    } else {
        url = QUrl(server + "/authenticate");
        QString username = m_data->yggdrasilToken.extra.value("username").toString();
        if (username.isEmpty()) {
            username = m_data->yggdrasilToken.extra.value("userName").toString();
        }
        QString password = m_data->yggdrasilToken.extra.value("password").toString();

        QJsonObject agent;
        agent.insert("name", QString("Minecraft"));
        agent.insert("version", 1);

        QJsonObject req;
        req.insert("agent", agent);
        req.insert("username", username);
        req.insert("password", password);
        req.insert("requestUser", false);
        req.insert("clientToken", clientToken);
        body = QJsonDocument(req).toJson();
    }

    auto headers = QList<Net::HeaderPair>{
        { "Content-Type", "application/json" },
        { "Accept", "application/json" },
    };

    auto [request, response] = Net::Request::makeByteArray(url, body);
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request->enableAutoRetry(true);

    m_task.reset(new NetJob("YggdrasilAuthStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
    qDebug() << "Yggdrasil auth request to" << url;
}

void YggdrasilAuthStep::onRequestDone(QByteArray* response)
{
    if (m_request->error() != QNetworkReply::NoError) {
        qWarning() << "Reply error:" << m_request->error();
        if (Net::isApplicationError(m_request->error()) && !Net::isServerError(m_request->error())) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT,
                          tr("Failed to authenticate with Yggdrasil server: %1").arg(m_request->errorString()));
        } else {
            m_data->networkError = m_request->error();
            emit finished(AccountTaskState::STATE_OFFLINE, tr("Failed to authenticate with Yggdrasil server: %1").arg(m_request->errorString()));
        }
        return;
    }

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(*response, &jsonError);
    if (jsonError.error != QJsonParseError::NoError) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT,
                      tr("Failed to parse authentication server response JSON response: %1 at offset %2.")
                          .arg(jsonError.errorString())
                          .arg(jsonError.offset));
        return;
    }

    QJsonObject obj = doc.object();
    if (obj.contains("error")) {
        QJsonValue errorVal = obj.value("error");
        QJsonValue errorMessageValue = obj.value("errorMessage");
        if (errorVal.isString() && errorMessageValue.isString()) {
            emit finished(AccountTaskState::STATE_FAILED_HARD, errorMessageValue.toString());
        } else {
            emit finished(AccountTaskState::STATE_FAILED_HARD, tr("An unknown Yggdrasil error occurred."));
        }
        return;
    }

    QString responseClientToken = obj.value("clientToken").toString();
    if (responseClientToken.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Authentication server didn't send a client token."));
        return;
    }
    QString clientToken = m_data->yggdrasilToken.extra.value("clientToken").toString();
    if (clientToken.isEmpty()) {
        m_data->yggdrasilToken.extra["clientToken"] = responseClientToken;
    } else if (clientToken != responseClientToken) {
        emit finished(AccountTaskState::STATE_FAILED_HARD,
                      tr("Authentication server attempted to change the client token. This isn't supported."));
        return;
    }

    QString accessToken = obj.value("accessToken").toString();
    if (accessToken.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Authentication server didn't send an access token."));
        return;
    }

    QJsonObject selectedProfile = obj.value("selectedProfile").toObject();
    if (selectedProfile.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Authentication server didn't send a selected profile."));
        return;
    }

    m_data->yggdrasilToken.token = accessToken;
    m_data->yggdrasilToken.refresh_token.clear();
    m_data->yggdrasilToken.extra["clientToken"] = responseClientToken;
    m_data->yggdrasilToken.extra.remove("password");
    m_data->yggdrasilToken.issueInstant = QDateTime::currentDateTimeUtc();
    m_data->yggdrasilToken.notAfter = QDateTime::currentDateTimeUtc().addSecs(24 * 3600);
    m_data->yggdrasilToken.validity = Validity::Certain;

    m_data->minecraftProfile.id = selectedProfile.value("id").toString();
    m_data->minecraftProfile.name = selectedProfile.value("name").toString();
    m_data->minecraftProfile.validity = Validity::Certain;

    m_data->minecraftEntitlement.ownsMinecraft = true;
    m_data->minecraftEntitlement.canPlayMinecraft = true;
    m_data->minecraftEntitlement.validity = Validity::Certain;

    m_data->validity_ = Validity::Certain;

    emit finished(AccountTaskState::STATE_WORKING, tr("Authenticated with Yggdrasil server"));
}