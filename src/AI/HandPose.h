#pragma once

#include <array>
#include <vector>

struct HandKeypoint
{
	float X = 0;
	float Y = 0;
	float Confidence = 0;
	// MediaPipe 相对腕部的深度，单位与图像坐标相同。
	float Z = 0;
};

struct HandPose
{
	// 原始相机图像的像素坐标，框采用左上角及宽高。
	float X = 0;
	float Y = 0;
	float Width = 0;
	float Height = 0;
	float Confidence = 0;
	int ClassId = 0;
	// wrist，以及 thumb、index、middle、ring、pinky 各自从根部到指尖的 4 个点。
	std::array<HandKeypoint, 21> Keypoints{};
	// MediaPipe 世界坐标，单位为米；右手概率范围 [0, 1]。
	std::array<std::array<float, 3>, 21> WorldKeypoints{};
	float RightHandProbability = 0;
	bool HasWorldKeypoints = false;
};

struct HandPoseResult
{
	int ImageWidth = 0;
	int ImageHeight = 0;
	std::vector<HandPose> Hands;
};
