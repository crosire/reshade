#include "log.hpp"
#include "input.hpp"

#include <algorithm>
#include <WindowsX.h>

namespace reshade
{
	unsigned long input::s_eyex_initialized = 0;
	std::unordered_map<HWND, HHOOK> input::s_raw_input_hooks;
	std::unordered_map<HWND, std::unique_ptr<input>> input::s_windows;

	input::input(HWND hwnd) : _hwnd(hwnd), _keys(), _mouse_position(), _mouse_buttons(), _gaze_position(), _eyex(TX_EMPTY_HANDLE), _eyex_interactor(TX_EMPTY_HANDLE), _eyex_interactor_snapshot(TX_EMPTY_HANDLE)
	{
		_hook_wndproc = SetWindowsHookEx(WH_GETMESSAGE, &handle_window_message, nullptr, GetWindowThreadProcessId(hwnd, nullptr));

		if (s_eyex_initialized && txCreateContext(&_eyex, TX_FALSE) == TX_RESULT_OK)
		{
			LOG(INFO) << "Enabling connection with EyeX client ...";

			TX_GAZEPOINTDATAPARAMS params = { TX_GAZEPOINTDATAMODE_LIGHTLYFILTERED };

			txCreateGlobalInteractorSnapshot(_eyex, "ReShade", &_eyex_interactor_snapshot, &_eyex_interactor);
			txCreateGazePointDataBehavior(_eyex_interactor, &params);

			TX_TICKET ticket1 = TX_INVALID_TICKET, ticket2 = TX_INVALID_TICKET;

			txRegisterEventHandler(_eyex, &ticket1, &handle_eyex_event, this);
			txRegisterConnectionStateChangedHandler(_eyex, &ticket2, &handle_eyex_connection_state, this);

			txEnableConnection(_eyex);
		}
	}
	input::~input()
	{
		UnhookWindowsHookEx(_hook_wndproc);

		if (_eyex != TX_EMPTY_HANDLE)
		{
			LOG(INFO) << "Disabling connection with EyeX client ...";

			txDisableConnection(_eyex);

			txReleaseObject(&_eyex_interactor);
			txReleaseObject(&_eyex_interactor_snapshot);

			txShutdownContext(_eyex, TX_CLEANUPTIMEOUT_DEFAULT, TX_FALSE);
			txReleaseContext(&_eyex);
		}
	}

	void input::load_eyex()
	{
		if (s_eyex_initialized != 0)
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
			s_eyex_initialized++;
		}
		else
		{
			LOG(ERROR) << "EyeX initialization failed with error code " << initresult << ".";
		}
	}
	void input::unload_eyex()
	{
		if (s_eyex_initialized == 0 || --s_eyex_initialized != 0)
		{
			return;
		}

		LOG(INFO) << "Shutting down Tobii EyeX library ...";

		txUninitializeEyeX();
	}

	void input::register_raw_input_device(const RAWINPUTDEVICE &device)
	{
		const auto insert = s_raw_input_hooks.emplace(device.hwndTarget, nullptr);

		if (insert.second)
		{
			LOG(INFO) << "Starting raw input capture for window " << device.hwndTarget << " ...";

			insert.first->second = SetWindowsHookEx(WH_GETMESSAGE, &handle_window_message, nullptr, GetWindowThreadProcessId(device.hwndTarget, nullptr));
		}
	}
	input *input::register_window(HWND hwnd)
	{
		const auto insert = s_windows.emplace(hwnd, nullptr);

		if (insert.second)
		{
			LOG(INFO) << "Starting legacy input capture for window " << hwnd << " ...";

			insert.first->second.reset(new input(hwnd));
		}

		return insert.first->second.get();
	}
	void input::uninstall()
	{
		for (auto &it : s_raw_input_hooks)
		{
			UnhookWindowsHookEx(it.second);
		}

		s_raw_input_hooks.clear();
		s_windows.clear();
	}

	LRESULT CALLBACK input::handle_window_message(int nCode, WPARAM wParam, LPARAM lParam)
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

		auto it = s_windows.find(details.hwnd);

		if (it == s_windows.end())
		{
			if (s_raw_input_hooks.find(details.hwnd) != s_raw_input_hooks.end() && !s_windows.empty())
			{
				// This is a raw input message. Just reroute it to the first window for now, since it is rare to have more than one active at a time.
				it = s_windows.begin();
			}
			else
			{
				return CallNextHookEx(nullptr, nCode, wParam, lParam);
			}
		}

		input &input = *it->second;

		ScreenToClient(input._hwnd, &details.pt);

		input._mouse_position.x = details.pt.x;
		input._mouse_position.y = details.pt.y;

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
								input._mouse_buttons[0] = 1;
							}
							else if (data.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
							{
								input._mouse_buttons[0] = -1;
							}
							if (data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
							{
								input._mouse_buttons[2] = 1;
							}
							else if (data.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
							{
								input._mouse_buttons[2] = -1;
							}
							if (data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
							{
								input._mouse_buttons[1] = 1;
							}
							else if (data.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
							{
								input._mouse_buttons[1] = -1;
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
				input._mouse_buttons[0] = 1;
				break;
			case WM_LBUTTONUP:
				input._mouse_buttons[0] = -1;
				break;
			case WM_RBUTTONDOWN:
				input._mouse_buttons[2] = 1;
				break;
			case WM_RBUTTONUP:
				input._mouse_buttons[2] = -1;
				break;
			case WM_MBUTTONDOWN:
				input._mouse_buttons[1] = 1;
				break;
			case WM_MBUTTONUP:
				input._mouse_buttons[1] = -1;
				break;
		}

		return CallNextHookEx(nullptr, nCode, wParam, lParam);
	}
	void TX_CALLCONVENTION input::handle_eyex_event(TX_CONSTHANDLE hAsyncData, TX_USERPARAM userParam)
	{
		auto &input = *static_cast<class input *>(userParam);
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
				input._gaze_position.x = static_cast<LONG>(data.X);
				input._gaze_position.y = static_cast<LONG>(data.Y);

				ScreenToClient(input._hwnd, &input._gaze_position);
			}

			txReleaseObject(&behavior);
		}

		txReleaseObject(&event);
	}
	void TX_CALLCONVENTION input::handle_eyex_connection_state(TX_CONNECTIONSTATE connectionState, TX_USERPARAM userParam)
	{
		auto &input = *static_cast<class input *>(userParam);

		switch (connectionState)
		{
			case TX_CONNECTIONSTATE_CONNECTED:
				LOG(TRACE) << "EyeX client connected.";
				txCommitSnapshotAsync(input._eyex_interactor_snapshot, nullptr, nullptr);
				break;
			case TX_CONNECTIONSTATE_DISCONNECTED:
				LOG(TRACE) << "EyeX client disconnected.";
				break;
		}
	}

	void input::next_frame()
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
			if (_mouse_buttons[i] == 1)
			{
				_mouse_buttons[i] = 2;
			}
			else if (_mouse_buttons[i] == -1)
			{
				_mouse_buttons[i] = 0;
			}
		}
	}
}
