#include "utils.h"


QString sysErrStr(const char* prefix)
{
    return QString("%1: %2")
        .arg(prefix, QString::fromLocal8Bit(strerror(errno)));
}
