#include "ipcclient.h"

#include <QLocalSocket>

#include "config.h"
#include "tibackupapi.h"

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
        apiData[tiBackupApi::API_VAR::API_VAR_CMD] = QString::number(tiBackupApi::API_CMD::API_CMD_START);
        apiData[tiBackupApi::API_VAR::API_VAR_BACKUPJOB] = jobname;
        out << apiData;

        apiClient->write(block);
        apiClient->flush();

        ret.status = STATUS::STATUS_OK;
    }
    else
    {
        ret.status = STATUS::STATUS_CONNECT_FAILED;
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
        out.setVersion(tibackup_config::ipc_version);
        QHash<tiBackupApi::API_VAR, QString> apiData;
        apiData[tiBackupApi::API_VAR::API_VAR_CMD] = QString::number(tiBackupApi::API_CMD::API_CMD_DISK_GET_PARTITIONS);
        apiData[tiBackupApi::API_VAR::API_VAR_DEVNAME] = devname;
        out << apiData;

        apiClient->write(block);
        apiClient->flush();

        apiClient->waitForReadyRead(5000);

        QDataStream in(apiClient);
        in.setVersion(tibackup_config::ipc_version);
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
        apiData[tiBackupApi::API_VAR::API_VAR_CMD] = QString::number(tiBackupApi::API_CMD::API_CMD_DISK_GET_PARTITION_BY_DEVNAME_UUID);
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
        apiData[tiBackupApi::API_VAR::API_VAR_CMD] = QString::number(tiBackupApi::API_CMD::API_CMD_DISK_GET_PARTITION_BY_UUID);
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

ipcClient::STATUS_ANSWER ipcClient::mountPartition(const DeviceDiskPartition &part, const tiBackupJob &job)
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
        apiData[tiBackupApi::API_VAR::API_VAR_CMD] = QString::number(tiBackupApi::API_CMD::API_CMD_PART_MOUNT);
        apiData[tiBackupApi::API_VAR::API_VAR_PART_UUID] = part.uuid;
        apiData[tiBackupApi::API_VAR::API_VAR_JOB_LUKS_TYPE] = QString::number(static_cast<int>(job.encLUKSType));
        apiData[tiBackupApi::API_VAR::API_VAR_JOB_LUKS_FILEPATH] = job.encLUKSFilePath;
        out << apiData;

        apiClient->write(block);
        apiClient->flush();

        ret.status = STATUS::STATUS_OK;
    }
    else
    {
        ret.status = STATUS::STATUS_CONNECT_FAILED;
        ret.msg = apiClient->errorString();
    }

    apiClient->close();
    apiClient->disconnect();

    return ret;
}

ipcClient::STATUS_ANSWER ipcClient::checkHealth()
{
    ipcClient::STATUS_ANSWER ret;

    QLocalSocket *apiClient = new QLocalSocket(this);
    apiClient->connectToServer(tibackup_config::api_sock_name);
    if(apiClient->waitForConnected(1000))
    {
        ret.status = STATUS::STATUS_OK;
    }
    else
    {
        ret.status = STATUS::STATUS_CONNECT_FAILED;
        ret.msg = apiClient->errorString();
    }

    apiClient->close();
    apiClient->disconnect();

    return ret;
}

QHash<QString, backupManager::backupStatus> ipcClient::getBackupStatus()
{
    QLocalSocket *apiClient = new QLocalSocket(this);
    apiClient->connectToServer(tibackup_config::api_sock_name);
    QHash<QString, int> tmp;
    QHash<QString, backupManager::backupStatus> status;
    if(apiClient->waitForConnected(1000))
    {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(tibackup_config::ipc_version);
        QHash<tiBackupApi::API_VAR, QString> apiData;
        apiData[tiBackupApi::API_VAR::API_VAR_CMD] = QString::number(tiBackupApi::API_CMD::API_CMD_BACKUP_STATUS);
        out << apiData;

        apiClient->write(block);
        apiClient->flush();

        apiClient->waitForReadyRead(5000);

        QDataStream in(apiClient);
        in.setVersion(tibackup_config::ipc_version);
        in >> tmp;
    }
    else
    {
        qWarning() << apiClient->errorString();
    }

    apiClient->close();
    apiClient->disconnect();

    for(auto it = tmp.cbegin(); it != tmp.cend(); ++it)
        status.insert(it.key(), static_cast<backupManager::backupStatus>(it.value()));

    return status;
}

backupManager::backupStatus ipcClient::getBackupStatus(const QString &jobname)
{
    QLocalSocket *apiClient = new QLocalSocket(this);
    apiClient->connectToServer(tibackup_config::api_sock_name);
    int status = static_cast<int>(backupManager::backupStatus::standby);
    if(apiClient->waitForConnected(1000))
    {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(tibackup_config::ipc_version);
        QHash<tiBackupApi::API_VAR, QString> apiData;
        apiData[tiBackupApi::API_VAR::API_VAR_CMD] = QString::number(tiBackupApi::API_CMD::API_CMD_BACKUP_STATUS);
        apiData[tiBackupApi::API_VAR::API_VAR_BACKUPJOB] = jobname;
        out << apiData;

        apiClient->write(block);
        apiClient->flush();

        apiClient->waitForReadyRead(5000);

        QDataStream in(apiClient);
        in.setVersion(tibackup_config::ipc_version);
        in >> status;
    }
    else
    {
        qWarning() << apiClient->errorString();
    }

    apiClient->close();
    apiClient->disconnect();

    return static_cast<backupManager::backupStatus>(status);
}
