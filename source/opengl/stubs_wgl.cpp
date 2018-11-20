/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "runtime_opengl.hpp"
#include "opengl_stubs_internal.hpp"
#include <assert.h>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <unordered_set>

DECLARE_HANDLE(HPBUFFERARB);

static std::mutex s_mutex;
static std::unordered_map<HWND, RECT> s_window_rects;
static std::unordered_set<HDC> s_pbuffer_device_contexts;
static std::unordered_map<HGLRC, HGLRC> s_shared_contexts;
std::unordered_map<HDC, reshade::opengl::runtime_opengl *> g_opengl_runtimes;

HOOK_EXPORT int   WINAPI wglChoosePixelFormat(HDC hdc, const PIXELFORMATDESCRIPTOR *ppfd)
{
	LOG(INFO) << "Redirecting '" << "wglChoosePixelFormat" << "(" << hdc << ", " << ppfd << ")' ...";

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
		LOG(ERROR) << "> Layered OpenGL contexts of type " << static_cast<int>(ppfd->iLayerType) << " are not supported.";

		SetLastError(ERROR_INVALID_PARAMETER);

		return 0;
	}
	else if ((ppfd->dwFlags & PFD_DOUBLEBUFFER) == 0)
	{
		LOG(WARNING) << "> Single buffered OpenGL contexts are not supported.";
	}

	const int format = reshade::hooks::call(&wglChoosePixelFormat)(hdc, ppfd);

	LOG(INFO) << "> Returned format: " << format;

	return format;
}
			BOOL  WINAPI wglChoosePixelFormatARB(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats)
{
	LOG(INFO) << "Redirecting '" << "wglChoosePixelFormatARB" << "(" << hdc << ", " << piAttribIList << ", " << pfAttribFList << ", " << nMaxFormats << ", " << piFormats << ", " << nNumFormats << ")' ...";

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
		LOG(ERROR) << "> Layered OpenGL contexts are not supported.";

		SetLastError(ERROR_INVALID_PARAMETER);

		return FALSE;
	}
	else if (!doublebuffered)
	{
		LOG(WARNING) << "> Single buffered OpenGL contexts are not supported.";
	}

	if (!reshade::hooks::call(&wglChoosePixelFormatARB)(hdc, piAttribIList, pfAttribFList, nMaxFormats, piFormats, nNumFormats))
	{
		LOG(WARNING) << "> 'wglChoosePixelFormatARB' failed with error code " << (GetLastError() & 0xFFFF) << "!";

		return FALSE;
	}

	assert(nNumFormats != nullptr);

	if (*nNumFormats < nMaxFormats)
	{
		nMaxFormats = *nNumFormats;
	}

	std::string formats;

	for (UINT i = 0; i < nMaxFormats; ++i)
	{
		assert(piFormats[i] != 0);

		formats += " " + std::to_string(piFormats[i]);
	}

	LOG(INFO) << "> Returned format(s):" << formats;

	return TRUE;
}
HOOK_EXPORT int   WINAPI wglGetPixelFormat(HDC hdc)
{
	return reshade::hooks::call(&wglGetPixelFormat)(hdc);
}
			BOOL  WINAPI wglGetPixelFormatAttribivARB(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, int *piValues)
{
	if (iLayerPlane != 0)
	{
		LOG(WARNING) << "Access to layer plane at index " << iLayerPlane << " is unsupported.";

		SetLastError(ERROR_INVALID_PARAMETER);

		return FALSE;
	}

	return reshade::hooks::call(&wglGetPixelFormatAttribivARB)(hdc, iPixelFormat, 0, nAttributes, piAttributes, piValues);
}
			BOOL  WINAPI wglGetPixelFormatAttribfvARB(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, FLOAT *pfValues)
{
	if (iLayerPlane != 0)
	{
		LOG(WARNING) << "Access to layer plane at index " << iLayerPlane << " is unsupported.";

		SetLastError(ERROR_INVALID_PARAMETER);

		return FALSE;
	}

	return reshade::hooks::call(&wglGetPixelFormatAttribfvARB)(hdc, iPixelFormat, 0, nAttributes, piAttributes, pfValues);
}
HOOK_EXPORT BOOL  WINAPI wglSetPixelFormat(HDC hdc, int iPixelFormat, const PIXELFORMATDESCRIPTOR *ppfd)
{
	LOG(INFO) << "Redirecting '" << "wglSetPixelFormat" << "(" << hdc << ", " << iPixelFormat << ", " << ppfd << ")' ...";

	if (!reshade::hooks::call(&wglSetPixelFormat)(hdc, iPixelFormat, ppfd))
	{
		LOG(WARNING) << "> 'wglSetPixelFormat' failed with error code " << (GetLastError() & 0xFFFF) << "!";

		return FALSE;
	}

	if (GetPixelFormat(hdc) == 0)
	{
		LOG(WARNING) << "> Application mistakenly called 'wglSetPixelFormat' directly. Passing on to 'SetPixelFormat':";

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
	LOG(INFO) << "Redirecting '" << "wglDescribeLayerPlane" << "(" << hdc << ", " << iPixelFormat << ", " << iLayerPlane << ", " << nBytes << ", " << plpd << ")' ...";
	LOG(WARNING) << "Access to layer plane at index " << iLayerPlane << " is unsupported.";

	SetLastError(ERROR_NOT_SUPPORTED);

	return FALSE;
}
HOOK_EXPORT BOOL  WINAPI wglRealizeLayerPalette(HDC hdc, int iLayerPlane, BOOL b)
{
	LOG(INFO) << "Redirecting '" << "wglRealizeLayerPalette" << "(" << hdc << ", " << iLayerPlane << ", " << b << ")' ...";
	LOG(WARNING) << "Access to layer plane at index " << iLayerPlane << " is unsupported.";

	SetLastError(ERROR_NOT_SUPPORTED);

	return FALSE;
}
HOOK_EXPORT int   WINAPI wglGetLayerPaletteEntries(HDC hdc, int iLayerPlane, int iStart, int cEntries, COLORREF *pcr)
{
	LOG(INFO) << "Redirecting '" << "wglGetLayerPaletteEntries" << "(" << hdc << ", " << iLayerPlane << ", " << iStart << ", " << cEntries << ", " << pcr << ")' ...";
	LOG(WARNING) << "Access to layer plane at index " << iLayerPlane << " is unsupported.";

	SetLastError(ERROR_NOT_SUPPORTED);

	return 0;
}
HOOK_EXPORT int   WINAPI wglSetLayerPaletteEntries(HDC hdc, int iLayerPlane, int iStart, int cEntries, const COLORREF *pcr)
{
	LOG(INFO) << "Redirecting '" << "wglSetLayerPaletteEntries" << "(" << hdc << ", " << iLayerPlane << ", " << iStart << ", " << cEntries << ", " << pcr << ")' ...";
	LOG(WARNING) << "Access to layer plane at index " << iLayerPlane << " is unsupported.";

	SetLastError(ERROR_NOT_SUPPORTED);

	return 0;
}

HOOK_EXPORT HGLRC WINAPI wglCreateContext(HDC hdc)
{
	LOG(INFO) << "Redirecting '" << "wglCreateContext" << "(" << hdc << ")' ...";
	LOG(INFO) << "> Passing on to 'wglCreateLayerContext' ...";

	return wglCreateLayerContext(hdc, 0);
}
			HGLRC WINAPI wglCreateContextAttribsARB(HDC hdc, HGLRC hShareContext, const int *piAttribList)
{
	LOG(INFO) << "Redirecting '" << "wglCreateContextAttribsARB" << "(" << hdc << ", " << hShareContext << ", " << piAttribList << ")' ...";

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
	bool core = true, compatibility = false;
	attribute attribs[8] = { };

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
				core = (attrib[1] & attribute::WGL_CONTEXT_CORE_PROFILE_BIT_ARB) != 0;
				compatibility = (attrib[1] & attribute::WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB) != 0;
				break;
		}
	}

	if (major < 3 || minor < 2)
	{
		core = compatibility = false;
	}

#ifdef _DEBUG
	flags |= attribute::WGL_CONTEXT_DEBUG_BIT_ARB;
#endif

	// This works because the specs specifically note that "If an attribute is specified more than once, then the last value specified is used."
	attribs[i].name = attribute::WGL_CONTEXT_FLAGS_ARB;
	attribs[i++].value = flags;
	attribs[i].name = attribute::WGL_CONTEXT_PROFILE_MASK_ARB;
	attribs[i++].value = compatibility ? attribute::WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB : attribute::WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

	LOG(INFO) << "> Requesting " << (core ? "core " : compatibility ? "compatibility " : "") << "OpenGL context for version " << major << '.' << minor << " ...";

	if (major < 4 || minor < 3)
	{
		LOG(WARNING) << "> Replacing requested version with 4.3 ...";

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

	const HGLRC hglrc = reshade::hooks::call(&wglCreateContextAttribsARB)(hdc, hShareContext, reinterpret_cast<const int *>(attribs));

	if (hglrc == nullptr)
	{
		LOG(WARNING) << "> 'wglCreateContextAttribsARB' failed with error code " << (GetLastError() & 0xFFFF) << "!";

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

	LOG(INFO) << "> Returning OpenGL context: " << hglrc;

	return hglrc;
}
HOOK_EXPORT HGLRC WINAPI wglCreateLayerContext(HDC hdc, int iLayerPlane)
{
	LOG(INFO) << "Redirecting '" << "wglCreateLayerContext" << "(" << hdc << ", " << iLayerPlane << ")' ...";

	if (iLayerPlane != 0)
	{
		LOG(WARNING) << "Access to layer plane at index " << iLayerPlane << " is unsupported.";

		SetLastError(ERROR_INVALID_PARAMETER);

		return nullptr;
	}

	const HGLRC hglrc = reshade::hooks::call(&wglCreateLayerContext)(hdc, iLayerPlane);

	if (hglrc == nullptr)
	{
		LOG(WARNING) << "> 'wglCreateLayerContext' failed with error code " << (GetLastError() & 0xFFFF) << "!";

		return nullptr;
	}

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		s_shared_contexts.emplace(hglrc, nullptr);
	}

	LOG(INFO) << "> Returning OpenGL context: " << hglrc;

	return hglrc;
}
HOOK_EXPORT BOOL  WINAPI wglCopyContext(HGLRC hglrcSrc, HGLRC hglrcDst, UINT mask)
{
	return reshade::hooks::call(&wglCopyContext)(hglrcSrc, hglrcDst, mask);
}
HOOK_EXPORT BOOL  WINAPI wglDeleteContext(HGLRC hglrc)
{
	if (hglrc == wglGetCurrentContext())
	{
		// Unset the rendering context if it's the calling thread's current one
		wglMakeCurrent(nullptr, nullptr);
	}

	LOG(INFO) << "Redirecting '" << "wglDeleteContext" << "(" << hglrc << ")' ...";

	{ const std::lock_guard<std::mutex> lock(s_mutex);
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

	if (!reshade::hooks::call(&wglDeleteContext)(hglrc))
	{
		LOG(WARNING) << "> 'wglDeleteContext' failed with error code " << (GetLastError() & 0xFFFF) << "!";

		return FALSE;
	}

	return TRUE;
}

HOOK_EXPORT BOOL  WINAPI wglShareLists(HGLRC hglrc1, HGLRC hglrc2)
{
	LOG(INFO) << "Redirecting '" << "wglShareLists" << "(" << hglrc1 << ", " << hglrc2 << ")' ...";

	if (!reshade::hooks::call(&wglShareLists)(hglrc1, hglrc2))
	{
		LOG(WARNING) << "> 'wglShareLists' failed with error code " << (GetLastError() & 0xFFFF) << "!";

		return FALSE;
	}

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		s_shared_contexts[hglrc2] = hglrc1;
	}

	return TRUE;
}

HOOK_EXPORT BOOL  WINAPI wglMakeCurrent(HDC hdc, HGLRC hglrc)
{
	static const auto trampoline = reshade::hooks::call(&wglMakeCurrent);

	LOG(INFO) << "Redirecting '" << "wglMakeCurrent" << "(" << hdc << ", " << hglrc << ")' ...";

	const HDC hdc_previous = wglGetCurrentDC();
	const HGLRC hglrc_previous = wglGetCurrentContext();

	if (hdc == hdc_previous && hglrc == hglrc_previous)
	{
		return TRUE;
	}
	
	const std::lock_guard<std::mutex> lock(s_mutex);

	const bool is_pbuffer_device_context = s_pbuffer_device_contexts.find(hdc) != s_pbuffer_device_contexts.end();
	
	if (hdc_previous != nullptr)
	{
		const auto it = g_opengl_runtimes.find(hdc_previous);

		if (it != g_opengl_runtimes.end() && --it->second->_reference_count == 0 && !is_pbuffer_device_context)
		{
			LOG(INFO) << "> Cleaning up runtime " << it->second << " ...";

			it->second->on_reset();

			delete it->second;

			g_opengl_runtimes.erase(it);
		}
	}

	if (!trampoline(hdc, hglrc))
	{
		LOG(WARNING) << "> 'wglMakeCurrent' failed with error code " << (GetLastError() & 0xFFFF) << "!";

		return FALSE;
	}

	if (hglrc == nullptr)
	{
		return TRUE;
	}

	const auto it = g_opengl_runtimes.find(hdc);

	if (it != g_opengl_runtimes.end())
	{
		it->second->_reference_count++;

		LOG(INFO) << "> Switched to existing runtime " << it->second << ".";
	}
	else
	{
		const HWND hwnd = WindowFromDC(hdc);

		if (hwnd == nullptr || is_pbuffer_device_context)
		{
			LOG(WARNING) << "> Skipped because there is no window associated with this device context.";

			return TRUE;
		}
		else if ((GetClassLongPtr(hwnd, GCL_STYLE) & CS_OWNDC) == 0)
		{
			LOG(WARNING) << "> Window class style of window " << hwnd << " is missing 'CS_OWNDC' flag.";
		}

		if (s_shared_contexts.at(hglrc) != nullptr)
		{
			hglrc = s_shared_contexts.at(hglrc);

			LOG(INFO) << "> Using shared OpenGL context " << hglrc << ".";
		}

		gl3wInit();

		// Fix up gl3w to use the original OpenGL functions and not the hooked ones
		gl3wProcs.gl.BindTexture = reshade::hooks::call(&glBindTexture);
		gl3wProcs.gl.BlendFunc = reshade::hooks::call(&glBlendFunc);
		gl3wProcs.gl.Clear = reshade::hooks::call(&glClear);
		gl3wProcs.gl.ClearColor = reshade::hooks::call(&glClearColor);
		gl3wProcs.gl.ClearDepth = reshade::hooks::call(&glClearDepth);
		gl3wProcs.gl.ClearStencil = reshade::hooks::call(&glClearStencil);
		gl3wProcs.gl.ColorMask = reshade::hooks::call(&glColorMask);
		gl3wProcs.gl.CopyTexImage1D = reshade::hooks::call(&glCopyTexImage1D);
		gl3wProcs.gl.CopyTexImage2D = reshade::hooks::call(&glCopyTexImage2D);
		gl3wProcs.gl.CopyTexSubImage1D = reshade::hooks::call(&glCopyTexSubImage1D);
		gl3wProcs.gl.CopyTexSubImage2D = reshade::hooks::call(&glCopyTexSubImage2D);
		gl3wProcs.gl.CullFace = reshade::hooks::call(&glCullFace);
		gl3wProcs.gl.DeleteTextures = reshade::hooks::call(&glDeleteTextures);
		gl3wProcs.gl.DepthFunc = reshade::hooks::call(&glDepthFunc);
		gl3wProcs.gl.DepthMask = reshade::hooks::call(&glDepthMask);
		gl3wProcs.gl.DepthRange = reshade::hooks::call(&glDepthRange);
		gl3wProcs.gl.Disable = reshade::hooks::call(&glDisable);
		gl3wProcs.gl.DrawArrays = reshade::hooks::call(&glDrawArrays);
		gl3wProcs.gl.DrawArraysIndirect = reshade::hooks::call(&glDrawArraysIndirect);
		gl3wProcs.gl.DrawArraysInstanced = reshade::hooks::call(&glDrawArraysInstanced);
		gl3wProcs.gl.DrawArraysInstancedBaseInstance = reshade::hooks::call(&glDrawArraysInstancedBaseInstance);
		gl3wProcs.gl.DrawBuffer = reshade::hooks::call(&glDrawBuffer);
		gl3wProcs.gl.DrawElements = reshade::hooks::call(&glDrawElements);
		gl3wProcs.gl.DrawElementsBaseVertex = reshade::hooks::call(&glDrawElementsBaseVertex);
		gl3wProcs.gl.DrawElementsIndirect = reshade::hooks::call(&glDrawElementsIndirect);
		gl3wProcs.gl.DrawElementsInstanced = reshade::hooks::call(&glDrawElementsInstanced);
		gl3wProcs.gl.DrawElementsInstancedBaseVertex = reshade::hooks::call(&glDrawElementsInstancedBaseVertex);
		gl3wProcs.gl.DrawElementsInstancedBaseInstance = reshade::hooks::call(&glDrawElementsInstancedBaseInstance);
		gl3wProcs.gl.DrawElementsInstancedBaseVertexBaseInstance = reshade::hooks::call(&glDrawElementsInstancedBaseVertexBaseInstance);
		gl3wProcs.gl.DrawRangeElements = reshade::hooks::call(&glDrawRangeElements);
		gl3wProcs.gl.DrawRangeElementsBaseVertex = reshade::hooks::call(&glDrawRangeElementsBaseVertex);
		gl3wProcs.gl.Enable = reshade::hooks::call(&glEnable);
		gl3wProcs.gl.Finish = reshade::hooks::call(&glFinish);
		gl3wProcs.gl.Flush = reshade::hooks::call(&glFlush);
		gl3wProcs.gl.FramebufferRenderbuffer = reshade::hooks::call(&glFramebufferRenderbuffer);
		gl3wProcs.gl.FramebufferTexture = reshade::hooks::call(&glFramebufferTexture);
		gl3wProcs.gl.FramebufferTexture1D = reshade::hooks::call(&glFramebufferTexture1D);
		gl3wProcs.gl.FramebufferTexture2D = reshade::hooks::call(&glFramebufferTexture2D);
		gl3wProcs.gl.FramebufferTexture3D = reshade::hooks::call(&glFramebufferTexture3D);
		gl3wProcs.gl.FramebufferTextureLayer = reshade::hooks::call(&glFramebufferTextureLayer);
		gl3wProcs.gl.FrontFace = reshade::hooks::call(&glFrontFace);
		gl3wProcs.gl.GenTextures = reshade::hooks::call(&glGenTextures);
		gl3wProcs.gl.GetBooleanv = reshade::hooks::call(&glGetBooleanv);
		gl3wProcs.gl.GetDoublev = reshade::hooks::call(&glGetDoublev);
		gl3wProcs.gl.GetError = reshade::hooks::call(&glGetError);
		gl3wProcs.gl.GetFloatv = reshade::hooks::call(&glGetFloatv);
		gl3wProcs.gl.GetIntegerv = reshade::hooks::call(&glGetIntegerv);
		gl3wProcs.gl.GetPointerv = reshade::hooks::call(&glGetPointerv);
		gl3wProcs.gl.GetString = reshade::hooks::call(&glGetString);
		gl3wProcs.gl.GetTexImage = reshade::hooks::call(&glGetTexImage);
		gl3wProcs.gl.GetTexLevelParameterfv = reshade::hooks::call(&glGetTexLevelParameterfv);
		gl3wProcs.gl.GetTexLevelParameteriv = reshade::hooks::call(&glGetTexLevelParameteriv);
		gl3wProcs.gl.GetTexParameterfv = reshade::hooks::call(&glGetTexParameterfv);
		gl3wProcs.gl.GetTexParameteriv = reshade::hooks::call(&glGetTexParameteriv);
		gl3wProcs.gl.Hint = reshade::hooks::call(&glHint);
		gl3wProcs.gl.IsEnabled = reshade::hooks::call(&glIsEnabled);
		gl3wProcs.gl.IsTexture = reshade::hooks::call(&glIsTexture);
		gl3wProcs.gl.LineWidth = reshade::hooks::call(&glLineWidth);
		gl3wProcs.gl.LogicOp = reshade::hooks::call(&glLogicOp);
		gl3wProcs.gl.MultiDrawArrays = reshade::hooks::call(&glMultiDrawArrays);
		gl3wProcs.gl.MultiDrawArraysIndirect = reshade::hooks::call(&glMultiDrawArraysIndirect);
		gl3wProcs.gl.MultiDrawElements = reshade::hooks::call(&glMultiDrawElements);
		gl3wProcs.gl.MultiDrawElementsBaseVertex = reshade::hooks::call(&glMultiDrawElementsBaseVertex);
		gl3wProcs.gl.MultiDrawElementsIndirect = reshade::hooks::call(&glMultiDrawElementsIndirect);
		gl3wProcs.gl.PixelStoref = reshade::hooks::call(&glPixelStoref);
		gl3wProcs.gl.PixelStorei = reshade::hooks::call(&glPixelStorei);
		gl3wProcs.gl.PointSize = reshade::hooks::call(&glPointSize);
		gl3wProcs.gl.PolygonMode = reshade::hooks::call(&glPolygonMode);
		gl3wProcs.gl.PolygonOffset = reshade::hooks::call(&glPolygonOffset);
		gl3wProcs.gl.ReadBuffer = reshade::hooks::call(&glReadBuffer);
		gl3wProcs.gl.ReadPixels = reshade::hooks::call(&glReadPixels);
		gl3wProcs.gl.Scissor = reshade::hooks::call(&glScissor);
		gl3wProcs.gl.StencilFunc = reshade::hooks::call(&glStencilFunc);
		gl3wProcs.gl.StencilMask = reshade::hooks::call(&glStencilMask);
		gl3wProcs.gl.StencilOp = reshade::hooks::call(&glStencilOp);
		gl3wProcs.gl.TexImage1D = reshade::hooks::call(&glTexImage1D);
		gl3wProcs.gl.TexImage2D = reshade::hooks::call(&glTexImage2D);
		gl3wProcs.gl.TexImage3D = reshade::hooks::call(&glTexImage3D);
		gl3wProcs.gl.TexParameterf = reshade::hooks::call(&glTexParameterf);
		gl3wProcs.gl.TexParameterfv = reshade::hooks::call(&glTexParameterfv);
		gl3wProcs.gl.TexParameteri = reshade::hooks::call(&glTexParameteri);
		gl3wProcs.gl.TexParameteriv = reshade::hooks::call(&glTexParameteriv);
		gl3wProcs.gl.TexSubImage1D = reshade::hooks::call(&glTexSubImage1D);
		gl3wProcs.gl.TexSubImage2D = reshade::hooks::call(&glTexSubImage2D);
		gl3wProcs.gl.Viewport = reshade::hooks::call(&glViewport);

		if (gl3wIsSupported(4, 3))
		{
			const auto runtime = new reshade::opengl::runtime_opengl(hdc);

			g_opengl_runtimes[hdc] = runtime;

			LOG(INFO) << "> Switched to new runtime " << runtime << ".";
		}
		else
		{
			LOG(ERROR) << "Your graphics card does not seem to support OpenGL 4.3. Initialization failed.";
		}

		s_window_rects[hwnd].left = s_window_rects[hwnd].top = s_window_rects[hwnd].right = s_window_rects[hwnd].bottom = 0;
	}

	return TRUE;
}

HOOK_EXPORT HDC   WINAPI wglGetCurrentDC()
{
	static const auto trampoline = reshade::hooks::call(&wglGetCurrentDC);

	return trampoline();
}
HOOK_EXPORT HGLRC WINAPI wglGetCurrentContext()
{
	static const auto trampoline = reshade::hooks::call(&wglGetCurrentContext);

	return trampoline();
}

	  HPBUFFERARB WINAPI wglCreatePbufferARB(HDC hdc, int iPixelFormat, int iWidth, int iHeight, const int *piAttribList)
{
	LOG(INFO) << "Redirecting '" << "wglCreatePbufferARB" << "(" << hdc << ", " << iPixelFormat << ", " << iWidth << ", " << iHeight << ", " << piAttribList << ")' ...";

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

	const HPBUFFERARB hpbuffer = reshade::hooks::call(&wglCreatePbufferARB)(hdc, iPixelFormat, iWidth, iHeight, piAttribList);

	if (hpbuffer == nullptr)
	{
		LOG(WARNING) << "> 'wglCreatePbufferARB' failed with error code " << (GetLastError() & 0xFFFF) << "!";

		return nullptr;
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "> Returning pixel buffer: " << hpbuffer;
#endif

	return hpbuffer;
}
			BOOL  WINAPI wglDestroyPbufferARB(HPBUFFERARB hPbuffer)
{
	LOG(INFO) << "Redirecting '" << "wglDestroyPbufferARB" << "(" << hPbuffer << ")' ...";

	if (!reshade::hooks::call(&wglDestroyPbufferARB)(hPbuffer))
	{
		LOG(WARNING) << "> 'wglDestroyPbufferARB' failed with error code " << (GetLastError() & 0xFFFF) << "!";

		return FALSE;
	}

	return TRUE;
}
			BOOL  WINAPI wglQueryPbufferARB(HPBUFFERARB hPbuffer, int iAttribute, int *piValue)
{
	return reshade::hooks::call(&wglQueryPbufferARB)(hPbuffer, iAttribute, piValue);
}
			HDC   WINAPI wglGetPbufferDCARB(HPBUFFERARB hPbuffer)
{
	LOG(INFO) << "Redirecting '" << "wglGetPbufferDCARB" << "(" << hPbuffer << ")' ...";

	const HDC hdc = reshade::hooks::call(&wglGetPbufferDCARB)(hPbuffer);

	if (hdc == nullptr)
	{
		LOG(WARNING) << "> 'wglGetPbufferDCARB' failed with error code " << (GetLastError() & 0xFFFF) << "!";

		return nullptr;
	}

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		s_pbuffer_device_contexts.insert(hdc);
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "> Returning pixel buffer device context: " << hdc;
#endif

	return hdc;
}
			int   WINAPI wglReleasePbufferDCARB(HPBUFFERARB hPbuffer, HDC hdc)
{
	LOG(INFO) << "Redirecting '" << "wglReleasePbufferDCARB" << "(" << hPbuffer << ")' ...";

	if (!reshade::hooks::call(&wglReleasePbufferDCARB)(hPbuffer, hdc))
	{
		LOG(WARNING) << "> 'wglReleasePbufferDCARB' failed with error code " << (GetLastError() & 0xFFFF) << "!";

		return FALSE;
	}

	{ const std::lock_guard<std::mutex> lock(s_mutex);
		s_pbuffer_device_contexts.erase(hdc);
	}

	return TRUE;
}

			BOOL  WINAPI wglSwapIntervalEXT(int interval)
{
	static const auto trampoline = reshade::hooks::call(&wglSwapIntervalEXT);

	return trampoline(interval);
}
			int   WINAPI wglGetSwapIntervalEXT()
{
	static const auto trampoline = reshade::hooks::call(&wglGetSwapIntervalEXT);

	return trampoline();
}

HOOK_EXPORT BOOL  WINAPI wglSwapBuffers(HDC hdc)
{
	static const auto trampoline = reshade::hooks::call(&wglSwapBuffers);

	const auto it = g_opengl_runtimes.find(hdc);
	const HWND hwnd = WindowFromDC(hdc);

	if (hwnd != nullptr && it != g_opengl_runtimes.end())
	{
		assert(it->second != nullptr);
		assert(hdc == wglGetCurrentDC());

		RECT rect, &rectPrevious = s_window_rects.at(hwnd);
		GetClientRect(hwnd, &rect);

		if (rect.right != rectPrevious.right || rect.bottom != rectPrevious.bottom)
		{
			LOG(INFO) << "Resizing runtime " << it->second << " on device context " << hdc << " to " << rect.right << "x" << rect.bottom << " ...";

			it->second->on_reset();

			if (!(rect.right == 0 && rect.bottom == 0) && !it->second->on_init(static_cast<unsigned int>(rect.right), static_cast<unsigned int>(rect.bottom)))
			{
				LOG(ERROR) << "Failed to recreate OpenGL runtime environment on runtime " << it->second << ".";
			}

			rectPrevious = rect;
		}

		it->second->on_present();
	}

	return trampoline(hdc);
}
HOOK_EXPORT BOOL  WINAPI wglSwapLayerBuffers(HDC hdc, UINT i)
{
	if (i != WGL_SWAP_MAIN_PLANE)
	{
		const int index = i >= WGL_SWAP_UNDERLAY1 ? static_cast<int>(-std::log(i >> 16) / std::log(2) - 1) : static_cast<int>(std::log(i) / std::log(2));

		LOG(INFO) << "Redirecting '" << "wglSwapLayerBuffers" << "'(" << hdc << ", " << i << ")' ...";
		LOG(WARNING) << "Access to layer plane at index " << index << " is unsupported.";

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
	static const auto trampoline = reshade::hooks::call(&wglUseFontBitmapsA);

	return trampoline(hdc, dw1, dw2, dw3);
}
HOOK_EXPORT BOOL  WINAPI wglUseFontBitmapsW(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3)
{
	static const auto trampoline = reshade::hooks::call(&wglUseFontBitmapsW);

	return trampoline(hdc, dw1, dw2, dw3);
}
HOOK_EXPORT BOOL  WINAPI wglUseFontOutlinesA(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3, FLOAT f1, FLOAT f2, int i, LPGLYPHMETRICSFLOAT pgmf)
{
	static const auto trampoline = reshade::hooks::call(&wglUseFontOutlinesA);

	return trampoline(hdc, dw1, dw2, dw3, f1, f2, i, pgmf);
}
HOOK_EXPORT BOOL  WINAPI wglUseFontOutlinesW(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3, FLOAT f1, FLOAT f2, int i, LPGLYPHMETRICSFLOAT pgmf)
{
	static const auto trampoline = reshade::hooks::call(&wglUseFontOutlinesW);

	return trampoline(hdc, dw1, dw2, dw3, f1, f2, i, pgmf);
}

HOOK_EXPORT PROC  WINAPI wglGetProcAddress(LPCSTR lpszProc)
{
	static const auto trampoline = reshade::hooks::call(&wglGetProcAddress);

	const PROC address = trampoline(lpszProc);

	if (address == nullptr || lpszProc == nullptr)
		return nullptr;
	else if (strcmp(lpszProc, "glBindTexture") == 0)
		return reinterpret_cast<PROC>(&glBindTexture);
	else if (strcmp(lpszProc, "glBlendFunc") == 0)
		return reinterpret_cast<PROC>(&glBlendFunc);
	else if (strcmp(lpszProc, "glClear") == 0)
		return reinterpret_cast<PROC>(&glClear);
	else if (strcmp(lpszProc, "glClearColor") == 0)
		return reinterpret_cast<PROC>(&glClearColor);
	else if (strcmp(lpszProc, "glClearDepth") == 0)
		return reinterpret_cast<PROC>(&glClearDepth);
	else if (strcmp(lpszProc, "glClearStencil") == 0)
		return reinterpret_cast<PROC>(&glClearStencil);
	else if (strcmp(lpszProc, "glCopyTexImage1D") == 0)
		return reinterpret_cast<PROC>(&glCopyTexImage1D);
	else if (strcmp(lpszProc, "glCopyTexImage2D") == 0)
		return reinterpret_cast<PROC>(&glCopyTexImage2D);
	else if (strcmp(lpszProc, "glCopyTexSubImage1D") == 0)
		return reinterpret_cast<PROC>(&glCopyTexSubImage1D);
	else if (strcmp(lpszProc, "glCopyTexSubImage2D") == 0)
		return reinterpret_cast<PROC>(&glCopyTexSubImage2D);
	else if (strcmp(lpszProc, "glCullFace") == 0)
		return reinterpret_cast<PROC>(&glCullFace);
	else if (strcmp(lpszProc, "glDeleteTextures") == 0)
		return reinterpret_cast<PROC>(&glDeleteTextures);
	else if (strcmp(lpszProc, "glDepthFunc") == 0)
		return reinterpret_cast<PROC>(&glDepthFunc);
	else if (strcmp(lpszProc, "glDepthMask") == 0)
		return reinterpret_cast<PROC>(&glDepthMask);
	else if (strcmp(lpszProc, "glDepthRange") == 0)
		return reinterpret_cast<PROC>(&glDepthRange);
	else if (strcmp(lpszProc, "glDisable") == 0)
		return reinterpret_cast<PROC>(&glDisable);
	else if (strcmp(lpszProc, "glDrawArrays") == 0)
		return reinterpret_cast<PROC>(&glDrawArrays);
	else if (strcmp(lpszProc, "glDrawBuffer") == 0)
		return reinterpret_cast<PROC>(&glDrawBuffer);
	else if (strcmp(lpszProc, "glDrawElements") == 0)
		return reinterpret_cast<PROC>(&glDrawElements);
	else if (strcmp(lpszProc, "glEnable") == 0)
		return reinterpret_cast<PROC>(&glEnable);
	else if (strcmp(lpszProc, "glFinish") == 0)
		return reinterpret_cast<PROC>(&glFinish);
	else if (strcmp(lpszProc, "glFlush") == 0)
		return reinterpret_cast<PROC>(&glFlush);
	else if (strcmp(lpszProc, "glFrontFace") == 0)
		return reinterpret_cast<PROC>(&glFrontFace);
	else if (strcmp(lpszProc, "glGenTextures") == 0)
		return reinterpret_cast<PROC>(&glGenTextures);
	else if (strcmp(lpszProc, "glGetBooleanv") == 0)
		return reinterpret_cast<PROC>(&glGetBooleanv);
	else if (strcmp(lpszProc, "glGetDoublev") == 0)
		return reinterpret_cast<PROC>(&glGetDoublev);
	else if (strcmp(lpszProc, "glGetFloatv") == 0)
		return reinterpret_cast<PROC>(&glGetFloatv);
	else if (strcmp(lpszProc, "glGetIntegerv") == 0)
		return reinterpret_cast<PROC>(&glGetIntegerv);
	else if (strcmp(lpszProc, "glGetError") == 0)
		return reinterpret_cast<PROC>(&glGetError);
	else if (strcmp(lpszProc, "glGetPointerv") == 0)
		return reinterpret_cast<PROC>(&glGetPointerv);
	else if (strcmp(lpszProc, "glGetString") == 0)
		return reinterpret_cast<PROC>(&glGetString);
	else if (strcmp(lpszProc, "glGetTexImage") == 0)
		return reinterpret_cast<PROC>(&glGetTexImage);
	else if (strcmp(lpszProc, "glGetTexLevelParameterfv") == 0)
		return reinterpret_cast<PROC>(&glGetTexLevelParameterfv);
	else if (strcmp(lpszProc, "glGetTexLevelParameteriv") == 0)
		return reinterpret_cast<PROC>(&glGetTexLevelParameteriv);
	else if (strcmp(lpszProc, "glGetTexParameterfv") == 0)
		return reinterpret_cast<PROC>(&glGetTexParameterfv);
	else if (strcmp(lpszProc, "glGetTexParameteriv") == 0)
		return reinterpret_cast<PROC>(&glGetTexParameteriv);
	else if (strcmp(lpszProc, "glHint") == 0)
		return reinterpret_cast<PROC>(&glHint);
	else if (strcmp(lpszProc, "glIsEnabled") == 0)
		return reinterpret_cast<PROC>(&glIsEnabled);
	else if (strcmp(lpszProc, "glIsTexture") == 0)
		return reinterpret_cast<PROC>(&glIsTexture);
	else if (strcmp(lpszProc, "glLineWidth") == 0)
		return reinterpret_cast<PROC>(&glLineWidth);
	else if (strcmp(lpszProc, "glLogicOp") == 0)
		return reinterpret_cast<PROC>(&glLogicOp);
	else if (strcmp(lpszProc, "glPixelStoref") == 0)
		return reinterpret_cast<PROC>(&glPixelStoref);
	else if (strcmp(lpszProc, "glPixelStorei") == 0)
		return reinterpret_cast<PROC>(&glPixelStorei);
	else if (strcmp(lpszProc, "glPointSize") == 0)
		return reinterpret_cast<PROC>(&glPointSize);
	else if (strcmp(lpszProc, "glPolygonMode") == 0)
		return reinterpret_cast<PROC>(&glPolygonMode);
	else if (strcmp(lpszProc, "glPolygonOffset") == 0)
		return reinterpret_cast<PROC>(&glPolygonOffset);
	else if (strcmp(lpszProc, "glReadBuffer") == 0)
		return reinterpret_cast<PROC>(&glReadBuffer);
	else if (strcmp(lpszProc, "glReadPixels") == 0)
		return reinterpret_cast<PROC>(&glReadPixels);
	else if (strcmp(lpszProc, "glScissor") == 0)
		return reinterpret_cast<PROC>(&glScissor);
	else if (strcmp(lpszProc, "glStencilFunc") == 0)
		return reinterpret_cast<PROC>(&glStencilFunc);
	else if (strcmp(lpszProc, "glStencilMask") == 0)
		return reinterpret_cast<PROC>(&glStencilMask);
	else if (strcmp(lpszProc, "glStencilOp") == 0)
		return reinterpret_cast<PROC>(&glStencilOp);
	else if (strcmp(lpszProc, "glTexImage1D") == 0)
		return reinterpret_cast<PROC>(&glTexImage1D);
	else if (strcmp(lpszProc, "glTexImage2D") == 0)
		return reinterpret_cast<PROC>(&glTexImage2D);
	else if (strcmp(lpszProc, "glTexParameterf") == 0)
		return reinterpret_cast<PROC>(&glTexParameterf);
	else if (strcmp(lpszProc, "glTexParameterfv") == 0)
		return reinterpret_cast<PROC>(&glTexParameterfv);
	else if (strcmp(lpszProc, "glTexParameteri") == 0)
		return reinterpret_cast<PROC>(&glTexParameteri);
	else if (strcmp(lpszProc, "glTexParameteriv") == 0)
		return reinterpret_cast<PROC>(&glTexParameteriv);
	else if (strcmp(lpszProc, "glTexSubImage1D") == 0)
		return reinterpret_cast<PROC>(&glTexSubImage1D);
	else if (strcmp(lpszProc, "glTexSubImage2D") == 0)
		return reinterpret_cast<PROC>(&glTexSubImage2D);
	else if (strcmp(lpszProc, "glViewport") == 0)
		return reinterpret_cast<PROC>(&glViewport);
	else if (static bool s_hooks_not_installed = true; s_hooks_not_installed)
	{
		// Install all OpenGL hooks in a single batch job
		reshade::hooks::install("glDrawArraysIndirect", reinterpret_cast<reshade::hook::address>(trampoline("glDrawArraysIndirect")), reinterpret_cast<reshade::hook::address>(&glDrawArraysIndirect), true);
		reshade::hooks::install("glDrawArraysInstanced", reinterpret_cast<reshade::hook::address>(trampoline("glDrawArraysInstanced")), reinterpret_cast<reshade::hook::address>(&glDrawArraysInstanced), true);
		reshade::hooks::install("glDrawArraysInstancedARB", reinterpret_cast<reshade::hook::address>(trampoline("glDrawArraysInstancedARB")), reinterpret_cast<reshade::hook::address>(&glDrawArraysInstancedARB), true);
		reshade::hooks::install("glDrawArraysInstancedEXT", reinterpret_cast<reshade::hook::address>(trampoline("glDrawArraysInstancedEXT")), reinterpret_cast<reshade::hook::address>(&glDrawArraysInstancedEXT), true);
		reshade::hooks::install("glDrawArraysInstancedBaseInstance", reinterpret_cast<reshade::hook::address>(trampoline("glDrawArraysInstancedBaseInstance")), reinterpret_cast<reshade::hook::address>(&glDrawArraysInstancedBaseInstance), true);
		reshade::hooks::install("glDrawElementsBaseVertex", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsBaseVertex")), reinterpret_cast<reshade::hook::address>(&glDrawElementsBaseVertex), true);
		reshade::hooks::install("glDrawElementsIndirect", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsIndirect")), reinterpret_cast<reshade::hook::address>(&glDrawElementsIndirect), true);
		reshade::hooks::install("glDrawElementsInstanced", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsInstanced")), reinterpret_cast<reshade::hook::address>(&glDrawElementsInstanced), true);
		reshade::hooks::install("glDrawElementsInstancedARB", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsInstancedARB")), reinterpret_cast<reshade::hook::address>(&glDrawElementsInstancedARB), true);
		reshade::hooks::install("glDrawElementsInstancedEXT", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsInstancedEXT")), reinterpret_cast<reshade::hook::address>(&glDrawElementsInstancedEXT), true);
		reshade::hooks::install("glDrawElementsInstancedBaseVertex", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsInstancedBaseVertex")), reinterpret_cast<reshade::hook::address>(&glDrawElementsInstancedBaseVertex), true);
		reshade::hooks::install("glDrawElementsInstancedBaseInstance", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsInstancedBaseInstance")), reinterpret_cast<reshade::hook::address>(&glDrawElementsInstancedBaseInstance), true);
		reshade::hooks::install("glDrawElementsInstancedBaseVertexBaseInstance", reinterpret_cast<reshade::hook::address>(trampoline("glDrawElementsInstancedBaseVertexBaseInstance")), reinterpret_cast<reshade::hook::address>(&glDrawElementsInstancedBaseVertexBaseInstance), true);
		reshade::hooks::install("glDrawRangeElements", reinterpret_cast<reshade::hook::address>(trampoline("glDrawRangeElements")), reinterpret_cast<reshade::hook::address>(&glDrawRangeElements), true);
		reshade::hooks::install("glDrawRangeElementsBaseVertex", reinterpret_cast<reshade::hook::address>(trampoline("glDrawRangeElementsBaseVertex")), reinterpret_cast<reshade::hook::address>(&glDrawRangeElementsBaseVertex), true);
		reshade::hooks::install("glFramebufferRenderbuffer", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferRenderbuffer")), reinterpret_cast<reshade::hook::address>(&glFramebufferRenderbuffer), true);
		reshade::hooks::install("glFramebufferRenderbufferEXT", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferRenderbufferEXT")), reinterpret_cast<reshade::hook::address>(&glFramebufferRenderbufferEXT), true);
		reshade::hooks::install("glFramebufferTexture", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTexture")), reinterpret_cast<reshade::hook::address>(&glFramebufferTexture), true);
		reshade::hooks::install("glFramebufferTextureARB", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTextureARB")), reinterpret_cast<reshade::hook::address>(&glFramebufferTextureARB), true);
		reshade::hooks::install("glFramebufferTextureEXT", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTextureEXT")), reinterpret_cast<reshade::hook::address>(&glFramebufferTextureEXT), true);
		reshade::hooks::install("glFramebufferTexture1D", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTexture1D")), reinterpret_cast<reshade::hook::address>(&glFramebufferTexture1D), true);
		reshade::hooks::install("glFramebufferTexture1DEXT", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTexture1DEXT")), reinterpret_cast<reshade::hook::address>(&glFramebufferTexture1DEXT), true);
		reshade::hooks::install("glFramebufferTexture2D", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTexture2D")), reinterpret_cast<reshade::hook::address>(&glFramebufferTexture2D), true);
		reshade::hooks::install("glFramebufferTexture2DEXT", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTexture2DEXT")), reinterpret_cast<reshade::hook::address>(&glFramebufferTexture2DEXT), true);
		reshade::hooks::install("glFramebufferTexture3D", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTexture3D")), reinterpret_cast<reshade::hook::address>(&glFramebufferTexture3D), true);
		reshade::hooks::install("glFramebufferTexture3DEXT", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTexture3DEXT")), reinterpret_cast<reshade::hook::address>(&glFramebufferTexture3DEXT), true);
		reshade::hooks::install("glFramebufferTextureLayer", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTextureLayer")), reinterpret_cast<reshade::hook::address>(&glFramebufferTextureLayer), true);
		reshade::hooks::install("glFramebufferTextureLayerARB", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTextureLayerARB")), reinterpret_cast<reshade::hook::address>(&glFramebufferTextureLayerARB), true);
		reshade::hooks::install("glFramebufferTextureLayerEXT", reinterpret_cast<reshade::hook::address>(trampoline("glFramebufferTextureLayerEXT")), reinterpret_cast<reshade::hook::address>(&glFramebufferTextureLayerEXT), true);
		reshade::hooks::install("glMultiDrawArrays", reinterpret_cast<reshade::hook::address>(trampoline("glMultiDrawArrays")), reinterpret_cast<reshade::hook::address>(&glMultiDrawArrays), true);
		reshade::hooks::install("glMultiDrawArraysIndirect", reinterpret_cast<reshade::hook::address>(trampoline("glMultiDrawArraysIndirect")), reinterpret_cast<reshade::hook::address>(&glMultiDrawArraysIndirect), true);
		reshade::hooks::install("glMultiDrawElements", reinterpret_cast<reshade::hook::address>(trampoline("glMultiDrawElements")), reinterpret_cast<reshade::hook::address>(&glMultiDrawElements), true);
		reshade::hooks::install("glMultiDrawElementsBaseVertex", reinterpret_cast<reshade::hook::address>(trampoline("glMultiDrawElementsBaseVertex")), reinterpret_cast<reshade::hook::address>(&glMultiDrawElementsBaseVertex), true);
		reshade::hooks::install("glMultiDrawElementsIndirect", reinterpret_cast<reshade::hook::address>(trampoline("glMultiDrawElementsIndirect")), reinterpret_cast<reshade::hook::address>(&glMultiDrawElementsIndirect), true);
		reshade::hooks::install("glTexImage3D", reinterpret_cast<reshade::hook::address>(trampoline("glTexImage3D")), reinterpret_cast<reshade::hook::address>(&glTexImage3D), true);

		reshade::hooks::install("wglChoosePixelFormatARB", reinterpret_cast<reshade::hook::address>(trampoline("wglChoosePixelFormatARB")), reinterpret_cast<reshade::hook::address>(&wglChoosePixelFormatARB), true);
		reshade::hooks::install("wglCreateContextAttribsARB", reinterpret_cast<reshade::hook::address>(trampoline("wglCreateContextAttribsARB")), reinterpret_cast<reshade::hook::address>(&wglCreateContextAttribsARB), true);
		reshade::hooks::install("wglCreatePbufferARB", reinterpret_cast<reshade::hook::address>(trampoline("wglCreatePbufferARB")), reinterpret_cast<reshade::hook::address>(&wglCreatePbufferARB), true);
		reshade::hooks::install("wglDestroyPbufferARB", reinterpret_cast<reshade::hook::address>(trampoline("wglDestroyPbufferARB")), reinterpret_cast<reshade::hook::address>(&wglDestroyPbufferARB), true);
		reshade::hooks::install("wglGetPbufferDCARB", reinterpret_cast<reshade::hook::address>(trampoline("wglGetPbufferDCARB")), reinterpret_cast<reshade::hook::address>(&wglGetPbufferDCARB), true);
		reshade::hooks::install("wglGetPixelFormatAttribivARB", reinterpret_cast<reshade::hook::address>(trampoline("wglGetPixelFormatAttribivARB")), reinterpret_cast<reshade::hook::address>(&wglGetPixelFormatAttribivARB), true);
		reshade::hooks::install("wglGetPixelFormatAttribfvARB", reinterpret_cast<reshade::hook::address>(trampoline("wglGetPixelFormatAttribfvARB")), reinterpret_cast<reshade::hook::address>(&wglGetPixelFormatAttribfvARB), true);
		reshade::hooks::install("wglQueryPbufferARB", reinterpret_cast<reshade::hook::address>(trampoline("wglQueryPbufferARB")), reinterpret_cast<reshade::hook::address>(&wglQueryPbufferARB), true);
		reshade::hooks::install("wglReleasePbufferDCARB", reinterpret_cast<reshade::hook::address>(trampoline("wglReleasePbufferDCARB")), reinterpret_cast<reshade::hook::address>(&wglReleasePbufferDCARB), true);
		reshade::hooks::install("wglGetSwapIntervalEXT", reinterpret_cast<reshade::hook::address>(trampoline("wglGetSwapIntervalEXT")), reinterpret_cast<reshade::hook::address>(&wglGetSwapIntervalEXT), true);
		reshade::hooks::install("wglSwapIntervalEXT", reinterpret_cast<reshade::hook::address>(trampoline("wglSwapIntervalEXT")), reinterpret_cast<reshade::hook::address>(&wglSwapIntervalEXT), true);

		reshade::hook::apply_queued_actions();

		s_hooks_not_installed = false;
	}

	return address;
}
