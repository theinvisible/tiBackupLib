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

#include "tibackuplib.h"

#include <QDebug>
#include <QDir>
#include <QProcess>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <sys/time.h> //debug -> remove me
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/mount.h>
#include <mntent.h>

#include "config.h"

TiBackupLib::TiBackupLib()
{
}

QList<DeviceDisk> TiBackupLib::getAttachedDisks()
{
    QList<DeviceDisk> disks;
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;

    struct udev_list_entry *list_entry = 0;
    struct udev_list_entry* model_entry = 0;

    udev = udev_new();
    if (udev == NULL)
    {
        printf("udev_new FAILED \n");
    }

    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "block");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(dev_list_entry, devices)
    {
        struct udev_device *dev;
        const char* dev_path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, dev_path);

        //if( isDeviceUSB(dev) || isDeviceATA(dev) || isDeviceSCSI(dev) || isDeviceNVME(dev) )
        if( isDeviceDisk(dev) )
        {
            list_entry = udev_device_get_properties_list_entry(dev);

            model_entry = udev_list_entry_get_by_name(list_entry, "ID_SERIAL");
            QString udevName = udev_list_entry_get_value(model_entry);

            model_entry = udev_list_entry_get_by_name(list_entry, "ID_VENDOR");
            QString udevIDvendor = udev_list_entry_get_value(model_entry);

            model_entry = udev_list_entry_get_by_name(list_entry, "ID_MODEL");
            QString udevIDmodel = udev_list_entry_get_value(model_entry);

            model_entry = udev_list_entry_get_by_name(list_entry, "DEVNAME");
            QString udevDevname = udev_list_entry_get_value(model_entry);

            model_entry = udev_list_entry_get_by_name(list_entry, "DEVTYPE");
            QString udevDevtype = udev_list_entry_get_value(model_entry);

            DeviceDisk disk;
            disk.name = udevName;
            disk.devname = udevDevname;
            disk.devtype = udevDevtype;
            disk.vendor = udevIDvendor;
            disk.model = udevIDmodel;

            disks.append(disk);

            udev_device_unref(dev);
            continue;
        }

        udev_device_unref(dev);
    }
    udev_enumerate_unref(enumerate);

    return disks;
}

bool TiBackupLib::isDeviceUSB(struct udev_device *device)
{
    bool retVal = false;
    struct udev_list_entry *list_entry = 0;
    struct udev_list_entry* model_entry = 0;

    //print_device(device, "UDEV");

    list_entry = udev_device_get_properties_list_entry(device);
    model_entry = udev_list_entry_get_by_name(list_entry, "ID_BUS");
    if( 0 != model_entry )
    {
        const char* szModelValue = udev_list_entry_get_value(model_entry);
        if( strcmp( szModelValue, "usb") == 0 )
        {
            //printf("Device is SD \n");
            retVal = true;

            //print_device(device, "UDEV");
        }
    }
    return retVal;
}

bool TiBackupLib::isDeviceATA(udev_device *device)
{
    bool retVal = false;
    struct udev_list_entry *list_entry = 0;
    struct udev_list_entry* model_entry = 0;

    //print_device(device, "UDEV");

    list_entry = udev_device_get_properties_list_entry(device);
    model_entry = udev_list_entry_get_by_name(list_entry, "ID_BUS");
    if( 0 != model_entry )
    {
        const char* szModelValue = udev_list_entry_get_value(model_entry);
        if( strcmp( szModelValue, "ata") == 0 )
        {
            //printf("Device is SD \n");
            retVal = true;

            //print_device(device, "UDEV");
        }
    }
    return retVal;
}

bool TiBackupLib::isDeviceSCSI(udev_device *device)
{
    bool retVal = false;
    struct udev_list_entry *list_entry = 0;
    struct udev_list_entry* model_entry = 0;

    //print_device(device, "UDEV");

    list_entry = udev_device_get_properties_list_entry(device);
    model_entry = udev_list_entry_get_by_name(list_entry, "ID_BUS");
    if( 0 != model_entry )
    {
        const char* szModelValue = udev_list_entry_get_value(model_entry);
        if( strcmp( szModelValue, "scsi") == 0 )
        {
            //printf("Device is SD \n");
            retVal = true;

            //print_device(device, "UDEV");
        }
    }
    return retVal;
}

bool TiBackupLib::isDeviceNVME(udev_device *device)
{
    bool retVal = false;
    struct udev_list_entry *list_entry = 0;
    struct udev_list_entry* model_entry = 0;

    //print_device(device, "UDEV");

    list_entry = udev_device_get_properties_list_entry(device);
    model_entry = udev_list_entry_get_by_name(list_entry, "ID_BUS");
    if( 0 != model_entry )
    {
        const char* szModelValue = udev_list_entry_get_value(model_entry);
        if( strcmp( szModelValue, "nvme") == 0 )
        {
            //printf("Device is SD \n");
            retVal = true;

            //print_device(device, "UDEV");
        }
    }
    return retVal;
}

bool TiBackupLib::isDeviceDisk(udev_device *device)
{
    bool retVal = false;
    struct udev_list_entry *list_entry = 0;
    struct udev_list_entry* model_entry = 0;
    struct udev_list_entry* model_part_table_type = 0;

    //print_device(device, "UDEV");

    list_entry = udev_device_get_properties_list_entry(device);
    model_entry = udev_list_entry_get_by_name(list_entry, "DEVTYPE");
    model_part_table_type = udev_list_entry_get_by_name(list_entry, "ID_PART_TABLE_TYPE");
    if( 0 != model_entry && 0 != model_part_table_type )
    {
        const char* szModelValue = udev_list_entry_get_value(model_entry);
        const char* szModelValue2 = udev_list_entry_get_value(model_part_table_type);
        if( strcmp( szModelValue, "disk") == 0 && (strcmp( szModelValue2, "gpt") == 0 || strcmp( szModelValue2, "dos") == 0) )
        {
            //printf("Device is SD \n");
            retVal = true;

            //print_device(device, "UDEV");
        }
    }
    return retVal;
}

void TiBackupLib::print_device(struct udev_device *device, const char *source)
{
      struct timeval tv;
      struct timezone tz;

      gettimeofday(&tv, &tz);
      printf("%-6s[%llu.%06u] %-8s %s (%s)\n",
             source,
             (unsigned long long) tv.tv_sec, (unsigned int) tv.tv_usec,
             udev_device_get_action(device),
             udev_device_get_devpath(device),
             udev_device_get_subsystem(device));

            struct udev_list_entry *list_entry;

            udev_list_entry_foreach(list_entry, udev_device_get_properties_list_entry(device))
                  printf("%s=%s\n",
                         udev_list_entry_get_name(list_entry),
                         udev_list_entry_get_value(list_entry));
            printf("\n");

}

QString TiBackupLib::mountPartition(DeviceDiskPartition *part, tiBackupJob *job)
{
    QString mount_dir = QString(tibackup_config::mount_root).append("/").append(part->uuid);
    QDir m_dir(mount_dir);
    if(!m_dir.exists(mount_dir))
        m_dir.mkdir(mount_dir);

    QString mnt_src = part->name;

    if(part->type == "crypto_LUKS")
    {
        // We need further information for encryption
        if(job == 0)
            return "";

        QString pass;
        switch(job->encLUKSType)
        {
        case tiBackupEncLUKSNONE:
            return "";
        case tiBackupEncLUKSFILE:
        {
            QFile script(job->encLUKSFilePath);
            if(!script.exists())
                return "";
            script.open(QIODevice::ReadOnly | QIODevice::Text);
            pass = QString(script.readAll()).trimmed();
            script.close();
        }
            break;
        case tiBackupEncLUKSGENUSBDEV:
            break;
        }

        runCommandwithReturnCodePipe(QString("echo '%1' | cryptsetup luksOpen %2 tibackup_enc_%3").arg(pass, part->name, part->uuid));
        mnt_src = getMountPathSrc(part);
    }

    //int ret = mount(part->name.toStdString().c_str(), mount_dir.toStdString().c_str(), part->type.toStdString().c_str(), 0, 0);
    runCommandwithReturnCode(QString("mount %1 %2").arg(mnt_src, mount_dir));

    return mount_dir;
}

void TiBackupLib::umountPartition(DeviceDiskPartition *part)
{
    //QString mount_dir = QString(tibackup_config::mount_root).append("/").append(part->uuid);
    //int ret = umount(mount_dir.toStdString().c_str());
    QString mount_dir = getMountDir(part);
    runCommandwithReturnCode(QString("umount \"%1\"").arg(mount_dir));

    if(part->type == "crypto_LUKS")
    {
        runCommandwithReturnCode(QString("cryptsetup close tibackup_enc_%1").arg(part->uuid));
    }
}

bool TiBackupLib::isMounted(const QString &dev_path)
{
   FILE * mtab = NULL;
   struct mntent * part = NULL;
   bool is_mounted = false;

   if(( mtab = setmntent("/etc/mtab", "r") ) != NULL)
   {
       while((part = getmntent(mtab)) != NULL)
       {
            if((part->mnt_fsname != NULL) && ( strcmp(part->mnt_fsname, dev_path.toStdString().c_str() )) == 0 )
            {
                is_mounted = true;
            }
       }

       endmntent(mtab);
   }

   return is_mounted;
}

bool TiBackupLib::isMounted(DeviceDiskPartition *dev)
{
    QString dev_path = getMountPathSrc(dev);

    return isMounted(dev_path);
}

QString TiBackupLib::getMountDir(const QString &dev_path)
{
    FILE * mtab = NULL;
    struct mntent * part = NULL;
    QString mount_dir;

    if(( mtab = setmntent("/etc/mtab", "r") ) != NULL)
    {
        while((part = getmntent(mtab)) != NULL)
        {
             if((part->mnt_fsname != NULL) && ( strcmp(part->mnt_fsname, dev_path.toStdString().c_str() )) == 0 )
             {
                 mount_dir = part->mnt_dir;
             }
        }

        endmntent(mtab);
    }

    return mount_dir;
}

QString TiBackupLib::getMountDir(DeviceDiskPartition *dev)
{
    QString dev_path = getMountPathSrc(dev);

    return getMountDir(dev_path);
}

QString TiBackupLib::getMountPathSrc(DeviceDiskPartition *dev)
{
    QString dev_path;
    if(dev->type == "crypto_LUKS") {
        dev_path = "/dev/mapper/tibackup_enc_" + dev->uuid;
    } else {
        dev_path = dev->name;
    }

    return dev_path;
}

QString TiBackupLib::runCommandwithOutput(const QString &cmd, int timeout)
{
    QProcess proc;
    proc.start(cmd, QIODevice::ReadOnly);
    proc.waitForStarted(timeout);
    proc.waitForFinished(timeout);

    return proc.readLine();
}

int TiBackupLib::runCommandwithReturnCode(const QString &cmd, int timeout)
{
    qDebug() << "TiBackupLib::runCommandwithReturnCode() -> run command::" << cmd;

    QProcess proc;
    proc.start(cmd, QIODevice::ReadOnly);
    proc.waitForStarted(timeout);
    proc.waitForFinished(timeout);

    return proc.exitCode();
}

int TiBackupLib::runCommandwithReturnCodePipe(const QString &cmd, int timeout)
{
    qDebug() << "TiBackupLib::runCommandwithReturnCode() -> run command::" << cmd;

    QProcess proc;
    proc.start("bash", QStringList() << "-c" << cmd, QIODevice::ReadOnly);
    proc.waitForStarted(timeout);
    proc.waitForFinished(timeout);

    return proc.exitCode();
}

QString TiBackupLib::convertPath2Generic(const QString &path, const QString &mountdir)
{
    if(mountdir.isEmpty())
        return path;

    QString str = path;
    str.replace(mountdir, QString(tibackup_config::var_partbackup_dir));
    return str;
}

QString TiBackupLib::convertGeneric2Path(const QString &path, const QString &mountdir)
{
    QString str = path;
    str.replace(QString(tibackup_config::var_partbackup_dir), mountdir);
    return str;
}

DeviceDiskPartition TiBackupLib::getPartitionByUUID(const QString &uuid)
{
    TiBackupLib blib;
    QList<DeviceDisk> disks = blib.getAttachedDisks();
    DeviceDiskPartition retPart;
    for(int i=0; i < disks.count(); i++)
    {
        DeviceDisk disk = disks.at(i);

        if(disk.devtype == "disk")
        {
            disk.readPartitions();
            for(int j=0; j < disk.partitions.count(); j++)
            {
                DeviceDiskPartition part = disk.partitions.at(j);

                if(part.uuid.isEmpty())
                    continue;

                if(part.uuid == uuid)
                {
                    return part;
                }
            }
        }
    }

    return retPart;
}

tiBackupInitSystem TiBackupLib::getSystemInitModel()
{
    // Check for Systemd, see: https://www.freedesktop.org/software/systemd/man/sd_booted.html
    QDir dir("/run/systemd/system/");
    if(dir.exists())
    {
        return tiBackupInitSystem::Systemd;
    }
    else
    {
        return tiBackupInitSystem::Other;
    }
}
