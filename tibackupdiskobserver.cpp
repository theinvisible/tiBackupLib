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

#include "tibackupdiskobserver.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <QDebug>
#include <QCoreApplication>

static bool s_bSD_present;

tiBackupDiskObserver::tiBackupDiskObserver(QObject *parent) : QObject(parent)
{

}

void tiBackupDiskObserver::start()
{
    struct udev *udev;
    struct udev_list_entry *list_entry = nullptr;
    struct udev_list_entry *model_entry = nullptr;

    struct udev_monitor *udev_monitor = nullptr;
    fd_set readfds;
    s_bSD_present = false;

    udev = udev_new();
    if (udev == nullptr)
    {
        qCritical() << "udev_new FAILED";
    }

    udev_monitor = udev_monitor_new_from_netlink(udev, "udev");
    if (udev_monitor == nullptr)
    {
        qCritical() << "udev_monitor_new_from_netlink FAILED";
    }

    if( udev_monitor_filter_add_match_subsystem_devtype(udev_monitor, "block", "disk") < 0 )
    {
        qWarning() << "udev_monitor_filter_add_match_subsystem_devtype FAILED";
    }

    if (udev_monitor_enable_receiving(udev_monitor) < 0)
    {
        qWarning() << "udev_monitor_enable_receiving FAILED";
    }

    while (1) {
        qDebug() << "Polling devices...";

        QCoreApplication::processEvents();

        int fdcount = 0;

        FD_ZERO(&readfds);

        if (udev_monitor != nullptr)
        {
            FD_SET(udev_monitor_get_fd(udev_monitor), &readfds);
        }

        fdcount = select(udev_monitor_get_fd(udev_monitor)+1, &readfds, nullptr, nullptr, nullptr);
        if (fdcount < 0)
        {
            if (errno != EINTR)
                qWarning() << "Error receiving uevent message";
            continue;
        }

        if ((udev_monitor != nullptr) && FD_ISSET(udev_monitor_get_fd(udev_monitor), &readfds))
        {
            struct udev_device *device;

            device = udev_monitor_receive_device(udev_monitor);
            if (device == nullptr)
                continue;

            if( isDeviceUSB(device) || isDeviceATA(device) )
            {
                list_entry = udev_device_get_properties_list_entry(device);

                model_entry = udev_list_entry_get_by_name(list_entry, "ACTION");
                QString action = udev_list_entry_get_value(model_entry);

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

                qDebug() << "status: " << action;

                DeviceDisk disk;
                disk.name = udevName;
                disk.devname = udevDevname;
                disk.devtype = udevDevtype;
                disk.vendor = udevIDvendor;
                disk.model = udevIDmodel;

                if(action == "add")
                {
                    emit diskAdded(disk);
                }
                else if(action == "remove")
                {
                    emit diskRemoved(disk);
                }
            }
            else
            {
                if(s_bSD_present)
                {
                    s_bSD_present = false;
                    qDebug() << "SD has been removed";
                }
            }

            QCoreApplication::processEvents();

            udev_device_unref(device);
        }
    }
}

bool tiBackupDiskObserver::isDeviceUSB(struct udev_device *device)
{
    bool retVal = false;
    struct udev_list_entry *list_entry = nullptr;
    struct udev_list_entry *model_entry = nullptr;

    list_entry = udev_device_get_properties_list_entry(device);
    model_entry = udev_list_entry_get_by_name(list_entry, "ID_BUS");
    if( nullptr != model_entry )
    {
        const char* szModelValue = udev_list_entry_get_value(model_entry);
        if( strcmp( szModelValue, "usb") == 0 )
        {
            retVal = true;
            print_device(device, "UDEV");
        }
    }
    return retVal;
}

bool tiBackupDiskObserver::isDeviceATA(udev_device *device)
{
    bool retVal = false;
    struct udev_list_entry *list_entry = nullptr;
    struct udev_list_entry *model_entry = nullptr;

    list_entry = udev_device_get_properties_list_entry(device);
    model_entry = udev_list_entry_get_by_name(list_entry, "ID_BUS");
    if( nullptr != model_entry )
    {
        const char* szModelValue = udev_list_entry_get_value(model_entry);
        if( strcmp( szModelValue, "ata") == 0 )
        {
            retVal = true;
            print_device(device, "UDEV");
        }
    }
    return retVal;
}

void tiBackupDiskObserver::print_device(struct udev_device *device, const char *source)
{
    struct timeval tv;
    struct timezone tz;

    gettimeofday(&tv, &tz);
    qDebug() << QString::asprintf("%-6s[%llu.%06u] %-8s %s (%s)",
             source,
             (unsigned long long) tv.tv_sec, (unsigned int) tv.tv_usec,
             udev_device_get_action(device),
             udev_device_get_devpath(device),
             udev_device_get_subsystem(device));

    struct udev_list_entry *list_entry;
    udev_list_entry_foreach(list_entry, udev_device_get_properties_list_entry(device))
        qDebug() << QString::asprintf("%s=%s",
                     udev_list_entry_get_name(list_entry),
                     udev_list_entry_get_value(list_entry));
}
