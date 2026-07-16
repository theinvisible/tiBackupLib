#ifndef PBSCLIENT_H
#define PBSCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonDocument>

#include "obj/HttpStatusCodes.h"

class QSslCertificate;

class pbsClient : public QObject
{
    Q_OBJECT
private:
    QString host;
    QString username;
    QString password;
    int port;

    QString ticket;
    QString CSRF;

    // TLS certificate pinning. The PBS REST endpoint uses a self-signed cert, so
    // instead of blindly ignoring SSL errors we verify the presented leaf-cert
    // fingerprint against the expected one. captureMode accepts an unverified
    // cert once (explicit admin "Test connection") and records its fingerprint so
    // the admin can pin it; lastPeerFingerprint holds the most recently seen fp.
    QString expectedFingerprint;
    QString lastPeerFingerprint;
    bool captureMode = false;

    QNetworkAccessManager *nam;

    explicit pbsClient(QObject *parent = nullptr);

    QNetworkRequest getNRAuth(const QString &url);

public:
    static pbsClient* instance()
    {
        static pbsClient* inst = new pbsClient(nullptr);
        return inst;
    }
    static pbsClient* instanceUnique()
    {
        return new pbsClient(nullptr);
    }
    struct HttpResponse {
        HttpStatus::Code status;
        QJsonDocument data;
    };
    struct HttpResponseRaw {
        HttpStatus::Code status;
        QByteArray data;
    };

    // Pin the expected leaf-cert fingerprint (SHA-256, colon-hex, any case) before
    // issuing requests. When empty, verification fails closed (secure default).
    void setExpectedFingerprint(const QString &fp);
    // Trust-on-first-use probe: accept an unverified cert once and record its
    // fingerprint (retrievable via capturedFingerprint()). For "Test connection".
    void setCaptureMode(bool on);
    QString capturedFingerprint() const;

    // Normalise a fingerprint to bare upper-case hex (strip ':' and other
    // separators); format a certificate's SHA-256 digest as upper-case colon-hex;
    // compare two fingerprints for equality. Public + static so they are unit-testable.
    static QString normalizeFingerprint(const QString &fp);
    static QString fingerprintOf(const QSslCertificate &cert);
    static bool fingerprintMatches(const QString &a, const QString &b);

    HttpStatus::Code auth(const QString &host, const QString &username, const QString &password);
    HttpStatus::Code auth(const QString &host, int port, const QString &username, const QString &password);
    QString genPBSAPIPath(const QString &path);
    QString genPBSAPIPath(const QString &path, const QUrlQuery &query);

    HttpResponse getDatastores();
    HttpResponse getDatastoreSnapshots(const QString &datastore, const QString &backupid = "", const QString &backuptype = "");
    HttpResponse getDatastoreGroups(const QString &datastore);
    HttpResponseRaw getBackupFile(const QString &datastore, const QString &backupid, int backuptime, const QString &backuptype, const QString &filename);
    HttpResponse getBackupFiles(const QString &datastore, const QString &backupid, int backuptime, const QString &backuptype);

signals:

};

#endif // PBSCLIENT_H
