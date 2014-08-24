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
    void print_device(struct udev_device *device, const char *source);

};

#endif // TIBACKUPDISKOBSERVER_H
