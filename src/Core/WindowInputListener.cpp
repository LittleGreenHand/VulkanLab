#include "WindowInputListener.h"

#include <utility>

void WindowInputListener::SetCallbacks(Callbacks callbacks)
{
	m_callbacks = std::move(callbacks);
}

void WindowInputListener::ClearCallbacks()
{
	m_callbacks = {};
}

void WindowInputListener::NotifyKey(const WindowKeyEvent& event) const
{
	if (m_callbacks.key)
		m_callbacks.key(event);
}

void WindowInputListener::NotifyMouseButton(const WindowMouseButtonEvent& event) const
{
	if (m_callbacks.mouseButton)
		m_callbacks.mouseButton(event);
}

void WindowInputListener::NotifyCursorPosition(const WindowCursorPositionEvent& event) const
{
	if (m_callbacks.cursorPosition)
		m_callbacks.cursorPosition(event);
}

void WindowInputListener::NotifyScroll(const WindowScrollEvent& event) const
{
	if (m_callbacks.scroll)
		m_callbacks.scroll(event);
}

void WindowInputListener::NotifyFramebufferResize(const WindowFramebufferResizeEvent& event) const
{
	if (m_callbacks.framebufferResize)
		m_callbacks.framebufferResize(event);
}
