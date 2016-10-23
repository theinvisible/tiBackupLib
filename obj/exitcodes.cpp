#include "exitcodes.h"

QMap<int, QString> exitCodesRsync::codes = QMap<int, QString>();

exitCodesRsync::exitCodesRsync()
{

}

QString exitCodesRsync::getMsg(int code)
{
    initCodes();

    if(codes.contains(code))
        return codes[code];
    else
        return "Unknown status";
}

void exitCodesRsync::initCodes()
{
    if(codes.size() == 0)
    {
        codes[0] = "Success";
        codes[1] = "Syntax or usage error";
        codes[2] = "Protocol incompatibility";
        codes[3] = "Errors selecting input/output files, dirs";
        codes[4] = "Requested  action not supported: an attempt was made to manipulate 64-bit files on a platform that cannot support them; or an option was specified that is supported by the client and not by the server.";
        codes[5] = "Error starting client-server protocol";
        codes[6] = "Daemon unable to append to log-file";
        codes[10] = "Error in socket I/O";
        codes[11] = "Error in file I/O";
        codes[12] = "Error in rsync protocol data stream";
        codes[13] = "Errors with program diagnostics";
        codes[14] = "Error in IPC code";
        codes[20] = "Received SIGUSR1 or SIGINT";
        codes[21] = "Some error returned by waitpid()";
        codes[22] = "Error allocating core memory buffers";
        codes[23] = "Partial transfer due to error";
        codes[24] = "Partial transfer due to vanished source files";
        codes[25] = "The --max-delete limit stopped deletions";
        codes[30] = "Timeout in data send/receive";
        codes[35] = "Timeout waiting for daemon connection";
    }
}
