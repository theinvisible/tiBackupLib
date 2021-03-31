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

    QString ticket;
    QString CSRF;

    QNetworkAccessManager *nam;

    explicit pbsClient(QObject *parent = nullptr);
    static pbsClient* _instance;

    class CGuard
    {
    public:
       ~CGuard()
       {
          if( NULL != pbsClient::_instance )
          {
             delete pbsClient::_instance;
             pbsClient::_instance = NULL;
          }
       }
    };

    QNetworkRequest getNRAuth(const QString &url);

public:
    static pbsClient* instance()
    {
       static CGuard g;
       if (!_instance)
          _instance = new pbsClient();
       return _instance;
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
    QString genPBSAPIPath(const QString &path);
    QString genPBSAPIPath(const QString &path, const QUrlQuery &query);

    HttpResponse getDatastores();
    HttpResponse getDatastoreSnapshots(const QString &datastore, const QString &backupid = "");
    HttpResponse getDatastoreGroups(const QString &datastore);
    HttpResponseRaw getBackupFile(const QString &datastore, const QString &backupid, int backuptime, const QString &backuptype, const QString &filename);

signals:

};

#endif // PBSCLIENT_H