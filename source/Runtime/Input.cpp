#include "Log.hpp"
#include "Input.hpp"

#include <algorithm>
#include <windowsx.h>

namespace ReShade
{
	std::unordered_map<HWND, HHOOK> Input::sRawInputHooks;
	std::vector<std::pair<HWND, Input *>> Input::sWatchers;

	Input::Input(HWND hwnd) : _hwnd(hwnd), _keys(), _mousePosition(), _mouseButtons(), _eyePosition(), _eyeX(TX_EMPTY_HANDLE), _eyeXInteractorSnapshot(TX_EMPTY_HANDLE)
	{
		sWatchers.push_back(std::make_pair(hwnd, this));

		_hookWindowProc = SetWindowsHookEx(WH_GETMESSAGE, &HookWindowProc, nullptr, GetWindowThreadProcessId(hwnd, nullptr));

		if (txInitializeEyeX(TX_EYEXCOMPONENTOVERRIDEFLAG_NONE, NULL, NULL, NULL, NULL) != TX_RESULT_OK || txCreateContext(&_eyeX, TX_FALSE) != TX_RESULT_OK)
		{
			return;
		}

		TX_HANDLE interactor = TX_EMPTY_HANDLE;
		
		if (txCreateGlobalInteractorSnapshot(_eyeX, "ReShade", &_eyeXInteractorSnapshot, &interactor) != TX_RESULT_OK)
		{
			return;
		}

		TX_GAZEPOINTDATAPARAMS params = { TX_GAZEPOINTDATAMODE_LIGHTLYFILTERED };

		if (txCreateGazePointDataBehavior(interactor, &params) != TX_RESULT_OK)
		{
			txReleaseObject(&interactor);
			return;
		}

		txReleaseObject(&interactor);

		TX_TICKET hConnectionStateChangedTicket = TX_INVALID_TICKET, hEventHandlerTicket = TX_INVALID_TICKET;

		if (txRegisterConnectionStateChangedHandler(_eyeX, &hConnectionStateChangedTicket, &HandleEyeXConnectionState, this) != TX_RESULT_OK)
		{
			return;
		}
		if (txRegisterEventHandler(_eyeX, &hEventHandlerTicket, &HandleEyeXEvent, this) != TX_RESULT_OK)
		{
			return;
		}
		
		txEnableConnection(_eyeX);
	}
	Input::~Input()
	{
		sWatchers.erase(std::find_if(sWatchers.begin(), sWatchers.end(),
			[this](const std::pair<HWND, Input *> &it)
			{
				return it.second == this;
			}));

		UnhookWindowsHookEx(_hookWindowProc);

		if (_eyeX != TX_EMPTY_HANDLE)
		{
			txDisableConnection(_eyeX);

			if (_eyeXInteractorSnapshot != TX_EMPTY_HANDLE)
			{
				txReleaseObject(&_eyeXInteractorSnapshot);
			}

			txShutdownContext(_eyeX, TX_CLEANUPTIMEOUT_DEFAULT, TX_FALSE);
			txReleaseContext(&_eyeX);
			txUninitializeEyeX();
		}
	}

	LRESULT CALLBACK Input::HookWindowProc(int nCode, WPARAM wParam, LPARAM lParam)
	{
		if (nCode < HC_ACTION || (wParam & PM_REMOVE) == 0)
		{
			return CallNextHookEx(nullptr, nCode, wParam, lParam);
		}

		MSG details = *reinterpret_cast<LPMSG>(lParam);

		if (details.hwnd == nullptr)
		{
			details.hwnd = GetActiveWindow();
		}

		auto it = std::find_if(sWatchers.begin(), sWatchers.end(),
			[&details](const std::pair<HWND, Input *> &it)
			{
				return it.first == details.hwnd;
			});

		if (it == sWatchers.end())
		{
			if (!sWatchers.empty() && sRawInputHooks.find(details.hwnd) != sRawInputHooks.end())
			{
				// Just pick the first window watcher since it is rare to have more than one active window at a time.
				it = sWatchers.begin();
			}
			else
			{
				return CallNextHookEx(nullptr, nCode, wParam, lParam);
			}
		}

		ScreenToClient(it->second->_hwnd, &details.pt);

		it->second->_mousePosition.x = details.pt.x;
		it->second->_mousePosition.y = details.pt.y;

		switch (details.message)
		{
			case WM_INPUT:
				if (GET_RAWINPUT_CODE_WPARAM(details.wParam) == RIM_INPUT)
				{
					UINT dataSize = sizeof(RAWINPUT);
					RAWINPUT data = { 0 };

					if (GetRawInputData(reinterpret_cast<HRAWINPUT>(details.lParam), RID_INPUT, &data, &dataSize, sizeof(RAWINPUTHEADER)) == ~0u)
					{
						break;
					}

					switch (data.header.dwType)
					{
						case RIM_TYPEMOUSE:
							if (data.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
							{
								it->second->_mouseButtons[0] = 1;
							}
							else if (data.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
							{
								it->second->_mouseButtons[0] = -1;
							}
							if (data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
							{
								it->second->_mouseButtons[2] = 1;
							}
							else if (data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
							{
								it->second->_mouseButtons[2] = -1;
							}
							if (data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
							{
								it->second->_mouseButtons[1] = 1;
							}
							else if (data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
							{
								it->second->_mouseButtons[1] = -1;
							}
							break;
						case RIM_TYPEKEYBOARD:
							if (data.data.keyboard.VKey != 0xFF)
							{
								it->second->_keys[data.data.keyboard.VKey] = (data.data.keyboard.Flags & RI_KEY_BREAK) == 0 ? 1 : -1;
							}
							break;
					}
				}
				break;
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
				it->second->_keys[details.wParam] = 1;
				break;
			case WM_KEYUP:
			case WM_SYSKEYUP:
				it->second->_keys[details.wParam] = -1;
				break;
			case WM_LBUTTONDOWN:
				it->second->_mouseButtons[0] = 1;
				break;
			case WM_LBUTTONUP:
				it->second->_mouseButtons[0] = -1;
				break;
			case WM_RBUTTONDOWN:
				it->second->_mouseButtons[2] = 1;
				break;
			case WM_RBUTTONUP:
				it->second->_mouseButtons[2] = -1;
				break;
			case WM_MBUTTONDOWN:
				it->second->_mouseButtons[1] = 1;
				break;
			case WM_MBUTTONUP:
				it->second->_mouseButtons[1] = -1;
				break;
		}

		return CallNextHookEx(nullptr, nCode, wParam, lParam);
	}
	void TX_CALLCONVENTION Input::HandleEyeXEvent(TX_CONSTHANDLE hAsyncData, TX_USERPARAM userParam)
	{
		const auto watcher = static_cast<Input *>(userParam);

		TX_HANDLE event = TX_EMPTY_HANDLE, behavior = TX_EMPTY_HANDLE;

		txGetAsyncDataContent(hAsyncData, &event);

		if (txGetEventBehavior(event, &behavior, TX_BEHAVIORTYPE_GAZEPOINTDATA) != TX_RESULT_OK)
		{
			txReleaseObject(&event);
			return;
		}

		TX_GAZEPOINTDATAEVENTPARAMS eventParams;

		if (txGetGazePointDataEventParams(behavior, &eventParams) == TX_RESULT_OK)
		{
			POINT position;
			position.x = static_cast<LONG>(eventParams.X);
			position.y = static_cast<LONG>(eventParams.Y);

			ScreenToClient(watcher->_hwnd, &position);

			watcher->_eyePosition = position;

			OutputDebugStringA(std::string("EyeX: " + std::to_string(position.x) + ", EyeY: " + std::to_string(position.y) + "\n").c_str());
		}

		txReleaseObject(&behavior);
		txReleaseObject(&event);
	}
	void TX_CALLCONVENTION Input::HandleEyeXConnectionState(TX_CONNECTIONSTATE connectionState, TX_USERPARAM userParam)
	{
		const auto watcher = static_cast<Input *>(userParam);

		switch (connectionState)
		{
			case TX_CONNECTIONSTATE_CONNECTED:
				txCommitSnapshotAsync(watcher->_eyeXInteractorSnapshot, nullptr, nullptr);
				break;
			case TX_CONNECTIONSTATE_DISCONNECTED:
				break;
			case TX_CONNECTIONSTATE_TRYINGTOCONNECT:
				break;
			case TX_CONNECTIONSTATE_SERVERVERSIONTOOLOW:
			case TX_CONNECTIONSTATE_SERVERVERSIONTOOHIGH:
				break;
		}
	}

	void Input::RegisterRawInputDevice(const RAWINPUTDEVICE &device)
	{
		const auto insert = sRawInputHooks.insert(std::make_pair(device.hwndTarget, nullptr));

		if (insert.second)
		{
			insert.first->second = SetWindowsHookEx(WH_GETMESSAGE, &HookWindowProc, nullptr, GetWindowThreadProcessId(device.hwndTarget, nullptr));
		}
	}
	void Input::UnRegisterRawInputDevices()
	{
		for (auto &it : sRawInputHooks)
		{
			UnhookWindowsHookEx(it.second);
		}

		sRawInputHooks.clear();
	}

	void Input::NextFrame()
	{
		for (unsigned int i = 0; i < 256; i++)
		{
			if (_keys[i] == 1)
			{
				_keys[i] = 2;
			}
			else if (_keys[i] == -1)
			{
				_keys[i] = 0;
			}
		}

		for (unsigned int i = 0; i < 3; i++)
		{
			if (_mouseButtons[i] == 1)
			{
				_mouseButtons[i] = 2;
			}
			else if (_mouseButtons[i] == -1)
			{
				_mouseButtons[i] = 0;
			}
		}
	}
}
