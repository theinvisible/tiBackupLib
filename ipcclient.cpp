#include "ipcclient.h"

#include <QLocalSocket>

#include "config.h"
#include "tibackupapi.h"

ipcClient* ipcClient::_instance = 0;

ipcClient::ipcClient(QObject *parent) : QObject(parent)
{

}

ipcClient::STATUS_ANSWER ipcClient::startBackup(const QString &jobname)
{
    ipcClient::STATUS_ANSWER ret;

    QLocalSocket *apiClient = new QLocalSocket(this);
    apiClient->connectToServer(tibackup_config::api_sock_name);
    if(apiClient->waitForConnected(1000))
    {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(tibackup_config::ipc_version);
        QHash<tiBackupApi::API_VAR, QString> apiData;
        apiData[tiBackupApi::API_VAR::API_VAR_CMD] = tiBackupApi::API_CMD::API_CMD_START;
        apiData[tiBackupApi::API_VAR::API_VAR_BACKUPJOB] = jobname;
        out << apiData;

        apiClient->write(block);
        apiClient->flush();

        ret.status = ipcClient::STATUS_OK;
    }
    else
    {
        ret.status = ipcClient::STATUS_CONNECT_FAILED;
        ret.msg = apiClient->errorString();
    }

    apiClient->close();
    apiClient->disconnect();

    return ret;
}

QList<DeviceDiskPartition> ipcClient::getPartitionsForDevName(const QString &devname)
{
    QLocalSocket *apiClient = new QLocalSocket(this);
    apiClient->connectToServer(tibackup_config::api_sock_name);
    QList<DeviceDiskPartition> partitions;
    if(apiClient->waitForConnected(1000))
    {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_9);
        QHash<tiBackupApi::API_VAR, QString> apiData;
        apiData[tiBackupApi::API_VAR::API_VAR_CMD] = tiBackupApi::API_CMD::API_CMD_DISK_GET_PARTITIONS;
        apiData[tiBackupApi::API_VAR::API_VAR_DEVNAME] = devname;
        out << apiData;

        apiClient->write(block);
        apiClient->flush();

        apiClient->waitForReadyRead(5000);

        QDataStream in(apiClient);
        in.setVersion(QDataStream::Qt_5_9);
        in >> partitions;
    }
    else
    {
        qWarning() << apiClient->errorString();
    }

    apiClient->close();
    apiClient->disconnect();

    return partitions;
}

DeviceDiskPartition ipcClient::getPartitionByDevnameUUID(const QString &devname, const QString &uuid)
{
    QLocalSocket *apiClient = new QLocalSocket(this);
    apiClient->connectToServer(tibackup_config::api_sock_name);
    DeviceDiskPartition part;
    if(apiClient->waitForConnected(1000))
    {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(tibackup_config::ipc_version);
        QHash<tiBackupApi::API_VAR, QString> apiData;
        apiData[tiBackupApi::API_VAR::API_VAR_CMD] = tiBackupApi::API_CMD::API_CMD_DISK_GET_PARTITION_BY_DEVNAME_UUID;
        apiData[tiBackupApi::API_VAR::API_VAR_DEVNAME] = devname;
        apiData[tiBackupApi::API_VAR::API_VAR_PART_UUID] = uuid;
        out << apiData;

        apiClient->write(block);
        apiClient->flush();

        apiClient->waitForReadyRead(5000);

        QDataStream in(apiClient);
        in.setVersion(tibackup_config::ipc_version);
        in >> part;
    }
    else
    {
        qWarning() << apiClient->errorString();
    }

    apiClient->close();
    apiClient->disconnect();

    return part;
}

DeviceDiskPartition ipcClient::getPartitionByUUID(const QString &uuid)
{
    QLocalSocket *apiClient = new QLocalSocket(this);
    apiClient->connectToServer(tibackup_config::api_sock_name);
    DeviceDiskPartition part;
    if(apiClient->waitForConnected(1000))
    {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(tibackup_config::ipc_version);
        QHash<tiBackupApi::API_VAR, QString> apiData;
        apiData[tiBackupApi::API_VAR::API_VAR_CMD] = tiBackupApi::API_CMD::API_CMD_DISK_GET_PARTITION_BY_UUID;
        apiData[tiBackupApi::API_VAR::API_VAR_PART_UUID] = uuid;
        out << apiData;

        apiClient->write(block);
        apiClient->flush();

        apiClient->waitForReadyRead(5000);

        QDataStream in(apiClient);
        in.setVersion(tibackup_config::ipc_version);
        in >> part;
    }
    else
    {
        qWarning() << apiClient->errorString();
    }

    apiClient->close();
    apiClient->disconnect();

    return part;
}
