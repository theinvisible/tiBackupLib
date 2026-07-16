#include "backupmanager.h"

#include <QDebug>

backupManager::backupManager(QObject *parent) : QObject(parent)
{
    // Allow statusChanged() to cross thread boundaries (worker thread -> web layer).
    qRegisterMetaType<backupManager::backupStatus>("backupManager::backupStatus");

    backups = std::make_unique<QHash<QString, backupStatus>>();
    backupjobs = std::make_unique<tiConfBackupJobs>();
    backupjobs->readBackupJobs();
}

backupManager::~backupManager()
{
    // Todo: Check if there are running backups and stop them
}

bool backupManager::startBackup(const QString &name)
{
    // getJobByName re-reads internally and returns a value copy.
    std::optional<tiBackupJob> job = backupjobs->getJobByName(name);
    if(!job) {
        qWarning() << "backupManager::startBackup() -> Backupjob " << name << " not found";
        return false;
    }

    if(!backups->contains(name) || (backups->contains(name) && backups->value(name) != backupStatus::running))
    {
        qWarning() << "backupManager::startBackup() -> Backupjob " << name << " is starting now";

        (*backups)[name] = backupStatus::running;
        emit statusChanged(name, backupStatus::running);
        job->startBackupThread(this);
        return true;
    }
    else
    {
        qWarning() << "backupManager::startBackup() -> Backupjob " << name << " is already running, not starting backup";
        return false;
    }

}

QHash<QString, backupManager::backupStatus> *backupManager::getBackupStatus()
{
    return backups.get();
}

backupManager::backupStatus backupManager::getBackupStatus(const QString &name)
{
    return (backups->contains(name)) ? (*backups)[name] : backupStatus::standby;
}

void backupManager::onBackupFinished(const QString &name)
{
    (*backups)[name] = backupStatus::finished;
    emit statusChanged(name, backupStatus::finished);
}
