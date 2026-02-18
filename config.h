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

#ifndef CONFIG_H
#define CONFIG_H

#include <QDataStream>
#include <QLatin1String>

namespace tibackup_config
{
    [[maybe_unused]] static constexpr QLatin1String version{"0.7.7"};
    [[maybe_unused]] static constexpr QLatin1String file_main{"/etc/tibackup/main.conf"};
    [[maybe_unused]] static constexpr QLatin1String mount_root{"/mnt"};
    [[maybe_unused]] static constexpr QLatin1String initd_default{"/etc/init.d/tibackup"};
    [[maybe_unused]] static constexpr QLatin1String api_sock_name{"tibackup"};
    [[maybe_unused]] static constexpr QLatin1String systemd_name{"tibackupd"};
    [[maybe_unused]] static constexpr QDataStream::Version ipc_version = QDataStream::Qt_5_7;
    [[maybe_unused]] static constexpr QLatin1String backup_detail_folder{"backup_detail"};

    [[maybe_unused]] static constexpr QLatin1String var_partbackup_dir{"%MNTBACKUPDIR%"};
}

#endif // CONFIG_H
