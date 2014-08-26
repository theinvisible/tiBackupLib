#include "tibackuplib.h"

#include <QDebug>
#include <QDir>

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

        if( isDeviceUSB(dev) )
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

            print_device(device, "UDEV");
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

QString TiBackupLib::mountPartition(DeviceDiskPartition *part)
{
    QString mount_dir = QString(tibackup_config::mount_root).append("/").append(part->uuid);
    QDir m_dir(mount_dir);
    if(!m_dir.exists(mount_dir))
        m_dir.mkdir(mount_dir);

    int ret = mount(part->name.toStdString().c_str(), mount_dir.toStdString().c_str(), part->type.toStdString().c_str(), 0, 0);

    return mount_dir;
}

void TiBackupLib::umountPartition(DeviceDiskPartition *part)
{
    QString mount_dir = QString(tibackup_config::mount_root).append("/").append(part->uuid);
    int ret = umount(mount_dir.toStdString().c_str());
}
