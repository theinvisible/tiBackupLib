#include "pbsclient.h"

#include <QUrlQuery>
#include <QEventLoop>
#include <QNetworkReply>
#include <QJsonObject>
#include <QNetworkCookieJar>
#include <QNetworkCookie>

pbsClient* pbsClient::_instance = 0;

pbsClient::pbsClient(QObject *parent) : QObject(parent)
{
    nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::sslErrors, this, [](QNetworkReply *reply, const QList<QSslError> &errors) {
        qInfo() << "ignoring sslerror" << errors;
        reply->ignoreSslErrors(errors);
    });
    host = "";
    ticket = "";
    CSRF = "";
}

QNetworkRequest pbsClient::getNRAuth(const QString &url)
{
    QNetworkRequest request;
    request.setRawHeader("Accept", "application/json");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue(nam->cookieJar()->cookiesForUrl(QUrl("/"))));
    request.setUrl(QUrl(url));
    return request;
}

HttpStatus::Code pbsClient::auth(const QString &host, const QString &username, const QString &password)
{
    return auth(host, 8007, username, password);
}

HttpStatus::Code pbsClient::auth(const QString &host, int port, const QString &username, const QString &password)
{
    this->host = host;
    this->username = username;
    this->password = password;
    this->port = port;

    QString url = genPBSAPIPath("json/access/ticket");

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
    reply->deleteLater();

    ticket = json.object()["data"].toObject()["ticket"].toString();
    CSRF = json.object()["data"].toObject()["CSRFPreventionToken"].toString();

    qInfo() << "replyauth::" << status << url << reply->rawHeaderList() << ticket << CSRF;
    if(status == HttpStatus::Code::OK)
    {
        QNetworkCookie cookie = QNetworkCookie("PBSAuthCookie", ticket.toUtf8());
        nam->cookieJar()->insertCookie(cookie);
    }

    return static_cast<HttpStatus::Code>(status);
}

QString pbsClient::genPBSAPIPath(const QString &path)
{
    QString url = QString("https://%1:%2/api2/%3").arg(host).arg(port).arg(path);
    return url;
}

QString pbsClient::genPBSAPIPath(const QString &path, const QUrlQuery &query)
{
    QUrl url = QUrl(QString("https://%1:%2/api2/%3").arg(host).arg(port).arg(path));
    url.setQuery(query);
    return url.toString();
}

pbsClient::HttpResponse pbsClient::getDatastores()
{
    QString url = genPBSAPIPath("json/admin/datastore");

    QNetworkReply *reply = nam->get(getNRAuth(url));
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QByteArray rdata = reply->readAll();

    HttpResponse res;
    res.status = static_cast<HttpStatus::Code>(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    res.data = QJsonDocument::fromJson(rdata);

    if(res.status != HttpStatus::Code::OK)
        qInfo() << res.status << url << rdata;

    return res;
}

pbsClient::HttpResponse pbsClient::getDatastoreSnapshots(const QString &datastore, const QString &backupid)
{
    QUrlQuery q;
    if(backupid != "")
        q.addQueryItem("backup-id", backupid);
    QString url = genPBSAPIPath(QString("json/admin/datastore/%1/snapshots").arg(datastore), q);

    QNetworkReply *reply = nam->get(getNRAuth(url));
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QByteArray rdata = reply->readAll();

    HttpResponse res;
    res.status = static_cast<HttpStatus::Code>(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    res.data = QJsonDocument::fromJson(rdata);

    if(res.status != HttpStatus::Code::OK)
        qInfo() << res.status << url << rdata;

    return res;
}

pbsClient::HttpResponse pbsClient::getDatastoreGroups(const QString &datastore)
{
    QString url = genPBSAPIPath(QString("json/admin/datastore/%1/groups").arg(datastore));

    QNetworkReply *reply = nam->get(getNRAuth(url));
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QByteArray rdata = reply->readAll();

    HttpResponse res;
    res.status = static_cast<HttpStatus::Code>(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    res.data = QJsonDocument::fromJson(rdata);

    if(res.status != HttpStatus::Code::OK)
        qInfo() << res.status << url << rdata;

    return res;
}

pbsClient::HttpResponseRaw pbsClient::getBackupFile(const QString &datastore, const QString &backupid, int backuptime, const QString &backuptype, const QString &filename)
{
    QUrlQuery q;
    q.addQueryItem("backup-id", backupid);
    q.addQueryItem("backup-time", QString::number(backuptime));
    q.addQueryItem("backup-type", backuptype);
    q.addQueryItem("file-name", filename);
    QString url = genPBSAPIPath(QString("json/admin/datastore/%1/download-decoded").arg(datastore), q);

    QNetworkReply *reply = nam->get(getNRAuth(url));
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QByteArray rdata = reply->readAll();

    HttpResponseRaw res;
    res.status = static_cast<HttpStatus::Code>(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    res.data = rdata;

    if(res.status != HttpStatus::Code::OK)
        qInfo() << res.status << url << rdata;

    return res;
}
