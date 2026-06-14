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
> configuration handling, the IPC protocol, device/partition discovery and the
> backup engine used by **both** the daemon and the GUI. It is a build- and
> run-time dependency of the other two components — see [Architecture](#architecture).

## Architecture

tiBackup consists of three parts:

| Component | Role |
|-----------|------|
| **[tiBackupLib](https://github.com/theinvisible/tiBackupLib)** | Shared core library (config, IPC, device handling, backup engine). |
| **[tiBackup](https://github.com/theinvisible/tiBackup)** | Background daemon (`tibackupd`), runs as **root**, performs the actual backups and exposes an IPC + HTTP status API. |
| **[tiBackupUi](https://github.com/theinvisible/tiBackupUi)** | Qt Widgets GUI to configure jobs and settings; runs **unprivileged** and talks to the daemon over IPC. |

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

Pre-built Debian/Ubuntu packages are published via Cloudsmith. The library is
pulled in automatically as a dependency of `tibackup`/`tibackupui`:

```bash
curl -1sLf 'https://dl.cloudsmith.io/public/theinvisible/tibackup/setup.deb.sh' | sudo -E bash
sudo apt install tibackup tibackupui
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

> **Tip:** when the three repositories are checked out next to each other, the
> daemon and GUI builds fall back to building `tiBackupLib` from this source tree
> automatically, so no install step is required for local development.

## License

Licensed under the **GNU General Public License v3.0 or later** (`GPL-3.0-or-later`).
See [LICENSE](LICENSE).

## Package hosting

Package repository hosting is graciously provided by [Cloudsmith](https://cloudsmith.com).
Cloudsmith is the only fully hosted, cloud-native, universal package management
solution that lets you manage your software supply chain with confidence.
