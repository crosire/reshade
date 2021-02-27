/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "render_d3d11.hpp"

struct D3D11Device;

struct DECLSPEC_UUID("592F5E83-A17B-4EEB-A2BF-7568DA2A3728") D3D11CommandList final : ID3D11CommandList, public reshade::d3d11::command_list_impl
{
	D3D11CommandList(D3D11Device *device, ID3D11CommandList *original);

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

	ULONG _ref = 1;
	D3D11Device *const _device;
};
