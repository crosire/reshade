/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <D3D12Downlevel.h>
#include <memory> // std::shared_ptr

struct D3D12CommandQueue;
namespace reshade::d3d12 { class runtime_d3d12; }

struct DECLSPEC_UUID("98CF28C0-F383-487E-A61E-3A638FEE29BD") D3D12CommandQueueDownlevel : ID3D12CommandQueueDownlevel
{
	D3D12CommandQueueDownlevel(D3D12CommandQueue *queue, ID3D12CommandQueueDownlevel *original);

	D3D12CommandQueueDownlevel(const D3D12CommandQueueDownlevel &) = delete;
	D3D12CommandQueueDownlevel &operator=(const D3D12CommandQueueDownlevel &) = delete;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;

	#pragma region ID3D12CommandQueueDownlevel
	HRESULT STDMETHODCALLTYPE Present(ID3D12GraphicsCommandList *pOpenCommandList, ID3D12Resource *pSourceTex2D, HWND hWindow, D3D12_DOWNLEVEL_PRESENT_FLAGS Flags) override;
	#pragma endregion

	ULONG _ref = 1;
	ID3D12CommandQueueDownlevel *_orig;
	D3D12Device *const _device;
	std::unique_ptr<reshade::d3d12::runtime_d3d12> _runtime;
};
