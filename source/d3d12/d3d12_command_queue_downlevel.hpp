/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <D3D12Downlevel.h>
#include "runtime_d3d12.hpp"

struct D3D12CommandQueue;

struct DECLSPEC_UUID("98CF28C0-F383-487E-A61E-3A638FEE29BD") D3D12CommandQueueDownlevel final : ID3D12CommandQueueDownlevel, public reshade::d3d12::runtime_impl
{
	D3D12CommandQueueDownlevel(D3D12CommandQueue *queue, ID3D12CommandQueueDownlevel *original);

	D3D12CommandQueueDownlevel(const D3D12CommandQueueDownlevel &) = delete;
	D3D12CommandQueueDownlevel &operator=(const D3D12CommandQueueDownlevel &) = delete;

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region ID3D12CommandQueueDownlevel
	HRESULT STDMETHODCALLTYPE Present(ID3D12GraphicsCommandList *pOpenCommandList, ID3D12Resource *pSourceTex2D, HWND hWindow, D3D12_DOWNLEVEL_PRESENT_FLAGS Flags) override;
	#pragma endregion

	ID3D12CommandQueueDownlevel *_orig;
	D3D12CommandQueue *const _parent_queue;
};
