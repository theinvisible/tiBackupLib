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

#ifndef TIBACKUPSCHEDULER_H
#define TIBACKUPSCHEDULER_H

#include "tibackuplib_global.h"

#include <QDateTime>

#include "obj/tibackupjob.h"

// Stateless helper deciding whether a scheduled job is due. The decision is a
// PURE function of (job, now, lastRunEpoch, grace) - no wall-clock or config
// access - so it is deterministically unit-testable. The daemon (DiskMain) reads
// the current time and the persisted last-run and calls shouldRun() every tick.
//
// Strict-slot semantics: a job fires only inside [slot, slot+grace] and only once
// per slot (last-run advances past the slot). A slot missed entirely because the
// daemon was down over it is NOT resurrected - it waits for the next occurrence.
class TIBACKUPLIBSHARED_EXPORT tiBackupScheduler
{
public:
    // Grace window (seconds) after a scheduled slot. Must exceed the daemon's
    // tick interval (60 s) so a continuously-running daemon never misses its own
    // slot; kept small so longer downtime skips the slot rather than catching up.
    static constexpr int defaultGraceSecs = 120;

    static bool shouldRun(const tiBackupJob &job, const QDateTime &now,
                          qint64 lastRunEpoch, int graceSecs = defaultGraceSecs);
};

#endif // TIBACKUPSCHEDULER_H
