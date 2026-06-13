#ifndef IPCCLIENT_H
#define IPCCLIENT_H

#include <QObject>

#include "obj/devicedisk.h"
#include "obj/tibackupjob.h"
#include "obj/pbserver.h"

class ipcClient : public QObject
{
    Q_OBJECT
public:
    explicit ipcClient(QObject *parent = nullptr);

    enum class STATUS {
        STATUS_OK,
        STATUS_CONNECT_FAILED,
        STATUS_ERROR
    };

    struct STATUS_ANSWER
    {
        STATUS status;
        QString msg;
    };

    static ipcClient* instance()
    {
        static ipcClient* inst = new ipcClient(nullptr);
        return inst;
    }

    STATUS_ANSWER startBackup(const QString &jobname);
    QList<DeviceDiskPartition> getPartitionsForDevName(const QString &devname);
    DeviceDiskPartition getPartitionByDevnameUUID(const QString &devname, const QString &uuid);
    DeviceDiskPartition getPartitionByUUID(const QString &uuid);
    STATUS_ANSWER mountPartition(const DeviceDiskPartition &part, const tiBackupJob &job);
    STATUS_ANSWER checkHealth();
    QHash<QString, backupManager::backupStatus> getBackupStatus();
    backupManager::backupStatus getBackupStatus(const QString &jobname);

    // Privileged write operations relayed to the (root) daemon.
    STATUS_ANSWER setMainConf(const QHash<QString, QString> &values);
    STATUS_ANSWER saveJob(const tiBackupJob &job);
    STATUS_ANSWER deleteJob(const QString &jobname);
    STATUS_ANSWER renameJob(const QString &oldname, const QString &newname);
    STATUS_ANSWER savePBServer(const PBServer &item);
    STATUS_ANSWER deletePBServer(const QString &uuid);
    STATUS_ANSWER saveScript(const QString &path, const QString &content);

signals:

};

#endif // IPCCLIENT_H
