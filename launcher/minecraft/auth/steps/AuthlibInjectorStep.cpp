#include "AuthlibInjectorStep.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "Application.h"

AuthlibInjectorStep::AuthlibInjectorStep(AccountData* data) : AuthStep(data) {}

QString AuthlibInjectorStep::describe()
{
    return tr("Fetching authlib injector API URL");
}

void AuthlibInjectorStep::perform()
{
    // Default to the same as the base URL.
    QUrl url(m_data->yggdrasilServer);
    if (url.scheme() != "http" && url.scheme() != "https") {
        url = QUrl("https://" + m_data->yggdrasilServer);
    }
    qDebug() << url << url.toString() << url.isLocalFile();
    m_data->authlibInjectorApiLocation = url.toString();

    QNetworkRequest request(url);
    m_reply = APPLICATION->network()->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &AuthlibInjectorStep::onRequestDone);
    qDebug() << "Fetching authlib injector API URL";
}

void AuthlibInjectorStep::onRequestDone()
{
    if (m_reply->hasRawHeader("x-authlib-injector-api-location")) {
        QString authlibInjectorApiLocationHeader = m_reply->rawHeader("x-authlib-injector-api-location");
        QUrl url = QUrl(authlibInjectorApiLocationHeader);
        if (!url.isValid()) {
            qDebug() << "Invalid Authlib Injector API URL specified by server: " << authlibInjectorApiLocationHeader;
            emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Invalid authlib injector API URL"));
        } else {
            m_data->authlibInjectorApiLocation = authlibInjectorApiLocationHeader;
            qDebug() << "Authlib injector API URL: " << m_data->authlibInjectorApiLocation;
            emit finished(AccountTaskState::STATE_WORKING, tr("Fetched authlib injector API URL"));
        }
    } else {
        qDebug() << "Authlib injector API URL not found";
        emit finished(AccountTaskState::STATE_WORKING, tr("Authlib injector API URL not found, defaulting to the supplied base URL"));
    }
    m_reply->deleteLater();
    m_reply = nullptr;
}