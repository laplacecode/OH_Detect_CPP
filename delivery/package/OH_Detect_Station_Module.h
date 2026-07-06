#ifndef OH_DETECT_STATION_MODULE_H
#define OH_DETECT_STATION_MODULE_H

#include "base.h"
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QMap>
#include <numeric> // 包含 iota 函数的定义
#include <stdexcept> // 标准异常支持
#include <sstream>
#include <opencv2/core/utility.hpp>
#include <boost/filesystem.hpp>
#include <cmath>
#include <iomanip>
#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>
#include <string>
#include <fstream>
#include <cstdio>
#include <onnxruntime_cxx_api.h>
#include <Windows.h>
#include <direct.h>
#include <io.h>
#include <algorithm>
#include <assert.h>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <chrono>
#include <QVector>
#include <QCoreApplication>
#include <QDebug>
#include <QByteArray>
#include <QtEndian>
#include <locale>
#include <codecvt>
#include <QString>
#include <QTextCodec>


namespace fs = boost::filesystem;
using namespace Ort;
using namespace cv;
using namespace std;
#pragma execution_character_set("utf-8")

// 核心检测模块实现类，继承自Base类，实现工业检测核心逻辑
class OH_Detect_Station_Module : public Base {
	//Q_OBJECT  // 启用Qt元对象系统需取消注释

public:
	explicit OH_Detect_Station_Module(QObject* parent = nullptr);
	virtual ~OH_Detect_Station_Module();

private:


	//bool saveImageOrNot = false;//算法是否需要正常存图，如果需要就置为true，如果不需要就置为false
	//bool debugOrNot = false;//是否是调试状态，如果值为true，则为调试状态，调试状态时，会输出和显示很多调试信息	
	//bool ConvertAnnotatedImages_OrNot = false;//是否转换标注图像，如果设置为true，则算法会自动将图片转换为8位图像并增强后保存到指定的文件夹,正式交付时需要将该值置为false
	//bool drawBoundingBoxRect_OrNot = false;//调试时用于观察是否获取正确的ROI,正式交付时需要将该值置为false
	//




	bool saveImageOrNot = true;//算法是否需要正常存图，如果需要就置为true，如果不需要就置为false
	bool debugOrNot = true;//是否是调试状态，如果值为true，则为调试状态，调试状态时，会输出和显示很多调试信息	
	bool ConvertAnnotatedImages_OrNot = true;//是否转换标注图像，如果设置为true，则算法会自动将图片转换为8位图像并增强后保存到指定的文件夹,正式交付时需要将该值置为false
	bool drawBoundingBoxRect_OrNot = true;//调试时用于观察是否获取正确的ROI,正式交付时需要将该值置为false



	//******************************设置部分核心自定义参数*******************************//

	// 设置裁切起点
	const int startX = 0;     // 从最左侧开始裁切
	const int startY = 100;    // 垂直方向起点

	//标准16位ROI区域内指定区域的灰度值（通过跑料732张图片确定的数据）
	double std_ROIImage_16bit_GrayscaleValue = 5490;


	//*******************************初始化输入变量，initParas()方法*********************************//
	string model_path = "./model/39B_OH_ThreeClasses_768.onnx";//初始化模型加载路径
	string saveFolder_Path = "./ResultData";//文件保存路径
	double calibrationCoefficient = 0.086;//初始化标定系数，单位：0.022mm/pixel;需要通过initParas()方法进行初始化

	// 窗宽窗位参数
	double windowWidth = 5330;  // 窗宽 
	double windowLevel = 2665;   // 窗位 


	double max_set_H_OH_value = 5;//6.水平方向OH最大值
	double min_set_H_OH_value = 0.05;//7.水平方向OH最小值
	bool b_H_OH_value_useOrNot = true;//8.是否启用该判断条件

	double max_set_V_OH_value = 5;//9.竖直方向OH最大值
	double min_set_V_OH_value = 0.05;//10.竖直方向OH最小值
	bool b_V_OH_value_useOrNot = true;//11.是否启用该判断条件


	double max_set_H_PositivePlate_Align_value = 51;//12.水平方向正极片对齐度限值
	double max_set_H_NegativePlate_Align_value = 52;//13.水平方向负极片对齐度限值
	bool b_H_PositivePlate_NegativePlate_value_useOrNot = true;//14.是否启用该判断条件


	double max_set_V_PositivePlate_Align_value = 53;//15.竖直方向正极片对齐度限值
	double max_set_V_NegativePlate_Align_value = 54;//16.竖直方向负极片对齐度限值
	bool b_V_PositivePlate_NegativePlate_value_useOrNot = true;//17.是否启用该判断条件


	int set_PositivePlates_Count = 91;//18.正极片数量（>设定值为OK）
	int set_NegativePlates_Count = 92;//19.负极片数量（>设定值为OK）
	bool b_PositivePlate_NegativePlate_Count_useOrNot = true;//20.是否启用正负极片数量检测


	double BatteryAngle = 45;//21.产品角度（单位：°）
	double X_Ray_Angle = 45;//22.X光照射角度（单位：°）
	bool b_Angle_useOrNot = true;//23.是否启用外部角度计算


	double H_OH_Offet_value = 0.1;//24.水平方向OH补偿值
	double V_OH_Offet_value = 0.1;//25.竖直方向OH补偿值
	bool b_H_V_OH_Offet_useOrNot = true;//26.是否启用OH补偿值


	double H_PositivePlate_Offet_value = 0.1;//27.水平方向正极对齐度补偿值
	double H_NegativePlate_Offet_value = 0.1;//28.水平方向负极对齐度补偿值
	bool b_H_PositivePlate_NegativePlate_Offet_useOrNot = true;//29.是否启用水平方向极片对齐度补偿值


	double V_PositivePlate_Offet_value = 0.1;//30.竖直方向正极对齐度补偿值
	double V_NegativePlate_Offet_value = 0.1;//31.竖直方向负极对齐度补偿值
	bool b_V_PositivePlate_NegativePlate_Offet_useOrNot = true;//32.是否启用竖直方向极片对齐度补偿值


	int First_OH_Calculate_PartNum = 10;//33.图像左侧索引计算OH层数
	int Third_OH_Calculate_PartNum = 10;//34.图像右侧索引计算OH层数
	bool b_OH_Calculate_PartNum_useOrNot = true;//35.是否启用左右两侧计算OH层数


	//*****************************单次触发输入变量，reciveData()方法********************************//
	string CCD_Name = "Corner1";//相机名称（OH角位名称）
	string QR_Code = "OH_Detect";//产品编码或者产品二维码


	//*****************************算法运行时，需要用到的临时变量********************************//
	//设定值
	double max_set_HV_PositivePlate_Align_value = 0;//正极极片对齐度投影
	double max_set_HV_NegativePlate_Align_value = 0;//负极极片对齐度
	double max_set_HV_OH_value = 0;//OH投影
	double min_set_HV_OH_value = 0;//OH投影



	//计算的结果值
	double max_result_HV_OH_value = 0;//OH投影
	double min_result_HV_OH_value = 0;//OH投影
	double max_result_HV_PositivePlate_Align_value = 0;//正极极片对齐度投影
	double max_result_HV_NegativePlate_Align_value = 0;//负极极片对齐度



	//将软件传入的上下限，转换为能够直接使用的投影数据
	void calculate_HV_Value();
	


	bool b_initFlag = false;	//定义一个初始化标志位，保证类中的bool initParas(QByteArray& config) 部分方法只能初始化一次
	int ProcessResult = 1;//此处自定义一个结果变量；为1时，正常检测无异常；为2时，图片是黑图或者白图

	string strCurrentTime = "20250509195736";//产品编码或者产品二维码
	string strCurrentDate = "20250768";//当前日期，只获取年月日
	string strCurrentTime_NormalFormat = "2025-05-12 19:24:23";//获取正常格式的时间

	bool b_allResult = true;//全局标志位
	bool b_QR_Code = true;//扫码结果标志位

	cv::Mat g_roiImage_8bit;//8位ROI中的图像
	cv::Mat g_roiImage_16bit;//16位ROI中的图像

	cv::Mat m_rawImage_16bit;        // 16位原图
	cv::Mat m_rawImage_8bit;        // 8位原图(窗宽窗位调整后)

	cv::Mat m_enhancedImage_8bit;        // 8位增强图
	cv::Mat m_drawingImage_8bit;        // 8位结果图绘制图

	cv::Mat m_segImage;			// AI推理后的结果图（全黑图）
	cv::Mat m_segImageTo255;	// AI推理后的结果图（黑白图，用于直接观察AI推理结果）


	// 调用裁切函数
	std::vector<cv::Mat> croppedImages;

	cv::Mat croppedImage1;//三张裁切后的图像
	cv::Mat croppedImage2;
	cv::Mat croppedImage3;

	cv::Mat segImage1;//三张推理结果图
	cv::Mat segImage2;
	cv::Mat segImage3;

	std::vector<cv::Point> positivePoints;//正极点集合
	std::vector<cv::Point> negativePoints;//负极点集合
	
	//过滤后的正负极点集合
	std::vector<cv::Point> filter_PositivePoints;
	std::vector<cv::Point> filter_NegativePoints;


	double initParas_time = 9999;//参数初始化时间，包含模型预热时间
	double reciveData_time = 9999;//16bit转8位，以及8位图像增强时间
	double run_time = 9999;//AI模型推理时间，以及算法计算时间
	double sendData_time = 9999;//图像绘制、参数计算、综合判断时间

	double onnxModelInfer_time = 9999;//AI模型推理时间
	double totalAlgorithm_time = 9999;//从接收到图片到反馈结果的总时间

	double voltage_date = 0;//X射线管管电压
	double electricCurrent_date = 0;//X射线管管电流
	double X_RAY_used_time = 0;//X射线管已使用寿命
	double tray_number = 0;//托盘号，治具号



	//平均灰度值
	double m_enhancedImage_8bit_GrayscaleValue = 0;//8位图像平均灰度值
	double m_rawImage_16bit_GrayscaleValue = 0;//16位图像平均灰度值

	double roiImage_8bit_GrayscaleValue = 0;//8位ROI图像平均灰度值
	double roiImage_16bit_GrayscaleValue = 0;//16位ROI图像平均灰度值



	//*********************以下是算法计算后输出的结果变量*************************//
	double max_result_H_OH_value = 0;
	double min_result_H_OH_value = 0;

	double max_result_V_OH_value = 0;
	double min_result_V_OH_value = 0;
	int OH_isNgOrOK = 1;//OH检测子结果（1：OK;2:NG）


	double max_result_H_PositivePlate_Align_value = 0;
	double max_result_H_NegativePlate_Align_value = 0;
	double max_result_V_PositivePlate_Align_value = 0;
	double max_result_V_NegativePlate_Align_value = 0;
	int PositivePlate_NegativePlate_Align_isNgOrOK = 1;//正极极片对齐度、负极极片检测子结果（1：OK;2:NG）


	int result_PositivePlates_Count = 0;//正极片数量
	int result_NegativePlates_Count = 0;//负极片数量
	int PositivePlates_NegativePlates_Count_isNgOrOK = 1;//正负极片数量检测子结果（1：OK;2:NG）

	//********************************************************************//

	// 私有的中间变量,y坐标范围变量
	double positive_min_y = -1;
	double positive_max_y = -1;
	double positive_y_diff = -1;
	double negative_min_y = -1;
	double negative_max_y = -1;
	double negative_y_diff = -1;

	//私有变量，定义存放前后OH的vector（左侧计算的OH、）
	std::vector<double> forward_OH;
	std::vector<double> forward_OH_H;
	std::vector<double> forward_OH_V;

	std::vector<double> backward_OH;
	std::vector<double> backward_OH_H;
	std::vector<double> backward_OH_V;

	std::vector<double> forward_And_backward_OH;

	


	//AI推理部部分私有变量
	// ONNX环境对象
	Ort::Env env_;
	// 会话配置对象
	Ort::SessionOptions session_options_;
	// ONNX会话对象（初始化为空指针）
	Ort::Session session_{ nullptr };
	// 内存分配器
	Ort::AllocatorWithDefaultOptions allocator_;
	// 模型输入节点名称
	std::string input_name_;
	// 模型输出节点名称
	std::string output_name_;
	// 模型输入形状
	std::vector<int64_t> input_shape_;
	// GPU加速标志（默认使用GPU）
	bool use_gpu = true;


	// 创建会话配置选项
	Ort::SessionOptions CreateSessionOptions(bool use_gpu);
	// 图像预处理：缩放、填充等操作
	std::tuple<cv::Mat, cv::Size, cv::Rect> PreprocessImage(cv::Mat image);
	// 准备输入张量：将图像数据转换为模型输入格式
	std::vector<float> PrepareInputTensor(const cv::Mat& image);
	// 处理输出：将模型输出转换为分割掩码
	cv::Mat ProcessOutput(Ort::Value& output_tensor, const cv::Rect& pad_roi, const cv::Size& original_size);

	// 初始化推理环境：加载模型并配置计算设备
	void InitONNXInferEnv(const std::wstring& model_path, bool use_gpu = true);
	// 执行推理：输入图像，返回分割结果
	cv::Mat Infer(const cv::Mat& image);


	//@brief 将QString安全转换为std::string
	std::string qstring_to_stdstring(const QString& qstr, const char* codecName);
	//将string类型转化为宽字符类型
	std::wstring utf8_to_wstring(const std::string& str);




	//获取当前系统时间，并转为字符串类型20250509195523
	std::string get_current_time();
	std::string get_current_time(std::time_t now);

	//获取当前系统时间，并转为字符串类型20250707,用于生成文件夹存储路径
	std::string get_current_date();
	std::string get_current_date(std::time_t now);

	//获取当前的系统时间，精确到秒，保留所有的分割符号,即正常格式的时间
	std::string get_current_time_NormalFormat();
	std::string get_current_time_NormalFormat(std::time_t now);


	//将图像转化为分辨率为1536*1152大小的图片（方法返回值必须为1536*1152大小的图片）
	cv::Mat process16BitImage_To1536_1152(cv::Mat& m_rawImage_16bit);
	//将图像转化为分辨率为2048*1152大小的图片（方法返回值必须为2048*1152大小的图片）
	cv::Mat process16BitImage_To2048_1152( cv::Mat& m_rawImage_16bit);

	// 应用窗宽窗位调整（灰度映射）将输入图像的像素值通过线性变换映射到[0,255]的显示范围
	void applyWindowing_OH(const cv::Mat& src, cv::Mat& dst, double windowWidth, double windowLevel);



	void applyWindowing_OH(const cv::Mat& src, cv::Mat& dst,double windowWidth, double windowLevel,bool autoWindow);
	void calculateAutoWindow(const cv::Mat& src,double& windowWidth,double& windowLevel);
	void calculateStatisticalWindow(const cv::Mat& src,double& windowWidth,double& windowLevel);
	void calculateHistogramWindow(const cv::Mat& src,double& windowWidth,double& windowLevel);
	void calculatePercentileWindow(const cv::Mat& src,double& windowWidth,double& windowLevel);
	void calculateAdaptiveWindow(const cv::Mat& src,double& windowWidth,double& windowLevel);
	void autoConvert16BitTo8Bit(const cv::Mat& src, cv::Mat& dst);




	//在单通道灰度图中查找最大轮廓及其外接矩形和收缩矩形
	bool findLargestContourROI(Mat grayImage, Rect& expandedRect, Rect& bounding_Rect, Rect& shrunkenRect);



	//以矩形中心为基准扩展矩形尺寸
	Rect expandRectFromCenter(const Rect& original, int widthExpand, int heightExpand);
	//带边界检查的矩形中心扩展
	Rect expandRectFromCenter(const Rect& original, int widthExpand, int heightExpand, const Size& imageSize);
	//从中心点收缩矩形区域，此函数从矩形的中心点对称收缩指定的宽度和高度，
	Rect shrinkRectFromCenter(const Rect& originalRect, int widthReduction, int heightReduction);


	cv::Mat extractROIFrom_8bit_Image(const cv::Mat& enhancedImage, const cv::Rect& maxRectROI);
	cv::Mat extractROIFrom_16bit_Image(const cv::Mat& enhancedImage, const cv::Rect& maxRectROI);


	//找到图像上的最大轮廓
	bool findLargestContour(Mat grayImage, vector<Point>& contour);


	//计算轮廓的最大外接矩形和最大内接矩形
	bool computeContourRectangles(const Mat& inputImage, const vector<Point>& contour,
		Rect& boundingRect, Rect& innerRect);


	Rect findLargestInnerSquare(const Mat& image, const vector<Point>& contour);
	Rect findLargestSquareByDistanceTransform(const Mat& mask);
	Rect findLargestSquareFromCenter(const Mat& mask, const Point& center, int maxRadius);
	bool isSquareInsideContour(const Mat& mask, int left, int top, int size);
	Rect findLargestSquareBySlidingWindow(const Mat& mask);


	Rect expandRectangleFromPoint(const Mat& mask, const Point& center, int maxRadius);
	bool isRectangleInsideContour(const Mat& mask, const Rect& rect);
	Rect findInnerRectByRotatingCalipers(const vector<Point>& contour);




	// 将8位单通道灰度图转换为三通道彩色图
	cv::Mat convertTo3Channels(const cv::Mat& input);
	// 定义并实现一个将文字绘制到图片上的方法
	Mat putTextOnImage(Mat src, string text, int x, int y);
	Mat putTextOnImage(Mat src, string text, double fontScale, int thickness, Scalar color, int x, int y);

	// 检测区域处理 
	void ImgSeparation(cv::Mat InputArray, cv::Mat& OutputArray, cv::Rect& rect);
	// 创建LUT查找表
	void LUT(cv::Mat InputArray, cv::Mat& OutputArray, float gamma);
	// 卷积增强
	void ConvEnhance(cv::Mat InputArray, cv::Mat& OutputArray);
	// change对比度增强函数
	void Change(cv::Mat InputArray, cv::Mat& OutputArray, int val, int x);
	// 区域融合
	void Imgfusion(cv::Rect rect, cv::Mat InputArray, cv::Mat& ScrImg);

	void processImages(cv::Mat& _8bitImage, cv::Mat& _enhancedImage_8bit, float gamma, int val, int a);

	// 图像裁切函数
	std::vector<cv::Mat> cropImage(const cv::Mat& inputImage, int startX, int startY);

	// 将裁切图片拼接到原始图像的指定位置
	void stitchCroppedImages(cv::Mat& baseImage, const std::vector<cv::Mat>& croppedImages, int startX, int startY, int cropWidth);



	// 通过SegImage计算负极极点
	std::vector<cv::Point> negativePointsFromSegImage(const cv::Mat& SegImage);
	// 通过SegImage计算正极极点
	std::vector<cv::Point>positivePointsFromSegImage(const cv::Mat& SegImage);



	//负极点位筛选，将不合理的点位删掉
	std::vector<cv::Point> filterNegativePoints(const std::vector<cv::Point>& negativePoints);
	//正极点位筛选，将不合理的点位删掉
	std::vector<cv::Point> filterPositivePoints(const std::vector<cv::Point>& positivePoints);




	// 绘制正负极顶点
	cv::Mat drawPointsOnImage(const cv::Mat& m_drawingImage_8bit, const std::vector<cv::Point>& negativePoints, cv::Scalar Color);

	// 将分割图像转换为单像素宽度的二值图像
	Mat segImgToOnePixelImage(Mat segImg, uchar targetPixelValue);

	//将分割图像二值化为单像素宽度的二值图像（非零像素设为255）
	Mat segImgTo255PixelImage(Mat segImg);

	//清除掩码图像的边界，并将掩码图像中的轮廓点转换为全局坐标
	vector<Point2f> processMaskAndConvertToGlobal(const Mat& mask, const Rect& ketiBigestRect);

	// 将double转换为指定小数位数的字符串（工业级实现）
	std::string doubleToString_Stream(double value, int precision);

	// 移除文件名后缀，只保留文件名
	string removeFileExtension(const string& filename);


	//正负极片数量对应时（负极刚好比正极多一片）
	std::pair<std::vector<double>, std::vector<double>> calculateDistanceVectors(
		const std::vector<cv::Point>& positivePoints,
		const std::vector<cv::Point>& negativePoints,
		double& calibrationCoefficient);

	//计算Y方向的差值作为距离
	std::pair<std::vector<double>, std::vector<double>> calculateDistanceVectors_Y_abs(
		const std::vector<cv::Point>& positivePoints,
		const std::vector<cv::Point>& negativePoints,
		double& calibrationCoefficient);


	double calculateDistance(const cv::Point& p1, const cv::Point& p2, double calibrationCoefficient);

	cv::Point findClosestPoint(const std::vector<cv::Point>& points, const cv::Point& reference,
		bool findLeft, bool& found);


	//正负极片数量对应时（负极刚好比正极多一片）,
	//中间部分的OH计算，以正极点为基准，找到距离其最近的一个负极点，计算OH;前向OH与左侧最近的负极点计算，后向OH与右侧最近的负极点计算
	std::pair<std::vector<double>, std::vector<double>> calculateDistanceVectors_SecondMethod_Y_abs(
		const std::vector<cv::Point>& positivePoints,
		const std::vector<cv::Point>& negativePoints,
		double& calibrationCoefficient, int First_OH_Calculate_PartNum, int Third_OH_Calculate_PartNum);


	// 如果m-n<1,即负极片比正极片少很多，则以负极片的数量去计算，多余的正极片不做处理（此处需要一个新的计算逻辑）
	std::pair<std::vector<double>, std::vector<double>> calculateDistanceVectors2(
		const std::vector<cv::Point>& positivePoints,
		const std::vector<cv::Point>& negativePoints,
		double& calibrationCoefficient);



	//重置部分过程或者结果数据
	void reSetDate();


public:
	// 基类接口实现
	bool initParas(QByteArray& config) override;
	bool reciveData(std::vector<cv::Mat>& images, QByteArray& data) override;
	bool run() override;
	bool sendData(std::vector<cv::Mat>& images, QByteArray& data) override;

	//数据编码
	bool encodeResultValue(QVector<std::string> append_value, QByteArray& resultByte);
	//数据解码
	QVector<QString> decodeResult(QByteArray byte);

};


// 动态库导出接口
extern "C" {
	Q_DECL_EXPORT OH_Detect_Station_Module* create();
	Q_DECL_EXPORT void destory(OH_Detect_Station_Module* instance);
}

#endif // OH_DETECT_STATION_MODULE_H




