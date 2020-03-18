/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "com_ptr.hpp"
#include "dxgi/dxgi_device.hpp"
#include <d2d1_1.h>

HRESULT STDMETHODCALLTYPE ID2D1Factory1_CreateDevice(ID2D1Factory1 *factory, IDXGIDevice *dxgiDevice, ID2D1Device **d2dDevice)
{
	LOG(INFO) << "Redirecting ID2D1Factory1::CreateDevice" << '('
		<< "this = " << factory
		<< ", dxgiDevice = " << dxgiDevice
		<< ", d2dDevice = " << d2dDevice
		<< ')' << " ...";

	// Get original DXGI device and pass that into D2D1 (since D2D1 does not work properly when a wrapped device is passed to it)
	if (com_ptr<DXGIDevice> device_proxy;
		SUCCEEDED(dxgiDevice->QueryInterface(&device_proxy)))
		dxgiDevice = device_proxy->_orig;

	const HRESULT hr = reshade::hooks::call(ID2D1Factory1_CreateDevice)(factory, dxgiDevice, d2dDevice);
	if (FAILED(hr))
	{
		LOG(WARN) << "ID2D1Factory1::CreateDevice failed with error code " << hr << '!';
	}

	return hr;
}

HOOK_EXPORT HRESULT WINAPI D2D1CreateDevice(IDXGIDevice *dxgiDevice, CONST D2D1_CREATION_PROPERTIES *creationProperties, ID2D1Device **d2dDevice)
{
	LOG(INFO) << "Redirecting D2D1CreateDevice" << '('
		<< "dxgiDevice = " << dxgiDevice
		<< ", creationProperties = " << creationProperties
		<< ", d2dDevice = " << d2dDevice
		<< ')' << " ...";

	if (com_ptr<DXGIDevice> device_proxy;
		SUCCEEDED(dxgiDevice->QueryInterface(&device_proxy)))
		dxgiDevice = device_proxy->_orig;

	// Choose the correct overload (instead of the template D2D1CreateDevice functions also defined in the header)
	using D2D1CreateDevice_t = HRESULT(WINAPI *)(IDXGIDevice *, CONST D2D1_CREATION_PROPERTIES *, ID2D1Device **);

	const HRESULT hr = reshade::hooks::call<D2D1CreateDevice_t>(D2D1CreateDevice)(dxgiDevice, creationProperties, d2dDevice);
	if (FAILED(hr))
	{
		LOG(WARN) << "D2D1CreateDevice failed with error code " << hr << '!';
	}

	return hr;
}

HOOK_EXPORT HRESULT WINAPI D2D1CreateFactory(D2D1_FACTORY_TYPE factoryType, REFIID riid, CONST D2D1_FACTORY_OPTIONS *pFactoryOptions, void **ppIFactory)
{
	LOG(INFO) << "Redirecting CreateDXGIFactory1" << '('
		<< "factoryType = " << factoryType
		<< ", riid = " << riid
		<< ", pFactoryOptions = " << pFactoryOptions
		<< ", ppIFactory = " << ppIFactory
		<< ')' << " ...";

	// Choose the correct overload (instead of the template D2D1CreateFactory functions also defined in the header)
	using D2D1CreateFactory_t = HRESULT(WINAPI *)(D2D1_FACTORY_TYPE, REFIID, CONST D2D1_FACTORY_OPTIONS *, void **);

	const HRESULT hr = reshade::hooks::call<D2D1CreateFactory_t>(D2D1CreateFactory)(factoryType, riid, pFactoryOptions, ppIFactory);
	if (FAILED(hr))
	{
		LOG(WARN) << "D2D1CreateFactory failed with error code " << hr << '!';
		return hr;
	}

	ID2D1Factory *const factory = static_cast<ID2D1Factory *>(*ppIFactory);

	// Check for D2D1.1 support and install ID2D1Factory1 hooks if it exists
	if (com_ptr<ID2D1Factory1> factory1; SUCCEEDED(factory->QueryInterface(&factory1)))
	{
		reshade::hooks::install("ID2D1Factory1::CreateDevice", vtable_from_instance(factory1.get()), 17, reinterpret_cast<reshade::hook::address>(ID2D1Factory1_CreateDevice));
	}

	return hr;
}
