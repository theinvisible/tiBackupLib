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
