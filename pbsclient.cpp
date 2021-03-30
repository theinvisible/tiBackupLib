#include "pbsclient.h"

#include <QUrlQuery>
#include <QEventLoop>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

pbsClient* pbsClient::_instance = 0;

pbsClient::pbsClient(QObject *parent) : QObject(parent)
{
    nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::sslErrors, this, [](QNetworkReply *reply, const QList<QSslError> &errors) {
        qInfo() << "ignoring sslerror" << errors;
        reply->ignoreSslErrors(errors);
    });
}

HttpStatus::Code pbsClient::auth(const QString &host, const QString &username, const QString &password)
{
    this->host = host;
    this->username = username;
    this->password = password;

    QString url = QString("https://%1:8007/api2/json/access/ticket").arg(host);

    QNetworkRequest request;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setUrl(QUrl(url));

    QUrlQuery postData;
    postData.addQueryItem("username", username);
    postData.addQueryItem("password", password);

    QNetworkReply *reply = nam->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QJsonDocument json = QJsonDocument::fromJson(reply->readAll());
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    ticket = json.object()["data"].toObject()["ticket"].toString();
    CSRF = json.object()["data"].toObject()["CSRFPreventionToken"].toString();

    qInfo() << "replyauth::" << status << url << reply->rawHeaderList() << ticket << CSRF;

    return static_cast<HttpStatus::Code>(status);
}
