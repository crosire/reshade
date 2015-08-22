#pragma once

#include <vector>
#include <unordered_map>
#include <windows.h>

class WindowWatcher
{
public:
	static void RegisterRawInputDevice(const RAWINPUTDEVICE &device);
	static void UnRegisterRawInputDevices();

	WindowWatcher(HWND hwnd);
	~WindowWatcher();

	inline bool GetKeyState(UINT keycode) const
	{
		assert(keycode < 255);

		return this->mKeys[keycode] != 0;
	}
	inline bool GetKeyJustPressed(UINT keycode) const
	{
		assert(keycode < 255);

		return this->mKeys[keycode] == 1;
	}
	inline const POINT &GetMousePosition() const
	{
		return this->mMousePos;
	}

	void NextFrame();

private:
	static LRESULT CALLBACK HookWindowProc(int nCode, WPARAM wParam, LPARAM lParam);

	HWND mWnd;
	HHOOK mHookWindowProc;
	unsigned char mKeys[256];
	POINT mMousePos;
	unsigned char mMouseButtons[3];
	static std::unordered_map<HWND, HHOOK> sRawInputHooks;
	static std::vector<std::pair<HWND, WindowWatcher *>> sWatchers;
};
