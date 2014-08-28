#ifndef TICONF_H
#define TICONF_H

#include <QSettings>

#include "obj/tibackupjob.h"

class tiConfMain
{
public:
    tiConfMain();
    ~tiConfMain();

    QVariant getValue(const QString &iniPath);

private:
    QSettings *settings;
};

class tiConfBackupJobs
{
public:
    tiConfBackupJobs();
    ~tiConfBackupJobs();

    void saveBackupJob(const tiBackupJob &job);
    void readBackupJobs();

    QList<tiBackupJob*> getJobs();
    QList<tiBackupJob*> getJobsByUuid(const QString &uuid);
    tiBackupJob* gebJobByName(const QString &jobname);

    bool removeJob(const tiBackupJob &job);
    bool removeJobByName(const QString &jobname);

private:
    tiConfMain *main_settings;

    QList<tiBackupJob*> jobs;
};

#endif // TICONF_H
