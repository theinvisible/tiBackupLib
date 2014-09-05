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

#include "Poco/Net/NetException.h"
#include "Poco/Net/MailMessage.h"
#include "Poco/Net/SMTPClientSession.h"
#include "Poco/Net/FilePartSource.h"
#include "Poco/Net/StringPartSource.h"

#include "../tibackuplib.h"
#include "../ticonf.h"

tiBackupJob::tiBackupJob()
{
}

void tiBackupJob::startBackup()
{
    if(partition_uuid.isEmpty())
    {
        qDebug() << "tiBackupJob::startBackup() -> Partition-UUID is not set";
        return;
    }

    DeviceDiskPartition part = TiBackupLib::getPartitionByUUID(partition_uuid);
    if(part.name.isEmpty())
    {
        qDebug() << "tiBackupJob::startBackup() -> Disk for BackupJob ist not attached, aborting";
        return;
    }

    startBackup(&part);
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

    if(compare_via_checksum == true)
    {
        qDebug() << "tiBackupJob::startBackup() -> Checksum comparison enabled";
        backupArg.append("--checksum ");
    }

    QString logpath;
    if(save_log == true)
    {
        qDebug() << "tiBackupJob::startBackup() -> Rsync Log will be archived";
        QDateTime currentDate = QDateTime::currentDateTime();
        QString logpathdir = QString("%1/%2").arg(main_settings.getValue("paths/logs").toString(), name);
        QDir logdir(logpathdir);
        if(!logdir.exists())
            logdir.mkpath(logpathdir);

        logpath = QString("%1/%2.log").arg(logpathdir, currentDate.toString("yyyyMMdd-hhmmss"));
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

        lib.runCommandwithReturnCode(QString("rsync -a %1 %2 %3").arg(backupArg, src, dest), -1);
    }

    // We notify the recipients now about the status
    if(notify == true)
    {
        qDebug() << "tiBackupJob::startBackup() -> We send notification now to " << notifyRecipients;

        Poco::Net::MailRecipient recipient(Poco::Net::MailRecipient::PRIMARY_RECIPIENT, notifyRecipients.toStdString());

        Poco::Net::MailMessage mail;
        mail.addRecipient(recipient);
        mail.setSender("tiBackup Backupsystem <tibackup@iteas.at>");
        mail.setSubject(QString("Informationen zum Backupjob <%1>").arg(name).toStdString());

        QString mailMsg = QString("Der Backupjob %1 wurde abgeschlossen, Sie können das Laufwerk %2 nun entfernen.").arg(name, device);

        mail.addContent(new Poco::Net::StringPartSource(mailMsg.toStdString()));
        //mail.addPart("html_msg", new Poco::Net::StringPartSource("Der Backupjob <b>wurde</b> abgeschlossen.", "text/html; charset=utf-8"), Poco::Net::MailMessage::CONTENT_INLINE, Poco::Net::MailMessage::ENCODING_8BIT);

        if(save_log == true)
        {
            mail.addAttachment("rsync.log", new Poco::Net::FilePartSource(logpath.toStdString()));
        }

        Poco::Net::SMTPClientSession *smtp = 0;

        try
        {
            smtp = new Poco::Net::SMTPClientSession(main_settings.getValue("smtp/server").toString().toStdString());

            if(main_settings.getValue("smtp/auth").toBool() == true)
            {
                smtp->login(Poco::Net::SMTPClientSession::AUTH_PLAIN, main_settings.getValue("smtp/username").toString().toStdString(), main_settings.getValue("smtp/password").toString().toStdString());
            }
            else
            {
                smtp->login();
            }

            smtp->sendMessage(mail);
            smtp->close();

            qDebug() << "tiBackupJob::startBackup() -> Mail message was send to " << notifyRecipients;
        }
        catch(Poco::Net::SMTPException e)
        {
            qDebug() << "tiBackupJob::startBackup() -> Mail message was NOT send. Error occured: " << QString::fromStdString(e.message());
        }
        catch(Poco::Net::HostNotFoundException e)
        {
            qDebug() << "tiBackupJob::startBackup() -> Mail message was NOT send. Hostname not found: " << QString::fromStdString(e.message());
        }

        /*
        catch(...)
        {
            qDebug() << "tiBackupJob::startBackup() -> Mail message was NOT send. Undefined error occured: ";
        }
        */

        if(smtp != 0) delete smtp;
    }

    lib.umountPartition(part);
}
