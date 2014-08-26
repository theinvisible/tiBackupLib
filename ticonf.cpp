#include "ticonf.h"

#include <QDebug>
#include <QFile>

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
