#ifndef PBSCLIENT_H
#define PBSCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonDocument>

#include "obj/HttpStatusCodes.h"

class pbsClient : public QObject
{
    Q_OBJECT
private:
    QString host;
    QString username;
    QString password;
    int port;

    QString ticket;
    QString CSRF;

    QNetworkAccessManager *nam;

    explicit pbsClient(QObject *parent = nullptr);

    QNetworkRequest getNRAuth(const QString &url);

public:
    static pbsClient* instance()
    {
        static pbsClient* inst = new pbsClient(nullptr);
        return inst;
    }
    static pbsClient* instanceUnique()
    {
        return new pbsClient(nullptr);
    }
    struct HttpResponse {
        HttpStatus::Code status;
        QJsonDocument data;
    };
    struct HttpResponseRaw {
        HttpStatus::Code status;
        QByteArray data;
    };

    HttpStatus::Code auth(const QString &host, const QString &username, const QString &password);
    HttpStatus::Code auth(const QString &host, int port, const QString &username, const QString &password);
    QString genPBSAPIPath(const QString &path);
    QString genPBSAPIPath(const QString &path, const QUrlQuery &query);

    HttpResponse getDatastores();
    HttpResponse getDatastoreSnapshots(const QString &datastore, const QString &backupid = "", const QString &backuptype = "");
    HttpResponse getDatastoreGroups(const QString &datastore);
    HttpResponseRaw getBackupFile(const QString &datastore, const QString &backupid, int backuptime, const QString &backuptype, const QString &filename);
    HttpResponse getBackupFiles(const QString &datastore, const QString &backupid, int backuptime, const QString &backuptype);

signals:

};

#endif // PBSCLIENT_H
