/*
 *
tiBackupLib - A intelligent desktop/standalone backup system library

Copyright (C) 2014 Rene Hadler, rene@hadler.me, https://hadler.me

    This file is part of tiBackup.

    tiBackupLib is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    tiBackupLib is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with tiBackupLib.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "tibackupjob.h"

#include <QDebug>
#include <QDir>
#include <QDateTime>

#include "../tibackuplib.h"
#include "../ticonf.h"

tiBackupJob::tiBackupJob()
{
}

void tiBackupJob::startBackup(DeviceDiskPartition *part)
{
    TiBackupLib lib;
    tiConfMain main_settings;

    QString deviceMountDir;
    QString backupArg;

    if(delete_add_file_on_dest == true)
    {
        qDebug() << "tiBackupJob::startBackup() -> Additional files will be deleted";
        backupArg.append("--delete ");
    }

    if(save_log == true)
    {
        qDebug() << "tiBackupJob::startBackup() -> Rsync Log will be archived";
        QDateTime currentDate = QDateTime::currentDateTime();
        QString logpathdir = QString("%1/%2").arg(main_settings.getValue("paths/logs").toString(), name);
        QDir logdir(logpathdir);
        if(!logdir.exists())
            logdir.mkpath(logpathdir);

        QString logpath = QString("%1/%2.log").arg(logpathdir, currentDate.toString("yyyyMMdd-hhmmss"));
        backupArg.append(QString("--log-file=%1 ").arg(logpath));
    }

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
