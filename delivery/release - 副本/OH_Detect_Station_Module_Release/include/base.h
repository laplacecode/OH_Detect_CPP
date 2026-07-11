#ifndef BASECLASS_H
#define BASECLASS_H

#include <iostream>
#include <QObject>
#include <QByteArray>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

/**
 * @brief 模块化功能组件的抽象基类
 * 继承自QObject，支持Qt对象系统（若启用Q_OBJECT）
 * 提供数据传输、初始化和运行等通用接口
 */
class Base : public QObject
{
     //Q_OBJECT  // 若需要信号槽或Qt元对象特性，需取消注释并重新生成moc文件

public:
    // 公共成员变量（注意：通常建议封装为私有变量并通过接口访问）
    bool        init_flag;     ///< 模块初始化状态标志，false=未初始化，true=已初始化
    std::string paras;         ///< 模块配置参数（JSON/XML等结构化数据）
    cv::Mat     m_mat;         ///< 图像数据传递载体（OpenCV矩阵类型）
    std::string moduleName;    ///< 模块名称标识符

public:
    Base() {}                  ///< 默认构造函数
    virtual ~Base() {}         ///< 虚析构函数确保派生类正确释放资源

    /**
     * @brief 发送数据接口（需子类实现）
     * @param images 待发送的OpenCV图像数组
     * @param data 二进制数据载体（Qt字节数组）
     * @return 操作是否成功
     */
    virtual bool sendData(std::vector<cv::Mat>& images, QByteArray& data) { return true; }

    /**
     * @brief 接收数据接口（需子类实现）
     * @param images 接收到的OpenCV图像数组
     * @param data 接收到的二进制数据
     * @return 操作是否成功
     */
    virtual bool reciveData(std::vector<cv::Mat>& images, QByteArray& data) { return true; }

    /**
     * @brief 初始化模块参数接口（需子类实现）
     * @param config 包含配置参数的字节数组
     * @return 初始化是否成功
     */
    virtual bool initParas(QByteArray& config) { return true; }

    /**
     * @brief 主运行逻辑接口（需子类实现）
     * @return 运行是否成功
     */
    virtual bool run() { return true; }
};

#endif // __BASECLASS_H__