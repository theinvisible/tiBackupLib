#include "backupmanager.h"

#include <QDebug>

backupManager::backupManager(QObject *parent) : QObject(parent)
{
    backups = new QHash<QString, backupStatus>();
    backupjobs = new tiConfBackupJobs();
    backupjobs->readBackupJobs();
}

backupManager::~backupManager()
{
    // Todo: Check if there are running backups and stop them
    delete backups;
    delete backupjobs;
}

bool backupManager::startBackup(const QString &name)
{
    backupjobs->readBackupJobs();
    tiBackupJob* job = backupjobs->getJobByName(name);
    if(job == 0) {
        qDebug() << "backupManager::startBackup() -> Backupjob " << name << " not found";
        return false;
    }

    if(!backups->contains(name) || (backups->contains(name) and backups->value(name) != backupStatus::running))
    {
        qDebug() << "backupManager::startBackup() -> Backupjob " << name << " is starting now";

        (*backups)[name] = backupStatus::running;
        job->startBackupThread(this);
        return true;
    }
    else
    {
        qDebug() << "backupManager::startBackup() -> Backupjob " << name << " is already running, not starting backup";
        return false;
    }

}

void backupManager::onBackupFinished(QString name)
{
    (*backups)[name] = backupStatus::finished;
}
