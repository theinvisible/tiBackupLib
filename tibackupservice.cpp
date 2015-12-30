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

bool tiBackupService::install(const QString &path)
{
    qDebug() << "tiBackupService::install()";

    QFile *tiServicePath = new QFile(path);
    if(!tiServicePath->open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out(tiServicePath);

    QFile *tiServiceTemplate = new QFile(":/init/tibackup");
    if(!tiServiceTemplate->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        tiServicePath->close();
        tiServicePath->deleteLater();
        return false;
    }
    QTextStream in(tiServiceTemplate);

    out << in.readAll();
    out.flush();

    tiServicePath->setPermissions(QFile::ReadOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther);
    tiServicePath->close();
    tiServiceTemplate->close();

    tiServicePath->deleteLater();
    tiServiceTemplate->deleteLater();

    emit serviceInstalled();
    return true;
}

