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

#include "tibackupmailer.h"
#include "config.h"

#include <mutex>

#include <QFile>
#include <QFileInfo>

#include "Poco/Exception.h"
#include "Poco/SharedPtr.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/MailMessage.h"
#include "Poco/Net/MailRecipient.h"
#include "Poco/Net/StringPartSource.h"
#include "Poco/Net/FilePartSource.h"
#include "Poco/Net/SMTPClientSession.h"
#include "Poco/Net/SecureSMTPClientSession.h"
#include "Poco/Net/SecureStreamSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/Context.h"
#include "Poco/Net/SSLManager.h"
#include "Poco/Net/AcceptCertificateHandler.h"

namespace {

// Poco NetSSL needs a client SSL context / manager before any TLS socket is used.
// We do encryption WITHOUT certificate verification (VERIFY_NONE + accept-all
// handler): self-hosted mail servers routinely use self-signed certs, and this
// matches tiBackup's existing PBS "ignore TLS errors" stance. Runs exactly once,
// safely across the backup-worker and web (QtConcurrent) threads.
void ensureSSLInitialized()
{
    static std::once_flag once;
    std::call_once(once, [] {
        Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> pCert =
            new Poco::Net::AcceptCertificateHandler(false /* client side */);
        Poco::Net::Context::Ptr pContext =
            new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, "",
                                   Poco::Net::Context::VERIFY_NONE);
        Poco::Net::SSLManager::instance().initializeClient(nullptr, pCert, pContext);
    });
}

} // namespace

tiBackupMailer::Security tiBackupMailer::securityFromString(const QString &s)
{
    const QString v = s.trimmed().toLower();
    if(v == QLatin1String("starttls")) return Security::StartTls;
    if(v == QLatin1String("ssl") || v == QLatin1String("tls")) return Security::Ssl;
    return Security::None;
}

QString tiBackupMailer::securityToString(Security s)
{
    switch(s)
    {
    case Security::StartTls: return QStringLiteral("starttls");
    case Security::Ssl:      return QStringLiteral("ssl");
    case Security::None:     break;
    }
    return QStringLiteral("none");
}

tiBackupMailer::Result tiBackupMailer::send(const Params &p, const QString &recipient,
                                            const QString &subject, const QString &body,
                                            const QStringList &attachmentPaths)
{
    Result res;

    if(p.server.isEmpty())
    {
        res.message = QStringLiteral("SMTP server is not configured.");
        return res;
    }
    if(recipient.isEmpty())
    {
        res.message = QStringLiteral("No recipient given.");
        return res;
    }

    QString mailFrom = p.from;
    if(mailFrom.isEmpty())
        mailFrom = tibackup_config::mail_from_default;
    const Poco::UInt16 port = static_cast<Poco::UInt16>(p.port > 0 ? p.port : 25);

    // Build the message; Poco does the MIME/multipart + base64 for attachments.
    Poco::Net::MailMessage mail;
    mail.addRecipient(Poco::Net::MailRecipient(Poco::Net::MailRecipient::PRIMARY_RECIPIENT,
                                               recipient.toStdString()));
    mail.setSender(mailFrom.toStdString());
    mail.setSubject(subject.toStdString());
    mail.addContent(new Poco::Net::StringPartSource(body.toStdString()));
    for(const QString &path : attachmentPaths)
    {
        if(path.isEmpty() || !QFile::exists(path))
            continue;
        const QString name = QFileInfo(path).fileName();
        mail.addAttachment(name.toStdString(),
                           new Poco::Net::FilePartSource(path.toStdString()));
    }

    const std::string host = p.server.toStdString();
    const std::string user = p.username.toStdString();
    const std::string pass = p.password.toStdString();

    // EHLO + optional AUTH, then send. Reused for every transport mode.
    auto authAndSend = [&](Poco::Net::SMTPClientSession &s) {
        if(p.auth)
            s.login(Poco::Net::SMTPClientSession::AUTH_LOGIN, user, pass);
        else
            s.login();
        s.sendMessage(mail);
        s.close();
    };

    try
    {
        if(p.security == Security::Ssl)
        {
            // Implicit TLS: the connection is encrypted from the start (SMTPS).
            ensureSSLInitialized();
            Poco::Net::SecureStreamSocket socket(Poco::Net::SocketAddress(host, port));
            Poco::Net::SMTPClientSession session(socket);
            authAndSend(session);
        }
        else if(p.security == Security::StartTls)
        {
            // Plain connect, EHLO, then upgrade in place with STARTTLS.
            ensureSSLInitialized();
            Poco::Net::SecureSMTPClientSession session(host, port);
            session.login();          // EHLO on the plain connection (negotiates STARTTLS)
            session.startTLS();       // upgrade to TLS (uses the default client context)
            authAndSend(session);     // re-EHLO (+ AUTH) over the encrypted channel, then send
        }
        else
        {
            Poco::Net::SMTPClientSession session(host, port);
            authAndSend(session);
        }
    }
    catch(const Poco::Exception &e)
    {
        res.message = QString::fromStdString(e.displayText());
        return res;
    }

    res.ok = true;
    return res;
}
