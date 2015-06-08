#include "tibackupservice.h"

#include "tibackuplib.h"

#include <QFile>
#include <QDebug>

tiBackupService::tiBackupService(QObject *parent) : QObject(parent)
{
    main_settings = new tiConfMain;
}

tiBackupServiceStatus tiBackupService::start()
{
    qDebug() << "tiBackupService::start()";

    TiBackupLib lib;
    QString initd = main_settings->getValue("paths/initd").toString();
    QString cmd = QString("%1 start").arg(initd);

    if(!QFile::exists(initd))
        return tiBackupServiceStatusNotFound;

    lib.runCommandwithReturnCode(cmd);

    emit serviceStarted();
    return tiBackupServiceStatusStarted;
}

tiBackupServiceStatus tiBackupService::stop()
{
    qDebug() << "tiBackupService::stop()";

    TiBackupLib lib;
    QString initd = main_settings->getValue("paths/initd").toString();
    QString cmd = QString("%1 stop").arg(initd);

    if(!QFile::exists(initd))
        return tiBackupServiceStatusNotFound;

    lib.runCommandwithReturnCode(cmd);

    emit serviceStopped();
    return tiBackupServiceStatusStarted;
}

tiBackupServiceStatus tiBackupService::status()
{
    TiBackupLib lib;
    QString initd = main_settings->getValue("paths/initd").toString();
    QString cmd = QString("%1 status").arg(initd);

    if(!QFile::exists(initd))
        return tiBackupServiceStatusNotFound;

    if(lib.runCommandwithReturnCode(cmd) == 0)
        return tiBackupServiceStatusStarted;
    else
        return tiBackupServiceStatusStopped;

}

