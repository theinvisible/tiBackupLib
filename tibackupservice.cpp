#include "tibackupservice.h"

#include "tibackuplib.h"

tiBackupService::tiBackupService(QObject *parent) : QObject(parent)
{
    main_settings = new tiConfMain;
}

tiBackupServiceStatus tiBackupService::start()
{

}

tiBackupServiceStatus tiBackupService::stop()
{

}

tiBackupServiceStatus tiBackupService::status()
{
    TiBackupLib lib;
    QString cmd = QString("%1 status").arg(main_settings->getValue("paths/initd").toString());
    if(lib.runCommandwithReturnCode(cmd) == 0)
        return tiBackupServiceStatusStarted;
    else
        return tiBackupServiceStatusStopped;

}

