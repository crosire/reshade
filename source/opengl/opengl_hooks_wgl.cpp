/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "opengl_impl_device.hpp"
#include "opengl_impl_device_context.hpp"
#include "opengl_impl_swapchain.hpp"
#include "opengl_impl_type_convert.hpp"
#include "opengl_hooks.hpp" // Fix name clashes with gl3w
#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "addon_manager.hpp"
#include "runtime_manager.hpp"
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <cstring> // std::strcmp
#include <algorithm> // std::any_of, std::find_if

#define gl gl3wProcs.gl

struct wgl_attribute
{
	enum
	{
		// Pixel format attributes

		WGL_NUMBER_PIXEL_FORMATS_ARB = 0x2000,
		WGL_DRAW_TO_WINDOW_ARB = 0x2001,
		WGL_DRAW_TO_BITMAP_ARB = 0x2002,
		WGL_ACCELERATION_ARB = 0x2003,
		WGL_NEED_PALETTE_ARB = 0x2004,
		WGL_NEED_SYSTEM_PALETTE_ARB = 0x2005,
		WGL_SWAP_LAYER_BUFFERS_ARB = 0x2006,
		WGL_SWAP_METHOD_ARB = 0x2007,
		WGL_NUMBER_OVERLAYS_ARB = 0x2008,
		WGL_NUMBER_UNDERLAYS_ARB = 0x2009,
		WGL_TRANSPARENT_ARB = 0x200A,
		WGL_TRANSPARENT_RED_VALUE_ARB = 0x2037,
		WGL_TRANSPARENT_GREEN_VALUE_ARB = 0x2038,
		WGL_TRANSPARENT_BLUE_VALUE_ARB = 0x2039,
		WGL_TRANSPARENT_ALPHA_VALUE_ARB = 0x203A,
		WGL_TRANSPARENT_INDEX_VALUE_ARB = 0x203B,
		WGL_SHARE_DEPTH_ARB = 0x200C,
		WGL_SHARE_STENCIL_ARB = 0x200D,
		WGL_SHARE_ACCUM_ARB = 0x200E,
		WGL_SUPPORT_GDI_ARB = 0x200F,
		WGL_SUPPORT_OPENGL_ARB = 0x2010,
		WGL_DOUBLE_BUFFER_ARB = 0x2011,
		WGL_STEREO_ARB = 0x2012,
		WGL_PIXEL_TYPE_ARB = 0x2013,
		WGL_COLOR_BITS_ARB = 0x2014,
		WGL_RED_BITS_ARB = 0x2015,
		WGL_RED_SHIFT_ARB = 0x2016,
		WGL_GREEN_BITS_ARB = 0x2017,
		WGL_GREEN_SHIFT_ARB = 0x2018,
		WGL_BLUE_BITS_ARB = 0x2019,
		WGL_BLUE_SHIFT_ARB = 0x201A,
		WGL_ALPHA_BITS_ARB = 0x201B,
		WGL_ALPHA_SHIFT_ARB = 0x201C,
		WGL_ACCUM_BITS_ARB = 0x201D,
		WGL_ACCUM_RED_BITS_ARB = 0x201E,
		WGL_ACCUM_GREEN_BITS_ARB = 0x201F,
		WGL_ACCUM_BLUE_BITS_ARB = 0x2020,
		WGL_ACCUM_ALPHA_BITS_ARB = 0x2021,
		WGL_DEPTH_BITS_ARB = 0x2022,
		WGL_STENCIL_BITS_ARB = 0x2023,
		WGL_AUX_BUFFERS_ARB = 0x2024,
		WGL_DRAW_TO_PBUFFER_ARB = 0x202D,
		WGL_SAMPLE_BUFFERS_ARB = 0x2041,
		WGL_SAMPLES_ARB = 0x2042,
		WGL_BIND_TO_TEXTURE_RGB_ARB = 0x2070,
		WGL_BIND_TO_TEXTURE_RGBA_ARB = 0x2071,
		WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB = 0x20A9,

		WGL_NO_ACCELERATION_ARB = 0x2025,
		WGL_GENERIC_ACCELERATION_ARB = 0x2026,
		WGL_FULL_ACCELERATION_ARB = 0x2027,
		WGL_SWAP_EXCHANGE_ARB = 0x2028,
		WGL_SWAP_COPY_ARB = 0x2029,
		WGL_SWAP_UNDEFINED_ARB = 0x202A,
		WGL_TYPE_RGBA_ARB = 0x202B,
		WGL_TYPE_COLORINDEX_ARB = 0x202C,

		// Context creation attributes

		WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091,
		WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092,
		WGL_CONTEXT_LAYER_PLANE_ARB = 0x2093,
		WGL_CONTEXT_FLAGS_ARB = 0x2094,
		WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126,

		WGL_CONTEXT_DEBUG_BIT_ARB = 0x0001,
		WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB = 0x0002,
		WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x00000001,
		WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB = 0x00000002,
	};

	int name, value;
};

DECLARE_HANDLE(HPBUFFERARB);

static bool s_hooks_installed = false;
static std::shared_mutex s_global_mutex;
static std::unordered_set<HDC> s_pbuffer_device_contexts;
static std::unordered_set<HGLRC> s_legacy_contexts;
static std::unordered_map<HGLRC, HGLRC> s_shared_contexts;
static std::unordered_map<HGLRC, reshade::opengl::device_context_impl *> s_opengl_contexts;
static std::vector<class wgl_swapchain *> s_opengl_swapchains;

extern thread_local reshade::opengl::device_context_impl *g_opengl_context;

// Set during OpenGL presentation, to avoid hooking internal D3D devices and layered DXGI swapchain
extern thread_local bool g_in_dxgi_runtime;

class wgl_device : public reshade::opengl::device_impl, public reshade::opengl::device_context_impl
{
	friend class wgl_swapchain;

public:
	wgl_device(HDC initial_hdc, HGLRC hglrc, bool compatibility_context) :
		device_impl(initial_hdc, hglrc, compatibility_context),
		device_context_impl(this, hglrc)
	{
#if RESHADE_ADDON
		reshade::load_addons();

		reshade::invoke_addon_event<reshade::addon_event::init_device>(this);

		GLint max_combined_texture_image_units = 0;
		gl.GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_combined_texture_image_units);
		GLint max_shader_storage_buffer_bindings = 0;
		gl.GetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &max_shader_storage_buffer_bindings);
		GLint max_uniform_buffer_bindings = 0;
		gl.GetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &max_uniform_buffer_bindings);
		GLint max_image_units = 0;
		gl.GetIntegerv(GL_MAX_IMAGE_UNITS, &max_image_units);
		GLint max_uniform_locations = 0;
		gl.GetIntegerv(GL_MAX_UNIFORM_LOCATIONS, &max_uniform_locations);

		const reshade::api::pipeline_layout_param global_pipeline_layout_params[7] = {
			reshade::api::descriptor_range { 0, 0, 0, static_cast<uint32_t>(max_combined_texture_image_units), reshade::api::shader_stage::all, 1, reshade::api::descriptor_type::sampler_with_resource_view },
			reshade::api::descriptor_range { 0, 0, 0, static_cast<uint32_t>(max_shader_storage_buffer_bindings), reshade::api::shader_stage::all, 1, reshade::api::descriptor_type::shader_storage_buffer },
			reshade::api::descriptor_range { 0, 0, 0, static_cast<uint32_t>(max_uniform_buffer_bindings), reshade::api::shader_stage::all, 1, reshade::api::descriptor_type::constant_buffer },
			reshade::api::descriptor_range { 0, 0, 0, static_cast<uint32_t>(max_image_units), reshade::api::shader_stage::all, 1, reshade::api::descriptor_type::unordered_access_view },
			/* Float uniforms */ reshade::api::constant_range { UINT32_MAX, 0, 0, static_cast<uint32_t>(max_uniform_locations) * 4, reshade::api::shader_stage::all },
			/* Signed integer uniforms */ reshade::api::constant_range { UINT32_MAX, 0, 0, static_cast<uint32_t>(max_uniform_locations) * 4, reshade::api::shader_stage::all },
			/* Unsigned integer uniforms */ reshade::api::constant_range { UINT32_MAX, 0, 0, static_cast<uint32_t>(max_uniform_locations) * 4, reshade::api::shader_stage::all },
		};
		device_impl::create_pipeline_layout(static_cast<uint32_t>(std::size(global_pipeline_layout_params)), global_pipeline_layout_params, &_global_pipeline_layout);
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline_layout>(this, static_cast<uint32_t>(std::size(global_pipeline_layout_params)), global_pipeline_layout_params, _global_pipeline_layout);

		reshade::invoke_addon_event<reshade::addon_event::init_command_list>(this);
		reshade::invoke_addon_event<reshade::addon_event::init_command_queue>(this);
#endif
	}
	~wgl_device()
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::destroy_command_queue>(this);
		reshade::invoke_addon_event<reshade::addon_event::destroy_command_list>(this);

		reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline_layout>(this, _global_pipeline_layout);
		device_impl::destroy_pipeline_layout(_global_pipeline_layout);

		reshade::invoke_addon_event<reshade::addon_event::destroy_device>(this);

		reshade::unload_addons();
#endif
	}

	auto get_pixel_format() const { return _pixel_format; }

	auto get_pipeline_layout() const { return _global_pipeline_layout; }

	reshade::api::resource_desc get_resource_desc(reshade::api::resource resource) const final
	{
		reshade::api::resource_desc desc = device_impl::get_resource_desc(resource);

		if (g_opengl_context != nullptr && (resource.handle >> 40) == GL_FRAMEBUFFER_DEFAULT)
		{
			// While each swap chain will use the same pixel format, the dimensions may differ, so pull them from the current context
			desc.texture.width = g_opengl_context->_default_fbo_width;
			desc.texture.height = g_opengl_context->_default_fbo_height;
		}

		return desc;
	}

private:
	reshade::api::pipeline_layout _global_pipeline_layout = {};
};
class wgl_device_context : public reshade::opengl::device_context_impl
{
public:
	wgl_device_context(wgl_device *device, HGLRC hglrc) :
		device_context_impl(device, hglrc)
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_command_list>(this);
		reshade::invoke_addon_event<reshade::addon_event::init_command_queue>(this);
#endif
	}
	~wgl_device_context()
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::destroy_command_queue>(this);
		reshade::invoke_addon_event<reshade::addon_event::destroy_command_list>(this);
#endif
	}
};

class wgl_swapchain : public reshade::opengl::swapchain_impl
{
public:
	static constexpr reshade::api::resource default_rt = reshade::opengl::make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK);
	static constexpr reshade::api::resource_view default_rtv = reshade::opengl::make_resource_view_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK);
	static constexpr reshade::api::resource default_ds = reshade::opengl::make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_DEPTH_STENCIL_ATTACHMENT);
	static constexpr reshade::api::resource_view default_dsv = reshade::opengl::make_resource_view_handle(GL_FRAMEBUFFER_DEFAULT, GL_DEPTH_STENCIL_ATTACHMENT);

	wgl_swapchain(wgl_device *device, HDC hdc) :
		swapchain_impl(device, hdc)
	{
	}
	~wgl_swapchain()
	{
		on_reset(false);

		reshade::destroy_effect_runtime(this);
	}

	void on_init(reshade::opengl::device_context_impl *context, unsigned int width, unsigned int height)
	{
		assert(width != 0 && height != 0);

		// Ensure 'get_resource_desc' returns the right dimensions
		context->update_default_framebuffer(width, height);

		if (_last_width == width && _last_height == height)
			return;

		const bool resize = !(_last_width == 0 && _last_height == 0);
		on_reset(resize);

		_last_width = width;
		_last_height = height;
		_init_effect_runtime = true;

#if RESHADE_ADDON
		const auto device = static_cast<wgl_device *>(get_device());

		reshade::invoke_addon_event<reshade::addon_event::init_swapchain>(this, resize);

		// Communicate implicit back buffer render target view and depth-stencil view to add-ons
		assert(default_rt == get_back_buffer());

		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			device,
			default_rt,
			reshade::api::resource_usage::render_target,
			reshade::api::resource_view_desc(device->_default_fbo_desc.texture.samples > 1 ? reshade::api::resource_view_type::texture_2d_multisample : reshade::api::resource_view_type::texture_2d, device->_default_fbo_desc.texture.format, 0, 1, 0, 1),
			default_rtv);

		if (device->_default_depth_format != reshade::api::format::unknown)
		{
			reshade::invoke_addon_event<reshade::addon_event::init_resource>(
				device,
				device->get_resource_desc(default_ds),
				nullptr,
				reshade::api::resource_usage::depth_stencil,
				default_ds);
			reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
				device,
				default_ds,
				reshade::api::resource_usage::depth_stencil,
				reshade::api::resource_view_desc(device->_default_fbo_desc.texture.samples > 1 ? reshade::api::resource_view_type::texture_2d_multisample : reshade::api::resource_view_type::texture_2d, device->_default_depth_format, 0, 1, 0, 1),
				default_dsv);
		}

		// Communicate default state to add-ons
		reshade::invoke_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(
			context,
			1, &default_rtv,
			device->_default_depth_format != reshade::api::format::unknown ? default_dsv : reshade::api::resource_view {});
#endif
	}
	void on_reset(bool resize)
	{
		if (_last_width == 0 && _last_height == 0)
			return;

		reshade::reset_effect_runtime(this);

		_last_width = 0;
		_last_height = 0;

#if RESHADE_ADDON
		const auto device = static_cast<wgl_device *>(get_device());

		if (device->_default_depth_format != reshade::api::format::unknown)
		{
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(device, default_dsv);
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(device, default_ds);
		}

		reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(device, default_rtv);

		reshade::invoke_addon_event<reshade::addon_event::destroy_swapchain>(this, resize);
#else
		UNREFERENCED_PARAMETER(resize);
#endif
	}
	void on_present(reshade::opengl::device_context_impl *context)
	{
		RECT window_rect = {};
		GetClientRect(static_cast<HWND>(get_hwnd()), &window_rect);

		const auto width = static_cast<unsigned int>(window_rect.right);
		const auto height = static_cast<unsigned int>(window_rect.bottom);

		if (width == 0 || height == 0)
		{
			on_reset(false);
			return;
		}

		// Do not use default FBO description of device to compare, since it may be shared and changed repeatedly by multiple swap chains
		if (width != _last_width || height != _last_height)
		{
			reshade::log::message(reshade::log::level::info, "Resizing device context %p to %ux%u ...", _orig, width, height);

			on_init(context, width, height);
		}

		if (_init_effect_runtime)
		{
			reshade::create_effect_runtime(this, context);
			reshade::init_effect_runtime(this);

			_init_effect_runtime = false;
		}

#if RESHADE_ADDON
		// Behave as if immediate command list is flushed
		reshade::invoke_addon_event<reshade::addon_event::execute_command_list>(context, context);

		reshade::invoke_addon_event<reshade::addon_event::present>(context, this, nullptr, nullptr, 0, nullptr);
#endif

		// Assume that the correct OpenGL context is still current here
		reshade::present_effect_runtime(this);

#ifndef NDEBUG
		GLenum type = GL_NONE; char message[512] = "";
		while (gl.GetDebugMessageLog(1, 512, nullptr, &type, nullptr, nullptr, nullptr, message))
			if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)
				OutputDebugStringA(message), OutputDebugStringA("\n");
#endif
	}

private:
	unsigned int _last_width = 0;
	unsigned int _last_height = 0;
	bool _init_effect_runtime = true;
};

reshade::api::pipeline_layout get_opengl_pipeline_layout()
{
	return static_cast<wgl_device *>(g_opengl_context->get_device())->get_pipeline_layout();
}

extern "C" int   WINAPI wglChoosePixelFormat(HDC hdc, const PIXELFORMATDESCRIPTOR *ppfd)
{
	reshade::log::message(reshade::log::level::info, "Redirecting wglChoosePixelFormat(hdc = %p, ppfd = %p) ...", hdc, ppfd);

	assert(ppfd != nullptr);

	reshade::log::message(reshade::log::level::info, "> Dumping pixel format descriptor:");
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
	reshade::log::message(reshade::log::level::info, "  | Name                                    | Value                                   |");
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
	reshade::log::message(reshade::log::level::info, "  | Flags                                   |"                              " %-#39lx |", ppfd->dwFlags);
	reshade::log::message(reshade::log::level::info, "  | ColorBits                               |"                                " %-39u |", static_cast<unsigned int>(ppfd->cColorBits));
	reshade::log::message(reshade::log::level::info, "  | DepthBits                               |"                                " %-39u |", static_cast<unsigned int>(ppfd->cDepthBits));
	reshade::log::message(reshade::log::level::info, "  | StencilBits                             |"                                " %-39u |", static_cast<unsigned int>(ppfd->cStencilBits));
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");

	if (ppfd->iLayerType != PFD_MAIN_PLANE || ppfd->bReserved != 0)
	{
		reshade::log::message(reshade::log::level::warning, "Layered OpenGL contexts are not supported.");
	}
	else if ((ppfd->dwFlags & PFD_DOUBLEBUFFER) == 0)
	{
		reshade::log::message(reshade::log::level::warning, "Single buffered OpenGL contexts are not supported.");
	}

	// Note: Windows calls into 'wglDescribePixelFormat' repeatedly from this, so make sure it reports correct results
	const int format = reshade::hooks::call(wglChoosePixelFormat)(hdc, ppfd);
	if (format != 0)
		reshade::log::message(reshade::log::level::info, "Returning pixel format: %d", format);
	else
		reshade::log::message(reshade::log::level::warning, "wglChoosePixelFormat failed with error code %lu.", GetLastError() & 0xFFFF);

	return format;
}
		   BOOL  WINAPI wglChoosePixelFormatARB(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting wglChoosePixelFormatARB(hdc = %p, piAttribIList = %p, pfAttribFList = %p, nMaxFormats = %u, piFormats = %p, nNumFormats = %p) ...",
		hdc, piAttribIList, pfAttribFList, nMaxFormats, piFormats, nNumFormats);

	bool layerplanes = false, doublebuffered = false;

	reshade::log::message(reshade::log::level::info,"> Dumping attributes:");
	reshade::log::message(reshade::log::level::info,"  +-----------------------------------------+-----------------------------------------+");
	reshade::log::message(reshade::log::level::info,"  | Attribute                               | Value                                   |");
	reshade::log::message(reshade::log::level::info,"  +-----------------------------------------+-----------------------------------------+");

	for (const int *attrib = piAttribIList; attrib != nullptr && *attrib != 0; attrib += 2)
	{
		switch (attrib[0])
		{
		case wgl_attribute::WGL_DRAW_TO_WINDOW_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_DRAW_TO_WINDOW_ARB                  | %-39s |", attrib[1] != FALSE ? "TRUE" : "FALSE");
			break;
		case wgl_attribute::WGL_DRAW_TO_BITMAP_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_DRAW_TO_BITMAP_ARB                  | %-39s |", attrib[1] != FALSE ? "TRUE" : "FALSE");
			break;
		case wgl_attribute::WGL_ACCELERATION_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_ACCELERATION_ARB                    | %-#39x |", static_cast<unsigned int>(attrib[1]));
			break;
		case wgl_attribute::WGL_SWAP_LAYER_BUFFERS_ARB:
			layerplanes = layerplanes || attrib[1] != FALSE;
			reshade::log::message(reshade::log::level::info, "  | WGL_SWAP_LAYER_BUFFERS_ARB              | %-39s |", attrib[1] != FALSE ? "TRUE" : "FALSE");
			break;
		case wgl_attribute::WGL_SWAP_METHOD_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_SWAP_METHOD_ARB                     | %-#39x |", static_cast<unsigned int>(attrib[1]));
			break;
		case wgl_attribute::WGL_NUMBER_OVERLAYS_ARB:
			layerplanes = layerplanes || attrib[1] != 0;
			reshade::log::message(reshade::log::level::info, "  | WGL_NUMBER_OVERLAYS_ARB                 | %-39d |", attrib[1]);
			break;
		case wgl_attribute::WGL_NUMBER_UNDERLAYS_ARB:
			layerplanes = layerplanes || attrib[1] != 0;
			reshade::log::message(reshade::log::level::info, "  | WGL_NUMBER_UNDERLAYS_ARB                | %-39d |", attrib[1]);
			break;
		case wgl_attribute::WGL_SUPPORT_GDI_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_SUPPORT_GDI_ARB                     | %-39s |", attrib[1] != FALSE ? "TRUE" : "FALSE");
			break;
		case wgl_attribute::WGL_SUPPORT_OPENGL_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_SUPPORT_OPENGL_ARB                  | %-39s |", attrib[1] != FALSE ? "TRUE" : "FALSE");
			break;
		case wgl_attribute::WGL_DOUBLE_BUFFER_ARB:
			doublebuffered = attrib[1] != FALSE;
			reshade::log::message(reshade::log::level::info, "  | WGL_DOUBLE_BUFFER_ARB                   | %-39s |", attrib[1] != FALSE ? "TRUE" : "FALSE");
			break;
		case wgl_attribute::WGL_STEREO_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_STEREO_ARB                          | %-39s |", attrib[1] != FALSE ? "TRUE" : "FALSE");
			break;
		case wgl_attribute::WGL_PIXEL_TYPE_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_PIXEL_TYPE_ARB                      | %-#39x |", static_cast<unsigned int>(attrib[1]));
			break;
		case wgl_attribute::WGL_COLOR_BITS_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_COLOR_BITS_ARB                      | %-39d |", attrib[1]);
			break;
		case wgl_attribute::WGL_RED_BITS_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_RED_BITS_ARB                        | %-39d |", attrib[1]);
			break;
		case wgl_attribute::WGL_GREEN_BITS_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_GREEN_BITS_ARB                      | %-39d |", attrib[1]);
			break;
		case wgl_attribute::WGL_BLUE_BITS_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_BLUE_BITS_ARB                       | %-39d |", attrib[1]);
			break;
		case wgl_attribute::WGL_ALPHA_BITS_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_ALPHA_BITS_ARB                      | %-39d |", attrib[1]);
			break;
		case wgl_attribute::WGL_DEPTH_BITS_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_DEPTH_BITS_ARB                      | %-39d |", attrib[1]);
			break;
		case wgl_attribute::WGL_STENCIL_BITS_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_STENCIL_BITS_ARB                    | %-39d |", attrib[1]);
			break;
		case wgl_attribute::WGL_DRAW_TO_PBUFFER_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_DRAW_TO_PBUFFER_ARB                 | %-39s |", attrib[1] != FALSE ? "TRUE" : "FALSE");
			break;
		case wgl_attribute::WGL_SAMPLE_BUFFERS_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_SAMPLE_BUFFERS_ARB                  | %-39d |", attrib[1]);
			break;
		case wgl_attribute::WGL_SAMPLES_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_SAMPLES_ARB                         | %-39d |", attrib[1]);
			break;
		case wgl_attribute::WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB        | %-39s |", attrib[1] != FALSE ? "TRUE" : "FALSE");
			break;
		default:
			reshade::log::message(reshade::log::level::info, "  | %-#39x | %-39d |", static_cast<unsigned int>(attrib[0]), attrib[1]);
			break;
		}
	}

	for (const FLOAT *attrib = pfAttribFList; attrib != nullptr && *attrib != 0.0f; attrib += 2)
	{
		reshade::log::message(reshade::log::level::info, "  | %-#39x | %-39f |", static_cast<unsigned int>(attrib[0]), attrib[1]);
	}

	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");

	if (layerplanes)
	{
		reshade::log::message(reshade::log::level::warning, "Layered OpenGL contexts are not supported.");
	}
	else if (!doublebuffered)
	{
		reshade::log::message(reshade::log::level::warning, "Single buffered OpenGL contexts are not supported.");
	}

	if (!reshade::hooks::call(wglChoosePixelFormatARB)(hdc, piAttribIList, pfAttribFList, nMaxFormats, piFormats, nNumFormats))
	{
		reshade::log::message(reshade::log::level::warning, "wglChoosePixelFormatARB failed with error code %lu.", GetLastError() & 0xFFFF);
		return FALSE;
	}

	assert(nNumFormats != nullptr);
	if (nMaxFormats > *nNumFormats)
		nMaxFormats = *nNumFormats;

	std::string formats;
	for (UINT i = 0; i < nMaxFormats; ++i)
	{
		assert(piFormats[i] != 0);
		formats += ' ' + std::to_string(piFormats[i]);
	}

	reshade::log::message(reshade::log::level::info, "Returning pixel format(s):%s", formats.c_str());

	return TRUE;
}
extern "C" int   WINAPI wglGetPixelFormat(HDC hdc)
{
	static const auto trampoline = reshade::hooks::call(wglGetPixelFormat);
	return trampoline(hdc);
}
		   BOOL  WINAPI wglGetPixelFormatAttribivARB(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, int *piValues)
{
	if (iLayerPlane != 0)
	{
		reshade::log::message(reshade::log::level::warning, "Access to layer plane at index %d is unsupported.", iLayerPlane);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	return reshade::hooks::call(wglGetPixelFormatAttribivARB)(hdc, iPixelFormat, 0, nAttributes, piAttributes, piValues);
}
		   BOOL  WINAPI wglGetPixelFormatAttribfvARB(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, FLOAT *pfValues)
{
	if (iLayerPlane != 0)
	{
		reshade::log::message(reshade::log::level::warning, "Access to layer plane at index %d is unsupported.", iLayerPlane);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	return reshade::hooks::call(wglGetPixelFormatAttribfvARB)(hdc, iPixelFormat, 0, nAttributes, piAttributes, pfValues);
}
extern "C" BOOL  WINAPI wglSetPixelFormat(HDC hdc, int iPixelFormat, const PIXELFORMATDESCRIPTOR *ppfd)
{
	reshade::log::message(reshade::log::level::info, "Redirecting wglSetPixelFormat(hdc = %p, iPixelFormat = %d, ppfd = %p) ...", hdc, iPixelFormat, ppfd);

#if RESHADE_ADDON
	reshade::load_addons();

	const HWND hwnd = WindowFromDC(hdc);

	PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd) };
	DescribePixelFormat(hdc, iPixelFormat, sizeof(pfd), &pfd);

	reshade::api::swapchain_desc desc = {};
	desc.back_buffer.type = reshade::api::resource_type::surface;
	desc.back_buffer.texture.format = reshade::opengl::convert_pixel_format(pfd);
	desc.back_buffer.heap = reshade::api::memory_heap::gpu_only;
	desc.back_buffer.usage = reshade::api::resource_usage::render_target | reshade::api::resource_usage::copy_dest | reshade::api::resource_usage::copy_source | reshade::api::resource_usage::resolve_dest;
	desc.back_buffer_count = 1;

	if (hwnd != nullptr)
	{
		assert((pfd.dwFlags & PFD_DRAW_TO_WINDOW) != 0);

		RECT window_rect = {};
		GetClientRect(hwnd, &window_rect);

		desc.back_buffer.texture.width = window_rect.right;
		desc.back_buffer.texture.height = window_rect.bottom;
	}

	if (pfd.dwFlags & PFD_DOUBLEBUFFER)
		desc.back_buffer_count = 2;
	if (pfd.dwFlags & PFD_STEREO)
		desc.back_buffer.texture.depth_or_layers = 2;

	if (s_hooks_installed && reshade::hooks::call(wglGetPixelFormatAttribivARB) != nullptr)
	{
		int attrib_names[2] = { wgl_attribute::WGL_SAMPLES_ARB, wgl_attribute::WGL_SWAP_METHOD_ARB }, attrib_values[2] = {};
		if (reshade::hooks::call(wglGetPixelFormatAttribivARB)(hdc, iPixelFormat, 0, 2, attrib_names, attrib_values))
		{
			if (attrib_values[0] != 0)
				desc.back_buffer.texture.samples = static_cast<uint16_t>(attrib_values[0]);
			desc.present_mode = attrib_values[1];
		}
	}

	desc.present_flags = pfd.dwFlags;

	const auto wglSwapIntervalEXT = reinterpret_cast<BOOL(WINAPI *)(int interval)>(reshade::hooks::call(wglGetProcAddress)("wglSwapIntervalEXT"));
	const auto wglGetSwapIntervalEXT = reinterpret_cast<int(WINAPI *)()>(reshade::hooks::call(wglGetProcAddress)("wglGetSwapIntervalEXT"));
	if (wglGetSwapIntervalEXT != nullptr)
	{
		desc.sync_interval = wglGetSwapIntervalEXT();
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_swapchain>(reshade::api::device_api::opengl, desc, hwnd))
	{
		reshade::opengl::convert_pixel_format(desc.back_buffer.texture.format, pfd);

		pfd.dwFlags = desc.present_flags & ~(PFD_DOUBLEBUFFER | PFD_STEREO);

		if (desc.back_buffer_count > 1)
			pfd.dwFlags |= PFD_DOUBLEBUFFER;
		if (desc.back_buffer.texture.depth_or_layers == 2)
			pfd.dwFlags |= PFD_STEREO;

		if (s_hooks_installed && reshade::hooks::call(wglChoosePixelFormatARB) != nullptr)
		{
			int i = 14;
			wgl_attribute attribs[17] = {
				{ wgl_attribute::WGL_DRAW_TO_WINDOW_ARB, (pfd.dwFlags & PFD_DRAW_TO_WINDOW) != 0 },
				{ wgl_attribute::WGL_DRAW_TO_BITMAP_ARB, (pfd.dwFlags & PFD_DRAW_TO_BITMAP) != 0 },
				{ wgl_attribute::WGL_SUPPORT_GDI_ARB, (pfd.dwFlags & PFD_SUPPORT_GDI) != 0 },
				{ wgl_attribute::WGL_SUPPORT_OPENGL_ARB, (pfd.dwFlags & PFD_SUPPORT_OPENGL) != 0 },
				{ wgl_attribute::WGL_DOUBLE_BUFFER_ARB, (pfd.dwFlags & PFD_DOUBLEBUFFER) != 0 },
				{ wgl_attribute::WGL_STEREO_ARB, (pfd.dwFlags & PFD_STEREO) != 0 },
				{ wgl_attribute::WGL_RED_BITS_ARB, pfd.cRedBits },
				{ wgl_attribute::WGL_GREEN_BITS_ARB, pfd.cGreenBits },
				{ wgl_attribute::WGL_BLUE_BITS_ARB, pfd.cBlueBits },
				{ wgl_attribute::WGL_ALPHA_BITS_ARB, pfd.cAlphaBits },
				{ wgl_attribute::WGL_DEPTH_BITS_ARB, pfd.cDepthBits },
				{ wgl_attribute::WGL_STENCIL_BITS_ARB, pfd.cStencilBits },
				{ wgl_attribute::WGL_SAMPLE_BUFFERS_ARB, desc.back_buffer.texture.samples > 1 },
				{ wgl_attribute::WGL_SAMPLES_ARB, desc.back_buffer.texture.samples > 1 ? desc.back_buffer.texture.samples : 0 },
			};

			if (desc.present_mode)
			{
				attribs[i].name = wgl_attribute::WGL_SWAP_METHOD_ARB;
				attribs[i++].value = static_cast<int>(desc.present_mode);
			}

			if (desc.back_buffer.texture.format == reshade::api::format::r8g8b8a8_unorm_srgb || desc.back_buffer.texture.format == reshade::api::format::r8g8b8x8_unorm_srgb ||
				desc.back_buffer.texture.format == reshade::api::format::b8g8r8a8_unorm_srgb || desc.back_buffer.texture.format == reshade::api::format::b8g8r8x8_unorm_srgb ||
				desc.back_buffer.texture.format == reshade::api::format::bc1_unorm_srgb || desc.back_buffer.texture.format == reshade::api::format::bc2_unorm_srgb || desc.back_buffer.texture.format == reshade::api::format::bc3_unorm_srgb)
			{
				attribs[i].name = wgl_attribute::WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB;
				attribs[i++].value = 1;
			}

			UINT num_formats = 0;
			if (!reshade::hooks::call(wglChoosePixelFormatARB)(hdc, reinterpret_cast<const int *>(attribs), nullptr, 1, &iPixelFormat, &num_formats) || num_formats == 0)
			{
				reshade::log::message(reshade::log::level::error, "Failed to find a suitable pixel format with error code %lu!", GetLastError() & 0xFFFF);
			}
			else
			{
				ppfd = &pfd;
			}
		}
		else
		{
			assert(desc.back_buffer.texture.samples <= 1);

			const int pixel_format = reshade::hooks::call(wglChoosePixelFormat)(hdc, &pfd);
			if (pixel_format == 0)
			{
				reshade::log::message(reshade::log::level::error, "Failed to find a suitable pixel format with error code %lu!", GetLastError() & 0xFFFF);
			}
			else
			{
				iPixelFormat = pixel_format;
				ppfd = &pfd;
			}
		}

		if (wglSwapIntervalEXT != nullptr && desc.sync_interval != UINT32_MAX)
		{
			wglSwapIntervalEXT(static_cast<int>(desc.sync_interval));
		}
	}

	// This is not great, since it will mean add-ons are loaded/unloaded multiple times, but otherwise they will never be unloaded
	reshade::unload_addons();
#endif

	if (!reshade::hooks::call(wglSetPixelFormat)(hdc, iPixelFormat, ppfd))
	{
		reshade::log::message(reshade::log::level::warning, "wglSetPixelFormat failed with error code %lu.", GetLastError() & 0xFFFF);
		return FALSE;
	}

	if (GetPixelFormat(hdc) == 0)
	{
		reshade::log::message(reshade::log::level::warning, "Application mistakenly called wglSetPixelFormat directly. Passing on to SetPixelFormat ...");

		SetPixelFormat(hdc, iPixelFormat, ppfd);
	}

	return TRUE;
}
extern "C" int   WINAPI wglDescribePixelFormat(HDC hdc, int iPixelFormat, UINT nBytes, LPPIXELFORMATDESCRIPTOR ppfd)
{
	return reshade::hooks::call(wglDescribePixelFormat)(hdc, iPixelFormat, nBytes, ppfd);
}

extern "C" BOOL  WINAPI wglDescribeLayerPlane(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nBytes, LPLAYERPLANEDESCRIPTOR plpd)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting wglDescribeLayerPlane(hdc = %p, iPixelFormat = %d, iLayerPlane = %d, nBytes = %u, plpd = %p) ...",
		hdc, iPixelFormat, iLayerPlane, nBytes, plpd);
	reshade::log::message(reshade::log::level::warning, "Access to layer plane at index %d is unsupported.", iLayerPlane);

	SetLastError(ERROR_NOT_SUPPORTED);
	return FALSE;
}
extern "C" BOOL  WINAPI wglRealizeLayerPalette(HDC hdc, int iLayerPlane, BOOL b)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting wglRealizeLayerPalette(hdc = %p, iLayerPlane = %d, b = %s) ...",
		hdc, iLayerPlane, b ? "TRUE" : "FALSE");
	reshade::log::message(reshade::log::level::warning, "Access to layer plane at index %d is unsupported.", iLayerPlane);

	SetLastError(ERROR_NOT_SUPPORTED);
	return FALSE;
}
extern "C" int   WINAPI wglGetLayerPaletteEntries(HDC hdc, int iLayerPlane, int iStart, int cEntries, COLORREF *pcr)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting wglGetLayerPaletteEntries(hdc = %p, iLayerPlane = %d, iStart = %d, cEntries = %d, pcr = %p) ...",
		hdc, iLayerPlane, iStart, cEntries, pcr);
	reshade::log::message(reshade::log::level::warning, "Access to layer plane at index %d is unsupported.", iLayerPlane);

	SetLastError(ERROR_NOT_SUPPORTED);
	return 0;
}
extern "C" int   WINAPI wglSetLayerPaletteEntries(HDC hdc, int iLayerPlane, int iStart, int cEntries, const COLORREF *pcr)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting wglSetLayerPaletteEntries(hdc = %p, iLayerPlane = %d, iStart = %d, cEntries = %d, pcr = %p) ...",
		hdc, iLayerPlane, iStart, cEntries, pcr);
	reshade::log::message(reshade::log::level::warning, "Access to layer plane at index %d is unsupported.", iLayerPlane);

	SetLastError(ERROR_NOT_SUPPORTED);
	return 0;
}

extern "C" HGLRC WINAPI wglCreateContext(HDC hdc)
{
	reshade::log::message(reshade::log::level::info, "Redirecting wglCreateContext(hdc = %p) ...", hdc);
	reshade::log::message(reshade::log::level::info, "> Passing on to wglCreateLayerContext:");

	const HGLRC hglrc = wglCreateLayerContext(hdc, 0);
	if (hglrc == nullptr)
	{
		return nullptr;
	}

	// Keep track of legacy contexts here instead of in 'wglCreateLayerContext' because some drivers call the latter from within their 'wglCreateContextAttribsARB' implementation
	{ const std::unique_lock<std::shared_mutex> lock(s_global_mutex);
		s_legacy_contexts.emplace(hglrc);
	}

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Returning OpenGL context %p.", hglrc);
#endif
	return hglrc;
}
		   HGLRC WINAPI wglCreateContextAttribsARB(HDC hdc, HGLRC hShareContext, const int *piAttribList)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting wglCreateContextAttribsARB(hdc = %p, hShareContext = %p, piAttribList = %p) ...",
		hdc, hShareContext, piAttribList);

	int i = 0, major = 1, minor = 0, flags = 0;
	bool compatibility = false;
	wgl_attribute attribs[8] = {};

	for (const int *attrib = piAttribList; attrib != nullptr && *attrib != 0 && i < 5; attrib += 2, ++i)
	{
		attribs[i].name = attrib[0];
		attribs[i].value = attrib[1];

		switch (attrib[0])
		{
		case wgl_attribute::WGL_CONTEXT_MAJOR_VERSION_ARB:
			major = attrib[1];
			break;
		case wgl_attribute::WGL_CONTEXT_MINOR_VERSION_ARB:
			minor = attrib[1];
			break;
		case wgl_attribute::WGL_CONTEXT_FLAGS_ARB:
			flags = attrib[1];
			break;
		case wgl_attribute::WGL_CONTEXT_PROFILE_MASK_ARB:
			compatibility = (attrib[1] & wgl_attribute::WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB) != 0;
			break;
		}
	}

#if RESHADE_ADDON >= 2
	reshade::load_addons();

	uint32_t api_version = (major << 12) | (minor << 8);
	if (reshade::invoke_addon_event<reshade::addon_event::create_device>(reshade::api::device_api::opengl, api_version))
	{
		major = (api_version >> 12) & 0xF;
		minor = (api_version >>  8) & 0xF;
	}

	reshade::unload_addons();
#endif

	if (major < 3 || (major == 3 && minor < 2))
		compatibility = true;

#ifndef NDEBUG
	flags |= wgl_attribute::WGL_CONTEXT_DEBUG_BIT_ARB;
#endif

	// This works because the specs specifically notes that "If an attribute is specified more than once, then the last value specified is used."
	attribs[i].name = wgl_attribute::WGL_CONTEXT_FLAGS_ARB;
	attribs[i++].value = flags;
	attribs[i].name = wgl_attribute::WGL_CONTEXT_PROFILE_MASK_ARB;
	attribs[i++].value = compatibility ? wgl_attribute::WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB : wgl_attribute::WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

	reshade::log::message(reshade::log::level::info, "Requesting %s OpenGL context for version %d.%d.", compatibility ? "compatibility" : "core", major, minor);

	if (major < 4 || (major == 4 && minor < 3))
	{
		reshade::log::message(reshade::log::level::info, "> Replacing requested version with 4.3.");

		for (int k = 0; k < i; ++k)
		{
			switch (attribs[k].name)
			{
			case wgl_attribute::WGL_CONTEXT_MAJOR_VERSION_ARB:
				attribs[k].value = 4;
				break;
			case wgl_attribute::WGL_CONTEXT_MINOR_VERSION_ARB:
				attribs[k].value = 3;
				break;
			case wgl_attribute::WGL_CONTEXT_PROFILE_MASK_ARB:
				attribs[k].value = wgl_attribute::WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
				break;
			}
		}
	}

	const HGLRC hglrc = reshade::hooks::call(wglCreateContextAttribsARB)(hdc, hShareContext, reinterpret_cast<const int *>(attribs));
	if (hglrc == nullptr)
	{
		reshade::log::message(reshade::log::level::warning, "wglCreateContextAttribsARB failed with error code %lu.", GetLastError() & 0xFFFF);
		return nullptr;
	}

	{ const std::unique_lock<std::shared_mutex> lock(s_global_mutex);
		s_shared_contexts.emplace(hglrc, hShareContext);

		if (hShareContext != nullptr)
		{
			// Find the root share context
			auto it = s_shared_contexts.find(hShareContext);

			while (it != s_shared_contexts.end() && it->second != nullptr)
			{
				it = s_shared_contexts.find(s_shared_contexts.at(hglrc) = it->second);
			}
		}
	}

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Returning OpenGL context %p.", hglrc);
#endif
	return hglrc;
}
extern "C" HGLRC WINAPI wglCreateLayerContext(HDC hdc, int iLayerPlane)
{
	reshade::log::message(reshade::log::level::info, "Redirecting wglCreateLayerContext(hdc = %p, iLayerPlane = %d) ...", hdc, iLayerPlane);

	if (iLayerPlane != 0)
	{
		reshade::log::message(reshade::log::level::warning, "Access to layer plane at index %d is unsupported.", iLayerPlane);
		SetLastError(ERROR_INVALID_PARAMETER);
		return nullptr;
	}

	const HGLRC hglrc = reshade::hooks::call(wglCreateLayerContext)(hdc, iLayerPlane);
	if (hglrc == nullptr)
	{
		reshade::log::message(reshade::log::level::warning, "wglCreateLayerContext failed with error code %lu.", GetLastError() & 0xFFFF);
		return nullptr;
	}

	{ const std::unique_lock<std::shared_mutex> lock(s_global_mutex);
		s_shared_contexts.emplace(hglrc, nullptr);
	}

	return hglrc;
}
extern "C" BOOL  WINAPI wglCopyContext(HGLRC hglrcSrc, HGLRC hglrcDst, UINT mask)
{
	return reshade::hooks::call(wglCopyContext)(hglrcSrc, hglrcDst, mask);
}
extern "C" BOOL  WINAPI wglDeleteContext(HGLRC hglrc)
{
	reshade::log::message(reshade::log::level::info, "Redirecting wglDeleteContext(hglrc = %p) ...", hglrc);

	{ const std::unique_lock<std::shared_mutex> lock(s_global_mutex);

		if (const auto it = s_opengl_contexts.find(hglrc);
			it != s_opengl_contexts.end())
		{
			const auto device = static_cast<wgl_device *>(it->second->get_device());

			if (it->second != device)
			{
				// Separate contexts are only created for shared render contexts
				// It is important that the context that it is shared with still exists, otherwise accessing the device in the command queue object during destruction would crash
				const HGLRC prev_hglrc = wglGetCurrentContext();
				if (prev_hglrc == hglrc || (prev_hglrc != nullptr && prev_hglrc == s_shared_contexts.at(hglrc)))
				{
					delete static_cast<wgl_device_context *>(it->second);
				}
				else
				{
					reshade::log::message(reshade::log::level::warning, "Unable to make context current! Leaking resources ...");
				}
			}
			else
			{
				if (std::any_of(s_opengl_contexts.begin(), s_opengl_contexts.end(),
						[hglrc, device](const std::pair<HGLRC, reshade::opengl::device_context_impl *> &context_info) { return context_info.first != hglrc && device == context_info.second->get_device(); }))
				{
					reshade::log::message(reshade::log::level::warning, "Another context referencing the context being destroyed still exists! Application may crash.");
				}

				const HDC prev_hdc = wglGetCurrentDC();
				const HGLRC prev_hglrc = wglGetCurrentContext();

				// Set the render context current so its resources can be cleaned up
				bool hglrc_is_current = true;
				HWND dummy_window_handle = nullptr;

				if (prev_hglrc != hglrc)
				{
					// In case the original was destroyed already, create a dummy window to get a valid context
					dummy_window_handle = CreateWindow(TEXT("STATIC"), nullptr, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr, 0);
					const HDC hdc = GetDC(dummy_window_handle);
					const PIXELFORMATDESCRIPTOR dummy_pfd = { sizeof(dummy_pfd), 1, PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL };
					reshade::hooks::call(wglSetPixelFormat)(hdc, device->get_pixel_format(), &dummy_pfd);

					hglrc_is_current = reshade::hooks::call(wglMakeCurrent)(hdc, hglrc);
				}

				if (hglrc_is_current)
				{
					// Delete any swap chains referencing this device
					for (auto swapchain_it = s_opengl_swapchains.begin(); swapchain_it != s_opengl_swapchains.end();)
					{
						const auto swapchain = *swapchain_it;

						if (device == swapchain->get_device())
						{
							delete swapchain;
							swapchain_it = s_opengl_swapchains.erase(swapchain_it);
						}
						else
						{
							++swapchain_it;
						}
					}

					delete device;

					// Restore previous device and render context
					if (prev_hglrc != hglrc)
						reshade::hooks::call(wglMakeCurrent)(prev_hdc, prev_hglrc);
				}
				else
				{
					reshade::log::message(reshade::log::level::warning, "Unable to make context current (error code %lu)! Leaking resources ...", GetLastError() & 0xFFFF);
				}

				if (dummy_window_handle != nullptr)
					DestroyWindow(dummy_window_handle);
			}

			// Ensure the render context is not still current after deleting
			if (it->second == g_opengl_context)
				g_opengl_context = nullptr;

			s_opengl_contexts.erase(it);
		}

		s_legacy_contexts.erase(hglrc);

		for (auto it = s_shared_contexts.begin(); it != s_shared_contexts.end();)
		{
			if (it->first == hglrc)
			{
				it = s_shared_contexts.erase(it);
				continue;
			}
			else if (it->second == hglrc)
			{
				it->second = nullptr;
			}

			++it;
		}
	}

	if (!reshade::hooks::call(wglDeleteContext)(hglrc))
	{
		reshade::log::message(reshade::log::level::warning, "wglDeleteContext failed with error code %lu.", GetLastError() & 0xFFFF);
		return FALSE;
	}

	return TRUE;
}

extern "C" BOOL  WINAPI wglShareLists(HGLRC hglrc1, HGLRC hglrc2)
{
	reshade::log::message(reshade::log::level::info, "Redirecting wglShareLists(hglrc1 = %p, hglrc2 = %p) ...", hglrc1, hglrc2);

	if (!reshade::hooks::call(wglShareLists)(hglrc1, hglrc2))
	{
		reshade::log::message(reshade::log::level::warning, "wglShareLists failed with error code %lu.", GetLastError() & 0xFFFF);
		return FALSE;
	}

	{ const std::unique_lock<std::shared_mutex> lock(s_global_mutex);
		s_shared_contexts[hglrc2] = hglrc1;
	}

	return TRUE;
}

extern "C" BOOL  WINAPI wglMakeCurrent(HDC hdc, HGLRC hglrc)
{
#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::info, "Redirecting wglMakeCurrent(hdc = %p, hglrc = %p) ...", hdc, hglrc);
#endif

	HGLRC shared_hglrc = hglrc;
	const HGLRC prev_hglrc = wglGetCurrentContext();

	static const auto trampoline = reshade::hooks::call(wglMakeCurrent);
	if (!trampoline(hdc, hglrc))
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::warning, "wglMakeCurrent failed with error code %lu.", GetLastError() & 0xFFFF);
#endif
		return FALSE;
	}

	if (hglrc == prev_hglrc)
	{
		// Nothing has changed, so there is nothing more to do
		return TRUE;
	}
	else if (hglrc == nullptr)
	{
		g_opengl_context = nullptr;

		return TRUE;
	}

	const std::unique_lock<std::shared_mutex> lock(s_global_mutex);

	if (const auto it = s_shared_contexts.find(hglrc);
		it != s_shared_contexts.end() && it->second != nullptr)
	{
		shared_hglrc = it->second;

#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::debug, "> Using shared OpenGL context %p.", shared_hglrc);
#endif
	}

	if (const auto it = s_opengl_contexts.find(shared_hglrc);
		it != s_opengl_contexts.end())
	{
		g_opengl_context = it->second;
	}
	else
	{
		// Force installation of hooks in 'wglGetProcAddress' defined below in case it has not happened yet
		if (!s_hooks_installed)
			wglGetProcAddress("wglCreateContextAttribsARB");

		// Load original OpenGL functions instead of using the hooked ones
		gl3wInit2([](const char *name) {
			extern std::filesystem::path get_system_path();
			// First attempt to load from the OpenGL ICD
			FARPROC proc_address = reshade::hooks::call(wglGetProcAddress)(name);
			if (nullptr == proc_address)
				// Load from the Windows OpenGL DLL if that fails
				proc_address = GetProcAddress(GetModuleHandleW((get_system_path() / L"opengl32.dll").c_str()), name);
			// Get trampoline pointers to any hooked functions, so that effect runtime always calls into original OpenGL functions
			if (nullptr != proc_address && reshade::hooks::is_hooked(proc_address))
				proc_address = reshade::hooks::call<FARPROC>(nullptr, proc_address);
			return reinterpret_cast<GL3WglProc>(proc_address);
		});

		if (!gl3wIsSupported(4, 3))
		{
			reshade::log::message(reshade::log::level::error, "Your graphics card does not seem to support OpenGL 4.3. Initialization failed.");

			g_opengl_context = nullptr;

			return TRUE;
		}

		assert(s_hooks_installed);

		const bool legacy_context = s_legacy_contexts.find(shared_hglrc) != s_legacy_contexts.end();

		const auto device = new wgl_device(
			hdc, shared_hglrc,
			// Always set compatibility context flag on contexts that were created with 'wglCreateContext' instead of 'wglCreateContextAttribsARB'
			// This is necessary because with some pixel formats the 'GL_ARB_compatibility' extension is not exposed even though the context was not created with the core profile
			legacy_context);

		s_opengl_contexts.emplace(shared_hglrc, device);
		g_opengl_context = device;

		if (device->get_compatibility_context() && !legacy_context)
		{
			// In the OpenGL core profile, all generic vertex attributes have the initial values { 0.0, 0.0, 0.0, 1.0 }
			// In the OpenGL compatibility profile, some generic vertex attributes have different values, because they can be vertex coordinates, normals, colors (which have the initial value { 1.0, 1.0, 1.0, 1.0 }), secondary colors, texture coordinates, ...
			// Since ReShade can change the context to use the compatibility profile, need to adjust the initial values if the application is expecting a core profile, or vertex shading can break (e.g. in Vintage Story)
			GLint max_elements = 0;
			gl.GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_elements);

			for (GLsizei i = 0; i < max_elements; ++i)
				gl.VertexAttrib4f(i, 0.0f, 0.0f, 0.0f, 1.0f);
		}
	}

	assert(g_opengl_context != nullptr);
	const auto device = static_cast<wgl_device *>(g_opengl_context);

	if (hglrc != shared_hglrc)
	{
		if (const auto it = s_opengl_contexts.find(hglrc);
			it != s_opengl_contexts.end())
		{
			g_opengl_context = it->second;
		}
		else
		{
			const auto context = new wgl_device_context(device, hglrc);

			s_opengl_contexts.emplace(hglrc, context);
			g_opengl_context = context;
		}
	}

	const HWND hwnd = WindowFromDC(hdc);
	wgl_swapchain *swapchain = nullptr;

	if (const auto swapchain_it = std::find_if(s_opengl_swapchains.begin(), s_opengl_swapchains.end(),
			[hdc, hwnd, device](wgl_swapchain *const swapchain) {
				const HDC swapchain_hdc = reinterpret_cast<HDC>(swapchain->_orig);
				return (swapchain_hdc == hdc || (hwnd != nullptr && WindowFromDC(swapchain_hdc) == hwnd)) && swapchain->get_device() == device;
			});
		swapchain_it != s_opengl_swapchains.end())
	{
		swapchain = *swapchain_it;
	}
	else
	{
		if (hwnd == nullptr || s_pbuffer_device_contexts.find(hdc) != s_pbuffer_device_contexts.end())
		{
#if RESHADE_VERBOSE_LOG
			reshade::log::message(reshade::log::level::warning, "Skipping render context %p because there is no window associated with device context %p.", hglrc, hdc);
#endif
		}
		else
		{
			if ((GetClassLongPtr(hwnd, GCL_STYLE) & CS_OWNDC) == 0)
			{
				reshade::log::message(reshade::log::level::warning, "Window class style of window %p is missing 'CS_OWNDC' flag.", hwnd);
			}

			assert(wglGetPixelFormat(hdc) == device->get_pixel_format());

			swapchain = new wgl_swapchain(device, hdc);
			s_opengl_swapchains.push_back(swapchain);
		}
	}

	unsigned int width = 0;
	unsigned int height = 0;
	if (hwnd != nullptr)
	{
		RECT window_rect = {};
		GetClientRect(hwnd, &window_rect);

		width = static_cast<unsigned int>(window_rect.right);
		height = static_cast<unsigned int>(window_rect.bottom);
	}
	else
	{
		// This may not be accurate, could instead e.g. use 'wglQueryPbufferARB'
		GLint scissor_box[4] = {};
		gl.GetIntegerv(GL_SCISSOR_BOX, scissor_box);
		assert(scissor_box[0] == 0 && scissor_box[1] == 0);

		width = scissor_box[2] - scissor_box[0];
		height = scissor_box[3] - scissor_box[1];
	}

	// Wolfenstein: The Old Blood creates a window with a height of zero that is later resized
	if (swapchain != nullptr && width != 0 && height != 0)
		swapchain->on_init(g_opengl_context, width, height);
	else
		g_opengl_context->update_default_framebuffer(width, height);

	return TRUE;
}

extern "C" HDC   WINAPI wglGetCurrentDC()
{
	static const auto trampoline = reshade::hooks::call(wglGetCurrentDC);
	return trampoline();
}
extern "C" HGLRC WINAPI wglGetCurrentContext()
{
	static const auto trampoline = reshade::hooks::call(wglGetCurrentContext);
	return trampoline();
}

	 HPBUFFERARB WINAPI wglCreatePbufferARB(HDC hdc, int iPixelFormat, int iWidth, int iHeight, const int *piAttribList)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting wglCreatePbufferARB(hdc = %p, iPixelFormat = %d, iWidth = %d, iHeight = %d, piAttribList = %p) ...",
		hdc, iPixelFormat, iWidth, iHeight, piAttribList);

	struct attribute
	{
		enum
		{
			WGL_PBUFFER_LARGEST_ARB = 0x2033,
			WGL_TEXTURE_FORMAT_ARB = 0x2072,
			WGL_TEXTURE_TARGET_ARB = 0x2073,
			WGL_MIPMAP_TEXTURE_ARB = 0x2074,

			WGL_TEXTURE_RGB_ARB = 0x2075,
			WGL_TEXTURE_RGBA_ARB = 0x2076,
			WGL_NO_TEXTURE_ARB = 0x2077,
		};
	};

	reshade::log::message(reshade::log::level::info, "> Dumping attributes:");
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
	reshade::log::message(reshade::log::level::info, "  | Attribute                               | Value                                   |");
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");

	for (const int *attrib = piAttribList; attrib != nullptr && *attrib != 0; attrib += 2)
	{
		switch (attrib[0])
		{
		case attribute::WGL_PBUFFER_LARGEST_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_PBUFFER_LARGEST_ARB                 | %-39s |", attrib[1] != FALSE ? "TRUE" : "FALSE");
			break;
		case attribute::WGL_TEXTURE_FORMAT_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_TEXTURE_FORMAT_ARB                  | %-#39x |", static_cast<unsigned int>(attrib[1]));
			break;
		case attribute::WGL_TEXTURE_TARGET_ARB:
			reshade::log::message(reshade::log::level::info, "  | WGL_TEXTURE_TARGET_ARB                  | %-#39x |", static_cast<unsigned int>(attrib[1]));
			break;
		default:
			reshade::log::message(reshade::log::level::info, "  | %-#39x | %-39d |", static_cast<unsigned int>(attrib[0]), attrib[1]);
			break;
		}
	}

	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");

	const HPBUFFERARB hpbuffer = reshade::hooks::call(wglCreatePbufferARB)(hdc, iPixelFormat, iWidth, iHeight, piAttribList);
	if (hpbuffer == nullptr)
	{
		reshade::log::message(reshade::log::level::warning, "wglCreatePbufferARB failed with error code %lu.", GetLastError() & 0xFFFF);
		return nullptr;
	}

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::info, "Returning pixel buffer %p.", hpbuffer);
#endif
	return hpbuffer;
}
		   BOOL  WINAPI wglDestroyPbufferARB(HPBUFFERARB hPbuffer)
{
	reshade::log::message(reshade::log::level::info, "Redirecting wglDestroyPbufferARB(hPbuffer = %p) ...", hPbuffer);

	if (!reshade::hooks::call(wglDestroyPbufferARB)(hPbuffer))
	{
		reshade::log::message(reshade::log::level::warning, "wglDestroyPbufferARB failed with error code %lu.", GetLastError() & 0xFFFF);
		return FALSE;
	}

	return TRUE;
}
		   HDC   WINAPI wglGetPbufferDCARB(HPBUFFERARB hPbuffer)
{
	reshade::log::message(reshade::log::level::info, "Redirecting wglGetPbufferDCARB(hPbuffer = %p) ...", hPbuffer);

	const HDC hdc = reshade::hooks::call(wglGetPbufferDCARB)(hPbuffer);
	if (hdc == nullptr)
	{
		reshade::log::message(reshade::log::level::warning, "wglGetPbufferDCARB failed with error code %lu.", GetLastError() & 0xFFFF);
		return nullptr;
	}

	{ const std::unique_lock<std::shared_mutex> lock(s_global_mutex);
		s_pbuffer_device_contexts.insert(hdc);
	}

#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::info, "Returning pixel buffer device context %p.", hdc);
#endif
	return hdc;
}
		   int   WINAPI wglReleasePbufferDCARB(HPBUFFERARB hPbuffer, HDC hdc)
{
	reshade::log::message(reshade::log::level::info, "Redirecting wglReleasePbufferDCARB(hPbuffer = %p) ...", hPbuffer);

	if (!reshade::hooks::call(wglReleasePbufferDCARB)(hPbuffer, hdc))
	{
		reshade::log::message(reshade::log::level::warning, "wglReleasePbufferDCARB failed with error code %lu.", GetLastError() & 0xFFFF);
		return FALSE;
	}

	{ const std::unique_lock<std::shared_mutex> lock(s_global_mutex);
		s_pbuffer_device_contexts.erase(hdc);
	}

	return TRUE;
}

extern "C" BOOL  WINAPI wglSwapBuffers(HDC hdc)
{
	if (g_opengl_context != nullptr)
	{
		const std::shared_lock<std::shared_mutex> lock(s_global_mutex);

		if (const auto swapchain_it = std::find_if(s_opengl_swapchains.begin(), s_opengl_swapchains.end(),
				[hdc, hwnd = WindowFromDC(hdc)](wgl_swapchain *const swapchain) {
					const HDC swapchain_hdc = reinterpret_cast<HDC>(swapchain->_orig);
					// Fall back to checking for the same window, in case the device context handle has changed (without 'CS_OWNDC')
					return (swapchain_hdc == hdc || (hwnd != nullptr && WindowFromDC(swapchain_hdc) == hwnd)) && swapchain->get_device() == g_opengl_context->get_device();
				});
			swapchain_it != s_opengl_swapchains.end())
		{
			const auto swapchain = *swapchain_it;
			swapchain->on_present(g_opengl_context);
		}
	}

	static const auto trampoline = reshade::hooks::call(wglSwapBuffers);
	assert(!g_in_dxgi_runtime);
	g_in_dxgi_runtime = true;
	const BOOL result = trampoline(hdc);
	g_in_dxgi_runtime = false;
	return result;
}
extern "C" BOOL  WINAPI wglSwapLayerBuffers(HDC hdc, UINT i)
{
	if (i != WGL_SWAP_MAIN_PLANE)
	{
		int plane_index = 0;
		if (i >= WGL_SWAP_UNDERLAY1)
		{
			DWORD bit_index = 0;
			BitScanReverse(&bit_index, i >> 16);
			plane_index = -static_cast<int>(bit_index) - 1;
		}
		else
		{
			DWORD bit_index = 0;
			BitScanReverse(&bit_index, i);
			plane_index = +static_cast<int>(bit_index);
		}

#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::info, "Redirecting wglSwapLayerBuffers(hdc = %p, i = %x) ...", hdc, i);
		reshade::log::message(reshade::log::level::warning, "Access to layer plane at index %d is unsupported.", plane_index);
#endif

		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	return wglSwapBuffers(hdc);
}
extern "C" DWORD WINAPI wglSwapMultipleBuffers(UINT cNumBuffers, const WGLSWAP *pBuffers)
{
	DWORD result = 0;

	if (cNumBuffers <= WGL_SWAPMULTIPLE_MAX)
	{
		for (UINT i = 0; i < cNumBuffers; ++i)
		{
			assert(pBuffers != nullptr);

			if (wglSwapBuffers(pBuffers[i].hdc))
				result |= 1 << i;
		}
	}
	else
	{
		SetLastError(ERROR_INVALID_PARAMETER);
	}

	return result;
}

extern "C" BOOL  WINAPI wglUseFontBitmapsA(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3)
{
	static const auto trampoline = reshade::hooks::call(wglUseFontBitmapsA);
	return trampoline(hdc, dw1, dw2, dw3);
}
extern "C" BOOL  WINAPI wglUseFontBitmapsW(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3)
{
	static const auto trampoline = reshade::hooks::call(wglUseFontBitmapsW);
	return trampoline(hdc, dw1, dw2, dw3);
}
extern "C" BOOL  WINAPI wglUseFontOutlinesA(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3, FLOAT f1, FLOAT f2, int i, LPGLYPHMETRICSFLOAT pgmf)
{
	static const auto trampoline = reshade::hooks::call(wglUseFontOutlinesA);
	return trampoline(hdc, dw1, dw2, dw3, f1, f2, i, pgmf);
}
extern "C" BOOL  WINAPI wglUseFontOutlinesW(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3, FLOAT f1, FLOAT f2, int i, LPGLYPHMETRICSFLOAT pgmf)
{
	static const auto trampoline = reshade::hooks::call(wglUseFontOutlinesW);
	return trampoline(hdc, dw1, dw2, dw3, f1, f2, i, pgmf);
}

extern "C" PROC  WINAPI wglGetProcAddress(LPCSTR lpszProc)
{
	if (lpszProc == nullptr)
		return nullptr;
#if RESHADE_ADDON
	// Redirect some old extension functions to their modern variants in core OpenGL
	#pragma region GL_ARB_draw_instanced
	else if (0 == std::strcmp(lpszProc, "glDrawArraysInstancedARB"))
		lpszProc = "glDrawArraysInstanced";
	else if (0 == std::strcmp(lpszProc, "glDrawElementsInstancedARB"))
		lpszProc = "glDrawElementsInstanced";
	#pragma endregion
	#pragma region GL_ARB_vertex_buffer_object
	else if (0 == std::strcmp(lpszProc, "glIsBufferARB"))
		lpszProc = "glIsBuffer";
	else if (0 == std::strcmp(lpszProc, "glBindBufferARB"))
		lpszProc = "glBindBuffer";
	else if (0 == std::strcmp(lpszProc, "glGenBuffersARB"))
		lpszProc = "glGenBuffers";
	else if (0 == std::strcmp(lpszProc, "glDeleteBuffersARB"))
		lpszProc = "glDeleteBuffers";
	else if (0 == std::strcmp(lpszProc, "glBufferDataARB"))
		lpszProc = "glBufferData";
	else if (0 == std::strcmp(lpszProc, "glBufferSubDataARB"))
		lpszProc = "glBufferSubData";
	else if (0 == std::strcmp(lpszProc, "glGetBufferSubDataARB"))
		lpszProc = "glGetBufferSubData";
	else if (0 == std::strcmp(lpszProc, "glMapBufferARB"))
		lpszProc = "glMapBuffer";
	else if (0 == std::strcmp(lpszProc, "glUnmapBufferARB"))
		lpszProc = "glUnmapBuffer";
	else if (0 == std::strcmp(lpszProc, "glGetBufferParameterivARB"))
		lpszProc = "glGetBufferParameteriv";
	else if (0 == std::strcmp(lpszProc, "glGetBufferPointervARB"))
		lpszProc = "glGetBufferPointerv";
	#pragma endregion
	#pragma region GL_EXT_draw_instanced
	else if (0 == std::strcmp(lpszProc, "glDrawArraysInstancedEXT"))
		lpszProc = "glDrawArraysInstanced";
	else if (0 == std::strcmp(lpszProc, "glDrawElementsInstancedEXT"))
		lpszProc = "glDrawElementsInstanced";
	#pragma endregion
	#pragma region GL_EXT_framebuffer_object
	else if (0 == std::strcmp(lpszProc, "glIsFramebufferEXT"))
		lpszProc = "glIsFramebuffer";
	// glBindFramebuffer and glBindFramebufferEXT have different semantics (core variant requires an existing FBO), so do not redirect
	else if (0 == std::strcmp(lpszProc, "glGenFramebuffersEXT"))
		lpszProc = "glGenFramebuffers";
	else if (0 == std::strcmp(lpszProc, "glDeleteFramebuffersEXT"))
		lpszProc = "glDeleteFramebuffers";
	else if (0 == std::strcmp(lpszProc, "glCheckFramebufferStatusEXT"))
		lpszProc = "glCheckFramebufferStatus";
	else if (0 == std::strcmp(lpszProc, "glFramebufferRenderbufferEXT"))
		lpszProc = "glFramebufferRenderbuffer";
	else if (0 == std::strcmp(lpszProc, "glFramebufferTexture1DEXT"))
		lpszProc = "glFramebufferTexture1D";
	else if (0 == std::strcmp(lpszProc, "glFramebufferTexture2DEXT"))
		lpszProc = "glFramebufferTexture2D";
	else if (0 == std::strcmp(lpszProc, "glFramebufferTexture3DEXT"))
		lpszProc = "glFramebufferTexture3D";
	else if (0 == std::strcmp(lpszProc, "glGetFramebufferAttachmentParameterivEXT"))
		lpszProc = "glGetFramebufferAttachmentParameteriv";
	else if (0 == std::strcmp(lpszProc, "glIsRenderbufferEXT"))
		lpszProc = "glIsRenderbuffer";
	// glBindRenderbuffer and glBindRenderbufferEXT have different semantics (core variant requires an existing RBO), so do not redirect
	else if (0 == std::strcmp(lpszProc, "glGenRenderbuffersEXT"))
		lpszProc = "glGenRenderbuffers";
	else if (0 == std::strcmp(lpszProc, "glDeleteRenderbuffersEXT"))
		lpszProc = "glDeleteRenderbuffers";
	else if (0 == std::strcmp(lpszProc, "glRenderbufferStorageEXT"))
		lpszProc = "glRenderbufferStorage";
	else if (0 == std::strcmp(lpszProc, "glGetRenderbufferParameterivEXT"))
		lpszProc = "glGetRenderbufferParameteriv";
	else if (0 == std::strcmp(lpszProc, "glGenerateMipmapEXT"))
		lpszProc = "glGenerateMipmap";
	#pragma endregion
#endif

	static const auto trampoline = reshade::hooks::call(wglGetProcAddress);

	const PROC proc_address = trampoline(lpszProc);
	if (proc_address == nullptr)
		return nullptr;
	else if (0 == std::strcmp(lpszProc, "glBindTexture"))
		return reinterpret_cast<PROC>(glBindTexture);
	else if (0 == std::strcmp(lpszProc, "glBlendFunc"))
		return reinterpret_cast<PROC>(glBlendFunc);
	else if (0 == std::strcmp(lpszProc, "glClear"))
		return reinterpret_cast<PROC>(glClear);
	else if (0 == std::strcmp(lpszProc, "glClearColor"))
		return reinterpret_cast<PROC>(glClearColor);
	else if (0 == std::strcmp(lpszProc, "glClearDepth"))
		return reinterpret_cast<PROC>(glClearDepth);
	else if (0 == std::strcmp(lpszProc, "glClearStencil"))
		return reinterpret_cast<PROC>(glClearStencil);
	else if (0 == std::strcmp(lpszProc, "glCopyTexImage1D"))
		return reinterpret_cast<PROC>(glCopyTexImage1D);
	else if (0 == std::strcmp(lpszProc, "glCopyTexImage2D"))
		return reinterpret_cast<PROC>(glCopyTexImage2D);
	else if (0 == std::strcmp(lpszProc, "glCopyTexSubImage1D"))
		return reinterpret_cast<PROC>(glCopyTexSubImage1D);
	else if (0 == std::strcmp(lpszProc, "glCopyTexSubImage2D"))
		return reinterpret_cast<PROC>(glCopyTexSubImage2D);
	else if (0 == std::strcmp(lpszProc, "glCullFace"))
		return reinterpret_cast<PROC>(glCullFace);
	else if (0 == std::strcmp(lpszProc, "glDeleteTextures"))
		return reinterpret_cast<PROC>(glDeleteTextures);
	else if (0 == std::strcmp(lpszProc, "glDepthFunc"))
		return reinterpret_cast<PROC>(glDepthFunc);
	else if (0 == std::strcmp(lpszProc, "glDepthMask"))
		return reinterpret_cast<PROC>(glDepthMask);
	else if (0 == std::strcmp(lpszProc, "glDepthRange"))
		return reinterpret_cast<PROC>(glDepthRange);
	else if (0 == std::strcmp(lpszProc, "glDisable"))
		return reinterpret_cast<PROC>(glDisable);
	else if (0 == std::strcmp(lpszProc, "glDrawArrays"))
		return reinterpret_cast<PROC>(glDrawArrays);
	else if (0 == std::strcmp(lpszProc, "glDrawBuffer"))
		return reinterpret_cast<PROC>(glDrawBuffer);
	else if (0 == std::strcmp(lpszProc, "glDrawElements"))
		return reinterpret_cast<PROC>(glDrawElements);
	else if (0 == std::strcmp(lpszProc, "glEnable"))
		return reinterpret_cast<PROC>(glEnable);
	else if (0 == std::strcmp(lpszProc, "glFinish"))
		return reinterpret_cast<PROC>(glFinish);
	else if (0 == std::strcmp(lpszProc, "glFlush"))
		return reinterpret_cast<PROC>(glFlush);
	else if (0 == std::strcmp(lpszProc, "glFrontFace"))
		return reinterpret_cast<PROC>(glFrontFace);
	else if (0 == std::strcmp(lpszProc, "glGenTextures"))
		return reinterpret_cast<PROC>(glGenTextures);
	else if (0 == std::strcmp(lpszProc, "glGetBooleanv"))
		return reinterpret_cast<PROC>(glGetBooleanv);
	else if (0 == std::strcmp(lpszProc, "glGetDoublev"))
		return reinterpret_cast<PROC>(glGetDoublev);
	else if (0 == std::strcmp(lpszProc, "glGetFloatv"))
		return reinterpret_cast<PROC>(glGetFloatv);
	else if (0 == std::strcmp(lpszProc, "glGetIntegerv"))
		return reinterpret_cast<PROC>(glGetIntegerv);
	else if (0 == std::strcmp(lpszProc, "glGetError"))
		return reinterpret_cast<PROC>(glGetError);
	else if (0 == std::strcmp(lpszProc, "glGetPointerv"))
		return reinterpret_cast<PROC>(glGetPointerv);
	else if (0 == std::strcmp(lpszProc, "glGetString"))
		return reinterpret_cast<PROC>(glGetString);
	else if (0 == std::strcmp(lpszProc, "glGetTexImage"))
		return reinterpret_cast<PROC>(glGetTexImage);
	else if (0 == std::strcmp(lpszProc, "glGetTexLevelParameterfv"))
		return reinterpret_cast<PROC>(glGetTexLevelParameterfv);
	else if (0 == std::strcmp(lpszProc, "glGetTexLevelParameteriv"))
		return reinterpret_cast<PROC>(glGetTexLevelParameteriv);
	else if (0 == std::strcmp(lpszProc, "glGetTexParameterfv"))
		return reinterpret_cast<PROC>(glGetTexParameterfv);
	else if (0 == std::strcmp(lpszProc, "glGetTexParameteriv"))
		return reinterpret_cast<PROC>(glGetTexParameteriv);
	else if (0 == std::strcmp(lpszProc, "glHint"))
		return reinterpret_cast<PROC>(glHint);
	else if (0 == std::strcmp(lpszProc, "glIsEnabled"))
		return reinterpret_cast<PROC>(glIsEnabled);
	else if (0 == std::strcmp(lpszProc, "glIsTexture"))
		return reinterpret_cast<PROC>(glIsTexture);
	else if (0 == std::strcmp(lpszProc, "glLineWidth"))
		return reinterpret_cast<PROC>(glLineWidth);
	else if (0 == std::strcmp(lpszProc, "glLogicOp"))
		return reinterpret_cast<PROC>(glLogicOp);
	else if (0 == std::strcmp(lpszProc, "glPixelStoref"))
		return reinterpret_cast<PROC>(glPixelStoref);
	else if (0 == std::strcmp(lpszProc, "glPixelStorei"))
		return reinterpret_cast<PROC>(glPixelStorei);
	else if (0 == std::strcmp(lpszProc, "glPointSize"))
		return reinterpret_cast<PROC>(glPointSize);
	else if (0 == std::strcmp(lpszProc, "glPolygonMode"))
		return reinterpret_cast<PROC>(glPolygonMode);
	else if (0 == std::strcmp(lpszProc, "glPolygonOffset"))
		return reinterpret_cast<PROC>(glPolygonOffset);
	else if (0 == std::strcmp(lpszProc, "glReadBuffer"))
		return reinterpret_cast<PROC>(glReadBuffer);
	else if (0 == std::strcmp(lpszProc, "glReadPixels"))
		return reinterpret_cast<PROC>(glReadPixels);
	else if (0 == std::strcmp(lpszProc, "glScissor"))
		return reinterpret_cast<PROC>(glScissor);
	else if (0 == std::strcmp(lpszProc, "glStencilFunc"))
		return reinterpret_cast<PROC>(glStencilFunc);
	else if (0 == std::strcmp(lpszProc, "glStencilMask"))
		return reinterpret_cast<PROC>(glStencilMask);
	else if (0 == std::strcmp(lpszProc, "glStencilOp"))
		return reinterpret_cast<PROC>(glStencilOp);
	else if (0 == std::strcmp(lpszProc, "glTexImage1D"))
		return reinterpret_cast<PROC>(glTexImage1D);
	else if (0 == std::strcmp(lpszProc, "glTexImage2D"))
		return reinterpret_cast<PROC>(glTexImage2D);
	else if (0 == std::strcmp(lpszProc, "glTexParameterf"))
		return reinterpret_cast<PROC>(glTexParameterf);
	else if (0 == std::strcmp(lpszProc, "glTexParameterfv"))
		return reinterpret_cast<PROC>(glTexParameterfv);
	else if (0 == std::strcmp(lpszProc, "glTexParameteri"))
		return reinterpret_cast<PROC>(glTexParameteri);
	else if (0 == std::strcmp(lpszProc, "glTexParameteriv"))
		return reinterpret_cast<PROC>(glTexParameteriv);
	else if (0 == std::strcmp(lpszProc, "glTexSubImage1D"))
		return reinterpret_cast<PROC>(glTexSubImage1D);
	else if (0 == std::strcmp(lpszProc, "glTexSubImage2D"))
		return reinterpret_cast<PROC>(glTexSubImage2D);
	else if (0 == std::strcmp(lpszProc, "glViewport"))
		return reinterpret_cast<PROC>(glViewport);

	if (!s_hooks_installed)
	{
#if 1
	#define HOOK_PROC(name) \
		reshade::hooks::install(#name, reinterpret_cast<reshade::hook::address>(trampoline(#name)), reinterpret_cast<reshade::hook::address>(name), true)
#else
	// This does not work because the hooks are not registered and thus 'reshade::hooks::call' will fail
	#define HOOK_PROC(name) \
		if (0 == std::strcmp(lpszProc, #name)) \
			return reinterpret_cast<PROC>(name)
#endif

#if RESHADE_ADDON
#ifdef GL_VERSION_1_2
		HOOK_PROC(glTexImage3D);
		HOOK_PROC(glTexSubImage3D);
		HOOK_PROC(glCopyTexSubImage3D);
		HOOK_PROC(glDrawRangeElements);
#endif
#ifdef GL_VERSION_1_3
		HOOK_PROC(glCompressedTexImage1D);
		HOOK_PROC(glCompressedTexImage2D);
		HOOK_PROC(glCompressedTexImage3D);
		HOOK_PROC(glCompressedTexSubImage1D);
		HOOK_PROC(glCompressedTexSubImage2D);
		HOOK_PROC(glCompressedTexSubImage3D);
#endif
#ifdef GL_VERSION_1_4
		HOOK_PROC(glBlendFuncSeparate);
		HOOK_PROC(glBlendColor);
		HOOK_PROC(glBlendEquation);
		HOOK_PROC(glMultiDrawArrays);
		HOOK_PROC(glMultiDrawElements);
#endif
#ifdef GL_VERSION_1_5
		HOOK_PROC(glDeleteBuffers);
		HOOK_PROC(glBufferData);
		HOOK_PROC(glBufferSubData);
		HOOK_PROC(glMapBuffer);
		HOOK_PROC(glUnmapBuffer);
		HOOK_PROC(glBindBuffer);
#endif
#ifdef GL_VERSION_2_0
		HOOK_PROC(glDeleteProgram);
		HOOK_PROC(glLinkProgram);
		HOOK_PROC(glShaderSource);
		HOOK_PROC(glUseProgram);
		HOOK_PROC(glBlendEquationSeparate);
		HOOK_PROC(glStencilFuncSeparate);
		HOOK_PROC(glStencilOpSeparate);
		HOOK_PROC(glStencilMaskSeparate);
		HOOK_PROC(glUniform1f);
		HOOK_PROC(glUniform2f);
		HOOK_PROC(glUniform3f);
		HOOK_PROC(glUniform4f);
		HOOK_PROC(glUniform1i);
		HOOK_PROC(glUniform2i);
		HOOK_PROC(glUniform3i);
		HOOK_PROC(glUniform4i);
		HOOK_PROC(glUniform1fv);
		HOOK_PROC(glUniform2fv);
		HOOK_PROC(glUniform3fv);
		HOOK_PROC(glUniform4fv);
		HOOK_PROC(glUniform1iv);
		HOOK_PROC(glUniform2iv);
		HOOK_PROC(glUniform3iv);
		HOOK_PROC(glUniform4iv);
		HOOK_PROC(glUniformMatrix2fv);
		HOOK_PROC(glUniformMatrix3fv);
		HOOK_PROC(glUniformMatrix4fv);
		HOOK_PROC(glVertexAttribPointer);
#endif
#ifdef GL_VERSION_2_1
		HOOK_PROC(glUniformMatrix2x3fv);
		HOOK_PROC(glUniformMatrix3x2fv);
		HOOK_PROC(glUniformMatrix2x4fv);
		HOOK_PROC(glUniformMatrix4x2fv);
		HOOK_PROC(glUniformMatrix3x4fv);
		HOOK_PROC(glUniformMatrix4x3fv);
#endif
#ifdef GL_VERSION_3_0
		HOOK_PROC(glMapBufferRange);
		HOOK_PROC(glDeleteRenderbuffers);
		HOOK_PROC(glFramebufferTexture1D);
		HOOK_PROC(glFramebufferTexture2D);
		HOOK_PROC(glFramebufferTexture3D);
		HOOK_PROC(glFramebufferTextureLayer);
		HOOK_PROC(glFramebufferRenderbuffer);
		HOOK_PROC(glRenderbufferStorage);
		HOOK_PROC(glRenderbufferStorageMultisample);
		HOOK_PROC(glClearBufferiv);
		HOOK_PROC(glClearBufferuiv);
		HOOK_PROC(glClearBufferfv);
		HOOK_PROC(glClearBufferfi);
		HOOK_PROC(glBlitFramebuffer);
		HOOK_PROC(glGenerateMipmap);
		HOOK_PROC(glBindBufferBase);
		HOOK_PROC(glBindBufferRange);
		HOOK_PROC(glBindFramebuffer);
		HOOK_PROC(glBindVertexArray);
		HOOK_PROC(glUniform1ui);
		HOOK_PROC(glUniform2ui);
		HOOK_PROC(glUniform3ui);
		HOOK_PROC(glUniform4ui);
		HOOK_PROC(glUniform1uiv);
		HOOK_PROC(glUniform2uiv);
		HOOK_PROC(glUniform3uiv);
		HOOK_PROC(glUniform4uiv);
		HOOK_PROC(glDeleteVertexArrays);
		HOOK_PROC(glVertexAttribIPointer);
#endif
#ifdef GL_VERSION_3_1
		HOOK_PROC(glTexBuffer);
		HOOK_PROC(glCopyBufferSubData);
		HOOK_PROC(glDrawArraysInstanced);
		HOOK_PROC(glDrawElementsInstanced);
#endif
#ifdef GL_VERSION_3_2
		HOOK_PROC(glFramebufferTexture);
		HOOK_PROC(glTexImage2DMultisample);
		HOOK_PROC(glTexImage3DMultisample);
		HOOK_PROC(glDrawElementsBaseVertex);
		HOOK_PROC(glDrawRangeElementsBaseVertex);
		HOOK_PROC(glDrawElementsInstancedBaseVertex);
		HOOK_PROC(glMultiDrawElementsBaseVertex);
#endif
#ifdef GL_VERSION_4_0
		HOOK_PROC(glDrawArraysIndirect);
		HOOK_PROC(glDrawElementsIndirect);
#endif
#ifdef GL_VERSION_4_1
		HOOK_PROC(glScissorArrayv);
		HOOK_PROC(glScissorIndexed);
		HOOK_PROC(glScissorIndexedv);
		HOOK_PROC(glViewportArrayv);
		HOOK_PROC(glViewportIndexedf);
		HOOK_PROC(glViewportIndexedfv);
		HOOK_PROC(glVertexAttribLPointer);
#endif
#ifdef GL_VERSION_4_2
		HOOK_PROC(glTexStorage1D);
		HOOK_PROC(glTexStorage2D);
		HOOK_PROC(glTexStorage3D);
		HOOK_PROC(glBindImageTexture);
		HOOK_PROC(glDrawArraysInstancedBaseInstance);
		HOOK_PROC(glDrawElementsInstancedBaseInstance);
		HOOK_PROC(glDrawElementsInstancedBaseVertexBaseInstance);
#endif
#ifdef GL_VERSION_4_3
		HOOK_PROC(glTextureView);
		HOOK_PROC(glTexBufferRange);
		HOOK_PROC(glTexStorage2DMultisample);
		HOOK_PROC(glTexStorage3DMultisample);
		HOOK_PROC(glCopyImageSubData);
		HOOK_PROC(glBindVertexBuffer);
		HOOK_PROC(glDispatchCompute);
		HOOK_PROC(glDispatchComputeIndirect);
		HOOK_PROC(glMultiDrawArraysIndirect);
		HOOK_PROC(glMultiDrawElementsIndirect);
#endif
#ifdef GL_VERSION_4_4
		HOOK_PROC(glBufferStorage);
		HOOK_PROC(glBindBuffersBase);
		HOOK_PROC(glBindBuffersRange);
		HOOK_PROC(glBindTextures);
		HOOK_PROC(glBindImageTextures);
		HOOK_PROC(glBindVertexBuffers);
#endif
#ifdef GL_VERSION_4_5
		HOOK_PROC(glTextureBuffer);
		HOOK_PROC(glTextureBufferRange);
		HOOK_PROC(glNamedBufferData);
		HOOK_PROC(glNamedBufferStorage);
		HOOK_PROC(glTextureStorage1D);
		HOOK_PROC(glTextureStorage2D);
		HOOK_PROC(glTextureStorage2DMultisample);
		HOOK_PROC(glTextureStorage3D);
		HOOK_PROC(glTextureStorage3DMultisample);
		HOOK_PROC(glNamedBufferSubData);
		HOOK_PROC(glTextureSubImage1D);
		HOOK_PROC(glTextureSubImage2D);
		HOOK_PROC(glTextureSubImage3D);
		HOOK_PROC(glCompressedTextureSubImage1D);
		HOOK_PROC(glCompressedTextureSubImage2D);
		HOOK_PROC(glCompressedTextureSubImage3D);
		HOOK_PROC(glCopyTextureSubImage1D);
		HOOK_PROC(glCopyTextureSubImage2D);
		HOOK_PROC(glCopyTextureSubImage3D);
		HOOK_PROC(glMapNamedBuffer);
		HOOK_PROC(glMapNamedBufferRange);
		HOOK_PROC(glUnmapNamedBuffer);
		HOOK_PROC(glCopyNamedBufferSubData);
		HOOK_PROC(glNamedRenderbufferStorage);
		HOOK_PROC(glNamedRenderbufferStorageMultisample);
		HOOK_PROC(glClearNamedFramebufferiv);
		HOOK_PROC(glClearNamedFramebufferuiv);
		HOOK_PROC(glClearNamedFramebufferfv);
		HOOK_PROC(glClearNamedFramebufferfi);
		HOOK_PROC(glBlitNamedFramebuffer);
		HOOK_PROC(glGenerateTextureMipmap);
		HOOK_PROC(glBindTextureUnit);
#endif

		// GL_ARB_vertex_program / GL_ARB_fragment_program
		HOOK_PROC(glBindProgramARB);
		HOOK_PROC(glProgramStringARB);
		HOOK_PROC(glDeleteProgramsARB);

		// GL_EXT_framebuffer_object
		HOOK_PROC(glBindFramebufferEXT);

		// GL_EXT_direct_state_access
		HOOK_PROC(glBindMultiTextureEXT);
#endif

		// WGL_ARB_create_context
		HOOK_PROC(wglCreateContextAttribsARB);

		// WGL_ARB_pbuffer
		HOOK_PROC(wglCreatePbufferARB);
		HOOK_PROC(wglDestroyPbufferARB);
		HOOK_PROC(wglGetPbufferDCARB);
		HOOK_PROC(wglReleasePbufferDCARB);

		// WGL_ARB_pixel_format
		HOOK_PROC(wglChoosePixelFormatARB);
		HOOK_PROC(wglGetPixelFormatAttribivARB);
		HOOK_PROC(wglGetPixelFormatAttribfvARB);

		// Install all OpenGL hooks in a single batch job
		reshade::hook::apply_queued_actions();

		s_hooks_installed = true;
	}

	return proc_address;
}
