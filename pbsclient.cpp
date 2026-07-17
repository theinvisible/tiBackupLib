#include "pbsclient.h"

#include <QUrlQuery>
#include <QEventLoop>
#include <QNetworkReply>
#include <QJsonObject>
#include <QNetworkCookieJar>
#include <QNetworkCookie>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QCryptographicHash>

pbsClient::pbsClient(QObject *parent) : QObject(parent)
{
    nam = new QNetworkAccessManager(this);
    // Bound each request so a stuck/unreachable PBS can't block the caller
    // indefinitely (the REST calls wait on a nested QEventLoop with no timeout
    // of their own). Available since Qt 5.15.
    nam->setTransferTimeout(20000);
    connect(nam, &QNetworkAccessManager::sslErrors, this,
            [this](QNetworkReply *reply, const QList<QSslError> &errors) {
        // Verify the presented leaf certificate against the pinned fingerprint
        // instead of accepting anything. PBS uses a self-signed cert, so we can't
        // rely on the CA chain; the SHA-256 fingerprint is the trust anchor.
        const QSslCertificate cert = reply->sslConfiguration().peerCertificate();
        if(cert.isNull())
            return;   // no cert to verify -> leave the error standing (fail closed)

        lastPeerFingerprint = fingerprintOf(cert);

        if(!expectedFingerprint.isEmpty() &&
           fingerprintMatches(lastPeerFingerprint, expectedFingerprint))
        {
            reply->ignoreSslErrors(errors);   // pinned cert matches -> accept
            return;
        }

        if(captureMode)
        {
            reply->ignoreSslErrors(errors);   // explicit admin probe (trust on first use)
            return;
        }

        // Otherwise leave the SSL error standing: the request fails rather than
        // trusting an unpinned/mismatched certificate.
        qWarning() << "pbsClient: TLS fingerprint mismatch or no pin; rejecting connection to" << host;
    });
    host = "";
    ticket = "";
    CSRF = "";
}

void pbsClient::setExpectedFingerprint(const QString &fp)
{
    expectedFingerprint = fp;
}

void pbsClient::setCaptureMode(bool on)
{
    captureMode = on;
}

QString pbsClient::capturedFingerprint() const
{
    return lastPeerFingerprint;
}

QString pbsClient::normalizeFingerprint(const QString &fp)
{
    QString out;
    out.reserve(fp.size());
    for(const QChar c : fp)
        if(c.isLetterOrNumber())
            out.append(c.toUpper());
    return out;
}

QString pbsClient::fingerprintOf(const QSslCertificate &cert)
{
    // Upper-case colon-separated hex, matching Proxmox's fingerprint convention.
    return QString::fromLatin1(cert.digest(QCryptographicHash::Sha256).toHex(':')).toUpper();
}

bool pbsClient::fingerprintMatches(const QString &a, const QString &b)
{
    const QString na = normalizeFingerprint(a);
    const QString nb = normalizeFingerprint(b);
    if(na.isEmpty() || na.size() != nb.size())
        return false;
    // Constant-time over equal length (harmless hardening; the fingerprint is public).
    quint16 diff = 0;
    for(int i = 0; i < na.size(); ++i)
        diff |= static_cast<quint16>(na.at(i).unicode() ^ nb.at(i).unicode());
    return diff == 0;
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
    postData.addQueryItem("password", QUrl::toPercentEncoding(password));

    QNetworkReply *reply = nam->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QJsonDocument json = QJsonDocument::fromJson(reply->readAll());
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    ticket = json.object()["data"].toObject()["ticket"].toString();
    CSRF = json.object()["data"].toObject()["CSRFPreventionToken"].toString();

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

pbsClient::HttpResponse pbsClient::getDatastoreSnapshots(const QString &datastore, const QString &backupid, const QString &backuptype)
{
    QUrlQuery q;
    if(backupid != "")
        q.addQueryItem("backup-id", backupid);
    if(backuptype != "")
        q.addQueryItem("backup-type", backuptype);
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

pbsClient::HttpResponse pbsClient::getBackupFiles(const QString &datastore, const QString &backupid, int backuptime, const QString &backuptype)
{
    QUrlQuery q;
    q.addQueryItem("backup-id", backupid);
    q.addQueryItem("backup-time", QString::number(backuptime));
    q.addQueryItem("backup-type", backuptype);
    QString url = genPBSAPIPath(QString("json/admin/datastore/%1/files").arg(datastore), q);

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
