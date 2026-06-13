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
#include <QThread>

#include "Poco/Net/NetException.h"
#include "Poco/Net/MailMessage.h"
#include "Poco/Net/SMTPClientSession.h"
#include "Poco/Net/FilePartSource.h"
#include "Poco/Net/StringPartSource.h"

#include "exitcodes.h"
#include "../tibackuplib.h"
#include "../ticonf.h"
#include "../pbsclient.h"
#include "workers/tibackupjobworker.h"

tiBackupJob::tiBackupJob()
{
}

void tiBackupJob::startBackup()
{
    if(partition_uuid.isEmpty())
    {
        qWarning() << QString("tiBackupJob::startBackup() -> Partition-UUID for Backupjob %1 is not set").arg(name);
        return;
    }

    DeviceDiskPartition part = TiBackupLib::getPartitionByUUID(partition_uuid);
    if(part.name.isEmpty())
    {
        qWarning() << QString("tiBackupJob::startBackup() -> Disk for BackupJob %1 is not attached, aborting").arg(name);
        return;
    }

    startBackup(&part);
}

void tiBackupJob::startBackupThread(backupManager *manager)
{
    QThread* thread = new QThread;
    tiBackupJobWorker* worker = new tiBackupJobWorker();
    worker->setJobName(name);
    worker->moveToThread(thread);
    QObject::connect(thread, &QThread::started, worker, &tiBackupJobWorker::process);
    QObject::connect(worker, &tiBackupJobWorker::finished, thread, &QThread::quit);
    QObject::connect(worker, &tiBackupJobWorker::finished, worker, &QObject::deleteLater);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    QObject::connect(thread, &QThread::finished, manager, [manager, name = name]() { manager->onBackupFinished(name); });
    thread->start();
}

void tiBackupJob::startBackup(DeviceDiskPartition *part)
{
    TiBackupLib lib;
    tiConfMain main_settings;

    QString deviceMountDir;
    QString backupArg;
    QList<QString> bakMessages, pbsMessages;
    QList<tiBackupJobLog> bakLogs;
    QList<tiBackupJobPBSLog> bakPBSLogs;

    QDateTime currentDate = QDateTime::currentDateTime();
    QFile tibackupDetailLog(QString("%1/%2__%3.log").arg(main_settings.getLogsDetailDir(), currentDate.toString("yyyy-MM-dd_HH-mm"), name));
    tibackupDetailLog.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Unbuffered);

    QTextStream detailLog(&tibackupDetailLog);
    detailLog << QString("Starting backup for %1 at %2 on %3 (%4)").arg(name, currentDate.toString("yyyy-MM-dd_HH-mm"), device, partition_uuid) << "\n";
    detailLog.flush();

    if(delete_add_file_on_dest == true)
    {
        detailLog << "Feature: Additional files will be deleted" << "\n";
        detailLog.flush();
        backupArg.append("--delete ");
    }

    if(compare_via_checksum == true)
    {
        detailLog << "Feature: Checksum comparison enabled" << "\n";
        detailLog.flush();
        backupArg.append("--checksum ");
    }

    if(lib.isMounted(part))
    {
        deviceMountDir = lib.getMountDir(part);
        detailLog << "Device is already mounted on " << deviceMountDir << "\n";
        detailLog.flush();
    }
    else
    {
        deviceMountDir = lib.mountPartition(part, this);
        detailLog << "Device was not mounted, mounting on " << deviceMountDir << "\n";
        detailLog.flush();

        if(!lib.isMounted(part))
        {
            detailLog << "Device could not be mounted, aborting" << "\n";
            detailLog.flush();
            tibackupDetailLog.close();
            return;
        }
    }

    // Execute external script before backup if set
    if(!scriptBeforeBackup.isEmpty() && QFile::exists(scriptBeforeBackup))
    {
        detailLog << QString("Script <%1> will be taken as template").arg(scriptBeforeBackup) << "\n";
        detailLog.flush();

        QFile script(scriptBeforeBackup);
        script.open(QIODevice::ReadOnly | QIODevice::Text);
        QString scriptSource = QString(script.readAll());
        script.close();

        QDateTime currentDate = QDateTime::currentDateTime();
        QString tmpfilename = QString("/tmp/%1_%2").arg(name, currentDate.toString("yyyyMMddhhmmss"));
        QFile tmpScript(tmpfilename);
        tmpScript.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&tmpScript);
        QString tmpSource = TiBackupLib::convertGeneric2Path(scriptSource, deviceMountDir);
        out << tmpSource;
        tmpScript.setPermissions(QFile::ReadOwner | QFile::ExeOwner);
        tmpScript.close();

        detailLog << QString("Computed Script <%1> will be executed before backup:").arg(tmpfilename) << "\n";
        detailLog << "------------------------------" << "\n";
        detailLog << tmpSource << "\n";
        detailLog << "------------------------------" << "\n";
        detailLog.flush();

        if(lib.runCommandwithReturnCode(tmpfilename, -1) != 0)
        {
            QString msg("Script before Backup was not executed properly.");
            bakMessages.append(msg);
            detailLog << msg << "\n";
        }
        else
        {
            QString msg("Script before Backup was executed properly.");
            bakMessages.append(msg);
            detailLog << msg << "\n";
        }
        detailLog.flush();
        tmpScript.remove();
    }

    // We get now the folders to backup
    QString src, dest, logpath, backupFArgs;
    for(const auto &[srcKey, destVal] : backupdirs.asKeyValueRange())
    {
        tiBackupJobLog log;
        src = log.source = srcKey;
        dest = log.destination = TiBackupLib::convertGeneric2Path(destVal, deviceMountDir);
        backupFArgs = backupArg;

        if(save_log == true)
        {
            detailLog << "Feature: Rsync Log will be archived" << "\n";
            detailLog.flush();
            QDateTime currentDate = QDateTime::currentDateTime();
            QString logpathdir = QString("%1/%2").arg(main_settings.getValue("paths/logs").toString(), name);
            QDir logdir(logpathdir);
            if(!logdir.exists())
                logdir.mkpath(logpathdir);

            logpath = QString("%1/%2_%3.log").arg(logpathdir, currentDate.toString("yyyyMMdd-hhmmss"), QString::number(bakLogs.size()));
            backupFArgs.append(QString("--log-file=%1 ").arg(logpath));

            log.rsync_path = logpath;
        }

        detailLog << QString("RSYNC Backup: Backup %1 to %2").arg(src, dest) << "\n";
        detailLog.flush();

        QDir destdir(dest);
        if(!destdir.exists())
            destdir.mkpath(dest);

        log.ret_code = lib.runCommandwithReturnCode(QString("rsync -a %1 \"%2\" \"%3\"").arg(backupFArgs, src, dest), -1);
        if(log.ret_code != 0)
        {
            QString msg("RSYNC Backup failed (see detail log).");
            detailLog << msg << "\n";
        }
        else
        {
            QString msg("RSYNC Backup successful.");
            detailLog << msg << "\n";
        }
        detailLog.flush();
        bakLogs << log;
    }

    // Do PBS backup if enabled
    if(pbs)
    {
        // Check if /dev/stdout is a symlink, needed for PBS Backups
        QFileInfo finfo_stdout("/dev/stdout");
        if(finfo_stdout.isSymLink())
        {
            tiConfPBServers *pbsconf = tiConfPBServers::instance();
            PBServer *pb = pbsconf->getItemByUuid(pbs_server_uuid);
            if(pb != nullptr)
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
                    if(!pb->keypass.isEmpty())
                        env.insert("PBS_ENCRYPTION_PASSWORD", pb->keypass);

                    for(const auto &pbs_groupid : pbs_backup_ids)
                    {
                        QList<QString> pbs_id = pbs_groupid.split("/");
                        QString vmType = pbs_id[0];
                        QString vmID = pbs_id[1];
                        QDir vmdir(QString("%1/%2").arg(TiBackupLib::convertGeneric2Path(pbs_dest_folder, deviceMountDir), pbs_id[1]));
                        vmdir.mkpath(vmdir.path());

                        tiBackupJobPBSLog log;
                        log.vmid = pbs_groupid;

                        detailLog << "PBS Backup: Start backup for id " << pbs_groupid << " to path " << vmdir.path() << "\n";
                        detailLog.flush();

                        // Do additional auth to avoid pbs ticket timeouts
                        pbs->auth(pb->host, pb->port, pb->username, pb->password);
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
                                QDateTime dt = QDateTime::fromMSecsSinceEpoch(blastbackup * 1000).toUTC();
                                detailLog << "Newest backup file for" << pbs_groupid << " from " << dt.toString(Qt::ISODate) << "\n";
                                detailLog.flush();

                                QString vmConf = "";
                                QStringList vmImages;

                                bool errDownloadFiles = false;
                                QJsonArray files = snap["files"].toArray();
                                for(int j=0; j < files.count(); j++)
                                {
                                    QRegularExpression re("^(.*?)\\.[^.]*$");
                                    QRegularExpressionMatch match = re.match(files[j].toObject()["filename"].toString());
                                    QString file = match.captured(1);

                                    QString respec = QString("%1/%2").arg(pbs_groupid, dt.toString(Qt::ISODate));
                                    detailLog << "Backup restore :: " << file << " orig::" << files[j].toObject()["filename"].toString() << "\n";
                                    detailLog.flush();

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
                                        detailLog << "File " << file << " not needed, skipping" << "\n";
                                        detailLog.flush();
                                        continue;
                                    }

                                    // If to be restored file exists delete it because proxmox-backup-client cannot overwrite files
                                    if(QFile::exists(vmdir.path().append("/").append(file)))
                                    {
                                        detailLog << "File " << file << " already exists on target, deleting" << "\n";
                                        detailLog.flush();
                                        lib.runCommandwithReturnCodePipe(QString("rm -f %1").arg(vmdir.path().append("/").append(file)), -1);
                                    }

                                    QProcess p;
                                    p.setProcessEnvironment(env);
                                    p.setProcessChannelMode(QProcess::MergedChannels);
                                    QStringList startargs = QStringList() << "restore" << respec << file << vmdir.path().append("/").append(file);
                                    if(!pb->keyfile.isEmpty())
                                    {
                                        if(QFile::exists(pb->keyfile))
                                        {
                                            startargs << "--keyfile" << pb->keyfile;
                                        }
                                        else
                                        {
                                            QString errmsg = QString("Encryption file %1 not found!").arg(pb->keyfile);
                                            log.errmsg.append(errmsg).append(", ");
                                            detailLog << errmsg << "\n";
                                            detailLog.flush();
                                            errDownloadFiles = true;
                                        }
                                    }
                                    detailLog << "Start PBS backup cmd: " << "proxmox-backup-client " << startargs.join(",") << "\n";
                                    detailLog.flush();
                                    p.start("proxmox-backup-client", startargs);
                                    p.waitForStarted(-1);
                                    p.waitForFinished(-1);
                                    if(p.exitCode() == 0)
                                    {
                                        detailLog << "Successful backup for " << respec << file << p.readAll() << "\n";
                                    }
                                    else
                                    {
                                        QByteArray err = p.readAll();
                                        log.errmsg.append(err).append(", ");
                                        detailLog << "Failed backup for " << respec << file << err << "\n";
                                        errDownloadFiles = true;
                                    }
                                    detailLog.flush();
                                    p.close();
                                }

                                // Check if download is success, otherwise skip this VM
                                if(errDownloadFiles) {
                                    QString errmsg("Error processing proxmox-backup-client, check logs for details!");
                                    log.errmsg.append(errmsg).append(", ");
                                    detailLog << errmsg << "\n";
                                    detailLog.flush();
                                    continue;
                                }

                                // Package VM depending on type
                                if(vmType == "vm")
                                {
                                    QString images;
                                    for(const auto &img : vmImages)
                                    {
                                        QString devname = img.split(".")[0];
                                        images += devname + "=" + vmdir.path() + "/" + img + " ";
                                    }

                                    // Cleanup old backups
                                    lib.runCommandwithReturnCodePipe(QString("rm -f %1vzdump-qemu-*").arg(vmdir.path().append("/")), -1);

                                    detailLog << "Start compressing VM\n";
                                    QString outName = QString("vzdump-qemu-%1-%2.vma.zst").arg(vmID, dt.toString("yyyy_MM_dd-hh_mm_ss"));
                                    QString vmacmd = QString("vma create /dev/stdout -c %1 %2").arg(vmdir.path().append("/").append(vmConf), images);
                                    QString vmazstdcmd = QString("%1 | zstd -f -3 -T4 -o %2").arg(vmacmd, vmdir.path().append("/").append(outName));
                                    detailLog << "Execute VMA-ZStd CMD: " << vmazstdcmd << "\n";
                                    if(lib.runCommandwithReturnCodePipe(vmazstdcmd, -1) == 0)
                                    {
                                        QFileInfo outInfo(vmdir.path().append("/").append(outName));
                                        if(outInfo.size() > 1000) {
                                            QString msg = QString("Successful backup, files: %1, archive: %2 (Size: %3GB)").arg(vmImages.join(" "), outName).arg(QString::number(outInfo.size()/1024/1024/1024, 'f', 2));
                                            log.msg.append(msg);
                                            detailLog << msg << "\n";
                                        } else {
                                            QString msg = QString("Failed backup, compression or packaging failed, cmd: %1").arg(QString("%1 | zstd -f -3 -T4 -o %2").arg(vmacmd, vmdir.path().append("/").append(outName)));
                                            log.errmsg.append(msg);
                                            detailLog << msg << "\n";
                                        }
                                    }
                                    else
                                    {
                                        QString msg = QString("Compression or packaging failed, cmd: %1").arg(QString("%1 | zstd -f -3 -T4 -o %2").arg(vmacmd, vmdir.path().append("/").append(outName)));
                                        log.errmsg.append(msg);
                                        detailLog << msg << "\n";
                                    }
                                    detailLog.flush();
                                }
                                else if(vmType == "ct")
                                {
                                    QString outName = QString("vzdump-lxc-%1-%2.tar.zst").arg(vmID, dt.toString("yyyy_MM_dd-hh_mm_ss"));
                                    vmdir.mkpath(vmdir.path().append("/").append(vmImages[0]).append("/etc/vzdump/"));
                                    QFile::copy(vmdir.path().append("/").append(vmConf), vmdir.path().append("/").append(vmImages[0]).append("/etc/vzdump/").append(vmConf));

                                    // Cleanup old backups
                                    lib.runCommandwithReturnCodePipe(QString("rm -f %1vzdump-lxc-*").arg(vmdir.path().append("/")), -1);

                                    detailLog << "Start compressing CT\n";
                                    if(lib.runCommandwithReturnCode(QString("tar -C %1 -cf %2ct.tar .").arg(vmdir.path().append("/").append(vmImages[0]), vmdir.path().append("/")), -1) == 0)
                                    {
                                        if(lib.runCommandwithReturnCode(QString("zstd -f -3 -T4 --rm %1ct.tar -o %2").arg(vmdir.path().append("/"), vmdir.path().append("/").append(outName)), -1) == 0)
                                        {
                                            QFileInfo outInfo(vmdir.path().append("/").append(outName));
                                            if(outInfo.size() > 1000) {
                                                QString msg = QString("Successful backup, files: %1, archive: %2 (Size: %3GB)").arg(vmImages.join(" "), outName).arg(QString::number(outInfo.size()/1024/1024/1024, 'f', 2));
                                                log.msg.append(msg);
                                                detailLog << msg << "\n";
                                            } else {
                                                QString msg = QString("Failed backup, compression failed, cmd: %1").arg(QString("zstd -f -10 --rm %1ct.tar -o %2").arg(vmdir.path().append("/"), vmdir.path().append("/").append(outName)));
                                                log.errmsg.append(msg);
                                                detailLog << msg << "\n";
                                            }
                                        }
                                        else
                                        {
                                            QString msg = QString("Compression failed, cmd: %1").arg(QString("zstd -f -10 --rm %1ct.tar -o %2").arg(vmdir.path().append("/"), vmdir.path().append("/").append(outName)));
                                            log.errmsg.append(msg);
                                            detailLog << msg << "\n";
                                        }
                                    }
                                    else
                                    {
                                        QString msg = QString("Packaging failed, cmd: %1").arg(QString("tar -C %1 -cf %2ct.tar .").arg(vmdir.path().append("/").append(vmImages[0]), vmdir.path().append("/")));
                                        log.errmsg.append(msg);
                                        detailLog << msg << "\n";
                                    }
                                    detailLog.flush();
                                }

                                // Cleanup
                                vmdir.remove(vmdir.path().append("/").append(vmConf));
                                for(const auto &image : vmImages)
                                {
                                    if(vmType == "ct") {
                                        QDir archive_dir(vmdir.path().append("/").append(image));
                                        archive_dir.removeRecursively();
                                        detailLog << "Cleanup, remove dir " << vmdir.path().append("/").append(image) << "\n";
                                    }
                                    else {
                                        vmdir.remove(vmdir.path().append("/").append(image));
                                        detailLog << "Cleanup, remove file " << vmdir.path().append("/").append(image) << "\n";
                                    }
                                    detailLog.flush();
                                }
                            }
                            else
                            {
                                QString msg = QString("VM-ID %1 has no backups, skipping").arg(pbs_groupid);
                                pbsMessages.append(msg);
                                detailLog << msg << "\n";
                            }

                        }
                        else
                        {
                            QString msg = QString("PBS datastore listing for %1 not successful").arg(pbs_groupid);
                            pbsMessages.append(msg);
                            detailLog << msg << "\n";
                        }

                        bakPBSLogs << log;
                    }
                }
                else
                {
                    QString msg = QString("PBS %1 auth not successful").arg(pbs_server_uuid);
                    pbsMessages.append(msg);
                    detailLog << msg << "\n";
                }
                pbs->deleteLater();
            }
            else
            {
                QString msg = QString("PBS %1 not found in config").arg(pbs_server_uuid);
                pbsMessages.append(msg);
                detailLog << msg << "\n";
            }
        } else {
            QString msg = QString("File /dev/stdout is not a symlink, system integrity error! Cannot start PBS backup!");
            pbsMessages.append(msg);
            detailLog << msg << "\n";
        }
        detailLog.flush();
    }

    // Execute external script after backup if set
    if(!scriptAfterBackup.isEmpty() && QFile::exists(scriptAfterBackup))
    {
        detailLog << QString("Run script after backup: Script <%1> will be taken as template").arg(scriptAfterBackup) << "\n";
        detailLog.flush();

        QFile script(scriptAfterBackup);
        script.open(QIODevice::ReadOnly | QIODevice::Text);
        QString scriptSource = QString(script.readAll());
        script.close();

        QDateTime currentDate = QDateTime::currentDateTime();
        QString tmpfilename = QString("/tmp/%1_%2").arg(name, currentDate.toString("yyyyMMddhhmmss"));
        QFile tmpScript(tmpfilename);
        tmpScript.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&tmpScript);
        QString tmpSource = TiBackupLib::convertGeneric2Path(scriptSource, deviceMountDir);
        out << tmpSource;
        tmpScript.setPermissions(QFile::ReadOwner | QFile::ExeOwner);
        tmpScript.close();

        detailLog << QString("Computed Script <%1> will be executed after backup:").arg(tmpfilename) << "\n";
        detailLog << "------------------------------" << "\n";
        detailLog << tmpSource << "\n";
        detailLog << "------------------------------" << "\n";
        detailLog.flush();

        if(lib.runCommandwithReturnCode(tmpfilename, -1) != 0)
        {
            QString msg = QString("Script after Backup was not executed properly.");
            bakMessages.append(msg);
            detailLog << msg << "\n";
        }
        else
        {
            QString msg = QString("Script after Backup was executed properly.");
            bakMessages.append(msg);
            detailLog << msg << "\n";
        }
        detailLog.flush();
        tmpScript.remove();
    }

    // We notify the recipients now about the status
    if(notify == true)
    {
        detailLog << "We send notification now to " << notifyRecipients << "\n";
        detailLog.flush();

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
        for(const auto &msg : bakMessages)
        {
            mailMsg.append(QString("  %1\n").arg(msg));
        }

        if(pbs)
        {
            bool pbsok = true;
            for(const auto &pbslog : bakPBSLogs)
            {
                if(!pbslog.errmsg.isEmpty())
                    pbsok = false;
            }

            if(pbsMessages.count() == 0 && pbsok)
            {
                mailMsg.append(QString("  PBS-Backup is okay.\n"));
            }
            else
            {
                mailMsg.append(QString("  PBS-Backup problems occurred, see below.\n"));
            }

            for(const auto &msg : pbsMessages)
            {
                mailMsg.append(QString("  %1\n").arg(msg));
            }
        }

        // PBS status
        if(pbs)
        {
            mailMsg.append(QString("\nPBS status information: \n\n"));
            for(const auto &log : bakPBSLogs)
            {
                if(log.errmsg.isEmpty())
                    mailMsg.append(QString("  %1 -> %2\n").arg(log.vmid, log.msg));
                else
                    mailMsg.append(QString("  %1 -> %2, err: %3\n").arg(log.vmid, log.msg, log.errmsg));
            }
        }

        // Rsync status
        mailMsg.append(QString("\nRsync status information: \n\n"));
        for(const auto &log : bakLogs)
        {
            mailMsg.append(QString("  %1 -> %2 :: %3\n").arg(log.source, log.destination, exitCodesRsync::getMsg(log.ret_code)));
        }

        mail.addContent(new Poco::Net::StringPartSource(mailMsg.toStdString()));

        if(save_log == true && bakLogs.size() > 0)
        {
            for(int ia=0; ia<bakLogs.size(); ia++)
            {
                logpath = bakLogs.at(ia).rsync_path;
                if(QFile::exists(logpath))
                    mail.addAttachment(QString("rsync_%1.log").arg(ia).toStdString(), new Poco::Net::FilePartSource(logpath.toStdString()));
            }
        }

        try
        {
            Poco::Net::SMTPClientSession smtp(main_settings.getValue("smtp/server").toString().toStdString());

            if(main_settings.getValue("smtp/auth").toBool() == true)
            {
                smtp.login(Poco::Net::SMTPClientSession::AUTH_LOGIN,
                           main_settings.getValue("smtp/username").toString().toStdString(),
                           QString(QByteArray::fromBase64(main_settings.getValue("smtp/password").toString().toLatin1())).toStdString());
            }
            else
            {
                smtp.login();
            }

            smtp.sendMessage(mail);
            smtp.close();

            detailLog << "tiBackupJob::startBackup() -> Mail message was send successfully to " << notifyRecipients << "\n";
        }
        catch(Poco::Net::SMTPException &e)
        {
            detailLog << "tiBackupJob::startBackup() -> Mail message was NOT send. Error occured: " << QString::fromStdString(e.message()) << "\n";
        }
        catch(Poco::Net::HostNotFoundException &e)
        {
            detailLog << "tiBackupJob::startBackup() -> Mail message was NOT send. Hostname not found: " << QString::fromStdString(e.message()) << "\n";
        }

        QDateTime currentFinishDate = QDateTime::currentDateTime();
        detailLog << QString("Backup job finished at %1!").arg(currentFinishDate.toString("yyyy-MM-dd_HH-mm")) << "\n";
        detailLog.flush();
    }

    lib.umountPartition(part);
}

QDataStream &operator<<(QDataStream &ds, const tiBackupJob &obj)
{
    ds << obj.name << obj.device << obj.partition_uuid;

    // backupdirs is a QMultiHash: stream as count + key/value pairs to keep all values
    ds << static_cast<qint32>(obj.backupdirs.size());
    for(auto it = obj.backupdirs.cbegin(); it != obj.backupdirs.cend(); ++it)
        ds << it.key() << it.value();

    ds << obj.delete_add_file_on_dest << obj.start_backup_on_hotplug
       << obj.save_log << obj.compare_via_checksum;
    ds << obj.notify << obj.notifyRecipients;
    ds << obj.scriptBeforeBackup << obj.scriptAfterBackup;
    ds << static_cast<qint32>(obj.intervalType) << obj.intervalTime << static_cast<qint32>(obj.intervalDay);
    ds << static_cast<qint32>(obj.encLUKSType) << obj.encLUKSFilePath;
    ds << obj.pbs << obj.pbs_server_uuid << obj.pbs_server_storage;
    ds << obj.pbs_backup_ids;
    ds << obj.pbs_dest_folder;

    return ds;
}

QDataStream &operator>>(QDataStream &ds, tiBackupJob &obj)
{
    ds >> obj.name >> obj.device >> obj.partition_uuid;

    obj.backupdirs.clear();
    qint32 dirCount = 0;
    ds >> dirCount;
    for(qint32 i = 0; i < dirCount; ++i)
    {
        QString key, value;
        ds >> key >> value;
        obj.backupdirs.insert(key, value);
    }

    ds >> obj.delete_add_file_on_dest >> obj.start_backup_on_hotplug
       >> obj.save_log >> obj.compare_via_checksum;
    ds >> obj.notify >> obj.notifyRecipients;
    ds >> obj.scriptBeforeBackup >> obj.scriptAfterBackup;

    qint32 intervalType = 0, intervalDay = 0;
    ds >> intervalType >> obj.intervalTime >> intervalDay;
    obj.intervalType = static_cast<tiBackupJobInterval>(intervalType);
    obj.intervalDay = intervalDay;

    qint32 encLUKSType = 0;
    ds >> encLUKSType >> obj.encLUKSFilePath;
    obj.encLUKSType = static_cast<tiBackupEncLUKS>(encLUKSType);

    ds >> obj.pbs >> obj.pbs_server_uuid >> obj.pbs_server_storage;
    ds >> obj.pbs_backup_ids;
    ds >> obj.pbs_dest_folder;

    return ds;
}
