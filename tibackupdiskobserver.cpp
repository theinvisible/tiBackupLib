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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <sys/time.h> //debug -> remove me
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
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;

    struct udev_list_entry *list_entry = 0;
    struct udev_list_entry* model_entry = 0;

    struct udev_monitor *udev_monitor = NULL;
    fd_set readfds;
    s_bSD_present = false;

    udev = udev_new();
    if (udev == NULL)
    {
        printf("udev_new FAILED \n");
    }

    udev_monitor = udev_monitor_new_from_netlink(udev, "udev");
    if (udev_monitor == NULL) {
        printf("udev_monitor_new_from_netlink FAILED \n");
    }

    //add some filters
    if( udev_monitor_filter_add_match_subsystem_devtype(udev_monitor, "block", "disk") < 0 )
    {
        printf("udev_monitor_filter_add_match_subsystem_devtype FAILED \n");
    }

    if (udev_monitor_enable_receiving(udev_monitor) < 0)
    {
        printf("udev_monitor_enable_receiving FAILED \n");
    }

    while (1) {
        printf("Polling for new data... \n");

        QCoreApplication::processEvents();

        int fdcount = 0;

        FD_ZERO(&readfds);

        if (udev_monitor != NULL)
        {
            FD_SET(udev_monitor_get_fd(udev_monitor), &readfds);
        }

        fdcount = select(udev_monitor_get_fd(udev_monitor)+1, &readfds, NULL, NULL, NULL);
        if (fdcount < 0)
        {
            if (errno != EINTR)
                printf("Error receiving uevent message\n");
            continue;
        }

        if ((udev_monitor != NULL) && FD_ISSET(udev_monitor_get_fd(udev_monitor), &readfds))
        {
            struct udev_device *device;

            device = udev_monitor_receive_device(udev_monitor);
            if (device == NULL)
                continue;

            //check presence
            if( isDeviceUSB(device) || isDeviceATA(device) )
            {
                /*
                if(!s_bSD_present) //guard for double "change" events
                {
                    s_bSD_present = true;
                    printf("+++SD has been plugged in \n");
                }
                */

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

                DeviceDisk *disk = new DeviceDisk();
                disk->name = udevName;
                disk->devname = udevDevname;
                disk->devtype = udevDevtype;
                disk->vendor = udevIDvendor;
                disk->model = udevIDmodel;

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
                if(s_bSD_present) //not needed -> just keeping consistency
                {
                    s_bSD_present = false;
                    printf("---SD has been removed \n");
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

bool tiBackupDiskObserver::isDeviceATA(udev_device *device)
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
