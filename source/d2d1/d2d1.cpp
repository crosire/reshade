/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "dxgi/dxgi_device.hpp"
#include "com_ptr.hpp"
#include <d2d1_3.h>

#define ID2D1Factory_CreateDevice_Impl(vtable_offset, factory_interface_version, device_interface_version) \
	HRESULT STDMETHODCALLTYPE ID2D1Factory##factory_interface_version##_CreateDevice(ID2D1Factory##factory_interface_version *factory, IDXGIDevice *dxgiDevice, ID2D1Device##device_interface_version **d2dDevice) \
	{ \
		LOG(INFO) << "Redirecting " << "ID2D1Factory" #factory_interface_version "::CreateDevice" << '(' \
			<<   "this = " << factory \
			<< ", dxgiDevice = " << dxgiDevice \
			<< ", d2dDevice = " << d2dDevice \
			<< ')' << " ..."; \
		if (com_ptr<DXGIDevice> device_proxy; \
			SUCCEEDED(dxgiDevice->QueryInterface(&device_proxy))) \
			dxgiDevice = device_proxy->_orig; \
		const HRESULT hr = reshade::hooks::call(ID2D1Factory##factory_interface_version##_CreateDevice, vtable_from_instance(factory) + vtable_offset)(factory, dxgiDevice, d2dDevice); \
		if (FAILED(hr)) \
			LOG(WARN) << "ID2D1Factory" #factory_interface_version "::CreateDevice" << " failed with error code " << hr << '.'; \
		return hr; \
	}

ID2D1Factory_CreateDevice_Impl(17, 1,  )
ID2D1Factory_CreateDevice_Impl(27, 2, 1)
ID2D1Factory_CreateDevice_Impl(28, 3, 2)
ID2D1Factory_CreateDevice_Impl(29, 4, 3)
ID2D1Factory_CreateDevice_Impl(30, 5, 4)
ID2D1Factory_CreateDevice_Impl(31, 6, 5)
ID2D1Factory_CreateDevice_Impl(32, 7, 6)

HOOK_EXPORT HRESULT WINAPI D2D1CreateDevice(IDXGIDevice *dxgiDevice, CONST D2D1_CREATION_PROPERTIES *creationProperties, ID2D1Device **d2dDevice)
{
	LOG(INFO) << "Redirecting " << "D2D1CreateDevice" << '('
		<<   "dxgiDevice = " << dxgiDevice
		<< ", creationProperties = " << creationProperties
		<< ", d2dDevice = " << d2dDevice
		<< ')' << " ...";

	// Get original DXGI device and pass that into D2D1 (since D2D1 does not work properly when a wrapped device is passed to it)
	if (com_ptr<DXGIDevice> device_proxy;
		SUCCEEDED(dxgiDevice->QueryInterface(&device_proxy)))
		dxgiDevice = device_proxy->_orig;

	// Choose the correct overload (instead of the template D2D1CreateDevice functions also defined in the header)
	using D2D1CreateDevice_t = HRESULT(WINAPI *)(IDXGIDevice *, CONST D2D1_CREATION_PROPERTIES *, ID2D1Device **);

	const HRESULT hr = reshade::hooks::call<D2D1CreateDevice_t>(D2D1CreateDevice)(dxgiDevice, creationProperties, d2dDevice);
	if (FAILED(hr))
	{
		LOG(WARN) << "D2D1CreateDevice" << " failed with error code " << hr << '.';
	}

	return hr;
}

HOOK_EXPORT HRESULT WINAPI D2D1CreateFactory(D2D1_FACTORY_TYPE factoryType, REFIID riid, CONST D2D1_FACTORY_OPTIONS *pFactoryOptions, void **ppIFactory)
{
	LOG(INFO) << "Redirecting " << "D2D1CreateFactory" << '('
		<<   "factoryType = " << factoryType
		<< ", riid = " << riid
		<< ", pFactoryOptions = " << pFactoryOptions
		<< ", ppIFactory = " << ppIFactory
		<< ')' << " ...";

	// Choose the correct overload (instead of the template D2D1CreateFactory functions also defined in the header)
	using D2D1CreateFactory_t = HRESULT(WINAPI *)(D2D1_FACTORY_TYPE, REFIID, CONST D2D1_FACTORY_OPTIONS *, void **);

	const HRESULT hr = reshade::hooks::call<D2D1CreateFactory_t>(D2D1CreateFactory)(factoryType, riid, pFactoryOptions, ppIFactory);
	if (FAILED(hr))
	{
		LOG(WARN) << "D2D1CreateFactory" << " failed with error code " << hr << '.';
		return hr;
	}

	ID2D1Factory *const factory = static_cast<ID2D1Factory *>(*ppIFactory);

	// Check for factory interface version support and install 'CreateDevice' hooks for existing ones
	if (com_ptr<ID2D1Factory1> factory1; SUCCEEDED(factory->QueryInterface(&factory1)))
		reshade::hooks::install("ID2D1Factory1::CreateDevice", vtable_from_instance(factory1.get()), 17, reinterpret_cast<reshade::hook::address>(ID2D1Factory1_CreateDevice));
	if (com_ptr<ID2D1Factory2> factory2; SUCCEEDED(factory->QueryInterface(&factory2)))
		reshade::hooks::install("ID2D1Factory2::CreateDevice", vtable_from_instance(factory2.get()), 27, reinterpret_cast<reshade::hook::address>(ID2D1Factory2_CreateDevice));
	if (com_ptr<ID2D1Factory3> factory3; SUCCEEDED(factory->QueryInterface(&factory3)))
		reshade::hooks::install("ID2D1Factory3::CreateDevice", vtable_from_instance(factory3.get()), 28, reinterpret_cast<reshade::hook::address>(ID2D1Factory3_CreateDevice));
	if (com_ptr<ID2D1Factory4> factory4; SUCCEEDED(factory->QueryInterface(&factory4)))
		reshade::hooks::install("ID2D1Factory4::CreateDevice", vtable_from_instance(factory4.get()), 29, reinterpret_cast<reshade::hook::address>(ID2D1Factory4_CreateDevice));
	if (com_ptr<ID2D1Factory5> factory5; SUCCEEDED(factory->QueryInterface(&factory5)))
		reshade::hooks::install("ID2D1Factory5::CreateDevice", vtable_from_instance(factory5.get()), 30, reinterpret_cast<reshade::hook::address>(ID2D1Factory5_CreateDevice));
	if (com_ptr<ID2D1Factory6> factory6; SUCCEEDED(factory->QueryInterface(&factory6)))
		reshade::hooks::install("ID2D1Factory6::CreateDevice", vtable_from_instance(factory6.get()), 31, reinterpret_cast<reshade::hook::address>(ID2D1Factory6_CreateDevice));
	if (com_ptr<ID2D1Factory7> factory7; SUCCEEDED(factory->QueryInterface(&factory7)))
		reshade::hooks::install("ID2D1Factory7::CreateDevice", vtable_from_instance(factory7.get()), 32, reinterpret_cast<reshade::hook::address>(ID2D1Factory7_CreateDevice));

	return hr;
}
