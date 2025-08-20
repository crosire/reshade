/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <dxgi1_6.h>

struct DECLSPEC_UUID("E1688933-5520-4A9A-8791-CBE1E71520B8") DXGIOutput final : IDXGIOutput6
{
	DXGIOutput(IDXGIOutput *original);
	~DXGIOutput();

	DXGIOutput(const DXGIOutput &) = delete;
	DXGIOutput &operator=(const DXGIOutput &) = delete;

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region IDXGIObject
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData) override;
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) override;
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData) override;
	HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent) override;
	#pragma endregion
	#pragma region IDXGIOutput
	HRESULT STDMETHODCALLTYPE GetDesc(DXGI_OUTPUT_DESC *pDesc) override;
	HRESULT STDMETHODCALLTYPE GetDisplayModeList(DXGI_FORMAT EnumFormat, UINT Flags, UINT *pNumModes, DXGI_MODE_DESC *pDesc) override;
	HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(const DXGI_MODE_DESC *pModeToMatch, DXGI_MODE_DESC *pClosestMatch, IUnknown *pConcernedDevice) override;
	HRESULT STDMETHODCALLTYPE WaitForVBlank() override;
	HRESULT STDMETHODCALLTYPE TakeOwnership(IUnknown *pDevice, BOOL Exclusive) override;
	void    STDMETHODCALLTYPE ReleaseOwnership() override;
	HRESULT STDMETHODCALLTYPE GetGammaControlCapabilities(DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps) override;
	HRESULT STDMETHODCALLTYPE SetGammaControl(const DXGI_GAMMA_CONTROL *pArray) override;
	HRESULT STDMETHODCALLTYPE GetGammaControl(DXGI_GAMMA_CONTROL *pArray) override;
	HRESULT STDMETHODCALLTYPE SetDisplaySurface(IDXGISurface *pScanoutSurface) override;
	HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData(IDXGISurface *pDestination) override;
	HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats) override;
	#pragma endregion
	#pragma region IDXGIOutput1
	HRESULT STDMETHODCALLTYPE GetDisplayModeList1(DXGI_FORMAT EnumFormat, UINT Flags, UINT *pNumModes, DXGI_MODE_DESC1 *pDesc) override;
	HRESULT STDMETHODCALLTYPE FindClosestMatchingMode1(const DXGI_MODE_DESC1 *pModeToMatch, DXGI_MODE_DESC1 *pClosestMatch, IUnknown *pConcernedDevice) override;
	HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData1(IDXGIResource *pDestination) override;
	HRESULT STDMETHODCALLTYPE DuplicateOutput(IUnknown *pDevice, IDXGIOutputDuplication **ppOutputDuplication) override;
	#pragma endregion
	#pragma region IDXGIOutput2
	BOOL    STDMETHODCALLTYPE SupportsOverlays() override;
	#pragma endregion
	#pragma region IDXGIOutput3
	HRESULT STDMETHODCALLTYPE CheckOverlaySupport(DXGI_FORMAT EnumFormat, IUnknown *pConcernedDevice, UINT *pFlags) override;
	#pragma endregion
	#pragma region IDXGIOutput4
	HRESULT STDMETHODCALLTYPE CheckOverlayColorSpaceSupport(DXGI_FORMAT Format, DXGI_COLOR_SPACE_TYPE ColorSpace, IUnknown *pConcernedDevice, UINT *pFlags) override;
	#pragma endregion
	#pragma region IDXGIOutput5
	HRESULT STDMETHODCALLTYPE DuplicateOutput1(IUnknown *pDevice, UINT Flags, UINT SupportedFormatsCount, const DXGI_FORMAT *pSupportedFormats, IDXGIOutputDuplication **ppOutputDuplication) override;
	#pragma endregion
	#pragma region IDXGIOutput6
	HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_OUTPUT_DESC1 *pDesc) override;
	HRESULT STDMETHODCALLTYPE CheckHardwareCompositionSupport(UINT *pFlags) override;
	#pragma endregion

	bool check_and_upgrade_interface(REFIID riid);

	static bool check_and_proxy_interface(REFIID riid, void **object);
	template <typename T>
	static bool check_and_proxy_interface(T **object)
	{
		return check_and_proxy_interface(__uuidof(**object), reinterpret_cast<void **>(object));
	}

	IDXGIOutput *_orig;
	LONG _ref = 1;
	unsigned short _interface_version = 0;
	bool _temporary = false;
};
