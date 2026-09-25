#pragma once

#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

struct HandPoseResult;

struct CameraInfo
{
	// OpenCV 原生编号；直接使用时必须同时指定 Backend。
	cv::VideoCapture Capture;
	int DeviceIndex = -1;
	std::string DeviceName;
	std::string DevicePath;
	int Backend = cv::CAP_ANY;
	std::string BackendName;
	int refCount = 0; // 被 CameraDevice 引用的次数

	// 最近一次探测得到的默认参数，0 表示驱动未提供。
	int Width = 0;
	int Height = 0;
	double FPS = 0.0;
	bool Available = false;
	std::string Error;
};

class CameraDevice
{
public:
	CameraDevice() = default;
	~CameraDevice();
	CameraDevice(const CameraDevice&) = delete;
	CameraDevice& operator=(const CameraDevice&) = delete;

public:
	bool Open(int cameraIndex);
	void Close();
	bool Capture(cv::Mat& frame);
	// 在调用线程中处理 HighGUI 窗口事件，frame 应与 result 来自同一次推理。
	bool ShowHandPoseDebug(const cv::Mat& frame, const HandPoseResult& result, std::string& error);
	void CloseDebugWindow();
	bool IsOpened() const;
	int GetWidth() const;
	int GetHeight() const;
	double GetFPS() const;
public:
	int m_cameraIndex = -1;

public:
	static const std::vector<CameraInfo>& GetCameraDeviceList()
	{
		if (m_cameraDeviceList.empty())
			RefreshCameraList();
		return m_cameraDeviceList;
	}
	static int GetCameraDeviceCount() { return m_cameraDeviceList.size(); }
	static void RefreshCameraList();
	static std::vector<CameraInfo> m_cameraDeviceList;

private:
	std::string m_debugWindowName;
};
