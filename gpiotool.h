/****************************************************************************
*
* Copyright (C) 2026 MiaoQingrui. All rights reserved.
* Author: 缪庆瑞 <justdoit_mqr@163.com>
*
****************************************************************************/
/*
 *@author:  缪庆瑞
 *@date:    2026.07.17
 *@brief:   GPIO读写工具模块
 *
 *该模块是针对Linux系统下通用GPIO子系统驱动封装的一套应用层读写接口，内部使用sysfs接口。
 *该模块分为三个类，GpioBase作为基类提供基础的gpio操作接口，GpioOutput和GpioInput继承自GpioBase，
 *分别负责写IO和读IO。为了方便移植，这些类定义在一个文件模块中使用。
 *
 *注：因为Linux通用GPIO子系统已经涉及到底层硬件，所以权限要求比较严格，文件写入时需要root权限，所以该模块执行时
 *的EUID(有效用户id)需要是root，如果无法满足的话，则可以修改GpioBase::autoElevateWritePermission常变量为
 *true，作为应急方案解决。
 */
#ifndef GPIOTOOL_H
#define GPIOTOOL_H

#include <QObject>
#include <QSocketNotifier>
#include <string>

/********GPIO读写基类********/
class GpioBase : public QObject
{
    Q_OBJECT

public:
    //gpio电平
    enum Level {
        LEVEL_LOW  = 0,
        LEVEL_HIGH = 1
    };
    //gpio方向
    enum Direction {
        DIR_IN,
        DIR_OUT
    };
    //gpio输入边沿触发方式
    enum Edge {
        NONE,
        RISING,
        FALLING,
        BOTH
    };

    explicit GpioBase(int gpioNum, QObject *parent = nullptr);
    virtual ~GpioBase();
    bool isValid() const { return m_valueFd >= 0; }

protected:
    bool exportGpio();
    bool unexportGpio();

    bool setDirection(Direction dir);
    bool setEdge(Edge edge);

    int readValue() const;
    bool writeValue(Level level);

    //保护变量，传递给派生类使用
    int m_gpioNum;
    int m_valueFd;//value文件句柄
    std::string m_sysfsPath;

private:
    std::string buildPath(const std::string &sub) const;
    bool writeSysfsFile(const std::string &path, const std::string &content);
    /* 是否自动提升gpio相关文件的写权限
     * /sys/class/gpio/目录下的文件默认只有root权限才能写，如果当前进程的有效UID不是root(非root用户登录/没有使用sudo启动/没有setuid),
     * 则写文件时会因为权限问题失败。如果需要在代码中自动提升文件的写权限则将该变量置为true，这样内部会尝试使用sudo chmod修改文件的权限，前提
     * 是系统支持sudo命令，用户在sudo组内并且支持免密码。
     *
     * 注:该变量默认是false，因为从最佳实践和安全性角度来看，不建议在代码中直接修改gpio这种系统级的文件权限。推荐系统创建gpio组，并将使用的用
     * 户添加到该组，然后通过udev规则(RUN脚本)自动给/sys/class/gpio/修改所属组和权限。不过该方案需要对系统进行定制，如果无法实现的话，则推荐
     * 使用sudo执行程序，为整个进程提升权限，而不是系统文件。当然以上推荐是为了让解决方案更优雅，实在不行将此处改为false也可以解决权限问题。
     */
    const bool autoElevateWritePermission = false;
};

/********GPIO写输出类********/
class GpioOutput : public GpioBase
{
    Q_OBJECT

public:
    explicit GpioOutput(int gpioNum, QObject *parent = nullptr);

    bool setHigh() { return writeValue(LEVEL_HIGH); }
    bool setLow()  { return writeValue(LEVEL_LOW); }
    bool toggle();
};

/********GPIO读输入类********/
class GpioInput : public GpioBase
{
    Q_OBJECT

public:
    explicit GpioInput(int gpioNum, QObject *parent = nullptr);
    ~GpioInput();

    int readOnce() const { return readValue(); }
    bool startMonitoring(Edge edge = BOTH);
    void stopMonitoring();
    bool isMonitoring() const { return m_notifier != nullptr; }

signals:
    void edgeTriggered(int value);

private slots:
    void onNotifierActivated();

private:
    QSocketNotifier *m_notifier;
};

#endif // GPIOTOOL_H
