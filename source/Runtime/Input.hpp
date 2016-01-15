#pragma once

#include <vector>
#include <unordered_map>
#include <assert.h>
#include <Windows.h>
#include <EyeX.h>

namespace ReShade
{
	class Input
	{
	public:
		static void RegisterRawInputDevice(const RAWINPUTDEVICE &device);
		static void UnRegisterRawInputDevices();

		Input(HWND hwnd);
		~Input();

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
		const POINT &GetGazePosition() const
		{
			return _gazePosition;
		}

		void NextFrame();

	private:
		static LRESULT CALLBACK HandleWindowMessage(int nCode, WPARAM wParam, LPARAM lParam);
		static void TX_CALLCONVENTION HandleEyeXEvent(TX_CONSTHANDLE hAsyncData, TX_USERPARAM userParam);
		static void TX_CALLCONVENTION HandleEyeXConnectionState(TX_CONNECTIONSTATE connectionState, TX_USERPARAM userParam);

		HWND _hwnd;
		HHOOK _hookWindowProc;
		signed char _keys[256];
		POINT _mousePosition;
		signed char _mouseButtons[3];
		POINT _gazePosition;
		TX_CONTEXTHANDLE _eyeX;
		TX_HANDLE _eyeXInteractor, _eyeXInteractorSnapshot;
		static std::unordered_map<HWND, HHOOK> sRawInputHooks;
		static std::vector<std::pair<HWND, Input *>> sWatchers;
	};
}
