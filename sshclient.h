#ifndef SSHCLIENT_H
#define SSHCLIENT_H

#include <QString>
#include <QStringList>
#include <QList>

#include "obj/sshserver.h"

// Thin wrapper around the OpenSSH command-line tools (ssh, ssh-keyscan,
// ssh-keygen). Auth is always public key; host keys are pinned. All remote
// invocations go through TiBackupLib::runCommandwithReturnCode in argv form, so
// no local shell is involved; remote command strings are shell-quoted for the
// remote shell.
class sshClient
{
public:
    struct DirEntry {
        QString name;
        bool isDir = false;
        qint64 size = 0;    // not resolved via `ls -p` (kept for UI schema parity)
        qint64 mtime = 0;
    };

    struct TestResult {
        bool ok = false;
        QString fingerprint;   // human SHA256 fingerprint(s), for display
        QString hostkey;       // captured known_hosts line(s), to pin
        QString message;       // error text when !ok
    };

    struct ListResult {
        bool ok = false;
        QList<DirEntry> entries;
        QString message;       // error text when !ok
    };

    struct MkdirResult {
        bool ok = false;
        QString message;       // error text when !ok
    };

    // ssh argv (options only, no program/host) for direct argv-form ssh calls.
    static QStringList sshArgv(const SSHServer &srv, const QString &knownHostsPath);
    // Transport command string for rsync's -e / RSYNC_RSH (e.g. "ssh -p 22 -o ...").
    // knownHostsPath must be free of spaces (rsync -e splits on whitespace).
    static QString rshCommand(const SSHServer &srv, const QString &knownHostsPath);

    // Capture the remote host key (ssh-keyscan) + a display fingerprint (ssh-keygen).
    static bool scanHostKey(const QString &host, uint port, QString *hostkey, QString *fingerprint);

    // Test connectivity + public-key auth; also (re)captures the host key.
    static TestResult test(const SSHServer &srv);

    // List directories/files at a remote path (uses the server's pinned host key).
    static ListResult listDir(const SSHServer &srv, const QString &path);

    // Create a directory at an absolute remote path (uses the server's pinned host
    // key). `path` must already be the full remote path; it is shell-quoted before
    // being handed to the remote `mkdir`.
    static MkdirResult mkdir(const SSHServer &srv, const QString &path);

    // POSIX single-quote a string for safe use inside a remote shell command.
    static QString shellQuote(const QString &s);

private:
    static QStringList sshOptions(const SSHServer &srv, const QString &knownHostsPath);
};

#endif // SSHCLIENT_H
