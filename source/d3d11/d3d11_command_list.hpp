/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include "d3d11_impl_device_context.hpp"

class D3D11Device;

class DECLSPEC_UUID("EF948B70-4CD8-476B-AD25-2D7F3E521AA2") D3D11CommandList final : public ID3D11CommandList, public reshade::d3d11::command_list_impl
{
public:
	D3D11CommandList(D3D11Device *device, ID3D11CommandList *original);
	~D3D11CommandList();

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region ID3D11DeviceChild
	void    STDMETHODCALLTYPE GetDevice(ID3D11Device **ppDevice) override;
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData) override;
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData) override;
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData) override;
	#pragma endregion
	#pragma region ID3D11CommandList
	UINT    STDMETHODCALLTYPE GetContextFlags() override;
	#pragma endregion

	bool check_and_upgrade_interface(REFIID riid);

	using command_list_impl::_orig;
	LONG _ref = 1;
};
