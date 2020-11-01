/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "runtime_gl.hpp"
#include "opengl_hooks.hpp"
#include <mutex>
#include <memory>
#include <cassert>
#include <unordered_map>
#include <unordered_set>

DECLARE_HANDLE(HPBUFFERARB);

static std::mutex s_mutex;
static std::unordered_set<HDC> s_pbuffer_device_contexts;
static std::unordered_set<HGLRC> s_legacy_contexts;
static std::unordered_map<HGLRC, HGLRC> s_shared_contexts;
static std::unordered_map<HGLRC, reshade::opengl::runtime_gl *> s_opengl_runtimes;
thread_local reshade::opengl::runtime_gl *g_current_runtime = nullptr;

HOOK_EXPORT int   WINAPI wglChoosePixelFormat(HDC hdc, const PIXELFORMATDESCRIPTOR *ppfd)
{
	LOG(INFO) << "Redirecting " << "wglChoosePixelFormat" << '(' << "hdc = " << hdc << ", ppfd = " << ppfd << ')' << " ...";

	assert(ppfd != nullptr);

	LOG(INFO) << "> Dumping pixel format descriptor:";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Name                                    | Value                                   |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Flags                                   | " << std::setw(39) << std::hex << ppfd->dwFlags << std::dec << " |";
	LOG(INFO) << "  | ColorBits                               | " << std::setw(39) << static_cast<unsigned int>(ppfd->cColorBits) << " |";
	LOG(INFO) << "  | DepthBits                               | " << std::setw(39) << static_cast<unsigned int>(ppfd->cDepthBits) << " |";
	LOG(INFO) << "  | StencilBits                             | " << std::setw(39) << static_cast<unsigned int>(ppfd->cStencilBits) << " |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

	if (ppfd->iLayerType != PFD_MAIN_PLANE || ppfd->bReserved != 0)
	{
		LOG(ERROR) << "Layered OpenGL contexts of type " << static_cast<int>(ppfd->iLayerType) << " are not supported.";
		SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}
	else if ((ppfd->dwFlags & PFD_DOUBLEBUFFER) == 0)
	{
		LOG(WARN) << "Single buffered OpenGL contexts are not supported.";
	}

	// Note: Windows calls into 'wglDescribePixelFormat' repeatedly from this, so make sure it reports correct results
	const int format = reshade::hooks::call(wglChoosePixelFormat)(hdc, ppfd);
	if (format != 0)
		LOG(INFO) << "> Returning pixel format: " << format;
	else
		LOG(WARN) << "wglChoosePixelFormat" << " failed with error code " << (GetLastError() & 0xFFFF) << '!';

	return format;
}
			BOOL  WINAPI wglChoosePixelFormatARB(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats)
{
	LOG(INFO) << "Redirecting " << "wglChoosePixelFormatARB" << '(' << "hdc = " << hdc << ", piAttribIList = " << piAttribIList << ", pfAttribFList = " << pfAttribFList << ", nMaxFormats = " << nMaxFormats << ", piFormats = " << piFormats << ", nNumFormats = " << nNumFormats << ')' << " ...";

	struct attribute
	{
		enum
		{
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
			WGL_SAMPLE_BUFFERS_ARB = 0x2041,
			WGL_SAMPLES_ARB = 0x2042,
			WGL_DRAW_TO_PBUFFER_ARB = 0x202D,
			WGL_BIND_TO_TEXTURE_RGB_ARB = 0x2070,
			WGL_BIND_TO_TEXTURE_RGBA_ARB = 0x2071,

			WGL_NO_ACCELERATION_ARB = 0x2025,
			WGL_GENERIC_ACCELERATION_ARB = 0x2026,
			WGL_FULL_ACCELERATION_ARB = 0x2027,
			WGL_SWAP_EXCHANGE_ARB = 0x2028,
			WGL_SWAP_COPY_ARB = 0x2029,
			WGL_SWAP_UNDEFINED_ARB = 0x202A,
			WGL_TYPE_RGBA_ARB = 0x202B,
			WGL_TYPE_COLORINDEX_ARB = 0x202C,
		};
	};

	bool layerplanes = false, doublebuffered = false;

	LOG(INFO) << "> Dumping attributes:";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Attribute                               | Value                                   |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

	for (const int *attrib = piAttribIList; attrib != nullptr && *attrib != 0; attrib += 2)
	{
		switch (attrib[0])
		{
		case attribute::WGL_DRAW_TO_WINDOW_ARB:
			LOG(INFO) << "  | WGL_DRAW_TO_WINDOW_ARB                  | " << std::setw(39) << (attrib[1] != FALSE ? "TRUE" : "FALSE") << " |";
			break;
		case attribute::WGL_DRAW_TO_BITMAP_ARB:
			LOG(INFO) << "  | WGL_DRAW_TO_BITMAP_ARB                  | " << std::setw(39) << (attrib[1] != FALSE ? "TRUE" : "FALSE") << " |";
			break;
		case attribute::WGL_ACCELERATION_ARB:
			LOG(INFO) << "  | WGL_ACCELERATION_ARB                    | " << std::setw(39) << std::hex << attrib[1] << std::dec << " |";
			break;
		case attribute::WGL_SWAP_LAYER_BUFFERS_ARB:
			layerplanes = layerplanes || attrib[1] != FALSE;
			LOG(INFO) << "  | WGL_SWAP_LAYER_BUFFERS_ARB              | " << std::setw(39) << (attrib[1] != FALSE ? "TRUE" : "FALSE") << " |";
			break;
		case attribute::WGL_SWAP_METHOD_ARB:
			LOG(INFO) << "  | WGL_SWAP_METHOD_ARB                     | " << std::setw(39) << std::hex << attrib[1] << std::dec << " |";
			break;
		case attribute::WGL_NUMBER_OVERLAYS_ARB:
			layerplanes = layerplanes || attrib[1] != 0;
			LOG(INFO) << "  | WGL_NUMBER_OVERLAYS_ARB                 | " << std::setw(39) << attrib[1] << " |";
			break;
		case attribute::WGL_NUMBER_UNDERLAYS_ARB:
			layerplanes = layerplanes || attrib[1] != 0;
			LOG(INFO) << "  | WGL_NUMBER_UNDERLAYS_ARB                | " << std::setw(39) << attrib[1] << " |";
			break;
		case attribute::WGL_SUPPORT_GDI_ARB:
			LOG(INFO) << "  | WGL_SUPPORT_GDI_ARB                     | " << std::setw(39) << (attrib[1] != FALSE ? "TRUE" : "FALSE") << " |";
			break;
		case attribute::WGL_SUPPORT_OPENGL_ARB:
			LOG(INFO) << "  | WGL_SUPPORT_OPENGL_ARB                  | " << std::setw(39) << (attrib[1] != FALSE ? "TRUE" : "FALSE") << " |";
			break;
		case attribute::WGL_DOUBLE_BUFFER_ARB:
			doublebuffered = attrib[1] != FALSE;
			LOG(INFO) << "  | WGL_DOUBLE_BUFFER_ARB                   | " << std::setw(39) << (attrib[1] != FALSE ? "TRUE" : "FALSE") << " |";
			break;
		case attribute::WGL_STEREO_ARB:
			LOG(INFO) << "  | WGL_STEREO_ARB                          | " << std::setw(39) << (attrib[1] != FALSE ? "TRUE" : "FALSE") << " |";
			break;
		case attribute::WGL_RED_BITS_ARB:
			LOG(INFO) << "  | WGL_RED_BITS_ARB                        | " << std::setw(39) << attrib[1] << " |";
			break;
		case attribute::WGL_GREEN_BITS_ARB:
			LOG(INFO) << "  | WGL_GREEN_BITS_ARB                      | " << std::setw(39) << attrib[1] << " |";
			break;
		case attribute::WGL_BLUE_BITS_ARB:
			LOG(INFO) << "  | WGL_BLUE_BITS_ARB                       | " << std::setw(39) << attrib[1] << " |";
			break;
		case attribute::WGL_ALPHA_BITS_ARB:
			LOG(INFO) << "  | WGL_ALPHA_BITS_ARB                      | " << std::setw(39) << attrib[1] << " |";
			break;
		case attribute::WGL_COLOR_BITS_ARB:
			LOG(INFO) << "  | WGL_COLOR_BITS_ARB                      | " << std::setw(39) << attrib[1] << " |";
			break;
		case attribute::WGL_DEPTH_BITS_ARB:
			LOG(INFO) << "  | WGL_DEPTH_BITS_ARB                      | " << std::setw(39) << attrib[1] << " |";
			break;
		case attribute::WGL_STENCIL_BITS_ARB:
			LOG(INFO) << "  | WGL_STENCIL_BITS_ARB                    | " << std::setw(39) << attrib[1] << " |";
			break;
		case attribute::WGL_SAMPLE_BUFFERS_ARB:
			LOG(INFO) << "  | WGL_SAMPLE_BUFFERS_ARB                  | " << std::setw(39) << attrib[1] << " |";
			break;
		case attribute::WGL_SAMPLES_ARB:
			LOG(INFO) << "  | WGL_SAMPLES_ARB                         | " << std::setw(39) << attrib[1] << " |";
			break;
		case attribute::WGL_DRAW_TO_PBUFFER_ARB:
			LOG(INFO) << "  | WGL_DRAW_TO_PBUFFER_ARB                 | " << std::setw(39) << (attrib[1] != FALSE ? "TRUE" : "FALSE") << " |";
			break;
		default:
			LOG(INFO) << "  | " << std::hex << std::setw(39) << attrib[0] << " | " << std::setw(39) << attrib[1] << std::dec << " |";
			break;
		}
	}

	for (const FLOAT *attrib = pfAttribFList; attrib != nullptr && *attrib != 0.0f; attrib += 2)
	{
		LOG(INFO) << "  | " << std::hex << std::setw(39) << static_cast<int>(attrib[0]) << " | " << std::setw(39) << attrib[1] << std::dec << " |";
	}

	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

	if (layerplanes)
	{
		LOG(ERROR) << "Layered OpenGL contexts are not supported.";
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	else if (!doublebuffered)
	{
		LOG(WARN) << "Single buffered OpenGL contexts are not supported.";
	}

	if (!reshade::hooks::call(wglChoosePixelFormatARB)(hdc, piAttribIList, pfAttribFList, nMaxFormats, piFormats, nNumFormats))
	{
		LOG(WARN) << "wglChoosePixelFormatARB" << " failed with error code " << (GetLastError() & 0xFFFF) << '!';
		return FALSE;
	}

	assert(nNumFormats != nullptr);

	if (*nNumFormats < nMaxFormats)
		nMaxFormats = *nNumFormats;

	std::string formats;
	for (UINT i = 0; i < nMaxFormats; ++i)
	{
		assert(piFormats[i] != 0);
		formats += " " + std::to_string(piFormats[i]);
	}

	LOG(INFO) << "> Returning pixel format(s):" << formats;

	return TRUE;
}
HOOK_EXPORT int   WINAPI wglGetPixelFormat(HDC hdc)
{
	return reshade::hooks::call(wglGetPixelFormat)(hdc);
}
			BOOL  WINAPI wglGetPixelFormatAttribivARB(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, int *piValues)
{
	if (iLayerPlane != 0)
	{
		LOG(WARN) << "Access to layer plane at index " << iLayerPlane << " is unsupported.";
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	return reshade::hooks::call(wglGetPixelFormatAttribivARB)(hdc, iPixelFormat, 0, nAttributes, piAttributes, piValues);
}
			BOOL  WINAPI wglGetPixelFormatAttribfvARB(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, FLOAT *pfValues)
{
	if (iLayerPlane != 0)
	{
		LOG(WARN) << "Access to layer plane at index " << iLayerPlane << " is unsupported.";
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	return reshade::hooks::call(wglGetPixelFormatAttribfvARB)(hdc, iPixelFormat, 0, nAttributes, piAttributes, pfValues);
}
HOOK_EXPORT BOOL  WINAPI wglSetPixelFormat(HDC hdc, int iPixelFormat, const PIXELFORMATDESCRIPTOR *ppfd)
{
	LOG(INFO) << "Redirecting " << "wglSetPixelFormat" << '(' << "hdc = " << hdc << ", iPixelFormat = " << iPixelFormat << ", ppfd = " << ppfd << ')' << " ...";

	if (!reshade::hooks::call(wglSetPixelFormat)(hdc, iPixelFormat, ppfd))
	{
		LOG(WARN) << "wglSetPixelFormat" << " failed with error code " << (GetLastError() & 0xFFFF) << '!';
		return FALSE;
	}

	if (GetPixelFormat(hdc) == 0)
	{
		LOG(WARN) << "Application mistakenly called wglSetPixelFormat directly. Passing on to SetPixelFormat ...";

		SetPixelFormat(hdc, iPixelFormat, ppfd);
	}

	return TRUE;
}
HOOK_EXPORT int   WINAPI wglDescribePixelFormat(HDC hdc, int iPixelFormat, UINT nBytes, LPPIXELFORMATDESCRIPTOR ppfd)
{
	return reshade::hooks::call(wglDescribePixelFormat)(hdc, iPixelFormat, nBytes, ppfd);
}

HOOK_EXPORT BOOL  WINAPI wglDescribeLayerPlane(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nBytes, LPLAYERPLANEDESCRIPTOR plpd)
{
	LOG(INFO) << "Redirecting " << "wglDescribeLayerPlane" << '(' << "hdc = " << hdc << ", iPixelFormat = " << iPixelFormat << ", iLayerPlane = " << iLayerPlane << ", nBytes = " << nBytes << ", plpd = " << plpd << ')' << " ...";
	LOG(WARN) << "Access to layer plane at index " << iLayerPlane << " is unsupported.";

	SetLastError(ERROR_NOT_SUPPORTED);
	return FALSE;
}
HOOK_EXPORT BOOL  WINAPI wglRealizeLayerPalette(HDC hdc, int iLayerPlane, BOOL b)
{
	LOG(INFO) << "Redirecting " << "wglRealizeLayerPalette" << '(' << "hdc = " << hdc << ", iLayerPlane = " << iLayerPlane << ", b = " << (b ? "TRUE" : "FALSE") << ')' << " ...";
	LOG(WARN) << "Access to layer plane at index " << iLayerPlane << " is unsupported.";

	SetLastError(ERROR_NOT_SUPPORTED);
	return FALSE;
}
HOOK_EXPORT int   WINAPI wglGetLayerPaletteEntries(HDC hdc, int iLayerPlane, int iStart, int cEntries, COLORREF *pcr)
{
	LOG(INFO) << "Redirecting " << "wglGetLayerPaletteEntries" << '(' << "hdc = " << hdc << ", iLayerPlane = " << iLayerPlane << ", iStart = " << iStart << ", cEntries = " << cEntries << ", pcr = " << pcr << ')' << " ...";
	LOG(WARN) << "Access to layer plane at index " << iLayerPlane << " is unsupported.";

	SetLastError(ERROR_NOT_SUPPORTED);
	return 0;
}
HOOK_EXPORT int   WINAPI wglSetLayerPaletteEntries(HDC hdc, int iLayerPlane, int iStart, int cEntries, const COLORREF *pcr)
{
	LOG(INFO) << "Redirecting " << "wglSetLayerPaletteEntries" << '(' << "hdc = " << hdc << ", iLayerPlane = " << iLayerPlane << ", iStart = " << iStart << ", cEntries = " << cEntries << ", pcr = " << pcr << ')' << " ...";
	LOG(WARN) << "Access to layer plane at index " << iLayerPlane << " is unsupported.";

	SetLastError(ERROR_NOT_SUPPORTED);
	return 0;
}

HOOK_EXPORT HGLRC WINAPI wglCreateContext(HDC hdc)
{
	LOG(INFO) << "Redirecting " << "wglCreateContext" << '(' << "hdc = " << hdc << ')' << " ...";
	LOG(INFO) << "> Passing on to " << "wglCreateLayerContext" << ':';

	const HGLRC hglrc = wglCreateLayerContext(hdc, 0);
	if (hglrc == nullptr)
	{
		return nullptr;
	}

	// Keep track of legacy contexts here instead of in 'wglCreateLayerContext' because some drivers call the latter from within their 'wglCreateContextAttribsARB' implementation
	{ const std::lock_guard<std::mutex> lock(s_mutex);
		s_legacy_contexts.emplace(hglrc);
	}

	return hglrc;
}
			HGLRC WINAPI wglCreateContextAttribsARB(HDC hdc, HGLRC hShareContext, const int *piAttribList)
{
	LOG(INFO) << "Redirecting " << "wglCreateContextAttribsARB" << '(' << "hdc = " << hdc << ", hShareContext = " << hShareContext << ", piAttribList = " << piAttribList << ')' << " ...";

	struct attribute
	{
		enum
		{
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

	int i = 0, major = 1, minor = 0, flags = 0;
	bool compatibility = false;
	attribute attribs[8] = {};

	for (const int *attrib = piAttribList; attrib != nullptr && *attrib != 0 && i < 5; attrib += 2, ++i)
	{
		attribs[i].name = attrib[0];
		attribs[i].value = attrib[1];

		switch (attrib[0])
		{
		case attribute::WGL_CONTEXT_MAJOR_VERSION_ARB:
			major = attrib[1];
			break;
		case attribute::WGL_CONTEXT_MINOR_VERSION_ARB:
			minor = attrib[1];
			break;
		case attribute::WGL_CONTEXT_FLAGS_ARB:
			flags = attrib[1];
			break;
		case attribute::WGL_CONTEXT_PROFILE_MASK_ARB:
			compatibility = (attrib[1] & attribute::WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB) != 0;
			break;
		}
	}

	if (major < 3 || (major == 3 && minor < 2))
		compatibility = true;

#ifndef NDEBUG
	flags |= attribute::WGL_CONTEXT_DEBUG_BIT_ARB;
#endif

	// This works because the specs specifically note that "If an attribute is specified more than once, then the last value specified is used."
	attribs[i].name = attribute::WGL_CONTEXT_FLAGS_ARB;
	attribs[i++].value = flags;
	attribs[i].name = attribute::WGL_CONTEXT_PROFILE_MASK_ARB;
	attribs[i++].value = compatibility ? attribute::WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB : attribute::WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

	LOG(INFO) << "> Requesting " << (compatibility ? "compatibility" : "core") << " OpenGL context for version " << major << '.' << minor << " ...";

	if (major < 4 || (major == 4 && minor < 3))
	{
		LOG(INFO) << "> Replacing requested version with 4.3 ...";
	
		for (int k = 0; k < i; ++k)
		{
			switch (attribs[k].name)
			{
			case attribute::WGL_CONTEXT_MAJOR_VERSION_ARB:
				attribs[k].value = 4;
				break;
			case attribute::WGL_CONTEXT_MINOR_VERSION_ARB:
				attribs[k].value = 3;
				break;
			case attribute::WGL_CONTEXT_PROFILE_MASK_ARB:
				attribs[k].value = attribute::WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
				break;
			}
		}
	}

	const HGLRC hglrc = reshade::hooks::call(wglCreateContextAttribsARB)(hdc, hShareContext, reinterpret_cast<const int *>(attribs));
	if (hglrc == nullptr)
	{
		LOG(WARN) << "wglCreateContextAttribsARB" << " failed with error code " << (GetLastError() & 0xFFFF) << '!';
		return nullptr;
	}

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		s_shared_contexts.emplace(hglrc, hShareContext);

		if (hShareContext != nullptr)
		{
			// Find root share context
			auto it = s_shared_contexts.find(hShareContext);

			while (it != s_shared_contexts.end() && it->second != nullptr)
			{
				it = s_shared_contexts.find(s_shared_contexts.at(hglrc) = it->second);
			}
		}
	}

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "> Returning OpenGL context " << hglrc << '.';
#endif
	return hglrc;
}
HOOK_EXPORT HGLRC WINAPI wglCreateLayerContext(HDC hdc, int iLayerPlane)
{
	LOG(INFO) << "Redirecting " << "wglCreateLayerContext" << '(' << "hdc = " << hdc << ", iLayerPlane = " << iLayerPlane << ')' << " ...";

	if (iLayerPlane != 0)
	{
		LOG(WARN) << "Access to layer plane at index " << iLayerPlane << " is unsupported.";
		SetLastError(ERROR_INVALID_PARAMETER);
		return nullptr;
	}

	const HGLRC hglrc = reshade::hooks::call(wglCreateLayerContext)(hdc, iLayerPlane);
	if (hglrc == nullptr)
	{
		LOG(WARN) << "wglCreateLayerContext" << " failed with error code " << (GetLastError() & 0xFFFF) << '!';
		return nullptr;
	}

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		s_shared_contexts.emplace(hglrc, nullptr);
	}

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "> Returning OpenGL context " << hglrc << '.';
#endif
	return hglrc;
}
HOOK_EXPORT BOOL  WINAPI wglCopyContext(HGLRC hglrcSrc, HGLRC hglrcDst, UINT mask)
{
	return reshade::hooks::call(wglCopyContext)(hglrcSrc, hglrcDst, mask);
}
HOOK_EXPORT BOOL  WINAPI wglDeleteContext(HGLRC hglrc)
{
	LOG(INFO) << "Redirecting " << "wglDeleteContext" << '(' << "hglrc = " << hglrc << ')' << " ...";

	if (const auto it = s_opengl_runtimes.find(hglrc);
		it != s_opengl_runtimes.end())
	{
#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "> Cleaning up runtime " << it->second << " ...";
#endif

		// Set the render context current so its resources can be cleaned up
		const HGLRC prev_hglrc = wglGetCurrentContext();
		if (hglrc == prev_hglrc)
		{
			it->second->on_reset();

			delete it->second;
		}
		else
		{
			// Choose a device context to make current with
			HDC hdc = *it->second->_hdcs.begin();
			const HDC prev_hdc = wglGetCurrentDC();

			// In case the original was destroyed already, create a dummy window to get a valid context
			HWND dummy_window_handle = nullptr;
			if (!WindowFromDC(hdc))
			{
				dummy_window_handle = CreateWindow(TEXT("Static"), nullptr, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr, 0);
				hdc = GetDC(dummy_window_handle);
				PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1 };
				pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
				reshade::hooks::call(wglSetPixelFormat)(hdc,
					reshade::hooks::call(wglChoosePixelFormat)(hdc, &pfd), &pfd);
			}

			if (reshade::hooks::call(wglMakeCurrent)(hdc, hglrc))
			{
				it->second->on_reset();

				delete it->second;

				// Restore previous device and render context
				reshade::hooks::call(wglMakeCurrent)(prev_hdc, prev_hglrc);
			}
			else
			{
				LOG(WARN) << "Unable to make context current (error code " << (GetLastError() & 0xFFFF) << "), leaking resources ...";
			}

			if (dummy_window_handle != nullptr)
				DestroyWindow(dummy_window_handle);
		}

		// Ensure the runtime is not still current after deleting
		if (it->second == g_current_runtime)
			g_current_runtime = nullptr;

		s_opengl_runtimes.erase(it);
	}

	{ const std::lock_guard<std::mutex> lock(s_mutex);
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
		LOG(WARN) << "wglDeleteContext" << " failed with error code " << (GetLastError() & 0xFFFF) << '!';
		return FALSE;
	}

	return TRUE;
}

HOOK_EXPORT BOOL  WINAPI wglShareLists(HGLRC hglrc1, HGLRC hglrc2)
{
	LOG(INFO) << "Redirecting " << "wglShareLists" << '(' << "hglrc1 = " << hglrc1 << ", hglrc2 = " << hglrc2 << ')' << " ...";

	if (!reshade::hooks::call(wglShareLists)(hglrc1, hglrc2))
	{
		LOG(WARN) << "wglShareLists" << " failed with error code " << (GetLastError() & 0xFFFF) << '!';
		return FALSE;
	}

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		s_shared_contexts[hglrc2] = hglrc1;
	}

	return TRUE;
}

HOOK_EXPORT BOOL  WINAPI wglMakeCurrent(HDC hdc, HGLRC hglrc)
{
	static const auto trampoline = reshade::hooks::call(wglMakeCurrent);

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Redirecting " << "wglMakeCurrent" << '(' << "hdc = " << hdc << ", hglrc = " << hglrc << ')' << " ...";
#endif

	const HGLRC prev_hglrc = wglGetCurrentContext();

	if (!trampoline(hdc, hglrc))
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "wglMakeCurrent" << " failed with error code " << (GetLastError() & 0xFFFF) << '!';
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
		g_current_runtime = nullptr;

		return TRUE;
	}

	const std::lock_guard<std::mutex> lock(s_mutex);

	if (const auto it = s_shared_contexts.find(hglrc);
		it != s_shared_contexts.end() && it->second != nullptr)
	{
		hglrc = it->second;

#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "> Using shared OpenGL context " << hglrc << '.';
#endif
	}

	if (const auto it = s_opengl_runtimes.find(hglrc);
		it != s_opengl_runtimes.end())
	{
		if (it->second != g_current_runtime)
		{
			g_current_runtime = it->second;

#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "> Switched to existing runtime " << it->second << '.';
#endif
		}

		// Keep track of all device contexts that were used with this render context
		// Do this outside the above if statement since the application may change the device context without changing the render context and thus the current runtime
		it->second->_hdcs.insert(hdc);
	}
	else
	{
		const HWND hwnd = WindowFromDC(hdc);

		if (hwnd == nullptr || s_pbuffer_device_contexts.find(hdc) != s_pbuffer_device_contexts.end())
		{
			g_current_runtime = nullptr;

			LOG(DEBUG) << "Skipping render context " << hglrc << " because there is no window associated with its device context " << hdc << '.';
			return TRUE;
		}
		else if ((GetClassLongPtr(hwnd, GCL_STYLE) & CS_OWNDC) == 0)
		{
			LOG(WARN) << "Window class style of window " << hwnd << " is missing 'CS_OWNDC' flag.";
		}

		// Load original OpenGL functions instead of using the hooked ones
		gl3wInit2([](const char *name) {
			extern std::filesystem::path get_system_path();
			// First attempt to load from the OpenGL ICD
			FARPROC address = reshade::hooks::call(wglGetProcAddress)(name);
			if (address == nullptr) address = GetProcAddress( // Load from the Windows OpenGL DLL if that fails
				GetModuleHandleW((get_system_path() / "opengl32.dll").c_str()), name);
			return reinterpret_cast<GL3WglProc>(address);
		});

		if (gl3wIsSupported(4, 3))
		{
			const auto runtime = new reshade::opengl::runtime_gl();
			runtime->_hdcs.insert(hdc);

			// Always set compatibility context flag on contexts that were created with 'wglCreateContext' instead of 'wglCreateContextAttribsARB'
			// This is necessary because with some pixel formats the 'GL_ARB_compatibility' extension is not exposed even though the context was not created with the core profile
			if (s_legacy_contexts.find(hglrc) != s_legacy_contexts.end())
				runtime->_compatibility_context = true;

			g_current_runtime = s_opengl_runtimes[hglrc] = runtime;

#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "> Switched to new runtime " << runtime << '.';
#endif
		}
		else
		{
			LOG(ERROR) << "Your graphics card does not seem to support OpenGL 4.3. Initialization failed.";

			g_current_runtime = nullptr;
		}
	}

	return TRUE;
}

HOOK_EXPORT HDC   WINAPI wglGetCurrentDC()
{
	static const auto trampoline = reshade::hooks::call(wglGetCurrentDC);
	return trampoline();
}
HOOK_EXPORT HGLRC WINAPI wglGetCurrentContext()
{
	static const auto trampoline = reshade::hooks::call(wglGetCurrentContext);
	return trampoline();
}

	  HPBUFFERARB WINAPI wglCreatePbufferARB(HDC hdc, int iPixelFormat, int iWidth, int iHeight, const int *piAttribList)
{
	LOG(INFO) << "Redirecting " << "wglCreatePbufferARB" << '(' << "hdc = " << hdc << ", iPixelFormat = " << iPixelFormat << ", iWidth = " << iWidth << ", iHeight = " << iHeight << ", piAttribList = " << piAttribList << ')' << " ...";

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

	LOG(INFO) << "> Dumping attributes:";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Attribute                               | Value                                   |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

	for (const int *attrib = piAttribList; attrib != nullptr && *attrib != 0; attrib += 2)
	{
		switch (attrib[0])
		{
		case attribute::WGL_PBUFFER_LARGEST_ARB:
			LOG(INFO) << "  | WGL_PBUFFER_LARGEST_ARB                 | " << std::setw(39) << (attrib[1] != FALSE ? "TRUE" : "FALSE") << " |";
			break;
		case attribute::WGL_TEXTURE_FORMAT_ARB:
			LOG(INFO) << "  | WGL_TEXTURE_FORMAT_ARB                  | " << std::setw(39) << std::hex << attrib[1] << std::dec << " |";
			break;
		case attribute::WGL_TEXTURE_TARGET_ARB:
			LOG(INFO) << "  | WGL_TEXTURE_TARGET_ARB                  | " << std::setw(39) << std::hex << attrib[1] << std::dec << " |";
			break;
		default:
			LOG(INFO) << "  | " << std::hex << std::setw(39) << attrib[0] << " | " << std::setw(39) << attrib[1] << std::dec << " |";
			break;
		}
	}

	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

	const HPBUFFERARB hpbuffer = reshade::hooks::call(wglCreatePbufferARB)(hdc, iPixelFormat, iWidth, iHeight, piAttribList);
	if (hpbuffer == nullptr)
	{
		LOG(WARN) << "wglCreatePbufferARB" << " failed with error code " << (GetLastError() & 0xFFFF) << '!';
		return nullptr;
	}

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "> Returning pixel buffer " << hpbuffer << '.';
#endif
	return hpbuffer;
}
			BOOL  WINAPI wglDestroyPbufferARB(HPBUFFERARB hPbuffer)
{
	LOG(INFO) << "Redirecting " << "wglDestroyPbufferARB" << '(' << "hPbuffer = " << hPbuffer << ')' << " ...";

	if (!reshade::hooks::call(wglDestroyPbufferARB)(hPbuffer))
	{
		LOG(WARN) << "wglDestroyPbufferARB" << " failed with error code " << (GetLastError() & 0xFFFF) << '!';
		return FALSE;
	}

	return TRUE;
}
			BOOL  WINAPI wglQueryPbufferARB(HPBUFFERARB hPbuffer, int iAttribute, int *piValue)
{
	return reshade::hooks::call(wglQueryPbufferARB)(hPbuffer, iAttribute, piValue);
}
			HDC   WINAPI wglGetPbufferDCARB(HPBUFFERARB hPbuffer)
{
	LOG(INFO) << "Redirecting " << "wglGetPbufferDCARB" << '(' << "hPbuffer = " << hPbuffer << ')' << " ...";

	const HDC hdc = reshade::hooks::call(wglGetPbufferDCARB)(hPbuffer);
	if (hdc == nullptr)
	{
		LOG(WARN) << "wglGetPbufferDCARB" << " failed with error code " << (GetLastError() & 0xFFFF) << '!';
		return nullptr;
	}

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		s_pbuffer_device_contexts.insert(hdc);
	}

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "> Returning pixel buffer device context " << hdc << '.';
#endif
	return hdc;
}
			int   WINAPI wglReleasePbufferDCARB(HPBUFFERARB hPbuffer, HDC hdc)
{
	LOG(INFO) << "Redirecting " << "wglReleasePbufferDCARB" << '(' << "hPbuffer = " << hPbuffer << ')' << " ...";

	if (!reshade::hooks::call(wglReleasePbufferDCARB)(hPbuffer, hdc))
	{
		LOG(WARN) << "wglReleasePbufferDCARB" << " failed with error code " << (GetLastError() & 0xFFFF) << '!';
		return FALSE;
	}

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		s_pbuffer_device_contexts.erase(hdc);
	}

	return TRUE;
}

			BOOL  WINAPI wglSwapIntervalEXT(int interval)
{
	static const auto trampoline = reshade::hooks::call(wglSwapIntervalEXT);
	return trampoline(interval);
}
			int   WINAPI wglGetSwapIntervalEXT()
{
	static const auto trampoline = reshade::hooks::call(wglGetSwapIntervalEXT);
	return trampoline();
}

HOOK_EXPORT BOOL  WINAPI wglSwapBuffers(HDC hdc)
{
	static const auto trampoline = reshade::hooks::call(wglSwapBuffers);

	reshade::opengl::runtime_gl *runtime = g_current_runtime;
	if (runtime == nullptr || 0 == runtime->_hdcs.count(hdc))
	{
		// Find the runtime that is associated with this device context
		const auto it = std::find_if(s_opengl_runtimes.begin(), s_opengl_runtimes.end(),
			[hdc](const std::pair<HGLRC, reshade::opengl::runtime_gl *> &it) { return it.second->_hdcs.count(hdc); });
		runtime = it != s_opengl_runtimes.end() ? it->second : nullptr;
	}

	// The window handle can be invalid if the window was already destroyed
	if (const HWND hwnd = WindowFromDC(hdc);
		hwnd != nullptr && runtime != nullptr)
	{
		RECT rect = { 0, 0, 0, 0 };
		GetClientRect(hwnd, &rect);

		const auto width = static_cast<unsigned int>(rect.right);
		const auto height = static_cast<unsigned int>(rect.bottom);

		if (width != runtime->frame_width() || height != runtime->frame_height())
		{
			LOG(INFO) << "Resizing runtime " << runtime << " on device context " << hdc << " to " << width << "x" << height << " ...";

			runtime->on_reset();

			if (!(width == 0 && height == 0) && !runtime->on_init(hwnd, width, height))
				LOG(ERROR) << "Failed to recreate OpenGL runtime environment on runtime " << runtime << '.';
		}

		// Assume that the correct OpenGL context is still current here
		runtime->on_present();
	}

	return trampoline(hdc);
}
HOOK_EXPORT BOOL  WINAPI wglSwapLayerBuffers(HDC hdc, UINT i)
{
	if (i != WGL_SWAP_MAIN_PLANE)
	{
		const int index = i >= WGL_SWAP_UNDERLAY1 ? static_cast<int>(-std::log(i >> 16) / std::log(2) - 1) : static_cast<int>(std::log(i) / std::log(2));

#if RESHADE_VERBOSE_LOG
		LOG(INFO) << "Redirecting " << "wglSwapLayerBuffers" << '(' << "hdc = " << hdc << ", i = " << i << ')' << " ...";
		LOG(WARN) << "Access to layer plane at index " << index << " is unsupported.";
#endif

		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	return wglSwapBuffers(hdc);
}
HOOK_EXPORT DWORD WINAPI wglSwapMultipleBuffers(UINT cNumBuffers, const WGLSWAP *pBuffers)
{
	for (UINT i = 0; i < cNumBuffers; ++i)
	{
		assert(pBuffers != nullptr);

		wglSwapBuffers(pBuffers[i].hdc);
	}

	return 0;
}

HOOK_EXPORT BOOL  WINAPI wglUseFontBitmapsA(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3)
{
	static const auto trampoline = reshade::hooks::call(wglUseFontBitmapsA);
	return trampoline(hdc, dw1, dw2, dw3);
}
HOOK_EXPORT BOOL  WINAPI wglUseFontBitmapsW(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3)
{
	static const auto trampoline = reshade::hooks::call(wglUseFontBitmapsW);
	return trampoline(hdc, dw1, dw2, dw3);
}
HOOK_EXPORT BOOL  WINAPI wglUseFontOutlinesA(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3, FLOAT f1, FLOAT f2, int i, LPGLYPHMETRICSFLOAT pgmf)
{
	static const auto trampoline = reshade::hooks::call(wglUseFontOutlinesA);
	return trampoline(hdc, dw1, dw2, dw3, f1, f2, i, pgmf);
}
HOOK_EXPORT BOOL  WINAPI wglUseFontOutlinesW(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3, FLOAT f1, FLOAT f2, int i, LPGLYPHMETRICSFLOAT pgmf)
{
	static const auto trampoline = reshade::hooks::call(wglUseFontOutlinesW);
	return trampoline(hdc, dw1, dw2, dw3, f1, f2, i, pgmf);
}

HOOK_EXPORT PROC  WINAPI wglGetProcAddress(LPCSTR lpszProc)
{
	static const auto trampoline = reshade::hooks::call(wglGetProcAddress);
	const PROC address = trampoline(lpszProc);

	if (address == nullptr || lpszProc == nullptr)
		return nullptr;
	else if (0 == strcmp(lpszProc, "glBindTexture"))
		return reinterpret_cast<PROC>(glBindTexture);
	else if (0 == strcmp(lpszProc, "glBlendFunc"))
		return reinterpret_cast<PROC>(glBlendFunc);
	else if (0 == strcmp(lpszProc, "glClear"))
		return reinterpret_cast<PROC>(glClear);
	else if (0 == strcmp(lpszProc, "glClearColor"))
		return reinterpret_cast<PROC>(glClearColor);
	else if (0 == strcmp(lpszProc, "glClearDepth"))
		return reinterpret_cast<PROC>(glClearDepth);
	else if (0 == strcmp(lpszProc, "glClearStencil"))
		return reinterpret_cast<PROC>(glClearStencil);
	else if (0 == strcmp(lpszProc, "glCopyTexImage1D"))
		return reinterpret_cast<PROC>(glCopyTexImage1D);
	else if (0 == strcmp(lpszProc, "glCopyTexImage2D"))
		return reinterpret_cast<PROC>(glCopyTexImage2D);
	else if (0 == strcmp(lpszProc, "glCopyTexSubImage1D"))
		return reinterpret_cast<PROC>(glCopyTexSubImage1D);
	else if (0 == strcmp(lpszProc, "glCopyTexSubImage2D"))
		return reinterpret_cast<PROC>(glCopyTexSubImage2D);
	else if (0 == strcmp(lpszProc, "glCullFace"))
		return reinterpret_cast<PROC>(glCullFace);
	else if (0 == strcmp(lpszProc, "glDeleteTextures"))
		return reinterpret_cast<PROC>(glDeleteTextures);
	else if (0 == strcmp(lpszProc, "glDepthFunc"))
		return reinterpret_cast<PROC>(glDepthFunc);
	else if (0 == strcmp(lpszProc, "glDepthMask"))
		return reinterpret_cast<PROC>(glDepthMask);
	else if (0 == strcmp(lpszProc, "glDepthRange"))
		return reinterpret_cast<PROC>(glDepthRange);
	else if (0 == strcmp(lpszProc, "glDisable"))
		return reinterpret_cast<PROC>(glDisable);
	else if (0 == strcmp(lpszProc, "glDrawArrays"))
		return reinterpret_cast<PROC>(glDrawArrays);
	else if (0 == strcmp(lpszProc, "glDrawBuffer"))
		return reinterpret_cast<PROC>(glDrawBuffer);
	else if (0 == strcmp(lpszProc, "glDrawElements"))
		return reinterpret_cast<PROC>(glDrawElements);
	else if (0 == strcmp(lpszProc, "glEnable"))
		return reinterpret_cast<PROC>(glEnable);
	else if (0 == strcmp(lpszProc, "glFinish"))
		return reinterpret_cast<PROC>(glFinish);
	else if (0 == strcmp(lpszProc, "glFlush"))
		return reinterpret_cast<PROC>(glFlush);
	else if (0 == strcmp(lpszProc, "glFrontFace"))
		return reinterpret_cast<PROC>(glFrontFace);
	else if (0 == strcmp(lpszProc, "glGenTextures"))
		return reinterpret_cast<PROC>(glGenTextures);
	else if (0 == strcmp(lpszProc, "glGetBooleanv"))
		return reinterpret_cast<PROC>(glGetBooleanv);
	else if (0 == strcmp(lpszProc, "glGetDoublev"))
		return reinterpret_cast<PROC>(glGetDoublev);
	else if (0 == strcmp(lpszProc, "glGetFloatv"))
		return reinterpret_cast<PROC>(glGetFloatv);
	else if (0 == strcmp(lpszProc, "glGetIntegerv"))
		return reinterpret_cast<PROC>(glGetIntegerv);
	else if (0 == strcmp(lpszProc, "glGetError"))
		return reinterpret_cast<PROC>(glGetError);
	else if (0 == strcmp(lpszProc, "glGetPointerv"))
		return reinterpret_cast<PROC>(glGetPointerv);
	else if (0 == strcmp(lpszProc, "glGetString"))
		return reinterpret_cast<PROC>(glGetString);
	else if (0 == strcmp(lpszProc, "glGetTexImage"))
		return reinterpret_cast<PROC>(glGetTexImage);
	else if (0 == strcmp(lpszProc, "glGetTexLevelParameterfv"))
		return reinterpret_cast<PROC>(glGetTexLevelParameterfv);
	else if (0 == strcmp(lpszProc, "glGetTexLevelParameteriv"))
		return reinterpret_cast<PROC>(glGetTexLevelParameteriv);
	else if (0 == strcmp(lpszProc, "glGetTexParameterfv"))
		return reinterpret_cast<PROC>(glGetTexParameterfv);
	else if (0 == strcmp(lpszProc, "glGetTexParameteriv"))
		return reinterpret_cast<PROC>(glGetTexParameteriv);
	else if (0 == strcmp(lpszProc, "glHint"))
		return reinterpret_cast<PROC>(glHint);
	else if (0 == strcmp(lpszProc, "glIsEnabled"))
		return reinterpret_cast<PROC>(glIsEnabled);
	else if (0 == strcmp(lpszProc, "glIsTexture"))
		return reinterpret_cast<PROC>(glIsTexture);
	else if (0 == strcmp(lpszProc, "glLineWidth"))
		return reinterpret_cast<PROC>(glLineWidth);
	else if (0 == strcmp(lpszProc, "glLogicOp"))
		return reinterpret_cast<PROC>(glLogicOp);
	else if (0 == strcmp(lpszProc, "glPixelStoref"))
		return reinterpret_cast<PROC>(glPixelStoref);
	else if (0 == strcmp(lpszProc, "glPixelStorei"))
		return reinterpret_cast<PROC>(glPixelStorei);
	else if (0 == strcmp(lpszProc, "glPointSize"))
		return reinterpret_cast<PROC>(glPointSize);
	else if (0 == strcmp(lpszProc, "glPolygonMode"))
		return reinterpret_cast<PROC>(glPolygonMode);
	else if (0 == strcmp(lpszProc, "glPolygonOffset"))
		return reinterpret_cast<PROC>(glPolygonOffset);
	else if (0 == strcmp(lpszProc, "glReadBuffer"))
		return reinterpret_cast<PROC>(glReadBuffer);
	else if (0 == strcmp(lpszProc, "glReadPixels"))
		return reinterpret_cast<PROC>(glReadPixels);
	else if (0 == strcmp(lpszProc, "glScissor"))
		return reinterpret_cast<PROC>(glScissor);
	else if (0 == strcmp(lpszProc, "glStencilFunc"))
		return reinterpret_cast<PROC>(glStencilFunc);
	else if (0 == strcmp(lpszProc, "glStencilMask"))
		return reinterpret_cast<PROC>(glStencilMask);
	else if (0 == strcmp(lpszProc, "glStencilOp"))
		return reinterpret_cast<PROC>(glStencilOp);
	else if (0 == strcmp(lpszProc, "glTexImage1D"))
		return reinterpret_cast<PROC>(glTexImage1D);
	else if (0 == strcmp(lpszProc, "glTexImage2D"))
		return reinterpret_cast<PROC>(glTexImage2D);
	else if (0 == strcmp(lpszProc, "glTexParameterf"))
		return reinterpret_cast<PROC>(glTexParameterf);
	else if (0 == strcmp(lpszProc, "glTexParameterfv"))
		return reinterpret_cast<PROC>(glTexParameterfv);
	else if (0 == strcmp(lpszProc, "glTexParameteri"))
		return reinterpret_cast<PROC>(glTexParameteri);
	else if (0 == strcmp(lpszProc, "glTexParameteriv"))
		return reinterpret_cast<PROC>(glTexParameteriv);
	else if (0 == strcmp(lpszProc, "glTexSubImage1D"))
		return reinterpret_cast<PROC>(glTexSubImage1D);
	else if (0 == strcmp(lpszProc, "glTexSubImage2D"))
		return reinterpret_cast<PROC>(glTexSubImage2D);
	else if (0 == strcmp(lpszProc, "glViewport"))
		return reinterpret_cast<PROC>(glViewport);
	else if (static bool s_hooks_not_installed = true; s_hooks_not_installed)
	{
		// Install all OpenGL hooks in a single batch job
		reshade::hooks::install("glDeleteRenderbuffers", reinterpret_cast<reshade::hook::address>(trampoline("glDeleteRenderbuffers")), reinterpret_cast<reshade::hook::address>(glDeleteRenderbuffers), true);
		reshade::hooks::install("glDrawArraysIndirect", reinterpret_cast<reshade::hook::address>(trampoline("glDrawArraysIndirect")), reinterpret_cast<reshade::hook::address>(glDrawArraysIndirect), true);
		reshade::hooks::install("glDrawArraysInstanced", reinterpret_cast<reshade::hook::address>(trampoline("glDrawArraysInstanced")), reinterpret_cast<reshade::hook::address>(glDrawArraysInstanced), true);
		reshade::hooks::install("glDrawArraysInstancedARB", reinterpret_cast<reshade::hook::address>(trampoline("glDrawArraysInstancedARB")), reinterpret_cast<reshade::hook::address>(glDrawArraysInstancedARB), true);
		reshade::hooks::install("glDrawArraysInstancedEXT", reinterpret_cast<reshade::hook::address>(trampoline("glDrawArraysInstancedEXT")), reinterpret_cast<reshade::hook::address>(glDrawArraysInstancedEXT), true);
		reshade::hooks::install("glDrawArraysInstancedBaseInstance", reinterpret_cast<reshade::hook::address>(trampoline("glDrawArraysInstancedBaseInstance")), reinterpret_cast<reshade::hook::address>(glDrawArraysInstancedBaseInstance), true);
		reshade::hooks::install("glDrawElementsBaseVertex", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsBaseVertex")), reinterpret_cast<reshade::hook::address>(glDrawElementsBaseVertex), true);
		reshade::hooks::install("glDrawElementsIndirect", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsIndirect")), reinterpret_cast<reshade::hook::address>(glDrawElementsIndirect), true);
		reshade::hooks::install("glDrawElementsInstanced", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsInstanced")), reinterpret_cast<reshade::hook::address>(glDrawElementsInstanced), true);
		reshade::hooks::install("glDrawElementsInstancedARB", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsInstancedARB")), reinterpret_cast<reshade::hook::address>(glDrawElementsInstancedARB), true);
		reshade::hooks::install("glDrawElementsInstancedEXT", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsInstancedEXT")), reinterpret_cast<reshade::hook::address>(glDrawElementsInstancedEXT), true);
		reshade::hooks::install("glDrawElementsInstancedBaseVertex", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsInstancedBaseVertex")), reinterpret_cast<reshade::hook::address>(glDrawElementsInstancedBaseVertex), true);
		reshade::hooks::install("glDrawElementsInstancedBaseInstance", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsInstancedBaseInstance")), reinterpret_cast<reshade::hook::address>(glDrawElementsInstancedBaseInstance), true);
		reshade::hooks::install("glDrawElementsInstancedBaseVertexBaseInstance", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsInstancedBaseVertexBaseInstance")), reinterpret_cast<reshade::hook::address>(glDrawElementsInstancedBaseVertexBaseInstance), true);
		reshade::hooks::install("glDrawRangeElements", reinterpret_cast<reshade::hook::address>(trampoline("glDrawRangeElements")), reinterpret_cast<reshade::hook::address>(glDrawRangeElements), true);
		reshade::hooks::install("glDrawRangeElementsBaseVertex", reinterpret_cast<reshade::hook::address>(trampoline("glDrawRangeElementsBaseVertex")), reinterpret_cast<reshade::hook::address>(glDrawRangeElementsBaseVertex), true);
		reshade::hooks::install("glFramebufferRenderbuffer", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferRenderbuffer")), reinterpret_cast<reshade::hook::address>(glFramebufferRenderbuffer), true);
		reshade::hooks::install("glFramebufferRenderbufferEXT", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferRenderbufferEXT")), reinterpret_cast<reshade::hook::address>(glFramebufferRenderbufferEXT), true);
		reshade::hooks::install("glFramebufferTexture", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTexture")), reinterpret_cast<reshade::hook::address>(glFramebufferTexture), true);
		reshade::hooks::install("glFramebufferTextureARB", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTextureARB")), reinterpret_cast<reshade::hook::address>(glFramebufferTextureARB), true);
		reshade::hooks::install("glFramebufferTextureEXT", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTextureEXT")), reinterpret_cast<reshade::hook::address>(glFramebufferTextureEXT), true);
		reshade::hooks::install("glFramebufferTexture1D", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTexture1D")), reinterpret_cast<reshade::hook::address>(glFramebufferTexture1D), true);
		reshade::hooks::install("glFramebufferTexture1DEXT", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTexture1DEXT")), reinterpret_cast<reshade::hook::address>(glFramebufferTexture1DEXT), true);
		reshade::hooks::install("glFramebufferTexture2D", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTexture2D")), reinterpret_cast<reshade::hook::address>(glFramebufferTexture2D), true);
		reshade::hooks::install("glFramebufferTexture2DEXT", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTexture2DEXT")), reinterpret_cast<reshade::hook::address>(glFramebufferTexture2DEXT), true);
		reshade::hooks::install("glFramebufferTexture3D", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTexture3D")), reinterpret_cast<reshade::hook::address>(glFramebufferTexture3D), true);
		reshade::hooks::install("glFramebufferTexture3DEXT", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTexture3DEXT")), reinterpret_cast<reshade::hook::address>(glFramebufferTexture3DEXT), true);
		reshade::hooks::install("glFramebufferTextureLayer", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTextureLayer")), reinterpret_cast<reshade::hook::address>(glFramebufferTextureLayer), true);
		reshade::hooks::install("glFramebufferTextureLayerARB", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTextureLayerARB")), reinterpret_cast<reshade::hook::address>(glFramebufferTextureLayerARB), true);
		reshade::hooks::install("glFramebufferTextureLayerEXT", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTextureLayerEXT")), reinterpret_cast<reshade::hook::address>(glFramebufferTextureLayerEXT), true);
		reshade::hooks::install("glMultiDrawArrays", reinterpret_cast<reshade::hook::address>(trampoline("glMultiDrawArrays")), reinterpret_cast<reshade::hook::address>(glMultiDrawArrays), true);
		reshade::hooks::install("glMultiDrawArraysIndirect", reinterpret_cast<reshade::hook::address>(trampoline("glMultiDrawArraysIndirect")), reinterpret_cast<reshade::hook::address>(glMultiDrawArraysIndirect), true);
		reshade::hooks::install("glMultiDrawElements", reinterpret_cast<reshade::hook::address>(trampoline("glMultiDrawElements")), reinterpret_cast<reshade::hook::address>(glMultiDrawElements), true);
		reshade::hooks::install("glMultiDrawElementsBaseVertex", reinterpret_cast<reshade::hook::address>(trampoline("glMultiDrawElementsBaseVertex")), reinterpret_cast<reshade::hook::address>(glMultiDrawElementsBaseVertex), true);
		reshade::hooks::install("glMultiDrawElementsIndirect", reinterpret_cast<reshade::hook::address>(trampoline("glMultiDrawElementsIndirect")), reinterpret_cast<reshade::hook::address>(glMultiDrawElementsIndirect), true);
		reshade::hooks::install("glTexImage3D", reinterpret_cast<reshade::hook::address>(trampoline("glTexImage3D")), reinterpret_cast<reshade::hook::address>(glTexImage3D), true);

		reshade::hooks::install("wglChoosePixelFormatARB", reinterpret_cast<reshade::hook::address>(trampoline("wglChoosePixelFormatARB")), reinterpret_cast<reshade::hook::address>(wglChoosePixelFormatARB), true);
		reshade::hooks::install("wglCreateContextAttribsARB", reinterpret_cast<reshade::hook::address>(trampoline("wglCreateContextAttribsARB")), reinterpret_cast<reshade::hook::address>(wglCreateContextAttribsARB), true);
		reshade::hooks::install("wglCreatePbufferARB", reinterpret_cast<reshade::hook::address>(trampoline("wglCreatePbufferARB")), reinterpret_cast<reshade::hook::address>(wglCreatePbufferARB), true);
		reshade::hooks::install("wglDestroyPbufferARB", reinterpret_cast<reshade::hook::address>(trampoline("wglDestroyPbufferARB")), reinterpret_cast<reshade::hook::address>(wglDestroyPbufferARB), true);
		reshade::hooks::install("wglGetPbufferDCARB", reinterpret_cast<reshade::hook::address>(trampoline("wglGetPbufferDCARB")), reinterpret_cast<reshade::hook::address>(wglGetPbufferDCARB), true);
		reshade::hooks::install("wglGetPixelFormatAttribivARB", reinterpret_cast<reshade::hook::address>(trampoline("wglGetPixelFormatAttribivARB")), reinterpret_cast<reshade::hook::address>(wglGetPixelFormatAttribivARB), true);
		reshade::hooks::install("wglGetPixelFormatAttribfvARB", reinterpret_cast<reshade::hook::address>(trampoline("wglGetPixelFormatAttribfvARB")), reinterpret_cast<reshade::hook::address>(wglGetPixelFormatAttribfvARB), true);
		reshade::hooks::install("wglQueryPbufferARB", reinterpret_cast<reshade::hook::address>(trampoline("wglQueryPbufferARB")), reinterpret_cast<reshade::hook::address>(wglQueryPbufferARB), true);
		reshade::hooks::install("wglReleasePbufferDCARB", reinterpret_cast<reshade::hook::address>(trampoline("wglReleasePbufferDCARB")), reinterpret_cast<reshade::hook::address>(wglReleasePbufferDCARB), true);
		reshade::hooks::install("wglGetSwapIntervalEXT", reinterpret_cast<reshade::hook::address>(trampoline("wglGetSwapIntervalEXT")), reinterpret_cast<reshade::hook::address>(wglGetSwapIntervalEXT), true);
		reshade::hooks::install("wglSwapIntervalEXT", reinterpret_cast<reshade::hook::address>(trampoline("wglSwapIntervalEXT")), reinterpret_cast<reshade::hook::address>(wglSwapIntervalEXT), true);

		reshade::hook::apply_queued_actions();

		s_hooks_not_installed = false;
	}

	return address;
}
