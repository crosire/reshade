/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#if RESHADE_ADDON >= 2

extern thread_local bool g_in_d3d12_pipeline_creation;

HRESULT STDMETHODCALLTYPE ID3D12PipelineLibrary_LoadGraphicsPipeline(ID3D12PipelineLibrary *pPipelineLibrary, LPCWSTR pName, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState);
HRESULT STDMETHODCALLTYPE ID3D12PipelineLibrary_LoadComputePipeline(ID3D12PipelineLibrary *pPipelineLibrary, LPCWSTR pName, const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState);

HRESULT STDMETHODCALLTYPE ID3D12PipelineLibrary1_LoadPipeline(ID3D12PipelineLibrary1 *pPipelineLibrary, LPCWSTR pName, const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc, REFIID riid, void **ppPipelineState);

#endif
