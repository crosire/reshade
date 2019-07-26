/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "d3d12_device.hpp"

extern reshade::log::message &operator<<(reshade::log::message &m, REFIID riid);

HOOK_EXPORT HRESULT WINAPI D3D12CreateDevice(
	IUnknown *pAdapter,
	D3D_FEATURE_LEVEL MinimumFeatureLevel,
	REFIID riid,
	void **ppDevice)
{
	LOG(INFO) << "Redirecting D3D12CreateDevice" << '(' << pAdapter << ", " << std::hex << MinimumFeatureLevel << std::dec << ", " << riid << ", " << ppDevice << ')' << " ...";

	const HRESULT hr = reshade::hooks::call(D3D12CreateDevice)(pAdapter, MinimumFeatureLevel, riid, ppDevice);

	if (FAILED(hr))
	{
		LOG(WARN) << "> D3D12CreateDevice failed with error code " << std::hex << hr << std::dec << '!';
		return hr;
	}

	if (ppDevice == nullptr)
		return hr;

	// The returned device should alway implement the 'ID3D12Device' base interface
	const auto device_proxy = new D3D12Device(static_cast<ID3D12Device *>(*ppDevice));
	device_proxy->check_and_upgrade_interface(riid); // Upgrade to the actual interface version requested here

	*ppDevice = device_proxy;

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning ID3D12Device" << device_proxy->_interface_version << " object " << device_proxy << '.';
#endif
	return hr;
}

HOOK_EXPORT HRESULT WINAPI D3D12GetDebugInterface(
	REFIID riid,
	void **ppvDebug)
{
	return reshade::hooks::call(D3D12GetDebugInterface)(riid, ppvDebug);
}

HOOK_EXPORT HRESULT WINAPI D3D12CreateRootSignatureDeserializer(
	LPCVOID pSrcData,
	SIZE_T SrcDataSizeInBytes,
	REFIID pRootSignatureDeserializerInterface,
	void **ppRootSignatureDeserializer)
{
	return reshade::hooks::call(D3D12CreateRootSignatureDeserializer)(pSrcData, SrcDataSizeInBytes, pRootSignatureDeserializerInterface, ppRootSignatureDeserializer);
}

HOOK_EXPORT HRESULT WINAPI D3D12CreateVersionedRootSignatureDeserializer(
	LPCVOID pSrcData,
	SIZE_T SrcDataSizeInBytes,
	REFIID pRootSignatureDeserializerInterface,
	void **ppRootSignatureDeserializer)
{
	return reshade::hooks::call(D3D12CreateVersionedRootSignatureDeserializer)(pSrcData, SrcDataSizeInBytes, pRootSignatureDeserializerInterface, ppRootSignatureDeserializer);
}

HOOK_EXPORT HRESULT WINAPI D3D12EnableExperimentalFeatures(
	UINT NumFeatures,
	const IID *pIIDs,
	void *pConfigurationStructs,
	UINT *pConfigurationStructSizes)
{
	return reshade::hooks::call(D3D12EnableExperimentalFeatures)(NumFeatures, pIIDs, pConfigurationStructs, pConfigurationStructSizes);
}

HOOK_EXPORT HRESULT WINAPI D3D12SerializeRootSignature(
	const D3D12_ROOT_SIGNATURE_DESC *pRootSignature,
	D3D_ROOT_SIGNATURE_VERSION Version,
	ID3DBlob **ppBlob,
	ID3DBlob **ppErrorBlob)
{
	return reshade::hooks::call(D3D12SerializeRootSignature)(pRootSignature, Version, ppBlob, ppErrorBlob);
}

HOOK_EXPORT HRESULT WINAPI D3D12SerializeVersionedRootSignature(
	const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *pRootSignature,
	ID3DBlob **ppBlob,
	ID3DBlob **ppErrorBlob)
{
	return reshade::hooks::call(D3D12SerializeVersionedRootSignature)(pRootSignature, ppBlob, ppErrorBlob);
}
