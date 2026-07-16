#include "sshserver.h"

#include <QUuid>

SSHServer::SSHServer()
{
    uuid = QUuid::createUuid().toString().replace("{", "").replace("}", "");

    name = "";
    host = "";
    port = 22;
    username = "";
    keyfile = "";
    keypass = "";
    hostkey = "";
}

void SSHServer::genNewUuid()
{
    uuid = QUuid::createUuid().toString().replace("{", "").replace("}", "");
}

QDataStream &operator<<(QDataStream &ds, const SSHServer &obj)
{
    ds << obj.uuid << obj.name << obj.host << static_cast<quint32>(obj.port)
       << obj.username << obj.keyfile << obj.keypass << obj.hostkey;

    return ds;
}

QDataStream &operator>>(QDataStream &ds, SSHServer &obj)
{
    quint32 port = 0;
    ds >> obj.uuid >> obj.name >> obj.host >> port
       >> obj.username >> obj.keyfile >> obj.keypass >> obj.hostkey;
    obj.port = port;

    return ds;
}
