#include "Log.hpp"
#include "WindowWatcher.hpp"

std::unordered_map<HWND, WindowWatcher *> WindowWatcher::sWatchers;

WindowWatcher::WindowWatcher(HWND hwnd) : mWnd(hwnd), mKeys(), mMousePos()
{
	sWatchers.emplace(hwnd, this);

	const DWORD threadid = GetWindowThreadProcessId(hwnd, nullptr);
	this->mHookMouse = SetWindowsHookEx(WH_MOUSE, reinterpret_cast<HOOKPROC>(&HookMouse), nullptr, threadid);
	this->mHookKeyboard = SetWindowsHookEx(WH_KEYBOARD, reinterpret_cast<HOOKPROC>(&HookKeyboard), nullptr, threadid);
}
WindowWatcher::~WindowWatcher()
{
	sWatchers.erase(this->mWnd);

	UnhookWindowsHookEx(this->mHookMouse);
	UnhookWindowsHookEx(this->mHookKeyboard);
}

LRESULT CALLBACK WindowWatcher::HookMouse(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode != HC_ACTION)
	{
		return CallNextHookEx(nullptr, nCode, wParam, lParam);
	}

	MOUSEHOOKSTRUCT details = *reinterpret_cast<LPMOUSEHOOKSTRUCT>(lParam);
	const auto it = sWatchers.find(details.hwnd);

	if (it == sWatchers.end())
	{
		return CallNextHookEx(nullptr, nCode, wParam, lParam);
	}

	ScreenToClient(details.hwnd, &details.pt);

	WindowWatcher *const watcher = it->second;
	watcher->mMousePos = details.pt;

	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
LRESULT CALLBACK WindowWatcher::HookKeyboard(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode != HC_ACTION)
	{
		return CallNextHookEx(nullptr, nCode, wParam, lParam);
	}

	const HWND hwnd = GetActiveWindow();
	const auto it = sWatchers.find(hwnd);

	if (it == sWatchers.end())
	{
		return CallNextHookEx(nullptr, nCode, wParam, lParam);
	}

	const UINT keycode = static_cast<UINT>(wParam);
	const bool keydown = (lParam & 0x80000000) == 0;

	assert(keycode < 256);

	WindowWatcher *const watcher = it->second;
	watcher->mKeys[keycode] = keydown;

	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

bool WindowWatcher::GetKeyDown(UINT keycode) const
{
	if (keycode >= 256)
	{
		return false;
	}

	return this->mKeys[keycode] == 1;
}
bool WindowWatcher::GetKeyState(UINT keycode) const
{
	if (keycode >= 256)
	{
		return false;
	}

	return this->mKeys[keycode] != 0;
}
POINT WindowWatcher::GetMousePosition() const
{
	return this->mMousePos;
}

void WindowWatcher::NextFrame()
{
	for (unsigned int i = 0; i < 256; ++i)
	{
		if (this->mKeys[i] == 1)
		{
			this->mKeys[i] = 2;
		}
	}
}