/*
 *
tiBackupUi - A intelligent desktop/standalone backup system user interface

Copyright (C) 2014 Rene Hadler, rene@hadler.me, https://hadler.me

    This file is part of tiBackup.

    tiBackupUi is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    tiBackupUi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with tiBackupUi.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "tibackupjobworker.h"

#include <QDebug>

#include "ticonf.h"

tiBackupJobWorker::tiBackupJobWorker(QObject *parent) :
    QObject(parent)
{
}

void tiBackupJobWorker::setJobName(const QString &name)
{
    jobname = name;
}

void tiBackupJobWorker::process()
{
    // Work is here
    if(jobname.isEmpty())
    {
        emit finished(false);
        return;
    }

    qDebug() << "tiBackupJobWorker::process(): " << jobname;

    tiConfBackupJobs jobss;
    std::optional<tiBackupJob> job = jobss.getJobByName(jobname);
    if(!job)
    {
        // The job may have been deleted between scheduling and this thread
        // starting; abort cleanly instead of dereferencing a null pointer.
        qWarning() << "tiBackupJobWorker::process() -> job" << jobname
                   << "no longer exists, aborting";
        emit finished(false);
        return;
    }

    const bool ok = job->startBackup();

    emit finished(ok);
}
