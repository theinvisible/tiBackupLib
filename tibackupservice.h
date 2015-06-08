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

signals:
    void serviceStarted();
    void serviceStopped();

public slots:

private:
    tiConfMain *main_settings;
};

#endif // TIBACKUPSERVICE_H
