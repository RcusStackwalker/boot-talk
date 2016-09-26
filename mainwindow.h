#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <stdint.h>
namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
protected slots:
    void onReadTimerTimeout();
private slots:
    void on_pushButton_2_clicked();

    void on_rebootButton_clicked();

    void on_init1Button_clicked();

private:
    void adapterWrite(const uint8_t *data, unsigned size);
    void adapterWriteAndWait(const uint8_t *data, unsigned size, unsigned timeout = 500);
    void setDataRate(unsigned dr);
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
