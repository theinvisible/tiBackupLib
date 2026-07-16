#ifndef SSHSERVER_H
#define SSHSERVER_H

#include <QString>
#include <QDataStream>

class SSHServer
{
public:
    SSHServer();

    QString uuid;

    QString name;
    QString host;
    uint port;
    QString username;

    // Auth is always public key. keyfile is optional: when empty the daemon's
    // own default SSH identity (root's ~/.ssh) is used. keypass is an optional
    // passphrase for an encrypted private key.
    QString keyfile;
    QString keypass;

    // Pinned host key, stored as a known_hosts line captured via ssh-keyscan on
    // "test connection" and verified (StrictHostKeyChecking=yes) on every run.
    QString hostkey;

    void genNewUuid();
};

QDataStream &operator<<(QDataStream &ds, const SSHServer &obj);
QDataStream &operator>>(QDataStream &ds, SSHServer &obj);

#endif // SSHSERVER_H
