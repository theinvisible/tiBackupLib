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
