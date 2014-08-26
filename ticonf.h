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

private:
    tiConfMain *main_settings;

    QList<tiBackupJob*> jobs;
};

#endif // TICONF_H
