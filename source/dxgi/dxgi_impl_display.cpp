/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "dxgi_impl_display.hpp"

reshade::dxgi::display_impl::display_impl(IDXGIOutput6 *output, DISPLAYCONFIG_PATH_TARGET_INFO *target) :
	api_object_impl(output)
{
	assert(output != nullptr && target != nullptr);

	if (output == nullptr || target == nullptr)
		return;

	_target_info = *target;

	com_ptr<IDXGIAdapter> adapter;
	com_ptr<IDXGIFactory> factory;

	if (SUCCEEDED(output->GetParent(IID_IDXGIAdapter,reinterpret_cast<void**>(&adapter))) &&
		SUCCEEDED(adapter->GetParent(IID_IDXGIFactory,reinterpret_cast<void**>(&factory))))
	{
		factory->QueryInterface<IDXGIFactory1>(&_parent);
	}

	_output = output;
	output->GetDesc1(&_desc);

	_refresh_rate = {
		_target_info.refreshRate.Numerator,
		_target_info.refreshRate.Denominator
	};

	DISPLAYCONFIG_TARGET_DEVICE_NAME
	target_name                  = {};
	target_name.header.adapterId = _target_info.adapterId;
	target_name.header.id        = _target_info.id;
	target_name.header.type      = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
	target_name.header.size      = sizeof  (DISPLAYCONFIG_TARGET_DEVICE_NAME);

	if (ERROR_SUCCESS == DisplayConfigGetDeviceInfo(&target_name.header))
	{
		_name = target_name.monitorFriendlyDeviceName;
		_path = target_name.monitorDevicePath;
	}

	DISPLAYCONFIG_SDR_WHITE_LEVEL
	sdr_white_level                  = {};
	sdr_white_level.header.adapterId = _target_info.adapterId;
	sdr_white_level.header.id        = _target_info.id;
	sdr_white_level.header.type      = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
	sdr_white_level.header.size      = sizeof         (DISPLAYCONFIG_SDR_WHITE_LEVEL);
	
	if (ERROR_SUCCESS == DisplayConfigGetDeviceInfo(&sdr_white_level.header))
	{
		_sdr_nits = (static_cast<float>(sdr_white_level.SDRWhiteLevel) / 1000.0f) * 80.0f;
	}

#if (NTDDI_VERSION >= NTDDI_WIN11_GA)
	// Prefer this API if the build environment and current OS support it, it can distinguish WCG from HDR
	DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO_2
	get_color_info2                  = {};
	get_color_info2.header.type      = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO_2;
	get_color_info2.header.size      = sizeof     (DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO_2);
	get_color_info2.header.adapterId = _target_info.adapterId;
	get_color_info2.header.id        = _target_info.id;

	if (ERROR_SUCCESS == DisplayConfigGetDeviceInfo(&get_color_info2.header))
	{
		_hdr_supported = get_color_info2.highDynamicRangeSupported && !get_color_info2.advancedColorLimitedByPolicy;
		_hdr_enabled   = get_color_info2.advancedColorActive && get_color_info2.highDynamicRangeUserEnabled && get_color_info2.activeColorMode == DISPLAYCONFIG_ADVANCED_COLOR_MODE_HDR;
	}

	else
#endif
	{
		DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO
		get_color_info                  = {};
		get_color_info.header.type      = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
		get_color_info.header.size      = sizeof     (DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO);
		get_color_info.header.adapterId = _target_info.adapterId;
		get_color_info.header.id        = _target_info.id;

		if (ERROR_SUCCESS == DisplayConfigGetDeviceInfo(&get_color_info.header))
		{
			_hdr_supported = get_color_info.advancedColorSupported;
			_hdr_enabled   = get_color_info.advancedColorEnabled;
		}
	}
};

const wchar_t* reshade::dxgi::display_impl::get_device_name() const
{
	return _desc.DeviceName;
};

const wchar_t* reshade::dxgi::display_impl::get_device_path() const
{
	return _path.c_str();
};

const wchar_t* reshade::dxgi::display_impl::get_display_name() const
{
	return _name.c_str();
};

reshade::api::display::monitor reshade::dxgi::display_impl::get_monitor() const
{
	return _desc.Monitor;
};

reshade::api::rect reshade::dxgi::display_impl::get_desktop_coords() const
{
	return { _desc.DesktopCoordinates.left,  _desc.DesktopCoordinates.top,
			 _desc.DesktopCoordinates.right, _desc.DesktopCoordinates.bottom };
};

uint32_t reshade::dxgi::display_impl::get_color_depth() const
{
	return _desc.BitsPerColor;
};

reshade::api::rational reshade::dxgi::display_impl::get_refresh_rate() const
{
	return _refresh_rate;
};

reshade::api::color_space reshade::dxgi::display_impl::get_color_space() const
{
	switch (_desc.ColorSpace)
	{
	case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
		return api::color_space::srgb_nonlinear;
	case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
		return api::color_space::extended_srgb_linear;
	case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
		return api::color_space::hdr10_st2084;
	case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020:
		return api::color_space::hdr10_hlg;
	default:
		return api::color_space::unknown;
	}
};

reshade::api::colorimetry reshade::dxgi::display_impl::get_colorimetry() const
{
	return api::colorimetry {
		_desc.RedPrimary[0], _desc.RedPrimary[1],
		_desc.GreenPrimary[0], _desc.GreenPrimary[1],
		_desc.BluePrimary[0], _desc.BluePrimary[1],
		_desc.WhitePoint[0], _desc.WhitePoint[1]
	};
};

reshade::api::luminance_levels reshade::dxgi::display_impl::get_luminance_caps() const
{
	return api::luminance_levels {
		_desc.MinLuminance,
		_desc.MaxLuminance,
		_desc.MaxFullFrameLuminance
	};
};

float reshade::dxgi::display_impl::get_sdr_white_nits() const
{
	return _sdr_nits;
}

bool reshade::dxgi::display_impl::is_current() const
{
	return _parent->IsCurrent();
}

bool reshade::dxgi::display_impl::is_hdr_supported() const
{
	return _hdr_supported;
}

bool reshade::dxgi::display_impl::is_hdr_enabled() const
{
	return _hdr_enabled;
}

bool reshade::dxgi::display_impl::enable_hdr(bool enable)
{
	if (!_hdr_supported)
		return false;

#if (NTDDI_VERSION >= NTDDI_WIN11_GA)
	// Prefer this API if the build environment and current OS support it, it can distinguish WCG from HDR
	DISPLAYCONFIG_SET_HDR_STATE
	set_hdr_state                  = {};
	set_hdr_state.header.type      = DISPLAYCONFIG_DEVICE_INFO_SET_HDR_STATE;
	set_hdr_state.header.size      = sizeof     (DISPLAYCONFIG_SET_HDR_STATE);
	set_hdr_state.header.adapterId = _target_info.adapterId;
	set_hdr_state.header.id        = _target_info.id;

	set_hdr_state.enableHdr = enable;

	if (ERROR_SUCCESS == DisplayConfigSetDeviceInfo(&set_hdr_state.header))
	{
		_hdr_enabled = set_hdr_state.enableHdr;
		return true;
	}

	else
#endif
	{
		DISPLAYCONFIG_SET_ADVANCED_COLOR_STATE
		set_advanced_color_state                  = {};
		set_advanced_color_state.header.type      = DISPLAYCONFIG_DEVICE_INFO_SET_ADVANCED_COLOR_STATE;
		set_advanced_color_state.header.size      = sizeof     (DISPLAYCONFIG_SET_ADVANCED_COLOR_STATE);
		set_advanced_color_state.header.adapterId = _target_info.adapterId;
		set_advanced_color_state.header.id        = _target_info.id;

		set_advanced_color_state.enableAdvancedColor = enable;

		if (ERROR_SUCCESS == DisplayConfigSetDeviceInfo(&set_advanced_color_state.header))
		{
			_hdr_enabled = set_advanced_color_state.enableAdvancedColor;
			return true;
		}
	}

	return false;
}

void reshade::dxgi::display_impl::flush_cache(reshade::api::display_cache& displays)
{
	// Detect spurious cache flushes to eliminate sending false display change events to add-ons.
	bool flush_required = displays.empty();
	if (!flush_required)
	{
		for (const auto &display : displays)
			if (flush_required = !display.second->is_current())
				break;

		// If nothing has actually changed, then we are done here...
		if (!flush_required)
			return;
	}

	displays.clear();

	std::vector<DISPLAYCONFIG_PATH_INFO> paths;
	std::vector<DISPLAYCONFIG_MODE_INFO> modes;
	UINT32 flags = QDC_ONLY_ACTIVE_PATHS | QDC_VIRTUAL_MODE_AWARE | QDC_VIRTUAL_REFRESH_RATE_AWARE;
	LONG result = ERROR_SUCCESS;

	do
	{
		UINT32 pathCount, modeCount;
		if (GetDisplayConfigBufferSizes(flags, &pathCount, &modeCount) != ERROR_SUCCESS)
			break;
	
		paths.resize(pathCount);
		modes.resize(modeCount);
	
		result = QueryDisplayConfig(flags, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
	
		paths.resize(pathCount);
		modes.resize(modeCount);
	} while (result == ERROR_INSUFFICIENT_BUFFER);
	
	const auto CreateDXGIFactory1 = reinterpret_cast<HRESULT(WINAPI *)(REFIID riid, void **ppFactory)>(
	GetProcAddress(GetModuleHandleW(L"dxgi.dll"), "CreateDXGIFactory1"));

	if (CreateDXGIFactory1 == nullptr)
		return;
	
	com_ptr<IDXGIFactory> factory;
	CreateDXGIFactory1(IID_IDXGIFactory, reinterpret_cast<void**>(&factory));
	
	if (factory != nullptr)
	{
		com_ptr<IDXGIAdapter> adapter;
		UINT adapter_idx = 0;
		while (SUCCEEDED(factory->EnumAdapters(adapter_idx++, &adapter)))
		{
			com_ptr<IDXGIOutput> output;
			UINT output_idx = 0;
			while (SUCCEEDED(adapter->EnumOutputs(output_idx++, &output)))
			{
				com_ptr<IDXGIOutput6> output6;
				if (SUCCEEDED(output->QueryInterface<IDXGIOutput6>(&output6)))
				{
					DXGI_OUTPUT_DESC1 desc = {};
					output6->GetDesc1(&desc);
	
					for (auto &path : paths)
					{
						DISPLAYCONFIG_SOURCE_DEVICE_NAME
						source_name                  = {};
						source_name.header.adapterId = path.sourceInfo.adapterId;
						source_name.header.id        = path.sourceInfo.id;
						source_name.header.type      = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
						source_name.header.size      = sizeof  (DISPLAYCONFIG_SOURCE_DEVICE_NAME);
	
						if (DisplayConfigGetDeviceInfo(&source_name.header) != ERROR_SUCCESS)
							continue;
	
						if (_wcsicmp(desc.DeviceName, source_name.viewGdiDeviceName))
							continue;

						displays.emplace(desc.Monitor,
							std::make_unique<reshade::dxgi::display_impl>(output6.get(),&path.targetInfo));

						break;
					}
				}
				output.reset();
			}
			adapter.reset();
		}
	}
}
