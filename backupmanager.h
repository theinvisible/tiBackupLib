#ifndef BACKUPMANAGER_H
#define BACKUPMANAGER_H

#include <QObject>
#include <QHash>

#include "ticonf.h"

class tiConfBackupJobs;

class backupManager : public QObject
{
    Q_OBJECT
public:
    explicit backupManager(QObject *parent = nullptr);

    enum backupStatus {
        running,
        failed,
        finished
    };

    bool startBackup(const QString &name);

public slots:
    void onBackupFinished(QString name);

signals:

private:
    QHash<QString, backupStatus> *backups;
    tiConfBackupJobs *backupjobs;

    ~backupManager();

};

#endif // BACKUPMANAGER_H
