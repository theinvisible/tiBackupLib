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

#ifndef TIBACKUPJOB_H
#define TIBACKUPJOB_H

#include <QString>
#include <QHash>
#include <QMultiHash>
#include <QDataStream>

#include "devicedisk.h"
#include "backupmanager.h"

class backupManager;

enum class tiBackupJobInterval {
    NONE,
    DAILY,
    WEEKLY,
    MONTHLY
};

enum class tiBackupEncLUKS {
    NONE,
    FILE,
    GENUSBDEV
};

class tiBackupJobLog
{
public:
    QString rsync_path;
    QString source;
    QString destination;
    int ret_code;
};

class tiBackupJobPBSLog
{
public:
    QString vmid;
    QString msg;
    QString errmsg;
    int ret_code;
};

// One SSH source server referenced by a job, together with its remote-source ->
// local-destination folder mappings (dest may contain the %MNTBACKUPDIR% token).
class tiBackupJobSSHTarget
{
public:
    QString server_uuid;
    QMultiHash<QString, QString> backupdirs;
};

class tiBackupJob
{
public:
    tiBackupJob();

    QString name;
    QString device;
    QString partition_uuid;

    QMultiHash<QString, QString> backupdirs;

    bool delete_add_file_on_dest;
    bool start_backup_on_hotplug;
    bool save_log;
    bool compare_via_checksum;
    // Unmount the backup target after a successful run (default on). Off leaves a
    // permanently-mounted internal target disk mounted. Unmounts even when the
    // drive was already mounted at job start (e.g. auto-mounted by the OS).
    bool umount_after_backup = true;

    bool notify;
    QString notifyRecipients;

    QString scriptBeforeBackup;
    QString scriptAfterBackup;

    tiBackupJobInterval intervalType;
    QString intervalTime;
    int intervalDay;

    tiBackupEncLUKS encLUKSType;
    QString encLUKSFilePath;

    bool pbs;
    QString pbs_server_uuid;
    QString pbs_server_storage;
    QList<QString> pbs_backup_ids;
    QString pbs_dest_folder;

    bool ssh;
    QList<tiBackupJobSSHTarget> ssh_targets;

    // Return true when the backup ran; false on a hard abort (partition-uuid
    // unset, disk not attached, or the target could not be mounted). Per-item
    // rsync/PBS/SSH errors are logged but do NOT flip the result to false.
    bool startBackup();
    void startBackupThread(backupManager *manager);
    bool startBackup(DeviceDiskPartition *part);
};

QDataStream &operator<<(QDataStream &ds, const tiBackupJob &obj);
QDataStream &operator>>(QDataStream &ds, tiBackupJob &obj);

#endif // TIBACKUPJOB_H
