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
#include <QMutex>
#include <memory>
#include <optional>

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

    // True when the main config file exists after initMainConf(). Callers that
    // must have a usable config (the daemon at startup) check this; a false here
    // is fatal only where the caller decides so - the ctor no longer exit()s.
    bool isValid() const { return m_valid; }

    QVariant getValue(const QString &iniPath);
    void setValue(const QString &iniPath, const QVariant &val);
    void sync();

    QString getLogsDetailDir();

private:
    std::unique_ptr<QSettings> settings;
    bool m_valid = false;
};

class tiConfBackupJobs
{
public:
    tiConfBackupJobs();
    ~tiConfBackupJobs();

    void saveBackupJob(const tiBackupJob &job);
    // Kept public: backupManager's ctor warm-up calls it directly. The value
    // accessors below also (re)read internally, so an explicit call is optional.
    void readBackupJobs();

    // Value semantics: accessors return copies so a caller can safely hold the
    // result across long operations / other reads without dangling. (This class
    // is not a singleton - each user constructs its own - so it needs no mutex.)
    QList<tiBackupJob> getJobs();
    QList<tiBackupJob> getJobsByUuid(const QString &uuid);
    std::optional<tiBackupJob> getJobByName(const QString &jobname);

    bool removeJobByName(const QString &jobname);

    bool renameJob(const QString &oldname, const QString &newname);

private:
    std::unique_ptr<tiConfMain> main_settings;

    QList<tiBackupJob> jobs;
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

    // Value semantics + mutex: this is a process-wide singleton shared between
    // the main (web) thread and backup-worker threads. Accessors take m_mutex,
    // (re)read from disk, and return a COPY - so read+copy is atomic w.r.t. a
    // concurrent save/read and the caller can hold the result safely for hours.
    QList<PBServer> getItems();
    std::optional<PBServer> getItemByName(const QString &name);
    std::optional<PBServer> getItemByUuid(const QString &uuid);

    bool removeItemByName(const QString &name);
    bool removeItemByUuid(const QString &uuid);

    bool renameItem(const QString &oldname, const QString &newname);
    bool copyItem(const QString &origname, const QString &cpname);

private:
    tiConfPBServers();
    ~tiConfPBServers();

    // Assumes m_mutex is held by the caller.
    void readItems();

    std::unique_ptr<tiConfMain> main_settings;

    QList<PBServer> items;
    QMutex m_mutex;
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

    // Value semantics + mutex - see tiConfPBServers above for the rationale.
    QList<SSHServer> getItems();
    std::optional<SSHServer> getItemByName(const QString &name);
    std::optional<SSHServer> getItemByUuid(const QString &uuid);

    bool removeItemByName(const QString &name);
    bool removeItemByUuid(const QString &uuid);

    bool renameItem(const QString &oldname, const QString &newname);
    bool copyItem(const QString &origname, const QString &cpname);

private:
    tiConfSSHServers();
    ~tiConfSSHServers();

    // Assumes m_mutex is held by the caller.
    void readItems();

    std::unique_ptr<tiConfMain> main_settings;

    QList<SSHServer> items;
    QMutex m_mutex;
};

#endif // TICONF_H
