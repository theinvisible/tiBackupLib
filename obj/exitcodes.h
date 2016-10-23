#ifndef EXITCODES_H
#define EXITCODES_H

#include <QString>
#include <QMap>

class exitCodesRsync
{
public:
    exitCodesRsync();
    static QString getMsg(int code);

    static QMap<int, QString> codes;

private:
    static void initCodes();
};

#endif // EXITCODES_H
