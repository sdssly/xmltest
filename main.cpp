#include "mainwindow.h"
#include <QApplication>
#include <QtGui>
//#include <QtWebKitWidgets>
#include <QWebView>
#include <QtWebKit>
#include <QDebug>
#include <QUrl>

int main(int argc, char *argv[])
{
    int i = 0;
    char ch = 'a';
    QApplication a(argc, argv);
#if 0
    QWebView view;
    view.move(QPoint(0,0));
    view.setFixedSize(320,240);
    view.show();
    view.setUrl(QUrl("http://www.12306.cn/"));
#else
    MainWindow w;

    qDebug() <<__FUNCTION__<<endl;
    w.setFixedSize(320,240);
    w.move(QPoint(0,0));
    //w.setWindowFlags(Qt::SplashScreen);

    w.show();
    w.SetFocus();

#endif
    return a.exec();
}
