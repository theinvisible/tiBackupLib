#include "tibackupjob.h"

#include <QDebug>

#include "../tibackuplib.h"

tiBackupJob::tiBackupJob()
{
}

void tiBackupJob::startBackup(DeviceDiskPartition *part)
{
    TiBackupLib lib;

    QString deviceMountDir;
    QString backupArg;

    if(delete_add_file_on_dest == true)
        backupArg.append("--delete ");

    if(lib.isMounted(part->name))
    {
        deviceMountDir = lib.getMountDir(part->name);
        qDebug() << "tiBackupJob::startBackup() -> Device is already mounted on" << deviceMountDir;
    }
    else
    {
        deviceMountDir = lib.mountPartition(part);
        qDebug() << "tiBackupJob::startBackup() -> Device was not mounted, mounting on" << deviceMountDir;
    }

    // We get now the folders to backup
    QString src, dest;
    QHashIterator<QString, QString> it(backupdirs);
    while(it.hasNext())
    {
        it.next();

        src = it.key();
        dest = TiBackupLib::convertGeneric2Path(it.value(), deviceMountDir);

        qDebug() << QString("We backup now %1 to %2").arg(src, dest);

        lib.runCommandwithReturnCode(QString("rsync -a %1 %2 %3").arg(backupArg, src, dest));
    }

    lib.umountPartition(part);
}
