#pragma once

#include <vector>
#include <unordered_map>
#include <assert.h>
#include <Windows.h>

namespace ReShade
{
	class WindowWatcher
	{
	public:
		static void RegisterRawInputDevice(const RAWINPUTDEVICE &device);
		static void UnRegisterRawInputDevices();

		WindowWatcher(HWND hwnd);
		~WindowWatcher();

		bool GetKeyState(UINT keycode) const
		{
			assert(keycode < 255);

			return _keys[keycode] > 0;
		}
		bool GetKeyJustPressed(UINT keycode) const
		{
			assert(keycode < 255);

			return _keys[keycode] == 1;
		}
		bool GetMouseButtonState(UINT button) const
		{
			assert(button < 3);

			return _mouseButtons[button] > 0;
		}
		bool GetMouseButtonJustPressed(UINT button) const
		{
			assert(button < 3);

			return _mouseButtons[button] == 1;
		}
		bool GetMouseButtonJustReleased(UINT button) const
		{
			assert(button < 3);

			return _mouseButtons[button] == -1;
		}
		const POINT &GetMousePosition() const
		{
			return _mousePosition;
		}

		void NextFrame();

	private:
		static LRESULT CALLBACK HookWindowProc(int nCode, WPARAM wParam, LPARAM lParam);

		HWND _hwnd;
		HHOOK _hookWindowProc;
		signed char _keys[256];
		POINT _mousePosition;
		signed char _mouseButtons[3];
		static std::unordered_map<HWND, HHOOK> sRawInputHooks;
		static std::vector<std::pair<HWND, WindowWatcher *>> sWatchers;
	};
}
