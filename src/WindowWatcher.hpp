#pragma once

#include <vector>
#include <windows.h>

class WindowWatcher
{
public:
	WindowWatcher(HWND hwnd);
	~WindowWatcher();

	bool GetKeyDown(UINT keycode) const;
	bool GetKeyState(UINT keycode) const;
	POINT GetMousePosition() const;

	void NextFrame();

private:
	static LRESULT CALLBACK HookMouse(int nCode, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK HookKeyboard(int nCode, WPARAM wParam, LPARAM lParam);

	HWND mWnd;
	HHOOK mHookMouse, mHookKeyboard;
	POINT mMousePos;
	unsigned char mKeys[256];
	static std::vector<std::pair<HWND, WindowWatcher *>> sWatchers;
};