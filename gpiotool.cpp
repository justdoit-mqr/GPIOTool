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
 */
#include "gpiotool.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <cstdio>
#include <fstream>
#include <sstream>

/*
 *@brief:   GpioBase构造函数
 *负责传递使用gpio号，一个对象负责管理一个gpio
 *@date:    2026.07.17
 *@param:   gpioNum:gpio号，需要自行根据gpio组和索引计算gpio号
 */
GpioBase::GpioBase(int gpioNum, QObject *parent)
    : QObject(parent), m_gpioNum(gpioNum), m_valueFd(-1)
{
    m_sysfsPath = "/sys/class/gpio/gpio" + std::to_string(m_gpioNum) + "/";
}
/*
 *@brief:   GpioBase析构函数
 *负责对gpio进行资源释放处理
 *@date:    2026.07.17
 */
GpioBase::~GpioBase()
{
    unexportGpio();
}
/*
 *@brief:   路径拼接，主要用来拼接gpio路径下的value、edge、direction等子路径
 *@date:    2026.07.17
 *@param:   sub:子路径名
 *@return:  std::string:gpio对应文件的绝对路径
 */
std::string GpioBase::buildPath(const std::string &sub) const
{
    return m_sysfsPath + sub;
}
/*
 *@brief:   负责写入除value以外其他gpio相关文件的内容
 *@date:    2026.07.17
 *@param:   path:文件路径
 *@param:   content:写入的内容
 *@bool:    bool:true=成功
 */
bool GpioBase::writeSysfsFile(const std::string &path, const std::string &content)
{
    int fd = open(path.c_str(), O_WRONLY);
    if (fd < 0)
    {
        //如果权限不足，且开启了自动提升写权限，则尝试通过sudo chmod添加写权限
        if(errno == EACCES && autoElevateWritePermission)
        {
            std::string cmd = "sudo chmod a+w " + path + " 2>/dev/null";
            int ret = system(cmd.c_str());
            if (ret != 0)
            {
               fprintf(stderr, "GpioBase: failed to elevate write permission for %s: %s\n",
                       path.c_str(),strerror(errno));
               return false;
            }
            //权限提升成功后，重新打开文件
            fd = open(path.c_str(), O_WRONLY);
        }

        if(fd < 0)
        {
            fprintf(stderr, "GpioBase: failed to open %s: %s\n", path.c_str(), strerror(errno));
            return false;
        }
    }

    ssize_t ret = write(fd, content.c_str(), content.size());
    close(fd);

    if (ret != static_cast<ssize_t>(content.size()))
    {
        fprintf(stderr, "GpioBase: failed to write to %s: %s\n", path.c_str(), strerror(errno));
        return false;
    }
    return true;
}
/*
 *@brief:   导出gpio号，并打开value文件
 *@date:    2026.07.17
 *@return:  bool:true=成功
 */
bool GpioBase::exportGpio()
{
    //表示已经导出并打开文件
    if(m_valueFd >= 0)
    {
        return true;
    }

    struct stat st;
    if (stat(m_sysfsPath.c_str(), &st) < 0)//尚未导出
    {
        if (!writeSysfsFile("/sys/class/gpio/export", std::to_string(m_gpioNum)))
        {
            return false;
        }

        //导出后不会立即生效，所以需要轮询检查状态
        const int MAX_RETRIES = 100;
        const int SLEEP_US = 10000;
        bool created = false;
        for (int i = 0; i < MAX_RETRIES; ++i)
        {
            if (stat(m_sysfsPath.c_str(), &st) == 0)
            {
                created = true;
                break;
            }
            usleep(SLEEP_US);
        }
        if (!created)
        {
            fprintf(stderr, "GpioBase: export gpio %d timeout after %d ms\n",
                    m_gpioNum, (MAX_RETRIES * SLEEP_US) / 1000);
            return false;
        }
    }

    //导出成功后，立即打开value文件
    std::string valuePath = buildPath("value");
    m_valueFd = open(valuePath.c_str(), O_RDWR);
    if (m_valueFd < 0)
    {
        //如果权限不足，且开启了自动提升写权限，则尝试通过sudo chmod添加写权限
        if(errno == EACCES && autoElevateWritePermission)
        {
            std::string cmd = "sudo chmod a+rw " + valuePath + " 2>/dev/null";
            int ret = system(cmd.c_str());
            if (ret != 0)
            {
               fprintf(stderr, "GpioBase: failed to elevate write permission for %s: %s\n",
                       valuePath.c_str(),strerror(errno));
               return false;
            }
            //权限提升成功后，重新打开文件
            m_valueFd = open(valuePath.c_str(), O_RDWR);
        }

        if(m_valueFd < 0)
        {
            fprintf(stderr, "GpioBase: failed to open value file for gpio %d: %s\n",
                    m_gpioNum, strerror(errno));
            //如果打开失败，则取消导出
            writeSysfsFile("/sys/class/gpio/unexport", std::to_string(m_gpioNum));
            return false;
        }
    }
    return true;
}
/*
 *@brief:   取消导出gpio
 *@date:    2026.07.17
 *@return:  bool:true=成功
 */
bool GpioBase::unexportGpio()
{
    if (m_valueFd < 0)
    {
        return true;
    }

    close(m_valueFd);
    m_valueFd = -1;
    bool ok = writeSysfsFile("/sys/class/gpio/unexport", std::to_string(m_gpioNum));
    return ok;
}
/*
 *@brief:   设置gpio方向
 *@date:    2026.07.17
 *@param:   dir:输入/输出方向
 *@return:  bool:true=成功
 */
bool GpioBase::setDirection(Direction dir)
{
    if (m_valueFd < 0)
    {
        return false;
    }
    const char *dirStr = (dir == DIR_IN) ? "in" : "out";
    return writeSysfsFile(buildPath("direction"), dirStr);
}
/*
 *@brief:   设置gpio边沿触发方式
 *@date:    2026.07.17
 *@param:   Edge:边沿触发方式
 *@return:  bool:true=成功
 */
bool GpioBase::setEdge(Edge edge)
{
    if (m_valueFd < 0)
    {
        return false;
    }
    std::string edgeStr;
    switch (edge)
    {
        case NONE:    edgeStr = "none";    break;
        case RISING:  edgeStr = "rising";  break;
        case FALLING: edgeStr = "falling"; break;
        case BOTH:    edgeStr = "both";    break;
        default:      edgeStr = "none";    break;
    }
    return writeSysfsFile(buildPath("edge"), edgeStr);
}
/*
 *@brief:   读gpio值
 *@date:    2026.07.17
 *@return:  int:0/1  -1表示出错
 */
int GpioBase::readValue() const
{
    if (m_valueFd < 0)
    {
        return -1;
    }

    if (lseek(m_valueFd, 0, SEEK_SET) < 0)
    {
        fprintf(stderr, "GpioBase: lseek failed for gpio %d: %s\n", m_gpioNum, strerror(errno));
        return -1;
    }

    char ch = 0;
    ssize_t ret = read(m_valueFd, &ch, 1);
    if (ret != 1)
    {
        fprintf(stderr, "GpioBase: read failed for gpio %d: %s\n",
                m_gpioNum, (ret < 0) ? strerror(errno) : "unexpected EOF");
        return -1;
    }
    return (ch == '1') ? 1 : 0;
}
/*
 *@brief:   写gpio值
 *@date:    2026.07.17
 *@param:   level:gpio电平
 *@return:  bool:true=成功
 */
bool GpioBase::writeValue(Level level)
{
    if (m_valueFd < 0) {
        return false;
    }

    std::string val = (level == LEVEL_HIGH) ? "1" : "0";
    if (lseek(m_valueFd, 0, SEEK_SET) < 0)
    {
        fprintf(stderr, "GpioBase: lseek failed for gpio %d: %s\n", m_gpioNum, strerror(errno));
        return false;
    }
    ssize_t ret = write(m_valueFd, val.c_str(), val.size());
    if (ret != static_cast<ssize_t>(val.size()))
    {
        fprintf(stderr, "GpioBase: write failed for gpio %d: %s\n", m_gpioNum, strerror(errno));
        return false;
    }
    return true;
}
/*
 *@brief:   GpioOutput构造函数
 *@date:    2026.07.17
 *@param:   gpioNum:gpio号，需要自行根据gpio组和索引计算gpio号
 */
GpioOutput::GpioOutput(int gpioNum, QObject *parent)
    : GpioBase(gpioNum, parent)
{
    if (!exportGpio() || !setDirection(DIR_OUT))
    {
        fprintf(stderr, "GpioOutput: failed to initialize gpio %d\n", gpioNum);
    }
}
/*
 *@brief:   反转电平
 *@date:    2026.07.17
 *@return:  bool:true=成功
 */
bool GpioOutput::toggle()
{
    int current = readValue();
    if (current < 0)
    {
        return false;
    }
    return writeValue((current == 1) ? LEVEL_LOW : LEVEL_HIGH);
}

/*
 *@brief:   GpioInput构造函数
 *@date:    2026.07.17
 *@param:   gpioNum:gpio号，需要自行根据gpio组和索引计算gpio号
 */
GpioInput::GpioInput(int gpioNum, QObject *parent)
    : GpioBase(gpioNum, parent)
    , m_notifier(nullptr)
{
    if (!exportGpio() || !setDirection(DIR_IN))
    {
        fprintf(stderr, "GpioInput: failed to initialize gpio %d\n", gpioNum);
    }
}
/*
 *@brief:   GpioInput析构函数
 *@date:    2026.07.17
 */
GpioInput::~GpioInput()
{
    stopMonitoring();
}
/*
 *@brief:   启动边沿触发监控
 *@date:    2026.07.17
 *@param:   Edge:边沿触发模式
 *@return:  bool:true=成功
 */
bool GpioInput::startMonitoring(Edge edge)
{
    if (m_notifier)
    {
        fprintf(stderr, "GpioInput: already monitoring gpio %d\n", m_gpioNum);
        return false;
    }

    if (m_valueFd < 0)
    {
        fprintf(stderr, "GpioInput: GPIO %d is not valid\n", m_gpioNum);
        return false;
    }

    if (!setEdge(edge))
    {
        fprintf(stderr, "GpioInput: failed to set edge for gpio %d\n", m_gpioNum);
        return false;
    }

    //Linux内核GPIO驱动规定底层poll使用POLLPRI，即对应QSocketNotifier::Exception
    m_notifier = new QSocketNotifier(m_valueFd, QSocketNotifier::Exception, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &GpioInput::onNotifierActivated);
    m_notifier->setEnabled(true);

    printf("GpioInput: started monitoring gpio %d\n", m_gpioNum);
    return true;
}
/*
 *@brief:   停止边沿触发监控
 *@date:    2026.07.17
 */
void GpioInput::stopMonitoring()
{
    if (m_notifier)
    {
        m_notifier->deleteLater();
        m_notifier = nullptr;

        setEdge(NONE);
        printf("GpioInput: stopped monitoring gpio %d\n", m_gpioNum);
    }
}
/*
 *@brief:   GPIO输入边沿触发信号处理
 *@date:    2026.07.17
 */
void GpioInput::onNotifierActivated()
{
    if (m_notifier)
    {
        m_notifier->setEnabled(false);
    }

    int value = readValue();;
    emit edgeTriggered(value);

    if (m_notifier)
    {
        m_notifier->setEnabled(true);
    }
}
