#ifndef TIBACKUPMAILER_H
#define TIBACKUPMAILER_H

#include <QString>

// Thin wrapper around the Poco SMTP send that tiBackupJob uses for backup
// notifications, so the daemon can send a "test mail" over the exact same
// transport (plain SMTP on the default port, optional AUTH_LOGIN). Keeping the
// two in sync means a successful test really predicts a working notification.
class tiBackupMailer
{
public:
    struct Params {
        QString server;              // SMTP host
        bool    auth = false;        // requires AUTH_LOGIN
        QString username;
        QString password;            // PLAINTEXT (caller decodes any stored base64)
        QString from;                // sender; empty -> mail_from_default
    };
    struct Result {
        bool ok = false;
        QString message;             // error text when !ok
    };

    static Result send(const Params &p, const QString &recipient,
                       const QString &subject, const QString &body);
};

#endif // TIBACKUPMAILER_H
