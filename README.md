# tiBackupLib

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg?style=flat-square)](LICENSE)
[![build-check](https://github.com/theinvisible/tiBackupLib/actions/workflows/build.yml/badge.svg)](https://github.com/theinvisible/tiBackupLib/actions/workflows/build.yml)
[![Hosted By: Cloudsmith](https://img.shields.io/badge/OSS%20hosting%20by-cloudsmith-blue?logo=cloudsmith&style=flat-square)](https://cloudsmith.com)

Core library of **tiBackup** — an intelligent, disk-based backup system for Linux
desktops and servers. Plug in a USB disk and a predefined backup job runs
automatically, or schedule jobs daily/weekly/monthly — with rsync, optional LUKS
encryption, pre/post scripts, e-mail notifications and Proxmox Backup Server
integration.

> **This repository** contains `tiBackupLib`, the shared C++/Qt6 library providing
> configuration handling, device/partition discovery and the backup engine used by
> the daemon. It is a build- and run-time dependency of the `tiBackup` daemon — see
> [Architecture](#architecture).

## Architecture

tiBackup consists of two parts:

| Component | Role |
|-----------|------|
| **[tiBackupLib](https://github.com/theinvisible/tiBackupLib)** | Shared core library (config, device/partition handling, backup engine). |
| **[tiBackup](https://github.com/theinvisible/tiBackup)** | Background daemon (`tibackupd`), runs as **root**, performs the actual backups and **serves the built-in web UI**. |

The daemon links this library and calls it directly in-process to serve a modern,
login-protected **web interface** (Qt `QHttpServer` + `QWebSockets`) — there is no
separate GUI process and no IPC layer. The former Qt Widgets client (`tiBackupUi`)
has been **replaced** by that built-in web UI.

## Features

- **Hotplug backups** — connect a disk and the matching job starts automatically
- **Scheduled backups** — daily, weekly or monthly
- **rsync-based** incremental file backups, with optional checksum comparison
- **LUKS encryption** support for backup targets
- **Proxmox Backup Server (PBS)** integration
- **E-mail notifications** when a job finishes (SMTP)
- **Pre/post-backup scripts** with dynamic tiBackup variables
- Simple **INI configuration** under `/etc/tibackup` — usable on headless servers

## Installation

Pre-built packages are published via Cloudsmith for **Debian 13 (trixie)**,
**Ubuntu 24.04 (noble)** and **Ubuntu 26.04**. The library is pulled in
automatically as a dependency of `tibackup`:

```bash
curl -1sLf 'https://dl.cloudsmith.io/public/ti-9x5p/tibackup/setup.deb.sh' | sudo -E bash
sudo apt install tibackup
```

To build other software against the library, install the development package:

```bash
sudo apt install tibackuplib-dev
```

## Building from source

Requirements (Debian/Ubuntu):

```bash
sudo apt install build-essential cmake qt6-base-dev \
    libpoco-dev libudev-dev libblkid-dev uuid-dev rsync cryptsetup
```

Build and install:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build      # installs the .so, headers and the CMake package
```

Consumers can then use it from CMake:

```cmake
find_package(tiBackupLib CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE tiBackupLib::tiBackupLib)
```

> **Tip:** when the two repositories are checked out next to each other, the
> `tiBackup` daemon build falls back to building `tiBackupLib` from this source tree
> automatically, so no install step is required for local development.

## License

Licensed under the **GNU General Public License v3.0 or later** (`GPL-3.0-or-later`).
See [LICENSE](LICENSE).

## Package hosting

Package repository hosting is graciously provided by [Cloudsmith](https://cloudsmith.com).
Cloudsmith is the only fully hosted, cloud-native, universal package management
solution that lets you manage your software supply chain with confidence.
