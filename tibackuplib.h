#ifndef TIBACKUPLIB_H
#define TIBACKUPLIB_H

#include "tibackuplib_global.h"

#include <libudev.h>
#include "obj/devicedisk.h"

class TIBACKUPLIBSHARED_EXPORT TiBackupLib
{

public:
    TiBackupLib();

    QList<DeviceDisk> getAttachedDisks();
    bool isDeviceUSB(struct udev_device *device);
    void print_device(struct udev_device *device, const char *source);

    QString mountPartition(DeviceDiskPartition *part);
    void umountPartition(DeviceDiskPartition *part);

    bool isMounted(const QString &dev_path);
    QString getMountDir(const QString &dev_path);

    QString runCommandwithOutput(const QString &cmd);
    int runCommandwithReturnCode(const QString &cmd);
};

#endif // TIBACKUPLIB_H
