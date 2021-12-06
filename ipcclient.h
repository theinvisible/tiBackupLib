#ifndef IPCCLIENT_H
#define IPCCLIENT_H

#include <QObject>

#include "obj/devicedisk.h"
#include "obj/tibackupjob.h"

class ipcClient : public QObject
{
    Q_OBJECT
public:
    explicit ipcClient(QObject *parent = nullptr);

    enum STATUS {
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
       static CGuard g;
       if (!_instance)
          _instance = new ipcClient();
       return _instance;
    }

    STATUS_ANSWER startBackup(const QString &jobname);
    QList<DeviceDiskPartition> getPartitionsForDevName(const QString &devname);
    DeviceDiskPartition getPartitionByDevnameUUID(const QString &devname, const QString &uuid);
    DeviceDiskPartition getPartitionByUUID(const QString &uuid);
    STATUS_ANSWER mountPartition(const DeviceDiskPartition &part, const tiBackupJob &job);
    STATUS_ANSWER checkHealth();

private:
    static ipcClient* _instance;

    class CGuard
    {
    public:
       ~CGuard()
       {
          if( NULL != ipcClient::_instance )
          {
             delete ipcClient::_instance;
             ipcClient::_instance = NULL;
          }
       }
    };

signals:

};

#endif // IPCCLIENT_H
