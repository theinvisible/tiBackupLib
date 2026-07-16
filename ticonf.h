/*
 *
tiBackupLib - A intelligent desktop/standalone backup system library

Copyright (C) 2014 Rene Hadler, rene@hadler.me, https://hadler.me

    This file is part of tiBackup.

    tiBackupLib is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    tiBackupLib is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with tiBackupLib.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef TICONF_H
#define TICONF_H

#include <QSettings>
#include <memory>

#include "obj/tibackupjob.h"
#include "obj/pbserver.h"
#include "obj/sshserver.h"

class tiBackupJob;

class tiConfMain
{
public:
    tiConfMain();
    ~tiConfMain();

    void initMainConf();

    QVariant getValue(const QString &iniPath);
    void setValue(const QString &iniPath, const QVariant &val);
    void sync();

    QString getLogsDetailDir();

private:
    std::unique_ptr<QSettings> settings;
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
    tiBackupJob* getJobByName(const QString &jobname);

    bool removeJobByName(const QString &jobname);

    bool renameJob(const QString &oldname, const QString &newname);

private:
    std::unique_ptr<tiConfMain> main_settings;

    QList<tiBackupJob*> jobs;
};

class tiConfPBServers
{
public:
    static tiConfPBServers* instance()
    {
        static tiConfPBServers inst;
        return &inst;
    }

    void saveItem(const PBServer &item);
    void readItems();

    QList<PBServer*> getItems();
    PBServer* getItemByName(const QString &name);
    PBServer* getItemByUuid(const QString &uuid);

    bool removeItemByName(const QString &name);
    bool removeItemByUuid(const QString &uuid);

    bool renameItem(const QString &oldname, const QString &newname);
    bool copyItem(const QString &origname, const QString &cpname);

private:
    tiConfPBServers();
    ~tiConfPBServers();

    std::unique_ptr<tiConfMain> main_settings;

    QList<PBServer*> items;
};

class tiConfSSHServers
{
public:
    static tiConfSSHServers* instance()
    {
        static tiConfSSHServers inst;
        return &inst;
    }

    void saveItem(const SSHServer &item);
    void readItems();

    QList<SSHServer*> getItems();
    SSHServer* getItemByName(const QString &name);
    SSHServer* getItemByUuid(const QString &uuid);

    bool removeItemByName(const QString &name);
    bool removeItemByUuid(const QString &uuid);

    bool renameItem(const QString &oldname, const QString &newname);
    bool copyItem(const QString &origname, const QString &cpname);

private:
    tiConfSSHServers();
    ~tiConfSSHServers();

    std::unique_ptr<tiConfMain> main_settings;

    QList<SSHServer*> items;
};

#endif // TICONF_H
