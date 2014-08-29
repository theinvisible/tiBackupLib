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

    qDebug() << "dd1:" << devname ;

    if(devname.isEmpty())
        return;

    qDebug() << "dd11:" << devname ;


    blkid_probe pr = blkid_new_probe_from_filename(devname.toStdString().c_str());
    if (!pr) {
       return;
    }

    qDebug() << "dd2";

    // Get number of partitions
    blkid_partlist ls;
    int nparts, i;

    ls = blkid_probe_get_partitions(pr);
    nparts = blkid_partlist_numof_partitions(ls);

    qDebug() << "dd22";

    if (nparts <= 0){
       return;
    }

    qDebug() << "dd3";

    // Get UUID, label and type
    const char *uuid;
    const char *label;
    const char *type;

    for (i = 0; i < nparts; i++) {
       char dev_name[20];

       sprintf(dev_name, "%s%d", devname.toStdString().c_str(), (i+1));

       pr = blkid_new_probe_from_filename(dev_name);
       blkid_do_probe(pr);

       blkid_probe_lookup_value(pr, "UUID", &uuid, NULL);
       blkid_probe_lookup_value(pr, "LABEL", &label, NULL);
       blkid_probe_lookup_value(pr, "TYPE", &type, NULL);

       DeviceDiskPartition part;
       part.name = dev_name;
       part.uuid = uuid;
       part.label = label;
       part.type = type;

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
