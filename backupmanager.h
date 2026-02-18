#ifndef BACKUPMANAGER_H
#define BACKUPMANAGER_H

#include <QObject>
#include <QHash>
#include <memory>

#include "ticonf.h"

class tiConfBackupJobs;

class backupManager : public QObject
{
    Q_OBJECT
public:
    explicit backupManager(QObject *parent = nullptr);

    enum class backupStatus {
        running,
        failed,
        finished,
        standby
    };

    bool startBackup(const QString &name);
    QHash<QString, backupStatus> *getBackupStatus();
    backupStatus getBackupStatus(const QString &name);

public slots:
    void onBackupFinished(const QString &name);

signals:

private:
    std::unique_ptr<QHash<QString, backupStatus>> backups;
    std::unique_ptr<tiConfBackupJobs> backupjobs;

    ~backupManager();

};

#endif // BACKUPMANAGER_H
