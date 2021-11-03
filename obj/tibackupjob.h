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

#include "devicedisk.h"

enum tiBackupJobInterval {
    tiBackupJobIntervalNONE,
    tiBackupJobIntervalDAILY,
    tiBackupJobIntervalWEEKLY,
    tiBackupJobIntervalMONTHLY
};

enum tiBackupEncLUKS {
    tiBackupEncLUKSNONE,
    tiBackupEncLUKSFILE,
    tiBackupEncLUKSGENUSBDEV
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
    int ret_code;
};

class tiBackupJob
{
public:
    tiBackupJob();

    QString name;
    QString device;
    QString partition_uuid;

    QHash<QString, QString> backupdirs;

    bool delete_add_file_on_dest;
    bool start_backup_on_hotplug;
    bool save_log;
    bool compare_via_checksum;

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

    void startBackup();
    void startBackupThread();
    void startBackup(DeviceDiskPartition *part);
};

#endif // TIBACKUPJOB_H
