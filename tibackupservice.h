#ifndef TIBACKUPSERVICE_H
#define TIBACKUPSERVICE_H

#include <QObject>

#include "ticonf.h"

enum tiBackupServiceStatus {
    tiBackupServiceStatusStarted,
    tiBackupServiceStatusStopped,
    tiBackupServiceStatusUnknown,
    tiBackupServiceStatusFailed,
    tiBackupServiceStatusNotFound
};

class tiBackupService : public QObject
{
    Q_OBJECT
public:
    explicit tiBackupService(QObject *parent = 0);

    tiBackupServiceStatus start();
    tiBackupServiceStatus stop();
    tiBackupServiceStatus status();
    bool install(const QString &path);

signals:
    void serviceStarted();
    void serviceStopped();
    void serviceInstalled();

public slots:

private:
    tiConfMain *main_settings;
};

#endif // TIBACKUPSERVICE_H
