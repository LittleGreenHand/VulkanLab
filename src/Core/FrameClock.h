#pragma once

class FrameClock 
{
public:
	static FrameClock& Get()
	{
		static FrameClock instance;
		return instance;
	}
	FrameClock(const FrameClock&) = delete;
	FrameClock& operator=(const FrameClock&) = delete;
	FrameClock(FrameClock&&) = delete;
	FrameClock& operator=(FrameClock&&) = delete;

public:
	// 每一帧调用一次，并返回距离上一次更新的时间差（秒）
	double Tick();
	double DeltaSeconds() { return delta; }
	double FPS() { return smoothedFps; }

private:
	FrameClock() = default;
	~FrameClock() = default;

	// 上次更新时间，单位秒
	double lastTime = -1.0;
	// 距离上次更新时间的时间差，单位秒
	double delta = 0.0;
	double smoothedFps = 0.0;
};
