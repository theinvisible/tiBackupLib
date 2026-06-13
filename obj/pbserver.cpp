#include "pbserver.h"

#include <QUuid>

PBServer::PBServer()
{
    uuid = QUuid::createUuid().toString().replace("{", "").replace("}", "");

    name = "";
    host = "";
    port = 0;
    username = "";
    password = "";
    fingerprint = "";
    keyfile = "";
    keypass = "";
}

void PBServer::genNewUuid()
{
    uuid = QUuid::createUuid().toString().replace("{", "").replace("}", "");
}

QDataStream &operator<<(QDataStream &ds, const PBServer &obj)
{
    ds << obj.uuid << obj.name << obj.host << static_cast<quint32>(obj.port)
       << obj.username << obj.password << obj.fingerprint << obj.keyfile << obj.keypass;

    return ds;
}

QDataStream &operator>>(QDataStream &ds, PBServer &obj)
{
    quint32 port = 0;
    ds >> obj.uuid >> obj.name >> obj.host >> port
       >> obj.username >> obj.password >> obj.fingerprint >> obj.keyfile >> obj.keypass;
    obj.port = port;

    return ds;
}
