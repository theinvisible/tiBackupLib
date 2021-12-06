#ifndef PBSERVER_H
#define PBSERVER_H

#include <QString>

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

#endif // PBSERVER_H
