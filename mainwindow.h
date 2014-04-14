#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include "ctools.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void SetFocus();
    int count;
    QTimer *timer;
    CTools tool;

private slots:
    void on_MainWindow_windowIconChanged(const QIcon &icon);

    void on_pushButton_clicked();

    void on_pushButton_destroyed();
    void updateTime();

    void on_pushButton_2_clicked();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
