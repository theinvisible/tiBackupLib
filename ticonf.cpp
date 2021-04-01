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

tiConfPBServers* tiConfPBServers::_instance = 0;

tiConfMain::tiConfMain()
{
    settings = 0;

    initMainConf();

    if(!QFile(tibackup_config::file_main).exists())
    {
        qCritical() << QString("tiConfMain::tiConfMain() -> Main configuration file <").append(tibackup_config::file_main).append("> not found, please fix this...");
        exit(EXIT_FAILURE);
    }

    settings = new QSettings(tibackup_config::file_main, QSettings::IniFormat);
}

tiConfMain::~tiConfMain()
{
    if(settings != 0)
        delete settings;
}

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


tiConfBackupJobs::tiConfBackupJobs()
{
    main_settings = new tiConfMain();
    QList<tiBackupJob*> jobs;
}

tiConfBackupJobs::~tiConfBackupJobs()
{
    delete main_settings;
}

void tiConfBackupJobs::saveBackupJob(const tiBackupJob &job)
{
    QString filename = QString(main_settings->getValue("paths/backupjobs").toString()).append("/%1.conf").arg(job.name);
    QDir jobsdir(main_settings->getValue("paths/backupjobs").toString());
    if(!jobsdir.exists())
        jobsdir.mkpath(main_settings->getValue("paths/backupjobs").toString());

    if(QFile::exists(filename))
        QFile::remove(filename);

    QSettings *f = new QSettings(filename, QSettings::IniFormat);

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
    QHashIterator<QString, QString> it(job.backupdirs);
    int i = 0;
    while(it.hasNext())
    {
        it.next();
        f->setArrayIndex(i);
        f->setValue("source", it.key());
        f->setValue("dest", it.value());

        i++;
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
    delete f;
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

            QSettings *f = new QSettings(jobfilepath, QSettings::IniFormat);
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
                job->backupdirs.insertMulti(f->value("source").toString(), f->value("dest").toString());
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
            delete f;
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
    tiBackupJob *job = 0;

    for(int i=0; i < jobs.count(); i++)
    {
        job = jobs.at(i);
        if(job->partition_uuid == uuid)
            retjobs.append(job);
    }

    return retjobs;
}

tiBackupJob *tiConfBackupJobs::getJobByName(const QString &jobname)
{
    readBackupJobs();
    tiBackupJob *job = 0;

    for(int i=0; i < jobs.count(); i++)
    {
        job = jobs.at(i);
        if(job->name == jobname)
            return job;
    }

    return job;
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
    main_settings = new tiConfMain();
    QList<PBServer*> items;
}

tiConfPBServers::~tiConfPBServers()
{
    delete main_settings;
}

void tiConfPBServers::saveItem(const PBServer &item)
{
    QString filename = QString(main_settings->getValue("paths/pbservers").toString()).append("/%1.conf").arg(item.uuid);
    QDir itemdir(main_settings->getValue("paths/pbservers").toString());
    if(!itemdir.exists())
        itemdir.mkpath(main_settings->getValue("paths/pbservers").toString());

    if(QFile::exists(filename))
        QFile::remove(filename);

    QSettings *f = new QSettings(filename, QSettings::IniFormat);

    f->beginGroup("pbserver");
    f->setValue("uuid", item.uuid);
    f->setValue("name", item.name);
    f->setValue("host", item.host);
    f->setValue("port", item.port);
    f->setValue("username", item.username);
    f->setValue("password", item.password);
    f->endGroup();

    f->sync();
    delete f;
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

            QSettings *f = new QSettings(filepath, QSettings::IniFormat);
            PBServer *item = new PBServer;

            f->beginGroup("pbserver");
            item->uuid = f->value("uuid").toString();
            item->name = f->value("name").toString();
            item->host = f->value("host").toString();
            item->port = f->value("port").toUInt();
            item->username = f->value("username").toString();
            item->password = f->value("password").toString();
            f->endGroup();

            items.append(item);
            delete f;
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
    PBServer *item = 0;

    for(int i=0; i < items.count(); i++)
    {
        item = items.at(i);
        if(item->name == name)
            return item;
    }

    return item;
}

PBServer *tiConfPBServers::getItemByUuid(const QString &uuid)
{
    readItems();
    PBServer *item = 0;

    for(int i=0; i < items.count(); i++)
    {
        item = items.at(i);
        if(item->uuid == uuid)
            return item;
    }

    return item;
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
