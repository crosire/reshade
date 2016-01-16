#include "Log.hpp"
#include "Input.hpp"

#include <algorithm>
#include <WindowsX.h>

namespace ReShade
{
	unsigned long Input::sEyeXInitialized = 0;
	std::unordered_map<HWND, HHOOK> Input::sRawInputHooks;
	std::unordered_map<HWND, std::unique_ptr<Input>> Input::sWindows;

	Input::Input(HWND hwnd) : _hwnd(hwnd), _keys(), _mousePosition(), _mouseButtons(), _gazePosition(), _eyeX(TX_EMPTY_HANDLE), _eyeXInteractor(TX_EMPTY_HANDLE), _eyeXInteractorSnapshot(TX_EMPTY_HANDLE)
	{
		_hookWindowProc = SetWindowsHookEx(WH_GETMESSAGE, &HandleWindowMessage, nullptr, GetWindowThreadProcessId(hwnd, nullptr));

		if (sEyeXInitialized && txCreateContext(&_eyeX, TX_FALSE) == TX_RESULT_OK)
		{
			LOG(INFO) << "Enabling connection with EyeX client ...";

			TX_GAZEPOINTDATAPARAMS params = { TX_GAZEPOINTDATAMODE_LIGHTLYFILTERED };

			txCreateGlobalInteractorSnapshot(_eyeX, "ReShade", &_eyeXInteractorSnapshot, &_eyeXInteractor);
			txCreateGazePointDataBehavior(_eyeXInteractor, &params);

			TX_TICKET ticket1 = TX_INVALID_TICKET, ticket2 = TX_INVALID_TICKET;

			txRegisterEventHandler(_eyeX, &ticket1, &HandleEyeXEvent, this);
			txRegisterConnectionStateChangedHandler(_eyeX, &ticket2, &HandleEyeXConnectionState, this);

			txEnableConnection(_eyeX);
		}
	}
	Input::~Input()
	{
		UnhookWindowsHookEx(_hookWindowProc);

		if (_eyeX != TX_EMPTY_HANDLE)
		{
			LOG(INFO) << "Disabling connection with EyeX client ...";

			txDisableConnection(_eyeX);

			txReleaseObject(&_eyeXInteractor);
			txReleaseObject(&_eyeXInteractorSnapshot);

			txShutdownContext(_eyeX, TX_CLEANUPTIMEOUT_DEFAULT, TX_FALSE);
			txReleaseContext(&_eyeX);
		}
	}

	void Input::LoadEyeX()
	{
		if (sEyeXInitialized != 0)
		{
			return;
		}

		const auto eyeXModule = LoadLibraryA("Tobii.EyeX.Client.dll");

		if (eyeXModule == nullptr)
		{
			return;
		}

		FreeLibrary(eyeXModule);

		LOG(INFO) << "Found Tobii EyeX library. Initializing ...";

		const TX_RESULT initresult = txInitializeEyeX(TX_EYEXCOMPONENTOVERRIDEFLAG_NONE, nullptr, nullptr, nullptr, nullptr);

		if (initresult == TX_RESULT_OK)
		{
			sEyeXInitialized++;
		}
		else
		{
			LOG(ERROR) << "EyeX initialization failed with error code " << initresult << ".";
		}
	}
	void Input::UnLoadEyeX()
	{
		if (sEyeXInitialized == 0 || --sEyeXInitialized != 0)
		{
			return;
		}

		LOG(INFO) << "Shutting down Tobii EyeX library ...";

		txUninitializeEyeX();
	}

	void Input::RegisterRawInputDevice(const RAWINPUTDEVICE &device)
	{
		const auto insert = sRawInputHooks.emplace(device.hwndTarget, nullptr);

		if (insert.second)
		{
			LOG(INFO) << "Starting raw input capture for window " << device.hwndTarget << " ...";

			insert.first->second = SetWindowsHookEx(WH_GETMESSAGE, &HandleWindowMessage, nullptr, GetWindowThreadProcessId(device.hwndTarget, nullptr));
		}
	}
	Input *Input::RegisterWindow(HWND hwnd)
	{
		const auto insert = sWindows.emplace(hwnd, nullptr);

		if (insert.second)
		{
			LOG(INFO) << "Starting legacy input capture for window " << hwnd << " ...";

			insert.first->second.reset(new Input(hwnd));
		}

		return insert.first->second.get();
	}
	void Input::Uninstall()
	{
		for (auto &it : sRawInputHooks)
		{
			UnhookWindowsHookEx(it.second);
		}

		sRawInputHooks.clear();
		sWindows.clear();
	}

	LRESULT CALLBACK Input::HandleWindowMessage(int nCode, WPARAM wParam, LPARAM lParam)
	{
		if (nCode < HC_ACTION || (wParam & PM_REMOVE) == 0)
		{
			return CallNextHookEx(nullptr, nCode, wParam, lParam);
		}

		auto details = *reinterpret_cast<MSG *>(lParam);

		if (details.hwnd == nullptr)
		{
			details.hwnd = GetActiveWindow();
		}

		auto it = sWindows.find(details.hwnd);

		if (it == sWindows.end())
		{
			if (sRawInputHooks.find(details.hwnd) != sRawInputHooks.end() && !sWindows.empty())
			{
				// This is a raw input message. Just reroute it to the first window for now, since it is rare to have more than one active at a time.
				it = sWindows.begin();
			}
			else
			{
				return CallNextHookEx(nullptr, nCode, wParam, lParam);
			}
		}

		Input &input = *it->second;

		ScreenToClient(input._hwnd, &details.pt);

		input._mousePosition.x = details.pt.x;
		input._mousePosition.y = details.pt.y;

		switch (details.message)
		{
			case WM_INPUT:
				if (GET_RAWINPUT_CODE_WPARAM(details.wParam) == RIM_INPUT)
				{
					UINT dataSize = sizeof(RAWINPUT);
					RAWINPUT data = { 0 };

					if (GetRawInputData(reinterpret_cast<HRAWINPUT>(details.lParam), RID_INPUT, &data, &dataSize, sizeof(RAWINPUTHEADER)) == UINT(-1))
					{
						break;
					}

					switch (data.header.dwType)
					{
						case RIM_TYPEMOUSE:
							if (data.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
							{
								input._mouseButtons[0] = 1;
							}
							else if (data.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
							{
								input._mouseButtons[0] = -1;
							}
							if (data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
							{
								input._mouseButtons[2] = 1;
							}
							else if (data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
							{
								input._mouseButtons[2] = -1;
							}
							if (data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
							{
								input._mouseButtons[1] = 1;
							}
							else if (data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
							{
								input._mouseButtons[1] = -1;
							}
							break;
						case RIM_TYPEKEYBOARD:
							if (data.data.keyboard.VKey != 0xFF)
							{
								input._keys[data.data.keyboard.VKey] = (data.data.keyboard.Flags & RI_KEY_BREAK) == 0 ? 1 : -1;
							}
							break;
					}
				}
				break;
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
				input._keys[details.wParam] = 1;
				break;
			case WM_KEYUP:
			case WM_SYSKEYUP:
				input._keys[details.wParam] = -1;
				break;
			case WM_LBUTTONDOWN:
				input._mouseButtons[0] = 1;
				break;
			case WM_LBUTTONUP:
				input._mouseButtons[0] = -1;
				break;
			case WM_RBUTTONDOWN:
				input._mouseButtons[2] = 1;
				break;
			case WM_RBUTTONUP:
				input._mouseButtons[2] = -1;
				break;
			case WM_MBUTTONDOWN:
				input._mouseButtons[1] = 1;
				break;
			case WM_MBUTTONUP:
				input._mouseButtons[1] = -1;
				break;
		}

		return CallNextHookEx(nullptr, nCode, wParam, lParam);
	}
	void TX_CALLCONVENTION Input::HandleEyeXEvent(TX_CONSTHANDLE hAsyncData, TX_USERPARAM userParam)
	{
		auto &input = *static_cast<Input *>(userParam);
		TX_HANDLE event = TX_EMPTY_HANDLE, behavior = TX_EMPTY_HANDLE;

		if (txGetAsyncDataContent(hAsyncData, &event) != TX_RESULT_OK)
		{
			return;
		}

		if (txGetEventBehavior(event, &behavior, TX_BEHAVIORTYPE_GAZEPOINTDATA) == TX_RESULT_OK)
		{
			TX_GAZEPOINTDATAEVENTPARAMS data;

			if (txGetGazePointDataEventParams(behavior, &data) == TX_RESULT_OK)
			{
				input._gazePosition.x = static_cast<LONG>(data.X);
				input._gazePosition.y = static_cast<LONG>(data.Y);

				ScreenToClient(input._hwnd, &input._gazePosition);
			}

			txReleaseObject(&behavior);
		}

		txReleaseObject(&event);
	}
	void TX_CALLCONVENTION Input::HandleEyeXConnectionState(TX_CONNECTIONSTATE connectionState, TX_USERPARAM userParam)
	{
		auto &input = *static_cast<Input *>(userParam);

		switch (connectionState)
		{
			case TX_CONNECTIONSTATE_CONNECTED:
				LOG(TRACE) << "EyeX client connected.";
				txCommitSnapshotAsync(input._eyeXInteractorSnapshot, nullptr, nullptr);
				break;
			case TX_CONNECTIONSTATE_DISCONNECTED:
				LOG(TRACE) << "EyeX client disconnected.";
				break;
		}
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
