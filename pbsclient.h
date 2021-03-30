#ifndef PBSCLIENT_H
#define PBSCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>

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

public:
    static pbsClient* instance()
    {
       static CGuard g;
       if (!_instance)
          _instance = new pbsClient();
       return _instance;
    }

    HttpStatus::Code auth(const QString &host, const QString &username, const QString &password);

signals:

};

#endif // PBSCLIENT_H
