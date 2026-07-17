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

#include "ticonf.h"

#include <QDebug>
#include <QFile>
#include <QDirIterator>

#include "config.h"

tiConfMain::tiConfMain()
{
    initMainConf();

    // A library must not kill the host process: instead of exit()ing when the
    // main config is missing/unwritable, record it via isValid() and let callers
    // decide. The daemon does the fatal check once at startup (main.cpp); request
    // handlers keep working against QSettings defaults rather than tearing down
    // the whole process from deep inside a request.
    m_valid = QFile(tibackup_config::mainConfigFile()).exists();
    if(!m_valid)
        qCritical() << QString("tiConfMain::tiConfMain() -> Main configuration file <").append(tibackup_config::mainConfigFile()).append("> not found, please fix this...");

    settings = std::make_unique<QSettings>(tibackup_config::mainConfigFile(), QSettings::IniFormat);
}

tiConfMain::~tiConfMain() = default;

void tiConfMain::initMainConf()
{
    QFile conf_main(tibackup_config::mainConfigFile());
    QFileInfo finfo(tibackup_config::mainConfigFile());
    QDir conf_main_dir = finfo.absoluteDir();
    conf_main_dir.mkpath(conf_main_dir.absolutePath());

    if(!conf_main.exists())
    {
        QString backupjobs_dir = QString("%1/jobs").arg(conf_main_dir.absolutePath());
        QString pbservers_dir = QString("%1/pbservers").arg(conf_main_dir.absolutePath());
        QString sshservers_dir = QString("%1/sshservers").arg(conf_main_dir.absolutePath());
        // Logs live under /var/log/tibackup (FHS), not in the /etc config tree.
        QString logs_dir = tibackup_config::logsDir();
        QString scripts_dir = QString("%1/scripts").arg(conf_main_dir.absolutePath());

        QDir pbservers_dir_path(pbservers_dir);
        pbservers_dir_path.mkpath(pbservers_dir);
        QDir sshservers_dir_path(sshservers_dir);
        sshservers_dir_path.mkpath(sshservers_dir);
        QDir backupjobsdir_path(backupjobs_dir);
        backupjobsdir_path.mkpath(backupjobs_dir);
        QDir logsdir_path(logs_dir);
        logsdir_path.mkpath(logs_dir);
        QDir scriptsdir_path(scripts_dir);
        scriptsdir_path.mkpath(scripts_dir);

        QSettings conf(tibackup_config::mainConfigFile(), QSettings::IniFormat);
        conf.setValue("main/debug", false);
        conf.setValue("paths/backupjobs", backupjobs_dir);
        conf.setValue("paths/pbservers", pbservers_dir);
        conf.setValue("paths/sshservers", sshservers_dir);
        conf.setValue("paths/logs", logs_dir);
        conf.setValue("paths/scripts", scripts_dir);
        conf.setValue("paths/initd", tibackup_config::initd_default);
        // Non-root user that pre/post-backup scripts run as (root-only setting; never
        // web-settable). See tibackup_config::script_run_user.
        conf.setValue("scripts/run_user", tibackup_config::script_run_user);
        // Notification-mail sender; user-overridable (root or web Settings).
        conf.setValue("smtp/from", tibackup_config::mail_from_default);
        // Fresh installs listen on all interfaces (Proxmox-style): the package
        // ships a self-signed cert under <config-dir>/pki, so the web UI comes up
        // over HTTPS and is reachable on the LAN out of the box. Set only here (on
        // first init) so upgrades of existing installs keep their current bind.
        conf.setValue("web/bind", "0.0.0.0");
        conf.sync();
    }
    else
    {
        QSettings conf(tibackup_config::mainConfigFile(), QSettings::IniFormat);
        if(!conf.contains("paths/pbservers"))
        {
            QString pbservers_dir = QString("%1/pbservers").arg(conf_main_dir.absolutePath());
            QDir pbservers_dir_path(pbservers_dir);
            pbservers_dir_path.mkpath(pbservers_dir);

            conf.setValue("paths/pbservers", pbservers_dir);
            conf.sync();
        }
        if(!conf.contains("paths/sshservers"))
        {
            QString sshservers_dir = QString("%1/sshservers").arg(conf_main_dir.absolutePath());
            QDir sshservers_dir_path(sshservers_dir);
            sshservers_dir_path.mkpath(sshservers_dir);

            conf.setValue("paths/sshservers", sshservers_dir);
            conf.sync();
        }
        if(!conf.contains("scripts/run_user"))
        {
            // Seed the script run-user on upgrade so existing installs also drop
            // privileges for pre/post scripts instead of running them as root.
            conf.setValue("scripts/run_user", tibackup_config::script_run_user);
            conf.sync();
        }
        if(!conf.contains("smtp/from"))
        {
            // Seed the mail sender on upgrade to the historical hardcoded value so
            // notification mails keep the same From after upgrade.
            conf.setValue("smtp/from", tibackup_config::mail_from_default);
            conf.sync();
        }
        // Migrate logs off /etc: older installs stored paths/logs as the in-tree
        // <configdir>/logs (i.e. /etc/tibackup/logs). Move the pointer to the FHS
        // location; a value the admin customised to something else is left alone.
        // The package postinst moves any existing log files to the new dir.
        const QString oldDefaultLogs = QString("%1/logs").arg(conf_main_dir.absolutePath());
        if(conf.value("paths/logs").toString() == oldDefaultLogs &&
           oldDefaultLogs != tibackup_config::logsDir())
        {
            conf.setValue("paths/logs", tibackup_config::logsDir());
            conf.sync();
        }
    }

    // Ensure the effective (possibly migrated / custom) logs dir and its
    // backup_detail subdir exist. Read paths/logs back here since `settings` is
    // not constructed yet at this point in the ctor; fall back to the default.
    QSettings confLogs(tibackup_config::mainConfigFile(), QSettings::IniFormat);
    const QString logs_base = confLogs.value("paths/logs", tibackup_config::logsDir()).toString();
    QDir().mkpath(logs_base);
    QDir().mkpath(QString("%1/%2").arg(logs_base, tibackup_config::backup_detail_folder));
    // Detail/rsync logs can contain script bodies and source paths, so keep the
    // log dir root-only (files themselves are created 0600 under the daemon umask).
    QFile::setPermissions(logs_base,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    // The config tree stores the web admin password hash plus the PBS/SMTP
    // credentials, so confine it to root (0700 dir / 0600 file). Done on every
    // construction so it also tightens pre-existing installs that QSettings had
    // created world-readable under the daemon's default umask.
    QFile::setPermissions(conf_main_dir.absolutePath(),
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    if(QFile::exists(tibackup_config::mainConfigFile()))
        QFile::setPermissions(tibackup_config::mainConfigFile(),
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QVariant tiConfMain::getValue(const QString &iniPath)
{
    return settings->value(iniPath);
}

void tiConfMain::setValue(const QString &iniPath, const QVariant &val)
{
    settings->setValue(iniPath, val);
}

void tiConfMain::sync()
{
    settings->sync();
    // Re-assert 0600 after each write: the file carries the web password hash and
    // the (base64) SMTP password and must never be world-readable.
    QFile::setPermissions(tibackup_config::mainConfigFile(),
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QString tiConfMain::getLogsDetailDir()
{
    // Derived from the configured paths/logs so the detail logs follow the log
    // location (default /var/log/tibackup) instead of being pinned under /etc.
    const QString logs_base = settings->value("paths/logs", tibackup_config::logsDir()).toString();
    return QString("%1/%2").arg(logs_base, tibackup_config::backup_detail_folder);
}


tiConfBackupJobs::tiConfBackupJobs()
{
    main_settings = std::make_unique<tiConfMain>();
}

tiConfBackupJobs::~tiConfBackupJobs() = default;

void tiConfBackupJobs::saveBackupJob(const tiBackupJob &job)
{
    QString filename = QString("%1/%2.conf").arg(main_settings->getValue("paths/backupjobs").toString(), job.name);
    QDir jobsdir(main_settings->getValue("paths/backupjobs").toString());
    if(!jobsdir.exists())
        jobsdir.mkpath(main_settings->getValue("paths/backupjobs").toString());

    if(QFile::exists(filename))
        QFile::remove(filename);

    auto f = std::make_unique<QSettings>(filename, QSettings::IniFormat);

    f->beginGroup("job");
    f->setValue("name", job.name);
    f->setValue("device", job.device);
    f->setValue("partition_uuid", job.partition_uuid);
    f->endGroup();

    f->beginGroup("backup");

    f->setValue("delete_add_file_on_dest", job.delete_add_file_on_dest);
    f->setValue("start_backup_on_hotplug", job.start_backup_on_hotplug);
    f->setValue("save_log", job.save_log);
    f->setValue("compare_via_checksum", job.compare_via_checksum);
    f->setValue("umount_after_backup", job.umount_after_backup);

    f->beginWriteArray("folders");
    int i = 0;
    for(const auto &[source, dest] : job.backupdirs.asKeyValueRange())
    {
        f->setArrayIndex(i++);
        f->setValue("source", source);
        f->setValue("dest", dest);
    }
    f->endArray();
    f->endGroup();

    f->beginGroup("notify");
    f->setValue("enabled", job.notify);
    f->setValue("recipients", job.notifyRecipients);
    f->endGroup();

    f->beginGroup("scripts");
    f->setValue("beforeBackup", job.scriptBeforeBackup);
    f->setValue("afterBackup", job.scriptAfterBackup);
    f->endGroup();

    f->beginGroup("pbs");
    f->setValue("enabled", job.pbs);
    f->setValue("pbs_server_uuid", job.pbs_server_uuid);
    f->setValue("pbs_server_storage", job.pbs_server_storage);
    f->setValue("pbs_dest_folder", job.pbs_dest_folder);
    f->beginWriteArray("pbs_ids");
    for(int j = 0; j < job.pbs_backup_ids.size(); ++j)
    {
        f->setArrayIndex(j);
        f->setValue("id", job.pbs_backup_ids[j]);
    }
    f->endArray();
    f->endGroup();

    // SSH sources: flattened to one (server_uuid, source, dest) triple per array
    // entry (QSettings arrays don't nest cleanly); grouped back per server on read.
    f->beginGroup("ssh");
    f->setValue("enabled", job.ssh);
    f->beginWriteArray("entries");
    int sidx = 0;
    for(const auto &t : job.ssh_targets)
    {
        for(const auto &[source, dest] : t.backupdirs.asKeyValueRange())
        {
            f->setArrayIndex(sidx++);
            f->setValue("server_uuid", t.server_uuid);
            f->setValue("source", source);
            f->setValue("dest", dest);
        }
    }
    f->endArray();
    f->endGroup();

    f->beginGroup("task");
    f->setValue("type", static_cast<int>(job.intervalType));
    f->setValue("time", job.intervalTime);
    f->setValue("day", job.intervalDay);
    f->endGroup();

    f->beginGroup("encluks");
    f->setValue("type", static_cast<int>(job.encLUKSType));
    f->setValue("filepath", job.encLUKSFilePath);
    f->endGroup();

    f->sync();

    // May reference a LUKS keyfile path; keep job definitions root-only too.
    QFile::setPermissions(filename, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

void tiConfBackupJobs::readBackupJobs()
{
    // jobs holds values now, so clear() destroys the previous entries (no leak).
    jobs.clear();

    QString jobsdir = main_settings->getValue("paths/backupjobs").toString();
    QDirIterator it_jobdir(jobsdir);
    QString jobfilepath;
    while (it_jobdir.hasNext())
    {
        jobfilepath = it_jobdir.next();
        if(jobfilepath.endsWith(".conf"))
        {
            qDebug() << "tiConfBackupJobs::readBackupJobs() -> jobfile found:" << jobfilepath;

            auto f = std::make_unique<QSettings>(jobfilepath, QSettings::IniFormat);
            tiBackupJob job;

            f->beginGroup("job");
            job.name = f->value("name").toString();
            job.device = f->value("device").toString();
            job.partition_uuid = f->value("partition_uuid").toString();
            f->endGroup();

            f->beginGroup("backup");
            job.delete_add_file_on_dest = f->value("delete_add_file_on_dest").toBool();
            job.start_backup_on_hotplug = f->value("start_backup_on_hotplug").toBool();
            job.save_log = f->value("save_log").toBool();
            job.compare_via_checksum = f->value("compare_via_checksum").toBool();
            // Default true so jobs saved before this key existed keep unmounting.
            job.umount_after_backup = f->value("umount_after_backup", true).toBool();

            int size = f->beginReadArray("folders");
            for (int i = 0; i < size; ++i)
            {
                f->setArrayIndex(i);
                job.backupdirs.insert(f->value("source").toString(), f->value("dest").toString());
            }
            f->endArray();
            f->endGroup();

            f->beginGroup("notify");
            job.notify = f->value("enabled").toBool();
            job.notifyRecipients = f->value("recipients").toString();
            f->endGroup();

            f->beginGroup("scripts");
            job.scriptBeforeBackup = f->value("beforeBackup").toString();
            job.scriptAfterBackup = f->value("afterBackup").toString();
            f->endGroup();

            f->beginGroup("pbs");
            job.pbs = f->value("enabled").toBool();
            job.pbs_server_uuid = f->value("pbs_server_uuid").toString();
            job.pbs_server_storage = f->value("pbs_server_storage").toString();
            job.pbs_dest_folder = f->value("pbs_dest_folder").toString();
            int size2 = f->beginReadArray("pbs_ids");
            for (int i = 0; i < size2; ++i)
            {
                f->setArrayIndex(i);
                job.pbs_backup_ids.append(f->value("id").toString());
            }
            f->endArray();
            f->endGroup();

            f->beginGroup("ssh");
            job.ssh = f->value("enabled").toBool();
            int size3 = f->beginReadArray("entries");
            QHash<QString, int> sshTargetIdx;
            for (int i = 0; i < size3; ++i)
            {
                f->setArrayIndex(i);
                QString suuid = f->value("server_uuid").toString();
                int idx;
                if(sshTargetIdx.contains(suuid))
                {
                    idx = sshTargetIdx.value(suuid);
                }
                else
                {
                    tiBackupJobSSHTarget t;
                    t.server_uuid = suuid;
                    job.ssh_targets.append(t);
                    idx = job.ssh_targets.size() - 1;
                    sshTargetIdx.insert(suuid, idx);
                }
                job.ssh_targets[idx].backupdirs.insert(f->value("source").toString(),
                                                         f->value("dest").toString());
            }
            f->endArray();
            f->endGroup();

            f->beginGroup("task");
            job.intervalType = static_cast<tiBackupJobInterval>(f->value("type").toInt());
            job.intervalTime = f->value("time").toString();
            job.intervalDay = f->value("day").toInt();
            f->endGroup();

            f->beginGroup("encluks");
            job.encLUKSType = static_cast<tiBackupEncLUKS>(f->value("type").toInt());
            job.encLUKSFilePath = f->value("filepath").toString();
            f->endGroup();

            jobs.append(job);
        }
    }
}

QList<tiBackupJob> tiConfBackupJobs::getJobs()
{
    readBackupJobs();
    return jobs;
}

QList<tiBackupJob> tiConfBackupJobs::getJobsByUuid(const QString &uuid)
{
    readBackupJobs();
    QList<tiBackupJob> retjobs;

    for(const tiBackupJob &job : jobs)
    {
        if(job.partition_uuid == uuid)
            retjobs.append(job);
    }

    return retjobs;
}

std::optional<tiBackupJob> tiConfBackupJobs::getJobByName(const QString &jobname)
{
    readBackupJobs();

    for(const tiBackupJob &job : jobs)
    {
        if(job.name == jobname)
            return job;
    }

    return std::nullopt;
}

bool tiConfBackupJobs::removeJobByName(const QString &jobname)
{
    return QFile::remove(QString("%1/%2.conf").arg(main_settings->getValue("paths/backupjobs").toString(), jobname));
}

bool tiConfBackupJobs::renameJob(const QString &oldname, const QString &newname)
{
    return QFile::rename(QString("%1/%2.conf").arg(main_settings->getValue("paths/backupjobs").toString(), oldname),
                         QString("%1/%2.conf").arg(main_settings->getValue("paths/backupjobs").toString(), newname));
}

tiConfPBServers::tiConfPBServers()
{
    main_settings = std::make_unique<tiConfMain>();
}

tiConfPBServers::~tiConfPBServers() = default;

void tiConfPBServers::saveItem(const PBServer &item)
{
    QMutexLocker lock(&m_mutex);
    QString filename = QString("%1/%2.conf").arg(main_settings->getValue("paths/pbservers").toString(), item.uuid);
    QDir itemdir(main_settings->getValue("paths/pbservers").toString());
    if(!itemdir.exists())
        itemdir.mkpath(main_settings->getValue("paths/pbservers").toString());

    if(QFile::exists(filename))
        QFile::remove(filename);

    auto f = std::make_unique<QSettings>(filename, QSettings::IniFormat);

    f->beginGroup("pbserver");
    f->setValue("uuid", item.uuid);
    f->setValue("name", item.name);
    f->setValue("host", item.host);
    f->setValue("port", item.port);
    f->setValue("username", item.username);
    f->setValue("password", item.password);
    f->setValue("fingerprint", item.fingerprint);
    f->setValue("keyfile", item.keyfile);
    f->setValue("keypass", item.keypass);
    f->endGroup();

    f->sync();

    // Stores the PBS password and key passphrase in clear text — root only.
    QFile::setPermissions(filename, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

void tiConfPBServers::readItems()
{
    items.clear();

    QString dir = main_settings->getValue("paths/pbservers").toString();
    QDirIterator it_dir(dir);
    QString filepath;
    while (it_dir.hasNext())
    {
        filepath = it_dir.next();
        if(filepath.endsWith(".conf"))
        {
            qDebug() << "tiConfPBServers::readItems() -> item found:" << filepath;

            auto f = std::make_unique<QSettings>(filepath, QSettings::IniFormat);
            PBServer item;

            f->beginGroup("pbserver");
            item.uuid = f->value("uuid").toString();
            item.name = f->value("name").toString();
            item.host = f->value("host").toString();
            item.port = f->value("port").toUInt();
            item.username = f->value("username").toString();
            item.password = f->value("password").toString();
            item.fingerprint = f->value("fingerprint").toString();
            item.keyfile = f->value("keyfile").toString();
            item.keypass = f->value("keypass").toString();
            f->endGroup();

            items.append(item);
        }
    }
}

QList<PBServer> tiConfPBServers::getItems()
{
    QMutexLocker lock(&m_mutex);
    readItems();
    return items;
}

std::optional<PBServer> tiConfPBServers::getItemByName(const QString &name)
{
    QMutexLocker lock(&m_mutex);
    readItems();

    for(const PBServer &item : items)
    {
        if(item.name == name)
            return item;
    }

    return std::nullopt;
}

std::optional<PBServer> tiConfPBServers::getItemByUuid(const QString &uuid)
{
    QMutexLocker lock(&m_mutex);
    readItems();

    for(const PBServer &item : items)
    {
        if(item.uuid == uuid)
            return item;
    }

    return std::nullopt;
}

bool tiConfPBServers::removeItemByName(const QString &name)
{
    QMutexLocker lock(&m_mutex);
    return QFile::remove(QString("%1/%2.conf").arg(main_settings->getValue("paths/pbservers").toString(), name));
}

bool tiConfPBServers::removeItemByUuid(const QString &uuid)
{
    QMutexLocker lock(&m_mutex);
    return QFile::remove(QString("%1/%2.conf").arg(main_settings->getValue("paths/pbservers").toString(), uuid));
}

bool tiConfPBServers::renameItem(const QString &oldname, const QString &newname)
{
    QMutexLocker lock(&m_mutex);
    return QFile::rename(QString("%1/%2.conf").arg(main_settings->getValue("paths/pbservers").toString(), oldname),
                         QString("%1/%2.conf").arg(main_settings->getValue("paths/pbservers").toString(), newname));
}

bool tiConfPBServers::copyItem(const QString &origname, const QString &cpname)
{
    // No lock here: composes getItemByName() + saveItem(), each of which locks.
    std::optional<PBServer> item = getItemByName(origname);
    if(!item)
        return false;
    PBServer newitem = *item;
    newitem.genNewUuid();
    newitem.name = cpname;
    saveItem(newitem);

    return true;
}

tiConfSSHServers::tiConfSSHServers()
{
    main_settings = std::make_unique<tiConfMain>();
}

tiConfSSHServers::~tiConfSSHServers() = default;

void tiConfSSHServers::saveItem(const SSHServer &item)
{
    QMutexLocker lock(&m_mutex);
    QString filename = QString("%1/%2.conf").arg(main_settings->getValue("paths/sshservers").toString(), item.uuid);
    QDir itemdir(main_settings->getValue("paths/sshservers").toString());
    if(!itemdir.exists())
        itemdir.mkpath(main_settings->getValue("paths/sshservers").toString());

    if(QFile::exists(filename))
        QFile::remove(filename);

    auto f = std::make_unique<QSettings>(filename, QSettings::IniFormat);

    f->beginGroup("sshserver");
    f->setValue("uuid", item.uuid);
    f->setValue("name", item.name);
    f->setValue("host", item.host);
    f->setValue("port", item.port);
    f->setValue("username", item.username);
    f->setValue("keyfile", item.keyfile);
    f->setValue("keypass", item.keypass);
    f->setValue("hostkey", item.hostkey);
    f->endGroup();

    f->sync();

    // Stores the private-key passphrase in clear text — root only.
    QFile::setPermissions(filename, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

void tiConfSSHServers::readItems()
{
    items.clear();

    QString dir = main_settings->getValue("paths/sshservers").toString();
    QDirIterator it_dir(dir);
    QString filepath;
    while (it_dir.hasNext())
    {
        filepath = it_dir.next();
        if(filepath.endsWith(".conf"))
        {
            qDebug() << "tiConfSSHServers::readItems() -> item found:" << filepath;

            auto f = std::make_unique<QSettings>(filepath, QSettings::IniFormat);
            SSHServer item;

            f->beginGroup("sshserver");
            item.uuid = f->value("uuid").toString();
            item.name = f->value("name").toString();
            item.host = f->value("host").toString();
            item.port = f->value("port").toUInt();
            item.username = f->value("username").toString();
            item.keyfile = f->value("keyfile").toString();
            item.keypass = f->value("keypass").toString();
            item.hostkey = f->value("hostkey").toString();
            f->endGroup();

            items.append(item);
        }
    }
}

QList<SSHServer> tiConfSSHServers::getItems()
{
    QMutexLocker lock(&m_mutex);
    readItems();
    return items;
}

std::optional<SSHServer> tiConfSSHServers::getItemByName(const QString &name)
{
    QMutexLocker lock(&m_mutex);
    readItems();

    for(const SSHServer &item : items)
    {
        if(item.name == name)
            return item;
    }

    return std::nullopt;
}

std::optional<SSHServer> tiConfSSHServers::getItemByUuid(const QString &uuid)
{
    QMutexLocker lock(&m_mutex);
    readItems();

    for(const SSHServer &item : items)
    {
        if(item.uuid == uuid)
            return item;
    }

    return std::nullopt;
}

bool tiConfSSHServers::removeItemByName(const QString &name)
{
    QMutexLocker lock(&m_mutex);
    return QFile::remove(QString("%1/%2.conf").arg(main_settings->getValue("paths/sshservers").toString(), name));
}

bool tiConfSSHServers::removeItemByUuid(const QString &uuid)
{
    QMutexLocker lock(&m_mutex);
    return QFile::remove(QString("%1/%2.conf").arg(main_settings->getValue("paths/sshservers").toString(), uuid));
}

bool tiConfSSHServers::renameItem(const QString &oldname, const QString &newname)
{
    QMutexLocker lock(&m_mutex);
    return QFile::rename(QString("%1/%2.conf").arg(main_settings->getValue("paths/sshservers").toString(), oldname),
                         QString("%1/%2.conf").arg(main_settings->getValue("paths/sshservers").toString(), newname));
}

bool tiConfSSHServers::copyItem(const QString &origname, const QString &cpname)
{
    // No lock here: composes getItemByName() + saveItem(), each of which locks.
    std::optional<SSHServer> item = getItemByName(origname);
    if(!item)
        return false;
    SSHServer newitem = *item;
    newitem.genNewUuid();
    newitem.name = cpname;
    saveItem(newitem);

    return true;
}
