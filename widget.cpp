#include "widget.h"
#include "ui_widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
}

Widget::~Widget()
{
    delete ui;
    if(monitorGpioInput)
    {
        delete monitorGpioInput;
        monitorGpioInput = nullptr;
    }
}

//gpio写输出
void Widget::on_gpio_write_btn_clicked()
{
    GpioOutput gpioOutput(ui->gpio_write_no_spinBox->value());
    if(gpioOutput.isValid())
    {
        if(ui->gpio_write_level_comboBox->currentIndex() == 0)
        {
            gpioOutput.setLow();
        }
        else
        {
            gpioOutput.setHigh();
        }
    }
}
//gpio读输入
void Widget::on_gpio_read_btn_clicked()
{
    GpioInput gpioInput(ui->gpio_read_no_spinBox->value());
    if(gpioInput.isValid())
    {
        int val = gpioInput.readOnce();
        if(val != -1)
        {
            ui->gpio_read_level_lineEdit->setText(QString::number(val));
        }
    }
}

//gpio监测输入
void Widget::on_gpio_monitor_btn_clicked()
{
    if(monitorGpioInput)
    {
        delete monitorGpioInput;
        monitorGpioInput = nullptr;
    }

    monitorGpioInput = new GpioInput(ui->gpio_monitor_no_spinBox->value());
    connect(monitorGpioInput,&GpioInput::edgeTriggered,this,[this](int value){
        ui->gpio_monitor_textBrowser->append(QString("edgeTriggered:val= %1\n").arg(value));
    });
    if(monitorGpioInput->isValid())
    {
        if(ui->gpio_monitor_btn->text() == "开始监测")
        {
            bool ret = monitorGpioInput->startMonitoring((GpioBase::Edge)ui->gpio_monitor_edge_comboBox->currentIndex());
            if(ret)
            {
                ui->gpio_monitor_btn->setText("停止监测");
            }
            else
            {
                ui->gpio_monitor_btn->setChecked(false);
            }
        }
        else
        {
            monitorGpioInput->stopMonitoring();
            ui->gpio_monitor_btn->setText("开始监测");
        }
    }
}

