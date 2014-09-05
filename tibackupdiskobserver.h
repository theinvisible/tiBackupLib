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

#ifndef TIBACKUPDISKOBSERVER_H
#define TIBACKUPDISKOBSERVER_H

#include <QObject>

#include <libudev.h>
#include "obj/devicedisk.h"

class tiBackupDiskObserver : public QObject
{
    Q_OBJECT
public:
    explicit tiBackupDiskObserver(QObject *parent = 0);
    void start();

signals:
    void diskRemoved(DeviceDisk *disk);
    void diskAdded(DeviceDisk *disk);

public slots:

private:
    bool isDeviceUSB(struct udev_device *device);
    bool isDeviceATA(struct udev_device *device);
    void print_device(struct udev_device *device, const char *source);

};

#endif // TIBACKUPDISKOBSERVER_H
