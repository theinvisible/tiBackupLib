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

#include "devicedisk.h"

#include <stdio.h>
#include <string.h>

#include <QDebug>

DeviceDisk::DeviceDisk()
{
    QList<DeviceDiskPartition> partitions;
}

void DeviceDisk::readPartitions()
{
    partitions.clear();

    if(devname.isEmpty())
        return;

    blkid_probe pr = blkid_new_probe_from_filename(devname.toStdString().c_str());
    qDebug() << "DeviceDisk::readPartitions(): probe disk for partitions :: " << devname;
    if (!pr) {
        qDebug() << "DeviceDisk::readPartitions(): blkid_new_probe_from_filename() error occured, skipping scan :: " << devname;
        return;
    }

    // Get number of partitions
    blkid_partlist ls;
    int nparts, i;

    ls = blkid_probe_get_partitions(pr);
    if (!ls) {
        qDebug() << "DeviceDisk::readPartitions(): blkid_probe_get_partitions() error occured, skipping scan :: " << devname;
        blkid_free_probe(pr);
        return;
    }
    nparts = blkid_partlist_numof_partitions(ls);

    if (nparts <= 0) {
        blkid_free_probe(pr);
        return;
    }

    // Get UUID, label and type
    const char *uuid = 0;
    const char *label = 0;
    const char *type = 0;

    for (i = 0; i < nparts; i++) {
       char dev_name[50];

       sprintf(dev_name, "%s%d", devname.toStdString().c_str(), (i+1));

       pr = blkid_new_probe_from_filename(dev_name);
       if(pr == NULL)
       {
           // Quick fix for NVME drives
            sprintf(dev_name, "%sp%d", devname.toStdString().c_str(), (i+1));
            pr = blkid_new_probe_from_filename(dev_name);
            if(pr == NULL)
                continue;
       }

       blkid_do_probe(pr);

       qDebug() << "DeviceDisk::readPartitions(): read blkid values for " << dev_name;

       blkid_probe_lookup_value(pr, "UUID", &uuid, NULL);
       blkid_probe_lookup_value(pr, "LABEL", &label, NULL);
       blkid_probe_lookup_value(pr, "TYPE", &type, NULL);

       qDebug() << "DeviceDisk::readPartitions(): blkid VALUES::" << uuid << "::" << label << "::" << type;

       DeviceDiskPartition part;
       part.name = dev_name;
       part.uuid = uuid;
       part.label = label;
       part.type = type;

       uuid = 0;
       label = 0;
       type = 0;

       partitions.append(part);
    }

    blkid_free_probe(pr);
}

DeviceDiskPartition DeviceDisk::getPartitionByUUID(const QString &uuid)
{
    readPartitions();

    DeviceDiskPartition part;

    for (int i=0; i < partitions.count(); i++)
    {
        part = partitions.at(i);
        qDebug() << "1:" << part.uuid << ":2:" << uuid << ":";
        if(part.uuid == uuid)
            return part;
    }

    DeviceDiskPartition part_empty;
    return part_empty;
}

QDataStream &operator<<(QDataStream &ds, const DeviceDiskPartition &obj)
{
    ds << obj.name;
    ds << obj.uuid;
    ds << obj.label;
    ds << obj.type;

    return ds;
}

QDataStream &operator>>(QDataStream &ds, DeviceDiskPartition &obj)
{
    ds >> obj.name;
    ds >> obj.uuid;
    ds >> obj.label;
    ds >> obj.type;

    return ds;
}
