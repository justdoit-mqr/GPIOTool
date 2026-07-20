#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include "gpiotool.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    void on_gpio_write_btn_clicked();

    void on_gpio_read_btn_clicked();

    void on_gpio_monitor_btn_clicked();

private:
    Ui::Widget *ui;
    GpioInput *monitorGpioInput = nullptr;
};
#endif // WIDGET_H
