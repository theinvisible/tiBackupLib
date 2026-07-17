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
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>

#include <vector>

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>

#include <sys/mount.h>
#include <mntent.h>

#include "config.h"
#include "ticonf.h"

TiBackupLib::TiBackupLib()
{
}

QList<DeviceDisk> TiBackupLib::getAttachedDisks()
{
    QList<DeviceDisk> disks;
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;

    struct udev_list_entry *list_entry = nullptr;
    struct udev_list_entry *model_entry = nullptr;

    udev = udev_new();
    if (udev == nullptr)
    {
        qCritical() << "udev_new FAILED";
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
        }
    }
    return retVal;
}

bool TiBackupLib::isDeviceATA(udev_device *device)
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
        }
    }
    return retVal;
}

bool TiBackupLib::isDeviceSCSI(udev_device *device)
{
    bool retVal = false;
    struct udev_list_entry *list_entry = nullptr;
    struct udev_list_entry *model_entry = nullptr;

    list_entry = udev_device_get_properties_list_entry(device);
    model_entry = udev_list_entry_get_by_name(list_entry, "ID_BUS");
    if( nullptr != model_entry )
    {
        const char* szModelValue = udev_list_entry_get_value(model_entry);
        if( strcmp( szModelValue, "scsi") == 0 )
        {
            retVal = true;
        }
    }
    return retVal;
}

bool TiBackupLib::isDeviceNVME(udev_device *device)
{
    bool retVal = false;
    struct udev_list_entry *list_entry = nullptr;
    struct udev_list_entry *model_entry = nullptr;

    list_entry = udev_device_get_properties_list_entry(device);
    model_entry = udev_list_entry_get_by_name(list_entry, "ID_BUS");
    if( nullptr != model_entry )
    {
        const char* szModelValue = udev_list_entry_get_value(model_entry);
        if( strcmp( szModelValue, "nvme") == 0 )
        {
            retVal = true;
        }
    }
    return retVal;
}

bool TiBackupLib::isDeviceDisk(udev_device *device)
{
    bool retVal = false;
    struct udev_list_entry *list_entry = nullptr;
    struct udev_list_entry *model_entry = nullptr;
    struct udev_list_entry *model_part_table_type = nullptr;

    list_entry = udev_device_get_properties_list_entry(device);
    model_entry = udev_list_entry_get_by_name(list_entry, "DEVTYPE");
    model_part_table_type = udev_list_entry_get_by_name(list_entry, "ID_PART_TABLE_TYPE");
    if( nullptr != model_entry && nullptr != model_part_table_type )
    {
        const char* szModelValue = udev_list_entry_get_value(model_entry);
        const char* szModelValue2 = udev_list_entry_get_value(model_part_table_type);
        if( strcmp( szModelValue, "disk") == 0 && (strcmp( szModelValue2, "gpt") == 0 || strcmp( szModelValue2, "dos") == 0) )
        {
            retVal = true;
        }
    }
    return retVal;
}

void TiBackupLib::print_device(struct udev_device *device, const char *source)
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

QString TiBackupLib::mountPartition(DeviceDiskPartition *part, tiBackupJob *job)
{
    QString mount_dir = QDir(tibackup_config::mount_root).filePath(part->uuid);
    QDir m_dir(mount_dir);
    if(!m_dir.exists(mount_dir))
        m_dir.mkdir(mount_dir);

    QString mnt_src = part->name;

    if(part->type == "crypto_LUKS")
    {
        // We need further information for encryption
        if(job == nullptr)
            return "";

        QString pass;
        switch(job->encLUKSType)
        {
        case tiBackupEncLUKS::NONE:
            return "";
        case tiBackupEncLUKS::FILE:
        {
            QFile script(job->encLUKSFilePath);
            if(!script.exists())
                return "";
            script.open(QIODevice::ReadOnly | QIODevice::Text);
            pass = QString(script.readAll()).trimmed();
            script.close();
        }
            break;
        case tiBackupEncLUKS::GENUSBDEV:
            break;
        }

        // Hand the passphrase to cryptsetup over stdin (terminated by a newline,
        // exactly like the previous "echo '...' |" did) instead of building a
        // shell command. This removes the bash -c invocation entirely, so a
        // passphrase containing quotes or shell metacharacters can no longer
        // break out and execute arbitrary commands as root.
        int rc_luks = runCommandwithReturnCode(QStringLiteral("cryptsetup"),
                                 QStringList() << "luksOpen" << part->name
                                               << QString("tibackup_enc_%1").arg(part->uuid),
                                 pass.toUtf8() + '\n');
        if(rc_luks != 0)
            return "";   // could not unlock -> nothing to mount, report failure
        mnt_src = getMountPathSrc(part);
    }

    int rc = runCommandwithReturnCode(QStringLiteral("mount"), QStringList() << mnt_src << mount_dir);

    // Don't claim success when the mount did not take: return the empty sentinel
    // (callers treat "" as failure) and remove the empty mount dir we created.
    if(rc != 0 || !isMounted(part))
    {
        m_dir.rmdir(mount_dir);
        return "";
    }

    return mount_dir;
}

bool TiBackupLib::umountPartition(DeviceDiskPartition *part)
{
    QString mount_dir = getMountDir(part);
    int rc = runCommandwithReturnCode(QStringLiteral("umount"), QStringList() << mount_dir, -1);

    // Don't claim the device is free when umount failed (e.g. target still busy):
    // report failure and leave the LUKS mapping open - closing it would fail too
    // and could hide the real cause.
    if(rc != 0 || isMounted(part))
    {
        qWarning() << "TiBackupLib::umountPartition() -> failed to unmount" << mount_dir
                   << "(umount rc" << rc << ")";
        return false;
    }

    if(part->type == "crypto_LUKS")
    {
        int rc_luks = runCommandwithReturnCode(QStringLiteral("cryptsetup"),
                                 QStringList() << "close" << QString("tibackup_enc_%1").arg(part->uuid), -1);
        if(rc_luks != 0)
        {
            qWarning() << "TiBackupLib::umountPartition() -> failed to close LUKS mapping for"
                       << part->uuid << "(cryptsetup rc" << rc_luks << ")";
            return false;
        }
    }

    return true;
}

bool TiBackupLib::isMounted(const QString &dev_path)
{
   FILE * mtab = nullptr;
   struct mntent * part = nullptr;
   bool is_mounted = false;

   if(( mtab = setmntent("/etc/mtab", "r") ) != nullptr)
   {
       while((part = getmntent(mtab)) != nullptr)
       {
            if((part->mnt_fsname != nullptr) && ( strcmp(part->mnt_fsname, dev_path.toStdString().c_str() )) == 0 )
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
    FILE * mtab = nullptr;
    struct mntent * part = nullptr;
    QString mount_dir;

    if(( mtab = setmntent("/etc/mtab", "r") ) != nullptr)
    {
        while((part = getmntent(mtab)) != nullptr)
        {
             if((part->mnt_fsname != nullptr) && ( strcmp(part->mnt_fsname, dev_path.toStdString().c_str() )) == 0 )
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
    proc.startCommand(cmd, QIODevice::ReadOnly);
    proc.waitForStarted(timeout);
    proc.waitForFinished(timeout);

    return proc.readLine();
}

int TiBackupLib::runCommandwithReturnCode(const QString &cmd, int timeout, QString *output)
{
    qDebug() << "TiBackupLib::runCommandwithReturnCode() -> run command::" << cmd;

    QProcess proc;
    if(output != nullptr)
        proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.startCommand(cmd, QIODevice::ReadOnly);
    proc.waitForStarted(timeout);
    proc.waitForFinished(timeout);

    if(output != nullptr)
        *output = QString::fromUtf8(proc.readAll());

    return proc.exitCode();
}

int TiBackupLib::runCommandwithReturnCode(const QString &program, const QStringList &args,
                                          int timeout, QString *output)
{
    qDebug() << "TiBackupLib::runCommandwithReturnCode(argv) -> run::" << program << args;

    QProcess proc;
    if(output != nullptr)
        proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(program, args, QIODevice::ReadOnly);
    proc.waitForStarted(timeout);
    proc.waitForFinished(timeout);

    if(output != nullptr)
        *output = QString::fromUtf8(proc.readAll());

    return proc.exitCode();
}

int TiBackupLib::runCommandwithReturnCode(const QString &program, const QStringList &args,
                                          const QByteArray &stdinData, int timeout)
{
    qDebug() << "TiBackupLib::runCommandwithReturnCode(argv,stdin) -> run::" << program << args;

    QProcess proc;
    proc.start(program, args, QIODevice::ReadWrite);
    proc.waitForStarted(timeout);
    proc.write(stdinData);
    proc.closeWriteChannel();
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

int TiBackupLib::runScriptAsUser(const QString &scriptPath, const QString &username,
                                 int timeout, QString *output)
{
    // Resolve the target user in the PARENT: getpwnam_r/getgrouplist are not
    // async-signal-safe, so they must not run inside the child-process modifier.
    const QByteArray uname = username.toLocal8Bit();
    struct passwd pwd;
    struct passwd *res = nullptr;
    std::vector<char> pbuf(16384);
    if(getpwnam_r(uname.constData(), &pwd, pbuf.data(), pbuf.size(), &res) != 0 || res == nullptr)
    {
        qWarning() << "TiBackupLib::runScriptAsUser() -> user not found, refusing to run as root:" << username;
        return -1;   // fail closed: never fall back to root
    }
    const uid_t uid = pwd.pw_uid;
    const gid_t gid = pwd.pw_gid;
    const QString home = QString::fromLocal8Bit(pwd.pw_dir);

    // Supplementary groups (resolved in the parent, applied in the child).
    std::vector<gid_t> groups(32);
    int ngroups = static_cast<int>(groups.size());
    if(getgrouplist(uname.constData(), gid, groups.data(), &ngroups) < 0)
    {
        groups.resize(ngroups > 0 ? ngroups : 1);
        ngroups = static_cast<int>(groups.size());
        getgrouplist(uname.constData(), gid, groups.data(), &ngroups);
    }
    groups.resize(ngroups);

    // The dropped process must be able to read+exec the staged script, so hand it
    // ownership (the daemon is root here) and keep it owner-only (0700).
    if(::chown(scriptPath.toLocal8Bit().constData(), uid, gid) != 0)
        qWarning() << "TiBackupLib::runScriptAsUser() -> chown failed:" << strerror(errno);
    QFile::setPermissions(scriptPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    QProcess proc;
    if(output != nullptr)
        proc.setProcessChannelMode(QProcess::MergedChannels);

    // Minimal, predictable environment for the unprivileged script.
    QProcessEnvironment env;
    env.insert(QStringLiteral("HOME"), home.isEmpty() ? QStringLiteral("/tmp") : home);
    env.insert(QStringLiteral("USER"), username);
    env.insert(QStringLiteral("LOGNAME"), username);
    env.insert(QStringLiteral("PATH"),
               QStringLiteral("/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"));
    proc.setProcessEnvironment(env);

    // Drop privileges in the child after fork(), before exec(). Only async-signal-safe
    // syscalls here; the group vector was built in the parent above. On any failure
    // _exit() so we never exec the script with elevated privileges.
    proc.setChildProcessModifier([uid, gid, groups]() {
        if(setgroups(groups.size(), groups.data()) != 0) _exit(127);
        if(setgid(gid) != 0) _exit(127);
        if(setuid(uid) != 0) _exit(127);
    });

    proc.start(scriptPath, QStringList(), QIODevice::ReadOnly);
    proc.waitForStarted(timeout);
    proc.waitForFinished(timeout);

    if(output != nullptr)
        *output = QString::fromUtf8(proc.readAll());

    if(proc.error() != QProcess::UnknownError || proc.exitCode() != 0)
        qWarning() << "TiBackupLib::runScriptAsUser() ->" << scriptPath << "as" << username
                   << "uid" << uid << "gid" << gid
                   << "exitCode" << proc.exitCode() << "exitStatus" << proc.exitStatus()
                   << "procError" << proc.error() << proc.errorString();

    return proc.exitCode();
}

QString TiBackupLib::confineToScriptsDir(const QString &pathOrName)
{
    if(pathOrName.isEmpty())
        return QString();

    tiConfMain cfg;
    const QString scriptsDir = cfg.getValue("paths/scripts").toString();
    if(scriptsDir.isEmpty())
        return QString();

    // Bare names ("prebackup.sh") and relative paths resolve against the scripts dir.
    const QString lexBase = QDir::cleanPath(QDir(scriptsDir).absolutePath());
    QFileInfo fi(pathOrName);
    const QString absInput = fi.isAbsolute() ? QDir::cleanPath(pathOrName)
                                             : QDir::cleanPath(QDir(lexBase).filePath(pathOrName));

    // If the target exists, compare CANONICAL paths so a symlink inside the dir can't
    // point outside it (used at execution time). If it doesn't exist yet - e.g. saving
    // a job before its script is written - fall back to a lexical containment check.
    const QString canonTarget = QFileInfo(absInput).canonicalFilePath();
    if(!canonTarget.isEmpty())
    {
        const QString canonBase = QFileInfo(lexBase).canonicalFilePath();
        const QString base = canonBase.isEmpty() ? lexBase : canonBase;
        if(canonTarget == base || canonTarget.startsWith(base + QLatin1Char('/')))
            return canonTarget;
        return QString();
    }

    if(absInput == lexBase || absInput.startsWith(lexBase + QLatin1Char('/')))
        return absInput;
    return QString();
}

QString TiBackupLib::convertPath2Generic(const QString &path, const QString &mountdir)
{
    if(mountdir.isEmpty())
        return path;

    QString str = path;
    str.replace(mountdir, tibackup_config::var_partbackup_dir);
    return str;
}

QString TiBackupLib::convertGeneric2Path(const QString &path, const QString &mountdir)
{
    QString str = path;
    str.replace(tibackup_config::var_partbackup_dir, mountdir);
    return str;
}

DeviceDiskPartition TiBackupLib::getPartitionByUUID(const QString &uuid)
{
    TiBackupLib blib;
    QList<DeviceDisk> disks = blib.getAttachedDisks();
    DeviceDiskPartition retPart;

    for(auto &disk : disks)
    {
        if(disk.devtype == "disk")
        {
            disk.readPartitions();
            for(const auto &part : disk.partitions)
            {
                if(part.uuid.isEmpty())
                    continue;

                if(part.uuid == uuid)
                    return part;
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
