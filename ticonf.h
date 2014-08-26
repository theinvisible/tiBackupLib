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

private:
    tiConfMain *main_settings;
};

#endif // TICONF_H
