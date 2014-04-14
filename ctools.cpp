#include "ctools.h"

CTools::CTools(QObject *parent) :
    QObject(parent)
{
    char chdata[16];
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    adcfd = -1;
    gpioFile = NULL;

#if 1
    adcfd = open("/dev/fb0", O_RDONLY);
    qDebug("Open fb0 adcfd=%d", adcfd);

    if (-1 == adcfd) {
       qDebug("file open fail!");
       return;
    }

    if (ioctl(adcfd,FBIOGET_FSCREENINFO,&finfo)){
     qDebug("Error reading fixed information");
     return;
    }

    if (ioctl(adcfd,FBIOGET_VSCREENINFO,&vinfo)){
     qDebug("Error reading variable information");
     return;
    }

   qDebug("The xres is %d", vinfo.xres);
   qDebug("The yres is %d", vinfo.yres);
   close(adcfd);
#endif

   adcfd = open(IMX_ADC_DEVICE, O_RDONLY);
   qDebug("Open ADC adcfd=%d", adcfd);

   if (-1 == adcfd) {
      qDebug("file open fail!");
      return;
   }

    ::ioctl(adcfd, IMX_ADC_INIT);

   gpioFile = new QFile("/sys/class/gpio/unexport");
   
   qDebug("close all gpio");
   for (int pin = 0; pin < 64; pin++) {
		gpioFile->open(QIODevice::WriteOnly);
       sprintf(chdata, "%d", pin);
       gpioFile->write(chdata, sizeof(chdata));
       gpioFile->close();
   }
}

void CTools::setGPIOOutput(int gpionum) {
    char chdata[64];
    QFile gpiofile("/sys/class/gpio/export");
    memset(chdata, 0, sizeof(chdata));
    sprintf(chdata, "%d", gpionum);

    gpiofile.open(QIODevice::WriteOnly);
    gpiofile.write(chdata, sizeof(chdata));
    gpiofile.close();

    sprintf(chdata, "/sys/class/gpio/gpio%d/direction", gpionum);

    gpiofile.setFileName(chdata);
    gpiofile.open(QIODevice::WriteOnly);
    gpiofile.write("out", strlen("outs"));
    gpiofile.close();
    return;
}

void CTools::setGPIOvalue(int gpionum, int value) {
    char chdata[64];
    QFile gpiofile;
    char ch[4];

    if (0 == value) {
        sprintf(ch, "%d", 0);
    } else {
        sprintf(ch, "%d", 0);
    }

    sprintf(chdata, ("/sys/class/gpio/gpio%d/value"), gpionum);

    gpiofile.setFileName(chdata);
    gpiofile.open(QIODevice::WriteOnly);
    gpiofile.write(ch, sizeof(ch));
    gpiofile.close();
}
// gpio.c set gpio pins from 0 to 31


void CTools::Closefd()
{
    qDebug("close %d...", adcfd);
    if (-1 != adcfd) {
        //::ioctl(adcfd, IMX_ADC_DEINIT, NULL);
       // close(adcfd);
        adcfd = -1;
    }
}

CTools::~CTools()
{
    Closefd();

    if (NULL != gpioFile) {
        delete gpioFile;
        gpioFile = NULL;
    }
}

QString CTools::GetSysVersion()
{
    QFile LinuxVersionFile("/proc/version");
    QString LinuxVersion = "";
    char version[256];

    if (!LinuxVersionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            LinuxVersion = "Unknown";
    } else {
        LinuxVersionFile.readLine(version, sizeof(version));
        LinuxVersion = version;
    }

    LinuxVersionFile.close();

    return LinuxVersion;
}

int CTools::GetADC(int channel)
{
#if 1
    struct t_adc_convert_param convert_param;

    convert_param.result[0] = 0;

    switch (channel) {
       case 0:
          convert_param.channel = GER_PURPOSE_ADC0;
        break;
       case 1:
        convert_param.channel = GER_PURPOSE_ADC1;
        break;
       case 2:
        convert_param.channel = GER_PURPOSE_ADC2;
        break;
       default:
        convert_param.channel = GER_PURPOSE_ADC0;
        break;
     }

     ::ioctl(adcfd, IMX_ADC_CONVERT, &convert_param);

     qDebug("Converted value: %hu\n", convert_param.result[0]);
     return convert_param.result[0];
#endif
     return 0;
}

