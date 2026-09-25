#include "CameraDevice.h"
#include "AI/HandPose.h"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#include <dshow.h>
#include <wrl/client.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace
{
#if defined(_WIN32)
	using Microsoft::WRL::ComPtr;

	std::string ReadProperty(IPropertyBag* properties, const wchar_t* name)
	{
		VARIANT value;
		VariantInit(&value);
		std::string result;
		if (SUCCEEDED(properties->Read(name, &value, nullptr)) && value.vt == VT_BSTR && value.bstrVal)
		{
			const int length = static_cast<int>(SysStringLen(value.bstrVal));
			const int size = WideCharToMultiByte(CP_UTF8, 0, value.bstrVal, length, nullptr, 0, nullptr, nullptr);
			result.resize(size);
			if (size > 0)
				WideCharToMultiByte(CP_UTF8, 0, value.bstrVal, length, result.data(), size, nullptr, nullptr);
		}
		VariantClear(&value);
		return result;
	}

	std::vector<CameraInfo> EnumerateDevices()
	{
		std::vector<CameraInfo> cameras;
		const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
			return cameras;
		struct ComScope
		{
			bool Owned;
			~ComScope() { if (Owned) CoUninitialize(); }
		} scope{ SUCCEEDED(initialized) };
		ComPtr<ICreateDevEnum> devices;
		ComPtr<IEnumMoniker> enumerator;
		if (FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(devices.GetAddressOf()))) ||
			devices->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, enumerator.GetAddressOf(), 0) != S_OK)
			return cameras;
		ComPtr<IMoniker> moniker;
		int deviceIndex = 0;
		while (enumerator->Next(1, moniker.ReleaseAndGetAddressOf(), nullptr) == S_OK)
		{
			ComPtr<IPropertyBag> properties;
			if (FAILED(moniker->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(properties.GetAddressOf()))))
				continue;
			CameraInfo info;
			info.DeviceIndex = deviceIndex++;
			info.Backend = cv::CAP_DSHOW;
			info.BackendName = "DirectShow";
			info.DeviceName = ReadProperty(properties.Get(), L"FriendlyName");
			if (info.DeviceName.empty())
				info.DeviceName = ReadProperty(properties.Get(), L"Description");
			info.DevicePath = ReadProperty(properties.Get(), L"DevicePath");
			cameras.push_back(std::move(info));
		}
		return cameras;
	}
#elif defined(__linux__)
	std::vector<CameraInfo> EnumerateDevices()
	{
		std::vector<CameraInfo> cameras;
		std::error_code error;
		std::filesystem::directory_iterator iterator("/sys/class/video4linux", error);
		const std::filesystem::directory_iterator end;
		for (; !error && iterator != end; iterator.increment(error))
		{
			const std::string name = iterator->path().filename().string();
			if (name.size() <= 5 || name.compare(0, 5, "video") != 0 ||
				name.find_first_not_of("0123456789", 5) != std::string::npos)
				continue;
			CameraInfo info;
			try { info.DeviceIndex = std::stoi(name.substr(5)); }
			catch (const std::exception&) { continue; }
			info.DevicePath = "/dev/" + name;
			info.Backend = cv::CAP_V4L2;
			info.BackendName = "V4L2";
			std::ifstream label(iterator->path() / "name");
			std::getline(label, info.DeviceName);
			const int descriptor = open(info.DevicePath.c_str(), O_RDONLY | O_NONBLOCK);
			if (descriptor >= 0)
			{
				v4l2_capability capabilities{};
				const int status = ioctl(descriptor, VIDIOC_QUERYCAP, &capabilities);
				close(descriptor);
				if (status < 0)
					continue;
				const auto flags = capabilities.capabilities & V4L2_CAP_DEVICE_CAPS ? capabilities.device_caps : capabilities.capabilities;
				if (!(flags & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE)))
					continue;
			}
			cameras.push_back(std::move(info));
		}
		std::sort(cameras.begin(), cameras.end(), [](const CameraInfo& left, const CameraInfo& right) { return left.DeviceIndex < right.DeviceIndex; });
		return cameras;
	}
#else
#error Camera enumeration requires Windows, Linux or macOS.
#endif

	int GetDimension(double value)
	{
		return std::isfinite(value) && value > 0 && value <= std::numeric_limits<int>::max() ? static_cast<int>(value) : 0;
	}

	std::vector<CameraInfo> ScanCameras()
	{
		auto cameras = EnumerateDevices();
		for (auto& info : cameras)
		{
			if (info.DeviceName.empty())
				info.DeviceName = "Camera " + std::to_string(info.DeviceIndex);
			try
			{
				cv::VideoCapture capture;
				info.Available = capture.open(info.DeviceIndex, info.Backend);
				if (info.Available)
				{
					info.Width = GetDimension(capture.get(cv::CAP_PROP_FRAME_WIDTH));
					info.Height = GetDimension(capture.get(cv::CAP_PROP_FRAME_HEIGHT));
					const double fps = capture.get(cv::CAP_PROP_FPS);
					info.FPS = std::isfinite(fps) && fps > 0 ? fps : 0;
				}
				else
					info.Error = "Cannot open device (busy, disconnected, permission denied or backend unavailable).";
			}
			catch (const cv::Exception& exception)
			{
				info.Available = false;
				info.Error = exception.what();
			}
		}
		return cameras;
	}
}

std::vector<CameraInfo> CameraDevice::m_cameraDeviceList;
void CameraDevice::RefreshCameraList()
{
	m_cameraDeviceList.clear();
	m_cameraDeviceList = ScanCameras();
}

CameraDevice::~CameraDevice() { Close(); }

bool CameraDevice::Open(int cameraIndex)
{
	if (cameraIndex < 0 || cameraIndex >= m_cameraDeviceList.size())
		return false;
	if (cameraIndex == m_cameraIndex && IsOpened())
		return true;
	Close();
	auto& info = m_cameraDeviceList[cameraIndex];
	bool success = info.Capture.isOpened() || info.Capture.open(info.DeviceIndex, info.Backend);
	if (success)
	{
		m_cameraIndex = cameraIndex;
		info.refCount++;
	}
	return success;
}

void CameraDevice::Close() 
{ 
	CloseDebugWindow();
	if (m_cameraIndex < 0 || m_cameraIndex >= m_cameraDeviceList.size())
	{
		m_cameraIndex = -1;
		return;
	}
	auto& info = m_cameraDeviceList[m_cameraIndex];
	m_cameraIndex = -1;
	if (info.refCount > 0)
		info.refCount--;
	if (info.refCount <= 0)
		info.Capture.release();
}

bool CameraDevice::Capture(cv::Mat& frame)
{
	frame.release();
	if (!IsOpened())
		return false;
	return m_cameraDeviceList[m_cameraIndex].Capture.read(frame) && !frame.empty();
}

bool CameraDevice::ShowHandPoseDebug(const cv::Mat& frame, const HandPoseResult& result, std::string& error)
{
	error.clear();
	if (frame.empty() || frame.depth() != CV_8U || (frame.channels() != 1 && frame.channels() != 3 && frame.channels() != 4))
	{
		error = "Hand pose debug requires a nonempty 8-bit gray, BGR or BGRA frame.";
		return false;
	}
	if (result.ImageWidth != frame.cols || result.ImageHeight != frame.rows)
	{
		error = "Hand pose result dimensions do not match the debug frame.";
		return false;
	}
	try
	{
		cv::Mat debugFrame;
		if (frame.channels() == 1)
			cv::cvtColor(frame, debugFrame, cv::COLOR_GRAY2BGR);
		else if (frame.channels() == 4)
			cv::cvtColor(frame, debugFrame, cv::COLOR_BGRA2BGR);
		else
			debugFrame = frame.clone();
		const cv::Scalar colors[] = { { 80, 80, 255 }, { 80, 200, 255 }, { 80, 255, 100 }, { 255, 180, 80 }, { 255, 80, 200 } };
		auto point = [&](float x, float y)
		{
			return cv::Point(cvRound(std::clamp(x, 0.0f, static_cast<float>(frame.cols - 1))), cvRound(std::clamp(y, 0.0f, static_cast<float>(frame.rows - 1))));
		};
		for (const auto& hand : result.Hands)
		{
			if (std::isfinite(hand.X) && std::isfinite(hand.Y) && std::isfinite(hand.Width) && std::isfinite(hand.Height) && hand.Width > 0 && hand.Height > 0 && std::isfinite(hand.X + hand.Width) && std::isfinite(hand.Y + hand.Height))
			{
				const auto topLeft = point(hand.X, hand.Y);
				cv::rectangle(debugFrame, topLeft, point(hand.X + hand.Width, hand.Y + hand.Height), cv::Scalar(80, 255, 80), 2, cv::LINE_AA);
				cv::putText(debugFrame, cv::format("Hand %d %.2f", hand.ClassId, hand.Confidence), cv::Point(topLeft.x, std::max(16, topLeft.y - 6)), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(80, 255, 80), 1, cv::LINE_AA);
			}
			auto visible = [&](int index)
			{
				const auto& keypoint = hand.Keypoints[index];
				return std::isfinite(keypoint.X) && std::isfinite(keypoint.Y) && std::isfinite(keypoint.Confidence) && keypoint.Confidence >= 0.5f && keypoint.X >= 0 && keypoint.X < frame.cols && keypoint.Y >= 0 && keypoint.Y < frame.rows;
			};
			for (int finger = 0; finger < 5; ++finger)
			{
				int previous = 0;
				for (int joint = 1; joint <= 4; ++joint)
				{
					const int current = finger * 4 + joint;
					if (visible(previous) && visible(current))
						cv::line(debugFrame, point(hand.Keypoints[previous].X, hand.Keypoints[previous].Y), point(hand.Keypoints[current].X, hand.Keypoints[current].Y), colors[finger], 2, cv::LINE_AA);
					previous = current;
				}
			}
			for (int index = 0; index < 21; ++index)
				if (visible(index))
					cv::circle(debugFrame, point(hand.Keypoints[index].X, hand.Keypoints[index].Y), 3, index == 0 ? cv::Scalar(255, 255, 255) : colors[(index - 1) / 4], cv::FILLED, cv::LINE_AA);
		}
		if (m_debugWindowName.empty())
			m_debugWindowName = "Hand Pose Debug " + std::to_string(reinterpret_cast<std::uintptr_t>(this));
		cv::imshow(m_debugWindowName, debugFrame);
		cv::waitKey(1);
		return true;
	}
	catch (const cv::Exception& exception)
	{
		error = exception.what();
		return false;
	}
}

void CameraDevice::CloseDebugWindow()
{
	if (m_debugWindowName.empty())
		return;
	try { cv::destroyWindow(m_debugWindowName); }
	catch (const cv::Exception&) { }
	m_debugWindowName.clear();
}

bool CameraDevice::IsOpened() const 
{ 
	return m_cameraIndex >= 0 && m_cameraIndex < m_cameraDeviceList.size() && m_cameraDeviceList[m_cameraIndex].Capture.isOpened();
}
int CameraDevice::GetWidth() const 
{ 
	return IsOpened() ? GetDimension(m_cameraDeviceList[m_cameraIndex].Capture.get(cv::CAP_PROP_FRAME_WIDTH)) : 0; 
}
int CameraDevice::GetHeight() const 
{ 
	return IsOpened() ? GetDimension(m_cameraDeviceList[m_cameraIndex].Capture.get(cv::CAP_PROP_FRAME_HEIGHT)) : 0; 
}
double CameraDevice::GetFPS() const 
{ 
	return IsOpened() ? m_cameraDeviceList[m_cameraIndex].Capture.get(cv::CAP_PROP_FPS) : 0.0; 
}
