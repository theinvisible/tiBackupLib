#ifndef TIBACKUPMAILER_H
#define TIBACKUPMAILER_H

#include <QString>
#include <QStringList>

// Thin wrapper around the Poco SMTP send used for backup notifications and the
// web UI's "send test mail". Supports plain SMTP, STARTTLS and implicit SSL/TLS
// on a configurable port, and file attachments. Keeping both callers on this one
// path means a successful test mail really predicts a working notification.
class tiBackupMailer
{
public:
    enum class Security { None, StartTls, Ssl };

    struct Params {
        QString  server;               // SMTP host
        int      port = 25;            // SMTP port
        Security security = Security::None;
        bool     auth = false;         // requires AUTH LOGIN
        QString  username;
        QString  password;             // PLAINTEXT (caller decodes any stored base64)
        QString  from;                 // sender; empty -> mail_from_default
    };
    struct Result {
        bool ok = false;
        QString message;               // error text when !ok
    };

    // Map a config/API string ("none"|"starttls"|"ssl", case-insensitive) to a
    // Security value; anything unrecognised falls back to None.
    static Security securityFromString(const QString &s);
    static QString  securityToString(Security s);

    // Send a message. Any attachmentPaths that exist are attached (Poco base64-
    // encodes them into a multipart/mixed message).
    static Result send(const Params &p, const QString &recipient,
                       const QString &subject, const QString &body,
                       const QStringList &attachmentPaths = QStringList());
};

#endif // TIBACKUPMAILER_H
