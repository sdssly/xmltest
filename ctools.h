#ifndef CTOOLS_H
#define CTOOLS_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QDebug>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "imx_adc.h"

class CTools : public QObject
{
    Q_OBJECT
public:
    explicit CTools(QObject *parent = 0);
    ~CTools();
    QString GetSysVersion();
    void Closefd();
    int GetADC(int channel);
    void setGPIOOutput(int gpionum);
    void setGPIOvalue(int gpionum, int value);

signals:

public slots:

private:
   int adcfd;
   QFile *gpioFile;
};

#endif // CTOOLS_H
