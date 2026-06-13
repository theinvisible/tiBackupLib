#ifndef PBSERVER_H
#define PBSERVER_H

#include <QString>
#include <QDataStream>

class PBServer
{
public:
    PBServer();

    QString uuid;

    QString name;
    QString host;
    uint port;
    QString username;
    QString password;
    QString fingerprint;

    QString keyfile;
    QString keypass;

    void genNewUuid();
};

QDataStream &operator<<(QDataStream &ds, const PBServer &obj);
QDataStream &operator>>(QDataStream &ds, PBServer &obj);

#endif // PBSERVER_H
