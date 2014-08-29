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

#ifndef TIBACKUPLIB_GLOBAL_H
#define TIBACKUPLIB_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(TIBACKUPLIB_LIBRARY)
#  define TIBACKUPLIBSHARED_EXPORT Q_DECL_EXPORT
#else
#  define TIBACKUPLIBSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // TIBACKUPLIB_GLOBAL_H
