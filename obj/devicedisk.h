#ifndef DEVICEDISK_H
#define DEVICEDISK_H

#include <QString>
#include <QList>

#include <blkid/blkid.h>

class DeviceDiskPartition
{
public:
    QString name;
    QString uuid;
    QString label;
    QString type;
};

class DeviceDisk
{
public:
    QString name;
    QString uuid;
    QString devname;
    QString devtype;

    QString vendor;
    QString model;

    QList<DeviceDiskPartition> partitions;

    DeviceDisk();

    void readPartitions();
    DeviceDiskPartition getPartitionByUUID(const QString &uuid);
};

#endif // DEVICEDISK_H
