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
}

void PBServer::genNewUuid()
{
    uuid = QUuid::createUuid().toString().replace("{", "").replace("}", "");
}
