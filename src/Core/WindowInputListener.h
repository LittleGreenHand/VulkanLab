#pragma once

#include <functional>

struct WindowKeyEvent
{
	int key = 0;
	int scancode = 0;
	int action = 0;
	int mods = 0;
};

struct WindowMouseButtonEvent
{
	int button = 0;
	int action = 0;
	int mods = 0;
	double cursorX = 0.0;
	double cursorY = 0.0;
};

struct WindowCursorPositionEvent
{
	double x = 0.0;
	double y = 0.0;
};

struct WindowScrollEvent
{
	double xOffset = 0.0;
	double yOffset = 0.0;
};

struct WindowFramebufferResizeEvent
{
	int width = 0;
	int height = 0;
};

class WindowInputListener
{
public:
	struct Callbacks
	{
		std::function<void(const WindowKeyEvent&)> key;
		std::function<void(const WindowMouseButtonEvent&)> mouseButton;
		std::function<void(const WindowCursorPositionEvent&)> cursorPosition;
		std::function<void(const WindowScrollEvent&)> scroll;
		std::function<void(const WindowFramebufferResizeEvent&)> framebufferResize;
	};

	void SetCallbacks(Callbacks callbacks);
	void ClearCallbacks();

	void NotifyKey(const WindowKeyEvent& event) const;
	void NotifyMouseButton(const WindowMouseButtonEvent& event) const;
	void NotifyCursorPosition(const WindowCursorPositionEvent& event) const;
	void NotifyScroll(const WindowScrollEvent& event) const;
	void NotifyFramebufferResize(const WindowFramebufferResizeEvent& event) const;

private:
	Callbacks m_callbacks;
};
