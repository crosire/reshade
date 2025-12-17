/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <d2d1_3.h>
#include "dxgi/dxgi_device.hpp"
#include "com_ptr.hpp"
#include "dll_log.hpp" // Include late to get 'hr_to_string' helper function
#include "hook_manager.hpp"

#define ID2D1Factory_CreateDevice_Impl(vtable_index, factory_interface_version, device_interface_version) \
	HRESULT STDMETHODCALLTYPE ID2D1Factory##factory_interface_version##_CreateDevice(ID2D1Factory##factory_interface_version *factory, IDXGIDevice *dxgiDevice, ID2D1Device##device_interface_version **d2dDevice) \
	{ \
		reshade::log::message( \
			reshade::log::level::info, \
			"Redirecting ID2D1Factory" #factory_interface_version "::CreateDevice(this = %p, dxgiDevice = %p, d2dDevice = %p) ...", \
			factory, dxgiDevice , d2dDevice); \
		\
		if (com_ptr<DXGIDevice> device_proxy; \
			SUCCEEDED(dxgiDevice->QueryInterface(&device_proxy))) \
			dxgiDevice = device_proxy->_orig; \
		\
		const HRESULT hr = reshade::hooks::call(ID2D1Factory##factory_interface_version##_CreateDevice, reshade::hooks::vtable_from_instance(factory) + vtable_index)(factory, dxgiDevice, d2dDevice); \
		if (FAILED(hr)) \
		{ \
			reshade::log::message(reshade::log::level::warning, "ID2D1Factory" #factory_interface_version "::CreateDevice failed with error code %s.", reshade::log::hr_to_string(hr).c_str()); \
		} \
		return hr; \
	}

ID2D1Factory_CreateDevice_Impl(17, 1,  )
ID2D1Factory_CreateDevice_Impl(27, 2, 1)
ID2D1Factory_CreateDevice_Impl(28, 3, 2)
ID2D1Factory_CreateDevice_Impl(29, 4, 3)
ID2D1Factory_CreateDevice_Impl(30, 5, 4)
ID2D1Factory_CreateDevice_Impl(31, 6, 5)
ID2D1Factory_CreateDevice_Impl(32, 7, 6)

extern "C" HRESULT WINAPI D2D1CreateDevice(IDXGIDevice *dxgiDevice, CONST D2D1_CREATION_PROPERTIES *creationProperties, ID2D1Device **d2dDevice)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting D2D1CreateDevice(dxgiDevice = %p, creationProperties = %p, d2dDevice = %p) ...",
		dxgiDevice, creationProperties, d2dDevice);

	// Get original DXGI device and pass that into D2D1 (since D2D1 does not work properly when a wrapped device is passed to it)
	if (com_ptr<DXGIDevice> device_proxy;
		SUCCEEDED(dxgiDevice->QueryInterface(&device_proxy)))
		dxgiDevice = device_proxy->_orig;

	// Choose the correct overload (instead of the template D2D1CreateDevice functions also defined in the header)
	using D2D1CreateDevice_t = HRESULT(WINAPI *)(IDXGIDevice *, CONST D2D1_CREATION_PROPERTIES *, ID2D1Device **);

	const HRESULT hr = reshade::hooks::call<D2D1CreateDevice_t>(D2D1CreateDevice)(dxgiDevice, creationProperties, d2dDevice);
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "D2D1CreateDevice failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}

	return hr;
}

extern "C" HRESULT WINAPI D2D1CreateFactory(D2D1_FACTORY_TYPE factoryType, REFIID riid, CONST D2D1_FACTORY_OPTIONS *pFactoryOptions, void **ppIFactory)
{
	reshade::log::message(reshade::log::level::info,
		"Redirecting D2D1CreateFactory(factoryType = %d, riid = %s, pFactoryOptions = %p, ppIFactory = %p) ...",
		factoryType, reshade::log::iid_to_string(riid).c_str(), pFactoryOptions, ppIFactory);

	// Choose the correct overload (instead of the template D2D1CreateFactory functions also defined in the header)
	using D2D1CreateFactory_t = HRESULT(WINAPI *)(D2D1_FACTORY_TYPE, REFIID, CONST D2D1_FACTORY_OPTIONS *, void **);

	const HRESULT hr = reshade::hooks::call<D2D1CreateFactory_t>(D2D1CreateFactory)(factoryType, riid, pFactoryOptions, ppIFactory);
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "D2D1CreateFactory failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	ID2D1Factory *const factory = static_cast<ID2D1Factory *>(*ppIFactory);

	// Check for factory interface version support and install 'CreateDevice' hooks for existing ones
	if (com_ptr<ID2D1Factory1> factory1;
		SUCCEEDED(factory->QueryInterface(&factory1)))
		reshade::hooks::install("ID2D1Factory1::CreateDevice", reshade::hooks::vtable_from_instance(factory1.get()), 17, &ID2D1Factory1_CreateDevice);
	if (com_ptr<ID2D1Factory2> factory2;
		SUCCEEDED(factory->QueryInterface(&factory2)))
		reshade::hooks::install("ID2D1Factory2::CreateDevice", reshade::hooks::vtable_from_instance(factory2.get()), 27, &ID2D1Factory2_CreateDevice);
	if (com_ptr<ID2D1Factory3> factory3;
		SUCCEEDED(factory->QueryInterface(&factory3)))
		reshade::hooks::install("ID2D1Factory3::CreateDevice", reshade::hooks::vtable_from_instance(factory3.get()), 28, &ID2D1Factory3_CreateDevice);
	if (com_ptr<ID2D1Factory4> factory4;
		SUCCEEDED(factory->QueryInterface(&factory4)))
		reshade::hooks::install("ID2D1Factory4::CreateDevice", reshade::hooks::vtable_from_instance(factory4.get()), 29, &ID2D1Factory4_CreateDevice);
	if (com_ptr<ID2D1Factory5> factory5;
		SUCCEEDED(factory->QueryInterface(&factory5)))
		reshade::hooks::install("ID2D1Factory5::CreateDevice", reshade::hooks::vtable_from_instance(factory5.get()), 30, &ID2D1Factory5_CreateDevice);
	if (com_ptr<ID2D1Factory6> factory6;
		SUCCEEDED(factory->QueryInterface(&factory6)))
		reshade::hooks::install("ID2D1Factory6::CreateDevice", reshade::hooks::vtable_from_instance(factory6.get()), 31, &ID2D1Factory6_CreateDevice);
	if (com_ptr<ID2D1Factory7> factory7;
		SUCCEEDED(factory->QueryInterface(&factory7)))
		reshade::hooks::install("ID2D1Factory7::CreateDevice", reshade::hooks::vtable_from_instance(factory7.get()), 32, &ID2D1Factory7_CreateDevice);

	return hr;
}
