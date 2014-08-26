#ifndef TIBACKUPJOB_H
#define TIBACKUPJOB_H

#include <QString>
#include <QHash>

class tiBackupJob
{
public:
    tiBackupJob();

    QString name;
    QString device;
    QString partition_uuid;

    QHash<QString, QString> backupdirs;

    bool delete_add_file_on_dest;
};

#endif // TIBACKUPJOB_H
