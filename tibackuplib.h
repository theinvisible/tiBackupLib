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

#ifndef TIBACKUPLIB_H
#define TIBACKUPLIB_H

#include "tibackuplib_global.h"

#include <libudev.h>
#include <QStringList>
#include <QByteArray>
#include "obj/devicedisk.h"
#include "obj/tibackupjob.h"

enum class tiBackupInitSystem {
    Systemd,
    Other
};

class TIBACKUPLIBSHARED_EXPORT TiBackupLib
{

public:
    TiBackupLib();

    QList<DeviceDisk> getAttachedDisks();
    bool isDeviceUSB(struct udev_device *device);
    bool isDeviceATA(struct udev_device *device);
    bool isDeviceSCSI(struct udev_device *device);
    bool isDeviceNVME(struct udev_device *device);
    bool isDeviceDisk(struct udev_device *device);
    void print_device(struct udev_device *device, const char *source);

    QString mountPartition(DeviceDiskPartition *part, tiBackupJob *job = nullptr);
    void umountPartition(DeviceDiskPartition *part);

    bool isMounted(const QString &dev_path);
    bool isMounted(DeviceDiskPartition *dev);
    QString getMountDir(const QString &dev_path);
    QString getMountDir(DeviceDiskPartition *dev);
    QString getMountPathSrc(DeviceDiskPartition *dev);

    QString runCommandwithOutput(const QString &cmd, int timeout = 50000);
    // When output != nullptr, the process' merged stdout+stderr is captured into
    // *output (so callers like the rsync backup can log the actual error text).
    int runCommandwithReturnCode(const QString &cmd, int timeout = 50000, QString *output = nullptr);
    // argv form: the program and each argument are passed separately so no shell
    // is involved and arguments can never be misparsed/injected (no quoting, no
    // metacharacter expansion). Prefer this whenever any argument is data.
    int runCommandwithReturnCode(const QString &program, const QStringList &args,
                                 int timeout = 50000, QString *output = nullptr);
    // Like the argv form but feeds `stdinData` to the child's standard input and
    // closes it (used to hand secrets such as a LUKS passphrase to a tool without
    // ever placing them on a shell command line). Returns the process exit code.
    int runCommandwithReturnCode(const QString &program, const QStringList &args,
                                 const QByteArray &stdinData, int timeout = 50000);
    int runCommandwithReturnCodePipe(const QString &cmd, int timeout = 50000);

    // Execute `scriptPath` as the unprivileged system user `username`, dropping the
    // group set + gid + uid in the child before exec (so a job's pre/post script is
    // NOT run as root). Returns the script's exit code, or -1 if `username` cannot be
    // resolved (fail closed: the script is not run rather than falling back to root).
    int runScriptAsUser(const QString &scriptPath, const QString &username,
                        int timeout = 50000, QString *output = nullptr);

    // Resolve a job's script path/name to a canonical path INSIDE the configured
    // paths/scripts directory (symlinks resolved). Returns empty if it escapes the
    // scripts dir - used to keep the web layer from running arbitrary root files.
    static QString confineToScriptsDir(const QString &pathOrName);

    static QString convertPath2Generic(const QString &path, const QString &mountdir);
    static QString convertGeneric2Path(const QString &path, const QString &mountdir);

    static DeviceDiskPartition getPartitionByUUID(const QString &uuid);
    static tiBackupInitSystem getSystemInitModel();
};

#endif // TIBACKUPLIB_H
