#include "tibackupapi.h"

const QString tiBackupApi::API_CMD_START = "cmd_backup_start";
const QString tiBackupApi::API_CMD_STOP = "cmd_backup_stop";

const QString tiBackupApi::API_VAR_CMD = "var_cmd";
const QString tiBackupApi::API_VAR_BACKUPJOB = "var_backupjob";

tiBackupApi::tiBackupApi(QObject *parent) : QObject(parent)
{

}

