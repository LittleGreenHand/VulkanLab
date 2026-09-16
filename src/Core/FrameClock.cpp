#include <GLFW/glfw3.h>
#include "FrameClock.h"

double FrameClock::Tick()
{
	double now = glfwGetTime();
	if (lastTime < 0.0)
	{
		lastTime = now;
		delta = 0.0;
		smoothedFps = 0.0;
		return 0.0;
	}
	delta = now - lastTime;
	lastTime = now;
	double instantFps = (delta > 0.0) ? (1.0 / delta) : 0.0;
	const double kSmooth = 0.9;
	smoothedFps = kSmooth * smoothedFps + (1.0 - kSmooth) * instantFps;
	return delta;
}
