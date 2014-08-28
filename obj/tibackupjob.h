#ifndef TIBACKUPJOB_H
#define TIBACKUPJOB_H

#include <QString>
#include <QHash>

#include "devicedisk.h"

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

    void startBackup(DeviceDiskPartition *part);
};

#endif // TIBACKUPJOB_H
