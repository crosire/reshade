/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include "reshade_api_device.hpp"
#include <unordered_map>
#include <memory>

namespace reshade { namespace api
{
	/// <summary>
	/// Describes quantities defined precisely as the ratio of two integers, such as refresh rates.
	/// </summary>
	struct rational
	{
		uint32_t numerator = 0;
		uint32_t denomenator = 0;

		constexpr float as_float() const { return denomenator != 0 ? static_cast <float>(numerator)/static_cast<float>(denomenator) : 0.0f; }
	};

	/// <summary>
	/// Describes the colorimetry of a colorspace.
	/// </summary>
	struct colorimetry
	{
		float red [2] = { 0.0f, 0.0f };
		float green [2] = { 0.0f, 0.0f };
		float blue [2] = { 0.0f, 0.0f };
		float white [2] = { 0.0f, 0.0f };
	};

	/// <summary>
	/// Describes the dynamic range of a display or image.
	/// </summary>
	struct luminance_levels
	{
		float min_nits = 0.0f;
		float max_nits = 0.0f;
		float max_avg_nits = 0.0f;
	};

	/// <summary>
	/// An output display.
	/// <para>Functionally equivalent to a 'IDXGIOutput'.</para>
	/// </summary>
	struct __declspec(novtable) display : public api_object
	{
		/// <summary>
		/// Indicates if cached properties are valid or potentially stale (i.e. refresh rate or resolution have changed).
		/// </summary>
		/// <remarks>
		/// If this returns false, wait one frame and call runtime::get_active_display() to get updated data.
		/// </remarks>
		virtual bool is_current() const = 0;

		using monitor = void*;

		/// <summary>
		/// Gets the handle of the monitor this display encapsulates.
		/// </summary>
		virtual monitor get_monitor() const = 0;

		/// <summary>
		/// Gets the (GDI) device name (i.e. \\DISPLAY1) of the the monitor; do not use this as a persistent display identifier!
		/// </summary>
		/// <remarks>
		/// This device name is not valid as a persistent display identifier for storage in configuration files, it may not refer to the same display device after a reboot.
		/// </remarks>
		virtual const wchar_t* get_device_name() const = 0;

		/// <summary>
		/// Gets the device path (i.e. \\?\DISPLAY#DELA1E4#5&d93f871&0&UID33025#{e6f07b5f-ee97-4a90-b076-33f57bf4eaa7}) of the the monitor.
		/// </summary>
		/// <remarks>
		/// The device path uniquely identifies a specific monitor (down to its serial number and the port it is attached to) and remains valid across system reboots.
		/// This identifier is suitable for persistent user-defined display configuration, partial name matches can be used to identify instances of the same monitor when the port it attaches to is unimportant.
		/// </remarks>
		virtual const wchar_t* get_device_path() const = 0;

		/// <summary>
		/// Gets the human readable name (i.e. Dell AW3423DW) of the the monitor.
		/// </summary>
		virtual const wchar_t* get_display_name() const = 0;

		/// <summary>
		/// Gets the size and location of the display on the desktop.
		/// </summary>
		virtual rect get_desktop_coords() const = 0;

		/// <summary>
		/// Gets the current color depth of the display.
		/// </summary>
		virtual uint32_t get_color_depth() const = 0;

		/// <summary>
		/// Gets the current refresh rate of the display.
		/// </summary>
		virtual rational get_refresh_rate() const = 0;

		/// <summary>
		/// Gets the current color space used for desktop composition.
		/// </summary>
		/// <remarks>
		/// This is independent from swap chain colorspace, it identifies a display as HDR capable even when not rendering in HDR.
		/// </remarks>
		virtual color_space get_color_space() const = 0;

		/// <summary>
		/// Gets the display's reported native colorimetry (the data provided are often invalid or placeholders).
		/// </summary>
		/// <remarks>
		/// Users running Windows 11 or newer are encouraged to run "Windows HDR Calibration" to ensure the values reported to ReShade and games are accurate.
		/// </remarks>
		virtual colorimetry get_colorimetry() const = 0;

		/// <summary>
		/// Gets the display's light output capabilities (the data provided are often invalid or placeholders).
		/// </summary>
		/// <remarks>
		/// Users running Windows 11 or newer are encouraged to run "Windows HDR Calibration" to ensure the values reported to ReShade and games are accurate.
		/// </remarks>
		virtual luminance_levels get_luminance_caps() const = 0;

		/// <summary>
		/// Gets the desktop compositor's whitelevel for mapping SDR content to HDR.
		/// </summary>
		virtual float get_sdr_white_nits() const = 0;

		/// <summary>
		/// Checks if HDR is supported on the display, even if it is not currently enabled.
		/// </summary>
		virtual bool is_hdr_supported() const = 0;

		/// <summary>
		/// Checks if HDR is enabled on the display.
		/// </summary>
		virtual bool is_hdr_enabled() const = 0;

		/// <summary>
		/// Enables HDR on the display.
		/// </summary>
		/// <remarks>
		/// Be aware that this is a global display setting, and will not automatically revert to its original state when the application exits.
		/// </remarks>
		virtual bool enable_hdr(bool enable) = 0;
	};

	using display_cache = std::unordered_map<display::monitor, std::unique_ptr<display>>;
} }
