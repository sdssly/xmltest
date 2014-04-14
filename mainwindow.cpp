#include <QDebug>
#include <QFile>
#include <QTimer>
#include <QTime>
#include <QDateTime>
#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    QString appVersion = QString(__DATE__);
    QString LinuxVersion = "";

    LinuxVersion = tool.GetSysVersion();

    ui->setupUi(this);
    //ui->pushButton->setText("ADC");

    appVersion = "Build: " + appVersion + "-" + __TIME__;
    ui->labelappVersion->setText(appVersion);
    ui->labelsysVersion->setText(LinuxVersion);
    count = 0;
    timer = new QTimer(this);

    timer->setInterval(1000);

    connect(timer, SIGNAL(timeout()), this, SLOT(updateTime()));
    timer->start();
    updateTime();
}

void MainWindow::SetFocus()
{
    ui->pushButton_2->setAutoDefault(true);
}

MainWindow::~MainWindow()
{
    qDebug("del ui...");
    delete ui;
}

void MainWindow::on_MainWindow_windowIconChanged(const QIcon &icon)
{

}

void MainWindow::on_pushButton_clicked()
{
    char ch = 'b';
    int ret = QMessageBox::question(this, "Test", "Quit?",
                                    QMessageBox::Yes|QMessageBox::No);

    qDebug() << "ret=" << ret;
    if (QMessageBox::Yes == ret) {
        QApplication::quit();
    }

}

void MainWindow::updateTime()
{
     //;
    QDateTime dateTime = QDateTime::currentDateTime();
    QString dateTimeString = dateTime.toString("yyyy-MM-dd HH:mm:ss");

    ui->labelStatus->setText(dateTimeString);

   QString a0 = QString::number(tool.GetADC(0), 10);
   QString a1 = QString::number(tool.GetADC(1), 10);
   QString a2 = QString::number(tool.GetADC(2), 10);

    ui->labelResult->setText(a0 + "/" + a2 + "/" + a2);
}
void MainWindow::on_pushButton_destroyed()
{

}

void MainWindow::on_pushButton_2_clicked()
{
    count++;
    qDebug() << count;
    ui->labelResult->setText("Result: " + QString::number(count, 10));
}
