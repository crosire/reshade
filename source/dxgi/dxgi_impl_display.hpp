/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <dxgi1_6.h>
#include <xstring>
#include "com_ptr.hpp"
#include "reshade_api_object_impl.hpp"
#include "reshade_api_display.hpp"

namespace reshade::dxgi
{
	class display_impl : public api::api_object_impl<IDXGIOutput6 *, api::display>
	{
	public:
		explicit display_impl(IDXGIOutput6 *output, DISPLAYCONFIG_PATH_TARGET_INFO *target);

		/// <summary>
		/// Indicates if cached properties are valid or potentially stale (i.e. refresh rate or resolution have changed).
		/// </summary>
		/// <remarks>
		/// If this returns false, wait one frame and call runtime::get_active_display() to get updated data.
		/// </remarks>
		virtual bool is_current() const final;

		/// <summary>
		/// Gets the handle of the monitor this display encapsulates.
		/// </summary>
		virtual monitor get_monitor() const final;

		/// <summary>
		/// Gets the (GDI) device name (i.e. \\DISPLAY1) of the the monitor; do not use this as a persistent display identifier!
		/// </summary>
		/// <remarks>
		/// This device name is not valid as a persistent display identifier for storage in configuration files, it may not refer to the same display device after a reboot.
		/// </remarks>
		virtual const wchar_t* get_device_name() const final;

		/// <summary>
		/// Gets the device path (i.e. \\?\DISPLAY#DELA1E4#5&d93f871&0&UID33025#{e6f07b5f-ee97-4a90-b076-33f57bf4eaa7}) of the the monitor.
		/// </summary>
		/// <remarks>
		/// The device path uniquely identifies a specific monitor (down to its serial number and the port it is attached to) and remains valid across system reboots.
		/// This identifier is suitable for persistent user-defined display configuration, partial name matches can be used to identify instances of the same monitor when the port it attaches to is unimportant.
		/// </remarks>
		virtual const wchar_t* get_device_path() const final;

		/// <summary>
		/// Gets the human readable name (i.e. Dell AW3423DW) of the the monitor.
		/// </summary>
		virtual const wchar_t* get_display_name() const final;

		/// <summary>
		/// Gets the size and location of the display on the desktop.
		/// </summary>
		virtual api::rect get_desktop_coords() const final;

		/// <summary>
		/// Gets the current color depth of the display.
		/// </summary>
		virtual uint32_t get_color_depth() const final;

		/// <summary>
		/// Gets the current refresh rate of the display.
		/// </summary>
		virtual api::rational get_refresh_rate() const final;

		/// <summary>
		/// Gets the current color space used for desktop composition.
		/// </summary>
		/// <remarks>
		/// This is independent from swap chain colorspace, it identifies a display as HDR capable even when not rendering in HDR.
		/// </remarks>
		virtual api::color_space get_color_space() const final;

		/// <summary>
		/// Gets the display's reported native colorimetry (the data provided are often invalid or placeholders).
		/// </summary>
		/// <remarks>
		/// Users running Windows 11 or newer are encouraged to run "Windows HDR Calibration" to ensure the values reported to ReShade and games are accurate.
		/// </remarks>
		virtual api::colorimetry get_colorimetry() const final;

		/// <summary>
		/// Gets the display's light output capabilities (the data provided are often invalid or placeholders).
		/// </summary>
		/// <remarks>
		/// Users running Windows 11 or newer are encouraged to run "Windows HDR Calibration" to ensure the values reported to ReShade and games are accurate.
		/// </remarks>
		virtual api::luminance_levels get_luminance_caps() const final;

		/// <summary>
		/// Gets the desktop compositor's whitelevel for mapping SDR content to HDR.
		/// </summary>
		virtual float get_sdr_white_nits() const final;

		/// <summary>
		/// Checks if HDR is supported on the display, even if it is not currently enabled.
		/// </summary>
		virtual bool is_hdr_supported() const final;

		/// <summary>
		/// Checks if HDR is enabled on the display.
		/// </summary>
		virtual bool is_hdr_enabled() const final;

		/// <summary>
		/// Enables HDR on the display.
		/// </summary>
		/// <remarks>
		/// Be aware that this is a global display setting, and will not revert to its original state when the application exits.
		/// </remarks>
		virtual bool enable_hdr(bool enable) final;

		static void flush_cache(reshade::api::display_cache& displays);

	protected:
		DISPLAYCONFIG_PATH_TARGET_INFO _target_info = {};
		com_ptr<IDXGIFactory1> _parent;
		com_ptr<IDXGIOutput6> _output;
		DXGI_OUTPUT_DESC1 _desc;
		std::wstring _name = L"";
		std::wstring _path = L"";
		bool _hdr_supported = false;
		bool _hdr_enabled = false;
		float _sdr_nits = 0.0f;
		api::rational _refresh_rate = {0};
	};
};
