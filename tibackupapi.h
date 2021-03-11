#ifndef TIBACKUPAPI_H
#define TIBACKUPAPI_H

#include <QObject>
#include <QString>

class tiBackupApi : public QObject
{
    Q_OBJECT
public:
    explicit tiBackupApi(QObject *parent = 0);

    enum API_CMD {
        API_CMD_START = 1,
        API_CMD_STOP,
        API_CMD_DISK_GET_PARTITIONS,
        API_CMD_DISK_GET_PARTITION_BY_DEVNAME_UUID,
        API_CMD_DISK_GET_PARTITION_BY_UUID
    };

    enum API_VAR {
        API_VAR_CMD = 1,
        API_VAR_BACKUPJOB,
        API_VAR_DEVNAME,
        API_VAR_PART_UUID
    };

signals:

public slots:
};

#endif // TIBACKUPAPI_H
