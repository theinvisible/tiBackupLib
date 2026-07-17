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
#include <QStorageInfo>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimeZone>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryFile>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QThread>

#include "tibackupmailer.h"

#include "exitcodes.h"
#include "../tibackuplib.h"
#include "../ticonf.h"
#include "../pbsclient.h"
#include "../sshclient.h"
#include "config.h"
#include "workers/tibackupjobworker.h"

tiBackupJob::tiBackupJob()
{
}

bool tiBackupJob::startBackup()
{
    if(partition_uuid.isEmpty())
    {
        qWarning() << QString("tiBackupJob::startBackup() -> Partition-UUID for Backupjob %1 is not set").arg(name);
        return false;
    }

    DeviceDiskPartition part = TiBackupLib::getPartitionByUUID(partition_uuid);
    if(part.name.isEmpty())
    {
        qWarning() << QString("tiBackupJob::startBackup() -> Disk for BackupJob %1 is not attached, aborting").arg(name);
        return false;
    }

    return startBackup(&part);
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
    // Carry the backup outcome to the manager so it can record finished vs failed.
    // manager lives on the main thread (context object) -> queued; bool is a
    // built-in metatype, safe across the thread boundary.
    QObject::connect(worker, &tiBackupJobWorker::finished, manager,
                     [manager, name = name](bool ok) { manager->onBackupFinished(name, ok); });
    thread->start();
}

bool tiBackupJob::startBackup(DeviceDiskPartition *part)
{
    TiBackupLib lib;
    tiConfMain main_settings;

    QString deviceMountDir;
    QStringList backupArg;   // rsync option flags, one element each (no shell)

    // Unprivileged user that pre/post scripts run as (root-only main.conf setting;
    // never web-settable). Falls back to the compiled-in default when unset.
    QString scriptUser = main_settings.getValue("scripts/run_user").toString();
    if(scriptUser.isEmpty())
        scriptUser = QString(tibackup_config::script_run_user);
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
        backupArg << "--delete";
    }

    if(compare_via_checksum == true)
    {
        detailLog << "Feature: Checksum comparison enabled" << "\n";
        detailLog.flush();
        backupArg << "--checksum";
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
            return false;
        }
    }

    // Execute external script before backup if set. The script must live INSIDE the
    // configured scripts directory (a job may not run an arbitrary file), and it runs
    // UNPRIVILEGED as scriptUser - root only via explicit sudo inside the script.
    if(!scriptBeforeBackup.isEmpty())
    {
        const QString scriptPath = TiBackupLib::confineToScriptsDir(scriptBeforeBackup);
        if(scriptPath.isEmpty())
        {
            QString msg = QString("Script before Backup <%1> is outside the scripts directory or missing, skipping").arg(scriptBeforeBackup);
            bakMessages.append(msg);
            detailLog << msg << "\n";
            detailLog.flush();
        }
        else
        {
            detailLog << QString("Script <%1> will be taken as template").arg(scriptPath) << "\n";
            detailLog.flush();

            QFile script(scriptPath);
            script.open(QIODevice::ReadOnly | QIODevice::Text);
            QString scriptSource = QString(script.readAll());
            script.close();

            QString tmpSource = TiBackupLib::convertGeneric2Path(scriptSource, deviceMountDir);

            // Materialise the computed script in a private temp file (QTemporaryFile:
            // atomic O_EXCL, random name, 0600 - defeats the predictable-name symlink
            // race the old fixed /tmp path had). Stage it in a nested scope so the
            // QTemporaryFile is DESTROYED (its write fd fully closed) before we exec it:
            // execve() returns ETXTBSY while any write fd to the target is still open,
            // and QTemporaryFile keeps its handle for the object's lifetime (close() is
            // not enough). setAutoRemove(false) keeps the file on disk after the scope.
            QString tmpfilename;
            {
                QTemporaryFile tmpScript(QDir::tempPath() + "/tibackup-XXXXXX.sh");
                tmpScript.setAutoRemove(false);
                if(tmpScript.open())
                {
                    tmpfilename = tmpScript.fileName();
                    tmpScript.write(tmpSource.toUtf8());
                    tmpScript.flush();
                    tmpScript.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
                }
            }
            if(tmpfilename.isEmpty())
            {
                QString msg("Script before Backup could not be staged.");
                bakMessages.append(msg);
                detailLog << msg << "\n";
                detailLog.flush();
            }
            else
            {
                detailLog << QString("Computed Script <%1> will be executed before backup as user '%2':").arg(tmpfilename, scriptUser) << "\n";
                detailLog << "------------------------------" << "\n";
                detailLog << tmpSource << "\n";
                detailLog << "------------------------------" << "\n";
                detailLog.flush();

                const int rc = lib.runScriptAsUser(tmpfilename, scriptUser, -1);
                if(rc == -1)
                {
                    QString msg = QString("Script before Backup NOT run: run-user '%1' not found (refusing to run as root).").arg(scriptUser);
                    bakMessages.append(msg);
                    detailLog << msg << "\n";
                }
                else if(rc != 0)
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
                QFile::remove(tmpfilename);
            }
        }
    }

    // We get now the folders to backup
    QString src, dest, logpath;
    for(const auto &[srcKey, destVal] : backupdirs.asKeyValueRange())
    {
        tiBackupJobLog log;
        src = log.source = srcKey;
        dest = log.destination = TiBackupLib::convertGeneric2Path(destVal, deviceMountDir);

        // Build rsync's arguments as a list so the source/destination paths are
        // handed to rsync verbatim. There is no shell and no quoting, so a path
        // containing spaces, quotes or "--rsync-path="-style text can neither be
        // resplit into extra arguments nor injected as an rsync option.
        QStringList rsyncArgs;
        rsyncArgs << "-a";

        // FAT/exFAT can't store Unix permissions/owner/group, so a plain "rsync -a"
        // exits 23 ("some files/attrs were not transferred") even though the file
        // data copied fine. Detect such destinations and drop the unsupported
        // attribute-preservation flags (--modify-window=1 absorbs FAT's 2s time
        // granularity so unchanged files aren't re-copied every run).
        const QByteArray fsType = QStorageInfo(dest).fileSystemType().toLower();
        if(fsType == "vfat" || fsType == "msdos" || fsType == "fat" || fsType == "exfat")
        {
            rsyncArgs << "--no-perms" << "--no-owner" << "--no-group" << "--modify-window=1";
            detailLog << QString("Destination filesystem '%1' has no POSIX attributes; "
                                 "using FAT/exFAT-safe rsync options").arg(QString::fromUtf8(fsType)) << "\n";
            detailLog.flush();
        }

        rsyncArgs += backupArg;   // --delete / --checksum (set above)

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
            rsyncArgs << QString("--log-file=%1").arg(logpath);

            log.rsync_path = logpath;
        }

        detailLog << QString("RSYNC Backup: Backup %1 to %2").arg(src, dest) << "\n";
        detailLog.flush();

        QDir destdir(dest);
        if(!destdir.exists())
            destdir.mkpath(dest);

        rsyncArgs << src << dest;

        QString rsyncOutput;
        log.ret_code = lib.runCommandwithReturnCode(QStringLiteral("rsync"), rsyncArgs, -1, &rsyncOutput);
        if(log.ret_code != 0)
        {
            detailLog << QString("RSYNC Backup failed (rsync exit code %1):").arg(log.ret_code) << "\n";
            if(!rsyncOutput.trimmed().isEmpty())
                detailLog << rsyncOutput.trimmed() << "\n";
            else
                detailLog << "(rsync produced no output)" << "\n";
        }
        else
        {
            detailLog << "RSYNC Backup successful." << "\n";
            if(!rsyncOutput.trimmed().isEmpty())
                detailLog << rsyncOutput.trimmed() << "\n";
        }
        detailLog.flush();
        bakLogs << log;
    }

    // Do SSH source backups if enabled (rsync pull from remote Linux servers).
    // Mirrors the local rsync loop above; the only differences are the ssh
    // transport (rsync -e) and the "user@host:path" remote source.
    if(ssh)
    {
        for(const tiBackupJobSSHTarget &target : ssh_targets)
        {
            std::optional<SSHServer> srvOpt = tiConfSSHServers::instance()->getItemByUuid(target.server_uuid);
            if(!srvOpt)
            {
                QString msg = QString("SSH server %1 not found in config").arg(target.server_uuid);
                bakMessages.append(msg);
                detailLog << msg << "\n";
                detailLog.flush();
                continue;
            }
            // Copy out of the shared singleton up front: this reference is used
            // across the whole (potentially hours-long) transfer, and the main
            // (web) thread may re-read the config meanwhile. A value copy can't dangle.
            const SSHServer srv = *srvOpt;
            if(srv.hostkey.isEmpty())
            {
                QString msg = QString("SSH server %1 (%2) has no pinned host key; run 'Test connection' first")
                                  .arg(srv.name, srv.host);
                bakMessages.append(msg);
                detailLog << msg << "\n";
                detailLog.flush();
                continue;
            }

            // Pin this server's host key in a private temp known_hosts that stays
            // alive for the duration of its transfers (StrictHostKeyChecking=yes).
            QTemporaryFile knownHosts(QDir::tempPath() + "/tibackup-kh-XXXXXX");
            if(!knownHosts.open())
            {
                QString msg("Could not stage temporary known_hosts for SSH backup.");
                bakMessages.append(msg);
                detailLog << msg << "\n";
                detailLog.flush();
                continue;
            }
            knownHosts.write(srv.hostkey.toUtf8());
            knownHosts.flush();

            const QString rsh = sshClient::rshCommand(srv, knownHosts.fileName());

            for(const auto &[srcKey, destVal] : target.backupdirs.asKeyValueRange())
            {
                tiBackupJobLog log;
                const QString remoteSrc = QString("%1@%2:%3").arg(srv.username, srv.host, srcKey);
                const QString dest = TiBackupLib::convertGeneric2Path(destVal, deviceMountDir);
                log.source = remoteSrc;
                log.destination = dest;

                QStringList rsyncArgs;
                rsyncArgs << "-a";

                const QByteArray fsType = QStorageInfo(dest).fileSystemType().toLower();
                if(fsType == "vfat" || fsType == "msdos" || fsType == "fat" || fsType == "exfat")
                {
                    rsyncArgs << "--no-perms" << "--no-owner" << "--no-group" << "--modify-window=1";
                    detailLog << QString("Destination filesystem '%1' has no POSIX attributes; "
                                         "using FAT/exFAT-safe rsync options").arg(QString::fromUtf8(fsType)) << "\n";
                    detailLog.flush();
                }

                rsyncArgs += backupArg;   // --delete / --checksum (set above)

                // Protect the remote path from the remote shell (no word-splitting/
                // globbing) and use the pinned ssh transport.
                rsyncArgs << "--protect-args";
                rsyncArgs << "-e" << rsh;

                if(save_log == true)
                {
                    detailLog << "Feature: Rsync Log will be archived" << "\n";
                    detailLog.flush();
                    QDateTime currentDate = QDateTime::currentDateTime();
                    QString logpathdir = QString("%1/%2").arg(main_settings.getValue("paths/logs").toString(), name);
                    QDir logdir(logpathdir);
                    if(!logdir.exists())
                        logdir.mkpath(logpathdir);

                    QString logpath = QString("%1/%2_%3.log").arg(logpathdir,
                        currentDate.toString("yyyyMMdd-hhmmss"), QString::number(bakLogs.size()));
                    rsyncArgs << QString("--log-file=%1").arg(logpath);

                    log.rsync_path = logpath;
                }

                detailLog << QString("SSH RSYNC Backup: Backup %1 to %2").arg(remoteSrc, dest) << "\n";
                detailLog.flush();

                QDir destdir(dest);
                if(!destdir.exists())
                    destdir.mkpath(dest);

                rsyncArgs << remoteSrc << dest;

                QString rsyncOutput;
                log.ret_code = lib.runCommandwithReturnCode(QStringLiteral("rsync"), rsyncArgs, -1, &rsyncOutput);
                if(log.ret_code != 0)
                {
                    detailLog << QString("SSH RSYNC Backup failed (rsync exit code %1):").arg(log.ret_code) << "\n";
                    if(!rsyncOutput.trimmed().isEmpty())
                        detailLog << rsyncOutput.trimmed() << "\n";
                    else
                        detailLog << "(rsync produced no output)" << "\n";
                }
                else
                {
                    detailLog << "SSH RSYNC Backup successful." << "\n";
                    if(!rsyncOutput.trimmed().isEmpty())
                        detailLog << rsyncOutput.trimmed() << "\n";
                }
                detailLog.flush();
                bakLogs << log;
            }
        }
    }

    // Do PBS backup if enabled
    if(pbs)
    {
        // Check if /dev/stdout is a symlink, needed for PBS Backups
        QFileInfo finfo_stdout("/dev/stdout");
        if(finfo_stdout.isSymLink())
        {
            std::optional<PBServer> pbOpt = tiConfPBServers::instance()->getItemByUuid(pbs_server_uuid);
            if(pbOpt)
            {
                // Copy out of the shared singleton up front: pb is used across a
                // long PBS restore while the main (web) thread may re-read the
                // config. A value copy can't dangle.
                const PBServer pb = *pbOpt;
                pbsClient *pbs = pbsClient::instanceUnique();
                pbs->setExpectedFingerprint(pb.fingerprint);
                HttpStatus::Code status = pb.fingerprint.isEmpty()
                    ? HttpStatus::Code::BadRequest
                    : pbs->auth(pb.host, pb.port, pb.username, pb.password);
                if(status == HttpStatus::Code::OK)
                {
                    // Create process environment for proxmox-backup-client
                    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                    env.insert("PBS_REPOSITORY", QString("%1@%2:%3").arg(pb.username, pb.host, pbs_server_storage));
                    env.insert("PBS_PASSWORD", pb.password);
                    env.insert("PBS_FINGERPRINT", pb.fingerprint);
                    env.insert("PROXMOX_OUTPUT_FORMAT", "json");
                    if(!pb.keypass.isEmpty())
                        env.insert("PBS_ENCRYPTION_PASSWORD", pb.keypass);

                    for(const auto &pbs_groupid : pbs_backup_ids)
                    {
                        QList<QString> pbs_id = pbs_groupid.split("/");
                        // Guard against malformed ids: split("/") on an id without a
                        // slash yields < 2 elements, so pbs_id[1] would be an
                        // out-of-bounds access that crashes the daemon.
                        if(pbs_id.size() < 2 || pbs_id[0].isEmpty() || pbs_id[1].isEmpty())
                        {
                            detailLog << "Skipping malformed PBS backup id: " << pbs_groupid << "\n";
                            detailLog.flush();
                            continue;
                        }
                        QString vmType = pbs_id[0];
                        QString vmID = pbs_id[1];
                        // Only vm/ct are packaged below; skip anything else (e.g. host/)
                        // instead of downloading files that would never be assembled.
                        if(vmType != "vm" && vmType != "ct")
                        {
                            detailLog << "Skipping unsupported PBS backup type '" << vmType << "' for id " << pbs_groupid << "\n";
                            detailLog.flush();
                            continue;
                        }
                        QDir vmdir(QString("%1/%2").arg(TiBackupLib::convertGeneric2Path(pbs_dest_folder, deviceMountDir), pbs_id[1]));
                        vmdir.mkpath(vmdir.path());

                        tiBackupJobPBSLog log;
                        log.vmid = pbs_groupid;

                        detailLog << "PBS Backup: Start backup for id " << pbs_groupid << " to path " << vmdir.path() << "\n";
                        detailLog.flush();

                        // Do additional auth to avoid pbs ticket timeouts
                        pbs->auth(pb.host, pb.port, pb.username, pb.password);
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
                                        QFile::remove(QString("%1/%2").arg(vmdir.path(), file));
                                    }

                                    QProcess p;
                                    p.setProcessEnvironment(env);
                                    p.setProcessChannelMode(QProcess::MergedChannels);
                                    QStringList startargs = QStringList() << "restore" << respec << file << vmdir.path().append("/").append(file);
                                    if(!pb.keyfile.isEmpty())
                                    {
                                        if(QFile::exists(pb.keyfile))
                                        {
                                            startargs << "--keyfile" << pb.keyfile;
                                        }
                                        else
                                        {
                                            QString errmsg = QString("Encryption file %1 not found!").arg(pb.keyfile);
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
                                    const QString base = vmdir.path();
                                    // Each image is one "devname=path" argv token; no shell,
                                    // so paths with spaces/metacharacters stay intact.
                                    QStringList imageArgs;
                                    for(const auto &img : vmImages)
                                    {
                                        const QString devname = img.split(".")[0];
                                        imageArgs << QString("%1=%2/%3").arg(devname, base, img);
                                    }

                                    // Cleanup old backups (shell-free glob)
                                    {
                                        QDir d(base);
                                        for(const QString &f : d.entryList(QStringList() << "vzdump-qemu-*", QDir::Files))
                                            QFile::remove(d.filePath(f));
                                    }

                                    detailLog << "Start compressing VM\n";
                                    QString outName = QString("vzdump-qemu-%1-%2.vma.zst").arg(vmID, dt.toString("yyyy_MM_dd-hh_mm_ss"));
                                    const QString vmConfPath = QString("%1/%2").arg(base, vmConf);
                                    const QString outPath = QString("%1/%2").arg(base, outName);

                                    // Run "vma create /dev/stdout -c <conf> <dev=img>..." piped into
                                    // "zstd -f -3 -T4 -o <out>" without bash, via QProcess chaining.
                                    QStringList vmaArgs = QStringList() << "create" << "/dev/stdout" << "-c" << vmConfPath;
                                    vmaArgs << imageArgs;
                                    detailLog << "Execute VMA-ZStd: vma " << vmaArgs.join(" ") << " | zstd -f -3 -T4 -o " << outPath << "\n";
                                    detailLog.flush();

                                    QProcess vma;
                                    QProcess zstd;
                                    vma.setStandardOutputProcess(&zstd);
                                    vma.start(QStringLiteral("vma"), vmaArgs);
                                    zstd.start(QStringLiteral("zstd"), QStringList() << "-f" << "-3" << "-T4" << "-o" << outPath);
                                    vma.waitForStarted(-1);
                                    zstd.waitForStarted(-1);
                                    vma.waitForFinished(-1);
                                    zstd.waitForFinished(-1);
                                    const bool pipeOk = vma.exitStatus() == QProcess::NormalExit && vma.exitCode() == 0
                                                     && zstd.exitStatus() == QProcess::NormalExit && zstd.exitCode() == 0;

                                    if(pipeOk)
                                    {
                                        QFileInfo outInfo(outPath);
                                        if(outInfo.size() > 1000) {
                                            QString msg = QString("Successful backup, files: %1, archive: %2 (Size: %3GB)").arg(vmImages.join(" "), outName).arg(QString::number(outInfo.size()/1024/1024/1024, 'f', 2));
                                            log.msg.append(msg);
                                            detailLog << msg << "\n";
                                        } else {
                                            QString msg = QString("Failed backup, compression or packaging produced an empty archive: %1").arg(outName);
                                            log.errmsg.append(msg);
                                            detailLog << msg << "\n";
                                        }
                                    }
                                    else
                                    {
                                        QString msg = QString("Compression or packaging failed for %1 (vma exit %2, zstd exit %3)").arg(outName).arg(vma.exitCode()).arg(zstd.exitCode());
                                        log.errmsg.append(msg);
                                        detailLog << msg << "\n";
                                    }
                                    detailLog.flush();
                                }
                                else if(vmType == "ct")
                                {
                                    const QString base = vmdir.path();
                                    QString outName = QString("vzdump-lxc-%1-%2.tar.zst").arg(vmID, dt.toString("yyyy_MM_dd-hh_mm_ss"));
                                    const QString imgDir = QString("%1/%2").arg(base, vmImages[0]);
                                    vmdir.mkpath(QString("%1/etc/vzdump/").arg(imgDir));
                                    QFile::copy(QString("%1/%2").arg(base, vmConf), QString("%1/etc/vzdump/%2").arg(imgDir, vmConf));

                                    // Cleanup old backups (shell-free glob)
                                    {
                                        QDir d(base);
                                        for(const QString &f : d.entryList(QStringList() << "vzdump-lxc-*", QDir::Files))
                                            QFile::remove(d.filePath(f));
                                    }

                                    detailLog << "Start compressing CT\n";
                                    const QString tarPath = QString("%1/ct.tar").arg(base);
                                    const QString outPath = QString("%1/%2").arg(base, outName);
                                    if(lib.runCommandwithReturnCode(QStringLiteral("tar"), QStringList() << "-C" << imgDir << "-cf" << tarPath << ".", -1) == 0)
                                    {
                                        if(lib.runCommandwithReturnCode(QStringLiteral("zstd"), QStringList() << "-f" << "-3" << "-T4" << "--rm" << tarPath << "-o" << outPath, -1) == 0)
                                        {
                                            QFileInfo outInfo(outPath);
                                            if(outInfo.size() > 1000) {
                                                QString msg = QString("Successful backup, files: %1, archive: %2 (Size: %3GB)").arg(vmImages.join(" "), outName).arg(QString::number(outInfo.size()/1024/1024/1024, 'f', 2));
                                                log.msg.append(msg);
                                                detailLog << msg << "\n";
                                            } else {
                                                QString msg = QString("Failed backup, compression produced an empty archive: %1").arg(outName);
                                                log.errmsg.append(msg);
                                                detailLog << msg << "\n";
                                            }
                                        }
                                        else
                                        {
                                            QString msg = QString("Compression failed for %1 (zstd)").arg(outName);
                                            log.errmsg.append(msg);
                                            detailLog << msg << "\n";
                                        }
                                    }
                                    else
                                    {
                                        QString msg = QString("Packaging failed for %1 (tar)").arg(outName);
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
                    QString msg = pb.fingerprint.isEmpty()
                        ? QString("PBS %1 (%2) has no pinned TLS fingerprint; run 'Test connection' and save it first").arg(pb.name, pb.host)
                        : QString("PBS %1 auth not successful").arg(pbs_server_uuid);
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

    // Execute external script after backup if set. Same rules as the before-backup
    // block: confined to the scripts directory, run unprivileged as scriptUser.
    if(!scriptAfterBackup.isEmpty())
    {
        const QString scriptPath = TiBackupLib::confineToScriptsDir(scriptAfterBackup);
        if(scriptPath.isEmpty())
        {
            QString msg = QString("Script after Backup <%1> is outside the scripts directory or missing, skipping").arg(scriptAfterBackup);
            bakMessages.append(msg);
            detailLog << msg << "\n";
            detailLog.flush();
        }
        else
        {
            detailLog << QString("Run script after backup: Script <%1> will be taken as template").arg(scriptPath) << "\n";
            detailLog.flush();

            QFile script(scriptPath);
            script.open(QIODevice::ReadOnly | QIODevice::Text);
            QString scriptSource = QString(script.readAll());
            script.close();

            QString tmpSource = TiBackupLib::convertGeneric2Path(scriptSource, deviceMountDir);

            // Private temp file (O_EXCL) staged in a nested scope so its write fd is
            // closed (QTemporaryFile destroyed) before exec — otherwise execve() gives
            // ETXTBSY. See the before-backup block for the full rationale.
            QString tmpfilename;
            {
                QTemporaryFile tmpScript(QDir::tempPath() + "/tibackup-XXXXXX.sh");
                tmpScript.setAutoRemove(false);
                if(tmpScript.open())
                {
                    tmpfilename = tmpScript.fileName();
                    tmpScript.write(tmpSource.toUtf8());
                    tmpScript.flush();
                    tmpScript.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
                }
            }
            if(tmpfilename.isEmpty())
            {
                QString msg("Script after Backup could not be staged.");
                bakMessages.append(msg);
                detailLog << msg << "\n";
                detailLog.flush();
            }
            else
            {
                detailLog << QString("Computed Script <%1> will be executed after backup as user '%2':").arg(tmpfilename, scriptUser) << "\n";
                detailLog << "------------------------------" << "\n";
                detailLog << tmpSource << "\n";
                detailLog << "------------------------------" << "\n";
                detailLog.flush();

                const int rc = lib.runScriptAsUser(tmpfilename, scriptUser, -1);
                if(rc == -1)
                {
                    QString msg = QString("Script after Backup NOT run: run-user '%1' not found (refusing to run as root).").arg(scriptUser);
                    bakMessages.append(msg);
                    detailLog << msg << "\n";
                }
                else if(rc != 0)
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
                QFile::remove(tmpfilename);
            }
        }
    }

    // We notify the recipients now about the status
    if(notify == true)
    {
        detailLog << "We send notification now to " << notifyRecipients << "\n";
        detailLog.flush();

        // Sender is configurable (smtp/from); fall back to the historical default
        // if the key is unset (e.g. a config that predates seeding).
        QString mailFrom = main_settings.getValue("smtp/from").toString();
        if(mailFrom.isEmpty())
            mailFrom = tibackup_config::mail_from_default;

        const QString mailSubject = QString("Information for backupjob <%1>").arg(name);

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

        // Attach the per-leg rsync logs (when the job keeps them). Attachment names
        // are the real log file names; the mailer base64-encodes them.
        QStringList mailAttachments;
        if(save_log == true && bakLogs.size() > 0)
        {
            for(int ia=0; ia<bakLogs.size(); ia++)
            {
                logpath = bakLogs.at(ia).rsync_path;
                if(QFile::exists(logpath))
                    mailAttachments << logpath;
            }
        }

        // One transport for both notifications and the web "test mail": honours the
        // configured port + security (none/STARTTLS/SSL). Password is stored base64.
        tiBackupMailer::Params mp;
        mp.server   = main_settings.getValue("smtp/server").toString();
        mp.port     = main_settings.getValue("smtp/port").toInt();   // 0 -> mailer uses 25
        mp.security = tiBackupMailer::securityFromString(main_settings.getValue("smtp/security").toString());
        mp.auth     = main_settings.getValue("smtp/auth").toBool();
        mp.username = main_settings.getValue("smtp/username").toString();
        mp.password = QString::fromUtf8(QByteArray::fromBase64(main_settings.getValue("smtp/password").toString().toLatin1()));
        mp.from     = mailFrom;

        const tiBackupMailer::Result mres =
            tiBackupMailer::send(mp, notifyRecipients, mailSubject, mailMsg, mailAttachments);
        if(mres.ok)
            detailLog << "tiBackupJob::startBackup() -> Mail message was send successfully to " << notifyRecipients << "\n";
        else
            detailLog << "tiBackupJob::startBackup() -> Mail message was NOT send. Error occured: " << mres.message << "\n";

        QDateTime currentFinishDate = QDateTime::currentDateTime();
        detailLog << QString("Backup job finished at %1!").arg(currentFinishDate.toString("yyyy-MM-dd_HH-mm")) << "\n";
        detailLog.flush();
    }

    // Unmount the target after a successful backup unless the job opts out (e.g. a
    // permanently-mounted internal disk). Default is on so a removable drive can be
    // safely detached afterwards. This unmounts even when the drive was ALREADY
    // mounted at job start (e.g. auto-mounted by the OS on insertion) - which is the
    // common case on a server and the reason a plain "we mounted it" guard would
    // wrongly leave the drive mounted.
    if(umount_after_backup)
    {
        if(!lib.umountPartition(part))
        {
            detailLog << "Warning: the backup target could not be unmounted (still busy?); "
                         "leaving it mounted." << "\n";
            detailLog.flush();
        }
    }
    else
    {
        detailLog << "Note: 'unmount after backup' is disabled for this job; leaving the "
                     "target mounted on " << deviceMountDir << "." << "\n";
        detailLog.flush();
    }

    return true;
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

    // SSH sources: bool + list of targets, each with a QMultiHash streamed as
    // count + key/value pairs (mirrors the backupdirs encoding above).
    ds << obj.ssh;
    ds << static_cast<qint32>(obj.ssh_targets.size());
    for(const auto &t : obj.ssh_targets)
    {
        ds << t.server_uuid;
        ds << static_cast<qint32>(t.backupdirs.size());
        for(auto it = t.backupdirs.cbegin(); it != t.backupdirs.cend(); ++it)
            ds << it.key() << it.value();
    }

    // Appended after the original format; readers guard with atEnd() for old data.
    ds << obj.umount_after_backup;

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

    ds >> obj.ssh;
    qint32 targetCount = 0;
    ds >> targetCount;
    obj.ssh_targets.clear();
    for(qint32 i = 0; i < targetCount; ++i)
    {
        tiBackupJobSSHTarget t;
        ds >> t.server_uuid;
        qint32 dirCount2 = 0;
        ds >> dirCount2;
        for(qint32 j = 0; j < dirCount2; ++j)
        {
            QString key, value;
            ds >> key >> value;
            t.backupdirs.insert(key, value);
        }
        obj.ssh_targets.append(t);
    }

    // Optional trailing field (see operator<<): default stays true for old streams.
    if(!ds.atEnd())
        ds >> obj.umount_after_backup;

    return ds;
}
