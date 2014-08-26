#include "ticonf.h"

#include <QDebug>
#include <QFile>
#include <QDirIterator>

#include "config.h"

tiConfMain::tiConfMain()
{
    settings = 0;

    if(!QFile(tibackup_config::file_main).exists())
    {
        qDebug() << QString("Main configuration file <").append(tibackup_config::file_main).append("> not found, please fix this...");
        exit(EXIT_FAILURE);
    }

    settings = new QSettings(tibackup_config::file_main, QSettings::IniFormat);
}

tiConfMain::~tiConfMain()
{
    if(settings != 0)
        delete settings;
}

QVariant tiConfMain::getValue(const QString &iniPath)
{
    return settings->value(iniPath);
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
    QSettings *f = new QSettings(filename, QSettings::IniFormat);

    f->beginGroup("job");
    f->setValue("name", job.name);
    f->setValue("device", job.device);
    f->setValue("partition_uuid", job.partition_uuid);
    f->endGroup();

    f->beginGroup("backup");

    f->setValue("delete_add_file_on_dest", job.delete_add_file_on_dest);

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
            qDebug() << "jobfile found:" << jobfilepath;

            QSettings *f = new QSettings(jobfilepath, QSettings::IniFormat);
            tiBackupJob *job = new tiBackupJob;

            f->beginGroup("job");
            job->name = f->value("name").toString();
            job->device = f->value("device").toString();
            job->partition_uuid = f->value("partition_uuid").toString();
            f->endGroup();

            f->beginGroup("backup");
            job->delete_add_file_on_dest = f->value("delete_add_file_on_dest").toBool();

            int size = f->beginReadArray("folders");
            for (int i = 0; i < size; ++i)
            {
                f->setArrayIndex(i);
                job->backupdirs.insertMulti(f->value("source").toString(), f->value("dest").toString());
            }
            f->endArray();

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
