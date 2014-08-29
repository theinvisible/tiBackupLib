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

#ifndef CONFIH_H
#define CONFIH_H

namespace tibackup_config
{
    static const char __attribute__ ((unused)) *version = "dev";
    static const char __attribute__ ((unused)) *file_main = "/usr/local/etc/tibackup/main.conf";
    static const char __attribute__ ((unused)) *mount_root = "/mnt";

    static const char __attribute__ ((unused)) *var_partbackup_dir = "%MNTBACKUPDIR%";
}

#endif // CONFIH_H
