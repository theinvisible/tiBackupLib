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

#include "Poco/Exception.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/MailMessage.h"
#include "Poco/Net/MailRecipient.h"
#include "Poco/Net/SMTPClientSession.h"
#include "Poco/Net/StringPartSource.h"

tiBackupMailer::Result tiBackupMailer::send(const Params &p, const QString &recipient,
                                            const QString &subject, const QString &body)
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

    Poco::Net::MailMessage mail;
    mail.addRecipient(Poco::Net::MailRecipient(Poco::Net::MailRecipient::PRIMARY_RECIPIENT,
                                               recipient.toStdString()));
    mail.setSender(mailFrom.toStdString());
    mail.setSubject(subject.toStdString());
    mail.addContent(new Poco::Net::StringPartSource(body.toStdString()));

    try
    {
        // Same transport as the backup-notification send in tiBackupJob: plain
        // SMTPClientSession (default port), AUTH_LOGIN only when configured.
        Poco::Net::SMTPClientSession smtp(p.server.toStdString());
        if(p.auth)
            smtp.login(Poco::Net::SMTPClientSession::AUTH_LOGIN,
                       p.username.toStdString(), p.password.toStdString());
        else
            smtp.login();

        smtp.sendMessage(mail);
        smtp.close();
    }
    catch(const Poco::Exception &e)
    {
        res.message = QString::fromStdString(e.displayText());
        return res;
    }

    res.ok = true;
    return res;
}
