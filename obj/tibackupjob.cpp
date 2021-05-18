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
#include <QJsonObject>
#include <QJsonArray>
#include <QTimeZone>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

#include "Poco/Net/NetException.h"
#include "Poco/Net/MailMessage.h"
#include "Poco/Net/SMTPClientSession.h"
#include "Poco/Net/FilePartSource.h"
#include "Poco/Net/StringPartSource.h"

#include "exitcodes.h"
#include "../tibackuplib.h"
#include "../ticonf.h"
#include "../pbsclient.h"

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
    QList<QString> bakMessages, pbsMessages;
    QList<tiBackupJobLog> bakLogs;

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

    if(lib.isMounted(part))
    {
        deviceMountDir = lib.getMountDir(part);
        qDebug() << "tiBackupJob::startBackup() -> Device is already mounted on" << deviceMountDir;
    }
    else
    {
        deviceMountDir = lib.mountPartition(part, this);
        qDebug() << "tiBackupJob::startBackup() -> Device was not mounted, mounting on" << deviceMountDir;

        if(!lib.isMounted(part))
        {
            qDebug() << "tiBackupJob::startBackup() -> Device could not be mounted, aborting";
            return;
        }
    }

    // Execute external script before backup if set
    if(!scriptBeforeBackup.isEmpty() && QFile::exists(scriptBeforeBackup))
    {
        qDebug() << QString("tiBackupJob::startBackup() -> Script <%1> will be taken as template").arg(scriptBeforeBackup);

        // We replace vars defined in scripts, so we write temporary file and execute it then
        QFile script(scriptBeforeBackup);
        script.open(QIODevice::ReadOnly | QIODevice::Text);
        QString scriptSource = QString(script.readAll());
        script.close();

        // We write now temporary file
        QDateTime currentDate = QDateTime::currentDateTime();
        QString tmpfilename = QString("/tmp/%1_%2").arg(name, currentDate.toString("yyyyMMddhhmmss"));
        QFile tmpScript(tmpfilename);
        tmpScript.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&tmpScript);
        QString tmpSource = TiBackupLib::convertGeneric2Path(scriptSource, deviceMountDir);
        out << tmpSource;
        tmpScript.setPermissions(QFile::ReadOwner | QFile::ExeOwner);
        tmpScript.close();

        qDebug() << QString("tiBackupJob::startBackup() -> Computed Script <%1> will be executed before backup:").arg(tmpfilename);
        qDebug() << "------------------------------";
        qDebug() << tmpSource;
        qDebug() << "------------------------------";

        if(lib.runCommandwithReturnCode(tmpfilename, -1) != 0)
        {
            bakMessages.append("Script before Backup was not executed properly.");
        }
        else
        {
            bakMessages.append("Script before Backup was executed properly.");
        }
        tmpScript.remove();
    }

    // We get now the folders to backup
    QString src, dest, logpath, backupFArgs;
    QHashIterator<QString, QString> it(backupdirs);
    while(it.hasNext())
    {
        it.next();

        tiBackupJobLog log;
        src = log.source = it.key();
        dest = log.destination = TiBackupLib::convertGeneric2Path(it.value(), deviceMountDir);
        backupFArgs = backupArg;

        if(save_log == true)
        {
            qDebug() << "tiBackupJob::startBackup() -> Rsync Log will be archived";
            QDateTime currentDate = QDateTime::currentDateTime();
            QString logpathdir = QString("%1/%2").arg(main_settings.getValue("paths/logs").toString(), name);
            QDir logdir(logpathdir);
            if(!logdir.exists())
                logdir.mkpath(logpathdir);

            logpath = QString("%1/%2_%3.log").arg(logpathdir, currentDate.toString("yyyyMMdd-hhmmss"), QString::number(bakLogs.size()));
            backupFArgs.append(QString("--log-file=%1 ").arg(logpath));

            log.rsync_path = logpath;
        }

        qDebug() << QString("tiBackupJob::startBackup() -> We backup now %1 to %2").arg(src, dest);

        QDir destdir(dest);
        if(!destdir.exists())
            destdir.mkpath(dest);

        log.ret_code = lib.runCommandwithReturnCode(QString("rsync -a %1 \"%2\" \"%3\"").arg(backupFArgs, src, dest), -1);
        bakLogs << log;
    }

    // Do PBS backup if enabled
    if(pbs)
    {
        tiConfPBServers *pbsconf = tiConfPBServers::instance();
        PBServer *pb = pbsconf->getItemByUuid(pbs_server_uuid);
        if(pb != 0)
        {
            pbsClient *pbs = pbsClient::instanceUnique();
            HttpStatus::Code status = pbs->auth(pb->host, pb->port, pb->username, pb->password);
            if(status == HttpStatus::Code::OK)
            {
                // Create process environment for proxmox-backup-client
                QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                env.insert("PBS_REPOSITORY", QString("%1@%2:%3").arg(pb->username, pb->host, pbs_server_storage));
                env.insert("PBS_PASSWORD", pb->password);
                env.insert("PBS_FINGERPRINT", pb->fingerprint);
                env.insert("PROXMOX_OUTPUT_FORMAT", "json");

                QListIterator<QString> pbs_items(pbs_backup_ids);
                while(pbs_items.hasNext())
                {
                    QString pbs_groupid = pbs_items.next();
                    QList<QString> pbs_id = pbs_groupid.split("/");
                    QString vmType = pbs_id[0];
                    QString vmID = pbs_id[1];
                    QDir vmdir(QString("%1/%2").arg(pbs_dest_folder, pbs_id[1]));
                    vmdir.mkpath(vmdir.path());

                    qInfo() << "start backup for id " << pbs_groupid << "path::" << vmdir.path();

                    pbsClient::HttpResponse ret = pbs->getDatastoreSnapshots(pbs_server_storage, pbs_id[1], pbs_id[0]);
                    if(ret.status == HttpStatus::Code::OK)
                    {
                        QJsonArray snapshots = ret.data.object()["data"].toArray();
                        if(snapshots.count() > 0)
                        {
                            // Get last backup
                            int lb = 0, li = 0;
                            for(int i=0; i < snapshots.count(); i++)
                            {
                                QJsonObject snap = snapshots[i].toObject();
                                if(snap["backup-time"].toInt() > lb) {
                                    li = i;
                                    lb = snap["backup-time"].toInt();
                                }
                            }

                            QJsonObject snap = snapshots[li].toObject();
                            qint64 blastbackup = snap["backup-time"].toInt();
                            QDateTime dt = QDateTime::fromMSecsSinceEpoch(blastbackup * 1000).toTimeSpec(Qt::UTC);
                            qInfo() << "pbs-last-backup" << pbs_groupid << dt.toString(Qt::ISODate);

                            QString vmConf = "";
                            QStringList vmImages;

                            QJsonArray files = snap["files"].toArray();
                            qInfo() << "pbs files to restore" << files;
                            for(int j=0; j < files.count(); j++)
                            {
                                QRegularExpression re("^(.*?)\\.[^.]*$");
                                QRegularExpressionMatch match = re.match(files[j].toObject()["filename"].toString());
                                QString file = match.captured(1);

                                //QString file = files[j].toString();
                                QString respec = QString("%1/%2").arg(pbs_groupid, dt.toString(Qt::ISODate));
                                qInfo() << "pbs do file backup file::" << file << "orig::" << files[j].toObject()["filename"].toString();

                                if(file.endsWith(".conf"))
                                {
                                    vmConf = file;
                                }
                                else if(file.endsWith(".pxar") || file.endsWith(".img"))
                                {
                                    vmImages << file;
                                }
                                else
                                {
                                    continue;
                                }

                                QProcess p;
                                p.setProcessEnvironment(env);
                                p.setProcessChannelMode(QProcess::MergedChannels);
                                p.start("proxmox-backup-client", QStringList() << "restore" << respec << file << vmdir.path().append("/").append(file));
                                p.waitForStarted();
                                p.waitForFinished();
                                if(p.exitCode() == 0)
                                {
                                    qInfo() << "Successful backup for " << respec << file << p.readAll();
                                }
                                else
                                {
                                    qInfo() << "Failed backup for " << respec << file << p.readAll();
                                }
                                p.close();
                            }

                            // Package VM depending on type
                            if(vmType == "vm")
                            {
                                QString images;
                                for(int k=0; k < vmImages.count(); k++)
                                {
                                    QString devname = vmImages[k].split(".")[0];
                                    images = images.append(devname).append("=").append(vmdir.path().append("/")).append(vmImages[k]).append(" ");
                                }

                                QString outName = QString("vzdump-qemu-%1-%2.vma.zst").arg(vmID, dt.toString("yyyy_MM_dd-hh_mm_ss"));
                                lib.runCommandwithReturnCode(QString("vma create %1vm.vma -c %2 %3").arg(vmdir.path().append("/"), vmdir.path().append("/").append(vmConf), images), -1);
                                lib.runCommandwithReturnCode(QString("zstd -f -10 --rm %1vm.vma -o %2").arg(vmdir.path().append("/"), vmdir.path().append("/").append(outName)), -1);
                            }
                            else if(vmType == "ct")
                            {
                                QString outName = QString("vzdump-lxc-%1-%2.tar.zst").arg(vmID, dt.toString("yyyy_MM_dd-hh_mm_ss"));
                                vmdir.mkpath(vmdir.path().append("/").append(vmImages[0]).append("/etc/vzdump/"));
                                QFile::copy(vmdir.path().append("/").append(vmConf), vmdir.path().append("/").append(vmImages[0]).append("/etc/vzdump/").append(vmConf));
                                lib.runCommandwithReturnCode(QString("tar -C %1 -cf %2ct.tar .").arg(vmdir.path().append("/").append(vmImages[0]), vmdir.path().append("/")), -1);
                                lib.runCommandwithReturnCode(QString("zstd -f -10 --rm %1ct.tar -o %2").arg(vmdir.path().append("/"), vmdir.path().append("/").append(outName)), -1);
                            }
                        }
                        else
                        {
                            pbsMessages.append(QString("VM-ID %1 has no backups, skipping").arg(pbs_groupid));
                        }
                    }
                    else
                    {
                        pbsMessages.append(QString("PBS datastore listing for %1 not successful").arg(pbs_groupid));
                    }
                }
            }
            else
            {
                pbsMessages.append(QString("PBS %1 auth not successful").arg(pbs_server_uuid));
            }
            pbs->deleteLater();
        }
        else
        {
            pbsMessages.append(QString("PBS %1 not found in config").arg(pbs_server_uuid));
        }
    }

    // Execute external script after backup if set
    if(!scriptAfterBackup.isEmpty() && QFile::exists(scriptAfterBackup))
    {
        qDebug() << QString("tiBackupJob::startBackup() -> Script <%1> will be taken as template").arg(scriptAfterBackup);

        // We replace vars defined in scripts, so we write temporary file and execute it then
        QFile script(scriptAfterBackup);
        script.open(QIODevice::ReadOnly | QIODevice::Text);
        QString scriptSource = QString(script.readAll());
        script.close();

        // We write now temporary file
        QDateTime currentDate = QDateTime::currentDateTime();
        QString tmpfilename = QString("/tmp/%1_%2").arg(name, currentDate.toString("yyyyMMddhhmmss"));
        QFile tmpScript(tmpfilename);
        tmpScript.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&tmpScript);
        QString tmpSource = TiBackupLib::convertGeneric2Path(scriptSource, deviceMountDir);
        out << tmpSource;
        tmpScript.setPermissions(QFile::ReadOwner | QFile::ExeOwner);
        tmpScript.close();

        qDebug() << QString("tiBackupJob::startBackup() -> Computed Script <%1> will be executed after backup:").arg(tmpfilename);
        qDebug() << "------------------------------";
        qDebug() << tmpSource;
        qDebug() << "------------------------------";

        if(lib.runCommandwithReturnCode(tmpfilename, -1) != 0)
        {
            bakMessages.append("Script after Backup was not executed properly.");
        }
        else
        {
            bakMessages.append("Script after Backup was executed properly.");
        }
        tmpScript.remove();
    }

    // We notify the recipients now about the status
    if(notify == true)
    {
        qDebug() << "tiBackupJob::startBackup() -> We send notification now to " << notifyRecipients;

        Poco::Net::MailRecipient recipient(Poco::Net::MailRecipient::PRIMARY_RECIPIENT, notifyRecipients.toStdString());

        Poco::Net::MailMessage mail;
        mail.addRecipient(recipient);
        mail.setSender("tiBackup Backupsystem <tibackup@iteas.at>");
        mail.setSubject(QString("Information for backupjob <%1>").arg(name).toStdString());

        QString mailMsg = QString("Backupjob <%1> was executed, you can now detach drive %2. \n\nGenerated backup information: \n\n").arg(name, device);
        if(bakMessages.count() == 0)
        {
            mailMsg.append(QString("  Backup scripts are okay.\n"));
        }
        for(int i=0; i < bakMessages.count() ; i++)
        {
            mailMsg.append(QString("  %1\n").arg(bakMessages.at(i)));
        }

        if(pbsMessages.count() == 0)
        {
            mailMsg.append(QString("  PBS-Backup is okay.\n"));
        }
        for(int i=0; i < pbsMessages.count() ; i++)
        {
            mailMsg.append(QString("  %1\n").arg(pbsMessages.at(i)));
        }

        // Rsync status
        mailMsg.append(QString("\nRsync status information: \n\n"));
        for(int ia=0; ia<bakLogs.size(); ia++)
        {
            tiBackupJobLog log = bakLogs.at(ia);
            mailMsg.append(QString("  %1 -> %2 :: %3\n").arg(log.source, log.destination, exitCodesRsync::getMsg(log.ret_code)));
        }

        mail.addContent(new Poco::Net::StringPartSource(mailMsg.toStdString()));
        //mail.addPart("html_msg", new Poco::Net::StringPartSource("Der Backupjob <b>wurde</b> abgeschlossen.", "text/html; charset=utf-8"), Poco::Net::MailMessage::CONTENT_INLINE, Poco::Net::MailMessage::ENCODING_8BIT);

        if(save_log == true && bakLogs.size() > 0)
        {
            for(int ia=0; ia<bakLogs.size(); ia++)
            {
                logpath = bakLogs.at(ia).rsync_path;
                if(QFile::exists(logpath))
                    mail.addAttachment(QString("rsync_%1.log").arg(ia).toStdString(), new Poco::Net::FilePartSource(logpath.toStdString()));
            }
        }

        Poco::Net::SMTPClientSession *smtp = 0;

        try
        {
            smtp = new Poco::Net::SMTPClientSession(main_settings.getValue("smtp/server").toString().toStdString());

            if(main_settings.getValue("smtp/auth").toBool() == true)
            {
                smtp->login(Poco::Net::SMTPClientSession::AUTH_LOGIN, main_settings.getValue("smtp/username").toString().toStdString(), QString(QByteArray().fromBase64(QByteArray().append(main_settings.getValue("smtp/password").toString()))).toStdString());
            }
            else
            {
                smtp->login();
            }

            smtp->sendMessage(mail);
            smtp->close();

            qDebug() << "tiBackupJob::startBackup() -> Mail message was send to " << notifyRecipients;
        }
        catch(Poco::Net::SMTPException &e)
        {
            qWarning() << "tiBackupJob::startBackup() -> Mail message was NOT send. Error occured: " << QString::fromStdString(e.message());
        }
        catch(Poco::Net::HostNotFoundException &e)
        {
            qWarning() << "tiBackupJob::startBackup() -> Mail message was NOT send. Hostname not found: " << QString::fromStdString(e.message());
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
