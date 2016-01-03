#ifndef TIBACKUPAPI_H
#define TIBACKUPAPI_H

#include <QObject>
#include <QString>

class tiBackupApi : public QObject
{
    Q_OBJECT
public:
    explicit tiBackupApi(QObject *parent = 0);

    static const QString API_CMD_START;
    static const QString API_CMD_STOP;

    static const QString API_VAR_CMD;
    static const QString API_VAR_BACKUPJOB;

signals:

public slots:
};

#endif // TIBACKUPAPI_H
