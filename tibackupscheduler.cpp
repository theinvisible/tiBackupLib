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

#include "tibackupscheduler.h"

bool tiBackupScheduler::shouldRun(const tiBackupJob &job, const QDateTime &now,
                                  qint64 lastRunEpoch, int graceSecs)
{
    if(job.intervalType == tiBackupJobInterval::NONE)
        return false;

    const QTime slotTime = QTime::fromString(job.intervalTime, QStringLiteral("hh:mm"));
    if(!slotTime.isValid())
        return false;

    // Is there a slot on today's date for this job's interval type? (The weekday
    // mapping 0=Mon..6=Sun matches the historical DiskMain::onTaskCheck() code.)
    switch(job.intervalType)
    {
    case tiBackupJobInterval::DAILY:
        break;                                      // every day has a slot
    case tiBackupJobInterval::WEEKLY:
        if((now.date().dayOfWeek() - 1) != job.intervalDay)
            return false;
        break;
    case tiBackupJobInterval::MONTHLY:
        if(now.date().day() != job.intervalDay)
            return false;
        break;
    case tiBackupJobInterval::NONE:
        return false;
    }

    const QDateTime slot(now.date(), slotTime);     // local time, same as `now`
    const qint64 slotEpoch = slot.toSecsSinceEpoch();
    const qint64 nowEpoch  = now.toSecsSinceEpoch();

    // Strict slot: only inside [slot, slot+grace]. Earlier than the slot, or more
    // than `grace` past it (missed run), => don't fire.
    if(nowEpoch < slotEpoch || nowEpoch > slotEpoch + graceSecs)
        return false;

    // Fire at most once per slot: once last-run is at/after the slot, we're done.
    return lastRunEpoch < slotEpoch;
}
