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

#include "sshclient.h"

#include <QTemporaryFile>
#include <QTextStream>
#include <QDir>

#include "tibackuplib.h"

QString sshClient::shellQuote(const QString &s)
{
    QString r = s;
    r.replace("'", "'\\''");
    return QString("'%1'").arg(r);
}

QStringList sshClient::sshOptions(const SSHServer &srv, const QString &knownHostsPath)
{
    QStringList a;
    a << "-p" << QString::number(srv.port);
    a << "-o" << "BatchMode=yes";
    a << "-o" << "ConnectTimeout=10";
    // Bound a stalled/dead connection: the backup rsync runs with no wall-clock
    // timeout (it must tolerate arbitrarily slow but live links), so without
    // keepalives a frozen connection would hang the job forever. Abort after
    // ~ServerAliveInterval*ServerAliveCountMax seconds of silence instead.
    a << "-o" << "ServerAliveInterval=15";
    a << "-o" << "ServerAliveCountMax=4";
    a << "-o" << "StrictHostKeyChecking=yes";
    if(!knownHostsPath.isEmpty())
        a << "-o" << QString("UserKnownHostsFile=%1").arg(knownHostsPath);
    // Public key only: never fall back to password/keyboard-interactive.
    a << "-o" << "PreferredAuthentications=publickey";
    a << "-o" << "PasswordAuthentication=no";
    if(!srv.keyfile.isEmpty())
        a << "-i" << srv.keyfile;
    return a;
}

QStringList sshClient::sshArgv(const SSHServer &srv, const QString &knownHostsPath)
{
    return sshOptions(srv, knownHostsPath);
}

QString sshClient::rshCommand(const SSHServer &srv, const QString &knownHostsPath)
{
    return QString("ssh %1").arg(sshOptions(srv, knownHostsPath).join(" "));
}

bool sshClient::scanHostKey(const QString &host, uint port, QString *hostkey, QString *fingerprint)
{
    TiBackupLib lib;

    // Collect the remote host key(s). ssh-keyscan writes key lines to stdout and
    // progress/comment lines (prefixed with '#') to stderr; the runner merges the
    // two, so filter '#' and blank lines to keep only the known_hosts entries.
    QString scanOut;
    QStringList scanArgs;
    scanArgs << "-T" << "10" << "-p" << QString::number(port) << host;
    lib.runCommandwithReturnCode(QStringLiteral("ssh-keyscan"), scanArgs, 20000, &scanOut);

    QStringList keyLines;
    for(const QString &line : scanOut.split('\n'))
    {
        const QString t = line.trimmed();
        if(t.isEmpty() || t.startsWith('#'))
            continue;
        keyLines << t;
    }

    if(keyLines.isEmpty())
        return false;

    const QString keys = keyLines.join("\n") + "\n";
    if(hostkey)
        *hostkey = keys;

    // Derive human-readable fingerprints for display via ssh-keygen -lf.
    if(fingerprint)
    {
        QTemporaryFile kf(QDir::tempPath() + "/tibackup-hk-XXXXXX");
        if(kf.open())
        {
            kf.write(keys.toUtf8());
            kf.flush();
            QString fpOut;
            QStringList fpArgs;
            fpArgs << "-l" << "-f" << kf.fileName();
            lib.runCommandwithReturnCode(QStringLiteral("ssh-keygen"), fpArgs, 10000, &fpOut);
            *fingerprint = fpOut.trimmed();
        }
    }

    return true;
}

sshClient::TestResult sshClient::test(const SSHServer &srv)
{
    TestResult res;

    // Always (re)capture the host key on an explicit test so the admin can pin it.
    QString hostkey, fingerprint;
    if(!scanHostKey(srv.host, srv.port, &hostkey, &fingerprint))
    {
        res.ok = false;
        res.message = QStringLiteral("Host key scan failed (host unreachable or SSH not responding).");
        return res;
    }
    res.hostkey = hostkey;
    res.fingerprint = fingerprint;

    // Verify public-key auth against the freshly scanned key.
    QTemporaryFile kh(QDir::tempPath() + "/tibackup-kh-XXXXXX");
    if(!kh.open())
    {
        res.ok = false;
        res.message = QStringLiteral("Could not create temporary known_hosts file.");
        return res;
    }
    kh.write(hostkey.toUtf8());
    kh.flush();

    TiBackupLib lib;
    QStringList args = sshArgv(srv, kh.fileName());
    args << QString("%1@%2").arg(srv.username, srv.host);
    args << QStringLiteral("true");

    QString out;
    const int rc = lib.runCommandwithReturnCode(QStringLiteral("ssh"), args, 20000, &out);
    res.ok = (rc == 0);
    if(!res.ok)
        res.message = out.trimmed().isEmpty()
            ? QStringLiteral("SSH connection/auth failed.")
            : out.trimmed();
    return res;
}

sshClient::ListResult sshClient::listDir(const SSHServer &srv, const QString &path)
{
    ListResult res;

    if(srv.hostkey.isEmpty())
    {
        res.ok = false;
        res.message = QStringLiteral("No pinned host key; run 'Test connection' first.");
        return res;
    }

    QTemporaryFile kh(QDir::tempPath() + "/tibackup-kh-XXXXXX");
    if(!kh.open())
    {
        res.ok = false;
        res.message = QStringLiteral("Could not create temporary known_hosts file.");
        return res;
    }
    kh.write(srv.hostkey.toUtf8());
    kh.flush();

    const QString target = path.isEmpty() ? QStringLiteral("/") : path;

    TiBackupLib lib;
    QStringList args = sshArgv(srv, kh.fileName());
    args << QString("%1@%2").arg(srv.username, srv.host);
    // Single argv element => ssh forwards it verbatim to the remote shell. The
    // path is shell-quoted so spaces/metacharacters can't be re-split or injected.
    // `ls -1Ap` lists one entry per line with a trailing '/' on directories.
    args << QString("ls -1Ap -- %1").arg(shellQuote(target));

    QString out;
    const int rc = lib.runCommandwithReturnCode(QStringLiteral("ssh"), args, 20000, &out);
    if(rc != 0)
    {
        res.ok = false;
        res.message = out.trimmed().isEmpty()
            ? QStringLiteral("Remote listing failed.")
            : out.trimmed();
        return res;
    }

    for(const QString &line : out.split('\n'))
    {
        const QString name = line;
        if(name.isEmpty())
            continue;
        DirEntry e;
        if(name.endsWith('/'))
        {
            e.isDir = true;
            e.name = name.left(name.length() - 1);
        }
        else
        {
            e.isDir = false;
            e.name = name;
        }
        if(e.name.isEmpty())
            continue;
        res.entries << e;
    }

    res.ok = true;
    return res;
}
