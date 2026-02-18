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

    if(!QFile(tibackup_config::file_main).exists())
    {
        qCritical() << QString("tiConfMain::tiConfMain() -> Main configuration file <").append(tibackup_config::file_main).append("> not found, please fix this...");
        exit(EXIT_FAILURE);
    }

    settings = std::make_unique<QSettings>(tibackup_config::file_main, QSettings::IniFormat);
}

tiConfMain::~tiConfMain() = default;

void tiConfMain::initMainConf()
{
    QFile conf_main(tibackup_config::file_main);
    QFileInfo finfo(tibackup_config::file_main);
    QDir conf_main_dir = finfo.absoluteDir();
    conf_main_dir.mkpath(conf_main_dir.absolutePath());

    if(!conf_main.exists())
    {
        QString backupjobs_dir = QString("%1/jobs").arg(conf_main_dir.absolutePath());
        QString pbservers_dir = QString("%1/pbservers").arg(conf_main_dir.absolutePath());
        QString logs_dir = QString("%1/logs").arg(conf_main_dir.absolutePath());
        QString scripts_dir = QString("%1/scripts").arg(conf_main_dir.absolutePath());

        QDir pbservers_dir_path(pbservers_dir);
        pbservers_dir_path.mkpath(pbservers_dir);
        QDir backupjobsdir_path(backupjobs_dir);
        backupjobsdir_path.mkpath(backupjobs_dir);
        QDir logsdir_path(logs_dir);
        logsdir_path.mkpath(logs_dir);
        QDir scriptsdir_path(scripts_dir);
        scriptsdir_path.mkpath(scripts_dir);

        QSettings conf(tibackup_config::file_main, QSettings::IniFormat);
        conf.setValue("main/debug", true);
        conf.setValue("paths/backupjobs", backupjobs_dir);
        conf.setValue("paths/pbservers", pbservers_dir);
        conf.setValue("paths/logs", logs_dir);
        conf.setValue("paths/scripts", scripts_dir);
        conf.setValue("paths/initd", tibackup_config::initd_default);
        conf.sync();
    }
    else
    {
        QSettings conf(tibackup_config::file_main, QSettings::IniFormat);
        if(!conf.contains("paths/pbservers"))
        {
            QString pbservers_dir = QString("%1/pbservers").arg(conf_main_dir.absolutePath());
            QDir pbservers_dir_path(pbservers_dir);
            pbservers_dir_path.mkpath(pbservers_dir);

            conf.setValue("paths/pbservers", pbservers_dir);
            conf.sync();
        }
    }

    QString logs_dir = QString("%1/logs/%2").arg(conf_main_dir.absolutePath(), tibackup_config::backup_detail_folder);
    QDir logsdir_detail_path(logs_dir);
    logsdir_detail_path.mkpath(logs_dir);
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
}

QString tiConfMain::getLogsDetailDir()
{
    QFileInfo finfo(tibackup_config::file_main);
    QDir conf_main_dir = finfo.absoluteDir();
    return QString("%1/logs/%2").arg(conf_main_dir.absolutePath(), tibackup_config::backup_detail_folder);
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
}

void tiConfBackupJobs::readBackupJobs()
{
    // TODO if job objects exist we must *delete* them first
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
            tiBackupJob *job = new tiBackupJob;

            f->beginGroup("job");
            job->name = f->value("name").toString();
            job->device = f->value("device").toString();
            job->partition_uuid = f->value("partition_uuid").toString();
            f->endGroup();

            f->beginGroup("backup");
            job->delete_add_file_on_dest = f->value("delete_add_file_on_dest").toBool();
            job->start_backup_on_hotplug = f->value("start_backup_on_hotplug").toBool();
            job->save_log = f->value("save_log").toBool();
            job->compare_via_checksum = f->value("compare_via_checksum").toBool();

            int size = f->beginReadArray("folders");
            for (int i = 0; i < size; ++i)
            {
                f->setArrayIndex(i);
                job->backupdirs.insert(f->value("source").toString(), f->value("dest").toString());
            }
            f->endArray();
            f->endGroup();

            f->beginGroup("notify");
            job->notify = f->value("enabled").toBool();
            job->notifyRecipients = f->value("recipients").toString();
            f->endGroup();

            f->beginGroup("scripts");
            job->scriptBeforeBackup = f->value("beforeBackup").toString();
            job->scriptAfterBackup = f->value("afterBackup").toString();
            f->endGroup();

            f->beginGroup("pbs");
            job->pbs = f->value("enabled").toBool();
            job->pbs_server_uuid = f->value("pbs_server_uuid").toString();
            job->pbs_server_storage = f->value("pbs_server_storage").toString();
            job->pbs_dest_folder = f->value("pbs_dest_folder").toString();
            int size2 = f->beginReadArray("pbs_ids");
            for (int i = 0; i < size2; ++i)
            {
                f->setArrayIndex(i);
                job->pbs_backup_ids.append(f->value("id").toString());
            }
            f->endArray();
            f->endGroup();

            f->beginGroup("task");
            job->intervalType = static_cast<tiBackupJobInterval>(f->value("type").toInt());
            job->intervalTime = f->value("time").toString();
            job->intervalDay = f->value("day").toInt();
            f->endGroup();

            f->beginGroup("encluks");
            job->encLUKSType = static_cast<tiBackupEncLUKS>(f->value("type").toInt());
            job->encLUKSFilePath = f->value("filepath").toString();
            f->endGroup();

            jobs.append(job);
        }
    }
}

QList<tiBackupJob *> tiConfBackupJobs::getJobs()
{
    return jobs;
}

QList<tiBackupJob *> tiConfBackupJobs::getJobsByUuid(const QString &uuid)
{
    readBackupJobs();
    QList<tiBackupJob*> retjobs;

    for(tiBackupJob *job : jobs)
    {
        if(job->partition_uuid == uuid)
            retjobs.append(job);
    }

    return retjobs;
}

tiBackupJob *tiConfBackupJobs::getJobByName(const QString &jobname)
{
    readBackupJobs();

    for(tiBackupJob *job : jobs)
    {
        if(job->name == jobname)
            return job;
    }

    return nullptr;
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
            PBServer *item = new PBServer;

            f->beginGroup("pbserver");
            item->uuid = f->value("uuid").toString();
            item->name = f->value("name").toString();
            item->host = f->value("host").toString();
            item->port = f->value("port").toUInt();
            item->username = f->value("username").toString();
            item->password = f->value("password").toString();
            item->fingerprint = f->value("fingerprint").toString();
            item->keyfile = f->value("keyfile").toString();
            item->keypass = f->value("keypass").toString();
            f->endGroup();

            items.append(item);
        }
    }
}

QList<PBServer *> tiConfPBServers::getItems()
{
    return items;
}

PBServer *tiConfPBServers::getItemByName(const QString &name)
{
    readItems();

    for(PBServer *item : items)
    {
        if(item->name == name)
            return item;
    }

    return nullptr;
}

PBServer *tiConfPBServers::getItemByUuid(const QString &uuid)
{
    readItems();

    for(PBServer *item : items)
    {
        if(item->uuid == uuid)
            return item;
    }

    return nullptr;
}

bool tiConfPBServers::removeItemByName(const QString &name)
{
    return QFile::remove(QString("%1/%2.conf").arg(main_settings->getValue("paths/pbservers").toString(), name));
}

bool tiConfPBServers::removeItemByUuid(const QString &uuid)
{
    return QFile::remove(QString("%1/%2.conf").arg(main_settings->getValue("paths/pbservers").toString(), uuid));
}

bool tiConfPBServers::renameItem(const QString &oldname, const QString &newname)
{
    return QFile::rename(QString("%1/%2.conf").arg(main_settings->getValue("paths/pbservers").toString(), oldname),
                         QString("%1/%2.conf").arg(main_settings->getValue("paths/pbservers").toString(), newname));
}

bool tiConfPBServers::copyItem(const QString &origname, const QString &cpname)
{
    PBServer *item = getItemByName(origname);
    PBServer newitem = *item;
    newitem.genNewUuid();
    newitem.name = cpname;
    saveItem(newitem);

    return true;
}
