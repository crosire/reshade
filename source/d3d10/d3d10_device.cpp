/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "d3d10_device.hpp"
#include "dxgi/dxgi_device.hpp"
#include "render_d3d10_utils.hpp"

D3D10Device::D3D10Device(IDXGIDevice1 *dxgi_device, ID3D10Device1 *original) :
	device_impl(original),
	_dxgi_device(new DXGIDevice(dxgi_device, this))
{
	assert(_orig != nullptr);

	// Add proxy object to the private data of the device, so that it can be retrieved again when only the original device is available
	D3D10Device *const device_proxy = this;
	_orig->SetPrivateData(__uuidof(D3D10Device), sizeof(device_proxy), &device_proxy);
}

bool D3D10Device::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		// IUnknown is handled by DXGIDevice
		riid == __uuidof(ID3D10Device) ||
		riid == __uuidof(ID3D10Device1))
		return true;

	return false;
}

HRESULT STDMETHODCALLTYPE D3D10Device::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	// Note: Objects must have an identity, so use DXGIDevice for IID_IUnknown
	// See https://docs.microsoft.com/windows/desktop/com/rules-for-implementing-queryinterface
	if (_dxgi_device->check_and_upgrade_interface(riid))
	{
		_dxgi_device->AddRef();
		*ppvObj = _dxgi_device;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D10Device::AddRef()
{
	_orig->AddRef();

	// Add references to other objects that are coupled with the device
	_dxgi_device->AddRef();

	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE D3D10Device::Release()
{
	// Release references to other objects that are coupled with the device
	_dxgi_device->Release();

	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
		return _orig->Release(), ref;

	const auto orig = _orig;
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroying " << "ID3D10Device1" << " object " << this << " (" << orig << ").";
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		LOG(WARN) << "Reference count for " << "ID3D10Device1" << " object " << this << " (" << orig << ") is inconsistent (" << ref_orig << ").";
	return 0;
}

void    STDMETHODCALLTYPE D3D10Device::VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppConstantBuffers)
{
	_orig->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

#if RESHADE_ADDON
	assert(NumBuffers <= D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);

	if (reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::push_descriptors)].empty())
		return;

#ifndef WIN64
	reshade::api::resource buffer_handles[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	for (UINT i = 0; i < NumBuffers; ++i)
		buffer_handles[i] = { reinterpret_cast<uintptr_t>(ppConstantBuffers[i]) };
#else
	static_assert(sizeof(*ppConstantBuffers) == sizeof(reshade::api::resource));
	const auto buffer_handles = reinterpret_cast<const reshade::api::resource *>(ppConstantBuffers);
#endif

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(this, reshade::api::shader_stage::vertex, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::constant_buffer, StartSlot, NumBuffers, buffer_handles);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

#if RESHADE_ADDON
	assert(NumViews <= D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);

	if (reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::push_descriptors)].empty())
		return;

#ifndef WIN64
	reshade::api::resource_view view_handles[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < NumViews; ++i)
		view_handles[i] = { reinterpret_cast<uintptr_t>(ppShaderResourceViews[i]) };
#else
	static_assert(sizeof(*ppShaderResourceViews) == sizeof(reshade::api::resource_view));
	const auto view_handles = reinterpret_cast<const reshade::api::resource_view *>(ppShaderResourceViews);
#endif

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(this, reshade::api::shader_stage::pixel, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::shader_resource_view, StartSlot, NumViews, view_handles);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::PSSetShader(ID3D10PixelShader *pPixelShader)
{
	_orig->PSSetShader(pPixelShader);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_type::graphics_pixel_shader, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pPixelShader) });
#endif
}
void    STDMETHODCALLTYPE D3D10Device::PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState *const *ppSamplers)
{
	_orig->PSSetSamplers(StartSlot, NumSamplers, ppSamplers);

#if RESHADE_ADDON
	assert(NumSamplers <= D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT);

	if (reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::push_descriptors)].empty())
		return;

#ifndef WIN64
	reshade::api::sampler sampler_handles[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
	for (UINT i = 0; i < NumSamplers; ++i)
		sampler_handles[i] = { reinterpret_cast<uintptr_t>(ppSamplers[i]) };
#else
	static_assert(sizeof(*ppSamplers) == sizeof(reshade::api::sampler));
	const auto sampler_handles = reinterpret_cast<const reshade::api::sampler *>(ppSamplers);
#endif

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(this, reshade::api::shader_stage::pixel, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::sampler, StartSlot, NumSamplers, sampler_handles);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::VSSetShader(ID3D10VertexShader *pVertexShader)
{
	_orig->VSSetShader(pVertexShader);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_type::graphics_vertex_shader, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pVertexShader) });
#endif
}
void    STDMETHODCALLTYPE D3D10Device::DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(this, IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0))
		return;
#endif
	_orig->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
}
void    STDMETHODCALLTYPE D3D10Device::Draw(UINT VertexCount, UINT StartVertexLocation)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::draw>(this, VertexCount, 1, StartVertexLocation, 0))
		return;
#endif
	_orig->Draw(VertexCount, StartVertexLocation);
}
void    STDMETHODCALLTYPE D3D10Device::PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppConstantBuffers)
{
	_orig->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

#if RESHADE_ADDON
	assert(NumBuffers <= D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);

	if (reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::push_descriptors)].empty())
		return;

#ifndef WIN64
	reshade::api::resource buffer_handles[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	for (UINT i = 0; i < NumBuffers; ++i)
		buffer_handles[i] = { reinterpret_cast<uintptr_t>(ppConstantBuffers[i]) };
#else
	static_assert(sizeof(*ppConstantBuffers) == sizeof(reshade::api::resource));
	const auto buffer_handles = reinterpret_cast<const reshade::api::resource *>(ppConstantBuffers);
#endif

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(this, reshade::api::shader_stage::pixel, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::constant_buffer, StartSlot, NumBuffers, buffer_handles);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::IASetInputLayout(ID3D10InputLayout *pInputLayout)
{
	_orig->IASetInputLayout(pInputLayout);
}
void    STDMETHODCALLTYPE D3D10Device::IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets)
{
	_orig->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);

#if RESHADE_ADDON
	assert(NumBuffers <= D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);

	if (reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::bind_vertex_buffers)].empty())
		return;

#ifndef WIN64
	reshade::api::resource buffer_handles[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < NumBuffers; ++i)
		buffer_handles[i] = { reinterpret_cast<uintptr_t>(ppVertexBuffers[i]) };
#else
	static_assert(sizeof(*ppVertexBuffers) == sizeof(reshade::api::resource));
	const auto buffer_handles = reinterpret_cast<const reshade::api::resource *>(ppVertexBuffers);
#endif

	uint64_t offsets[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < NumBuffers; ++i)
		offsets[i] = pOffsets[i];

	reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(this, StartSlot, NumBuffers, buffer_handles, offsets, pStrides);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::IASetIndexBuffer(ID3D10Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset)
{
	_orig->IASetIndexBuffer(pIndexBuffer, Format, Offset);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_index_buffer>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pIndexBuffer) }, Offset, pIndexBuffer == nullptr ? 0 : Format == DXGI_FORMAT_R16_UINT ? 2 : 4);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(this, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation))
		return;
#endif
	_orig->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}
void    STDMETHODCALLTYPE D3D10Device::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::draw>(this, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation))
		return;
#endif
	_orig->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}
void    STDMETHODCALLTYPE D3D10Device::GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppConstantBuffers)
{
	_orig->GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);

#if RESHADE_ADDON
	assert(NumBuffers <= D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);

	if (reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::push_descriptors)].empty())
		return;

#ifndef WIN64
	reshade::api::resource buffer_handles[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	for (UINT i = 0; i < NumBuffers; ++i)
		buffer_handles[i] = { reinterpret_cast<uintptr_t>(ppConstantBuffers[i]) };
#else
	static_assert(sizeof(*ppConstantBuffers) == sizeof(reshade::api::resource));
	const auto buffer_handles = reinterpret_cast<const reshade::api::resource *>(ppConstantBuffers);
#endif

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(this, reshade::api::shader_stage::geometry, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::constant_buffer, StartSlot, NumBuffers, buffer_handles);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::GSSetShader(ID3D10GeometryShader *pShader)
{
	_orig->GSSetShader(pShader);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_type::graphics_geometry_shader, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pShader) });
#endif
}
void    STDMETHODCALLTYPE D3D10Device::IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY Topology)
{
	_orig->IASetPrimitiveTopology(Topology);

#if RESHADE_ADDON
	const reshade::api::pipeline_state state = reshade::api::pipeline_state::primitive_topology;

	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, reinterpret_cast<const uint32_t *>(&Topology));
#endif
}
void    STDMETHODCALLTYPE D3D10Device::VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

#if RESHADE_ADDON
	assert(NumViews <= D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);

	if (reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::push_descriptors)].empty())
		return;

#ifndef WIN64
	reshade::api::resource_view view_handles[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < NumViews; ++i)
		view_handles[i] = { reinterpret_cast<uintptr_t>(ppShaderResourceViews[i]) };
#else
	static_assert(sizeof(*ppShaderResourceViews) == sizeof(reshade::api::resource_view));
	const auto view_handles = reinterpret_cast<const reshade::api::resource_view *>(ppShaderResourceViews);
#endif

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(this, reshade::api::shader_stage::vertex, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::shader_resource_view, StartSlot, NumViews, view_handles);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState *const *ppSamplers)
{
	_orig->VSSetSamplers(StartSlot, NumSamplers, ppSamplers);

#if RESHADE_ADDON
	assert(NumSamplers <= D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT);

	if (reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::push_descriptors)].empty())
		return;

#ifndef WIN64
	reshade::api::sampler sampler_handles[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
	for (UINT i = 0; i < NumSamplers; ++i)
		sampler_handles[i] = { reinterpret_cast<uintptr_t>(ppSamplers[i]) };
#else
	static_assert(sizeof(*ppSamplers) == sizeof(reshade::api::sampler));
	const auto sampler_handles = reinterpret_cast<const reshade::api::sampler *>(ppSamplers);
#endif

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(this, reshade::api::shader_stage::vertex, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::sampler, StartSlot, NumSamplers, sampler_handles);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::SetPredication(ID3D10Predicate *pPredicate, BOOL PredicateValue)
{
	_orig->SetPredication(pPredicate, PredicateValue);
}
void    STDMETHODCALLTYPE D3D10Device::GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

#if RESHADE_ADDON
	assert(NumViews <= D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);

	if (reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::push_descriptors)].empty())
		return;

#ifndef WIN64
	reshade::api::resource_view view_handles[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < NumViews; ++i)
		view_handles[i] = { reinterpret_cast<uintptr_t>(ppShaderResourceViews[i]) };
#else
	static_assert(sizeof(*ppShaderResourceViews) == sizeof(reshade::api::resource_view));
	const auto view_handles = reinterpret_cast<const reshade::api::resource_view *>(ppShaderResourceViews);
#endif

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(this, reshade::api::shader_stage::geometry, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::shader_resource_view, StartSlot, NumViews, view_handles);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState *const *ppSamplers)
{
	_orig->GSSetSamplers(StartSlot, NumSamplers, ppSamplers);

#if RESHADE_ADDON
	assert(NumSamplers <= D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT);

	if (reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::push_descriptors)].empty())
		return;

#ifndef WIN64
	reshade::api::sampler sampler_handles[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
	for (UINT i = 0; i < NumSamplers; ++i)
		sampler_handles[i] = { reinterpret_cast<uintptr_t>(ppSamplers[i]) };
#else
	static_assert(sizeof(*ppSamplers) == sizeof(reshade::api::sampler));
	const auto sampler_handles = reinterpret_cast<const reshade::api::sampler *>(ppSamplers);
#endif

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(this, reshade::api::shader_stage::geometry, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::sampler, StartSlot, NumSamplers, sampler_handles);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::OMSetRenderTargets(UINT NumViews, ID3D10RenderTargetView *const *ppRenderTargetViews, ID3D10DepthStencilView *pDepthStencilView)
{
#if RESHADE_ADDON
	if (_has_open_render_pass)
	{
		reshade::invoke_addon_event<reshade::addon_event::end_render_pass>(this);
		_has_open_render_pass = false;
	}
#endif

	_orig->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);

#if RESHADE_ADDON
	assert(NumViews <= D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT);

	if ((NumViews == 0 && pDepthStencilView == nullptr) ||
		reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::begin_render_pass)].empty())
		return;

#ifndef WIN64
	reshade::api::resource_view rtvs[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
	for (UINT i = 0; i < NumViews; ++i)
		rtvs[i] = { reinterpret_cast<uintptr_t>(ppRenderTargetViews[i]) };
#else
	static_assert(sizeof(*ppRenderTargetViews) == sizeof(reshade::api::resource_view));
	const auto rtvs = reinterpret_cast<const reshade::api::resource_view *>(ppRenderTargetViews);
#endif

	_has_open_render_pass = true;
	reshade::invoke_addon_event<reshade::addon_event::begin_render_pass>(this, NumViews, rtvs, reshade::api::resource_view { reinterpret_cast<uintptr_t>(pDepthStencilView) });
#endif
}
void    STDMETHODCALLTYPE D3D10Device::OMSetBlendState(ID3D10BlendState *pBlendState, const FLOAT BlendFactor[4], UINT SampleMask)
{
	_orig->OMSetBlendState(pBlendState, BlendFactor, SampleMask);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this,
		reshade::api::pipeline_type::graphics_blend_state, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pBlendState) });

	const reshade::api::pipeline_state states[2] = { reshade::api::pipeline_state::blend_constant, reshade::api::pipeline_state::sample_mask };
	const uint32_t values[2] = {
		(BlendFactor == nullptr) ? 0xFFFFFFFF : // Default blend factor is { 1, 1, 1, 1 }
			((static_cast<uint32_t>(BlendFactor[0] * 255.f) & 0xFF)) |
			((static_cast<uint32_t>(BlendFactor[1] * 255.f) & 0xFF) << 8) |
			((static_cast<uint32_t>(BlendFactor[2] * 255.f) & 0xFF) << 16) |
			((static_cast<uint32_t>(BlendFactor[3] * 255.f) & 0xFF) << 24), SampleMask };
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 2, states, values);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::OMSetDepthStencilState(ID3D10DepthStencilState *pDepthStencilState, UINT StencilRef)
{
	_orig->OMSetDepthStencilState(pDepthStencilState, StencilRef);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this,
		reshade::api::pipeline_type::graphics_depth_stencil_state, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pDepthStencilState) });

	const reshade::api::pipeline_state state = reshade::api::pipeline_state::stencil_reference_value;
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, &StencilRef);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::SOSetTargets(UINT NumBuffers, ID3D10Buffer *const *ppSOTargets, const UINT *pOffsets)
{
	_orig->SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
}
void    STDMETHODCALLTYPE D3D10Device::DrawAuto()
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::draw>(this, 0, 0, 0, 0))
		return;
#endif
	_orig->DrawAuto();
}
void    STDMETHODCALLTYPE D3D10Device::RSSetState(ID3D10RasterizerState *pRasterizerState)
{
	_orig->RSSetState(pRasterizerState);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this,
		reshade::api::pipeline_type::graphics_rasterizer_state, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pRasterizerState) });
#endif
}
void    STDMETHODCALLTYPE D3D10Device::RSSetViewports(UINT NumViewports, const D3D10_VIEWPORT *pViewports)
{
	_orig->RSSetViewports(NumViewports, pViewports);

#if RESHADE_ADDON
	assert(NumViewports <= D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);

	if (reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::bind_viewports)].empty())
		return;

	float viewport_data[6 * D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	for (UINT i = 0, k = 0; i < NumViewports; ++i, k += 6)
	{
		viewport_data[k + 0] = static_cast<float>(pViewports[i].TopLeftX);
		viewport_data[k + 1] = static_cast<float>(pViewports[i].TopLeftY);
		viewport_data[k + 2] = static_cast<float>(pViewports[i].Width);
		viewport_data[k + 3] = static_cast<float>(pViewports[i].Height);
		viewport_data[k + 4] = pViewports[i].MinDepth;
		viewport_data[k + 5] = pViewports[i].MaxDepth;
	}

	reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(this, 0, NumViewports, viewport_data);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::RSSetScissorRects(UINT NumRects, const D3D10_RECT *pRects)
{
	_orig->RSSetScissorRects(NumRects, pRects);

#if RESHADE_ADDON
	static_assert(sizeof(D3D10_RECT) == (sizeof(int32_t) * 4));

	reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(this, 0, NumRects, reinterpret_cast<const int32_t *>(pRects));
#endif
}
void    STDMETHODCALLTYPE D3D10Device::CopySubresourceRegion(ID3D10Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D10Resource *pSrcResource, UINT SrcSubresource, const D3D10_BOX *pSrcBox)
{
#if RESHADE_ADDON
	assert(pDstResource != nullptr && pSrcResource != nullptr);

	D3D10_RESOURCE_DIMENSION type;
	pDstResource->GetType(&type);
	if (type == D3D10_RESOURCE_DIMENSION_BUFFER)
	{
		assert(SrcSubresource == 0 && DstSubresource == 0);

		if (reshade::invoke_addon_event<reshade::addon_event::copy_buffer_region>(this,
			reshade::api::resource { reinterpret_cast<uintptr_t>(pSrcResource) }, pSrcBox != nullptr ? pSrcBox->left : 0,
			reshade::api::resource { reinterpret_cast<uintptr_t>(pDstResource) }, DstX, pSrcBox != nullptr ? pSrcBox->right - pSrcBox->left : ~0ull))
			return;
	}
	else
	{
		const int32_t dst_offset[3] = { static_cast<int32_t>(DstX), static_cast<int32_t>(DstY), static_cast<int32_t>(DstZ) };
		uint32_t size[3];
		if (pSrcBox != nullptr)
		{
			size[0] = pSrcBox->right - pSrcBox->left;
			size[1] = pSrcBox->bottom - pSrcBox->top;
			size[2] = pSrcBox->back - pSrcBox->front;
		}

		static_assert(sizeof(D3D10_BOX) == (sizeof(int32_t) * 6));

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(this,
			reshade::api::resource { reinterpret_cast<uintptr_t>(pSrcResource) }, SrcSubresource, reinterpret_cast<const int32_t *>(pSrcBox),
			reshade::api::resource { reinterpret_cast<uintptr_t>(pDstResource) }, DstSubresource, DstX != 0 || DstY != 0 || DstZ != 0 ? dst_offset : nullptr, pSrcBox != nullptr ? size : nullptr))
			return;
	}
#endif
	_orig->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
}
void    STDMETHODCALLTYPE D3D10Device::CopyResource(ID3D10Resource *pDstResource, ID3D10Resource *pSrcResource)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::copy_resource>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pSrcResource) }, reshade::api::resource { reinterpret_cast<uintptr_t>(pDstResource) }))
		return;
#endif
	_orig->CopyResource(pDstResource, pSrcResource);
}
void    STDMETHODCALLTYPE D3D10Device::UpdateSubresource(ID3D10Resource *pDstResource, UINT DstSubresource, const D3D10_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
{
#if RESHADE_ADDON
	assert(pDstResource != nullptr);

	D3D10_RESOURCE_DIMENSION type;
	pDstResource->GetType(&type);
	if (type == D3D10_RESOURCE_DIMENSION_BUFFER)
	{
		assert(DstSubresource == 0);

		if (reshade::invoke_addon_event<reshade::addon_event::upload_buffer_region>(this,
			pSrcData,
			reshade::api::resource { reinterpret_cast<uintptr_t>(pDstResource) },
			pDstBox != nullptr ? pDstBox->left : 0,
			pDstBox != nullptr ? pDstBox->right - pDstBox->left : SrcRowPitch))
			return;
	}
	else
	{
		static_assert(sizeof(D3D10_BOX) == (sizeof(int32_t) * 6));

		if (reshade::invoke_addon_event<reshade::addon_event::upload_texture_region>(this,
			pSrcData, SrcRowPitch, SrcDepthPitch,
			reshade::api::resource { reinterpret_cast<uintptr_t>(pDstResource) }, DstSubresource, reinterpret_cast<const int32_t *>(pDstBox)))
			return;
	}
#endif
	_orig->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
}
void    STDMETHODCALLTYPE D3D10Device::ClearRenderTargetView(ID3D10RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4])
{
#if RESHADE_ADDON
	if (const reshade::api::resource_view rtv = { reinterpret_cast<uintptr_t>(pRenderTargetView) };
		reshade::invoke_addon_event<reshade::addon_event::clear_render_target_views>(this, 1, &rtv, ColorRGBA))
		return;
#endif
	_orig->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
}
void    STDMETHODCALLTYPE D3D10Device::ClearDepthStencilView(ID3D10DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(this, reshade::api::resource_view { reinterpret_cast<uintptr_t>(pDepthStencilView) }, ClearFlags, Depth, Stencil))
		return;
#endif
	_orig->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
}
void    STDMETHODCALLTYPE D3D10Device::GenerateMips(ID3D10ShaderResourceView *pShaderResourceView)
{
	_orig->GenerateMips(pShaderResourceView);
}
void    STDMETHODCALLTYPE D3D10Device::ResolveSubresource(ID3D10Resource *pDstResource, UINT DstSubresource, ID3D10Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::resolve>(this,
		reshade::api::resource { reinterpret_cast<uintptr_t>(pSrcResource) }, SrcSubresource, nullptr,
		reshade::api::resource { reinterpret_cast<uintptr_t>(pDstResource) }, DstSubresource, nullptr, nullptr, reshade::d3d10::convert_format(Format)))
		return;
#endif
	_orig->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}
void    STDMETHODCALLTYPE D3D10Device::VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppConstantBuffers)
{
	_orig->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void    STDMETHODCALLTYPE D3D10Device::PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView **ppShaderResourceViews)
{
	_orig->PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void    STDMETHODCALLTYPE D3D10Device::PSGetShader(ID3D10PixelShader **ppPixelShader)
{
	_orig->PSGetShader(ppPixelShader);
}
void    STDMETHODCALLTYPE D3D10Device::PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState **ppSamplers)
{
	_orig->PSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void    STDMETHODCALLTYPE D3D10Device::VSGetShader(ID3D10VertexShader **ppVertexShader)
{
	_orig->VSGetShader(ppVertexShader);
}
void    STDMETHODCALLTYPE D3D10Device::PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppConstantBuffers)
{
	_orig->PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void    STDMETHODCALLTYPE D3D10Device::IAGetInputLayout(ID3D10InputLayout **ppInputLayout)
{
	_orig->IAGetInputLayout(ppInputLayout);
}
void    STDMETHODCALLTYPE D3D10Device::IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppVertexBuffers, UINT *pStrides, UINT *pOffsets)
{
	_orig->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}
void    STDMETHODCALLTYPE D3D10Device::IAGetIndexBuffer(ID3D10Buffer **pIndexBuffer, DXGI_FORMAT *Format, UINT *Offset)
{
	_orig->IAGetIndexBuffer(pIndexBuffer, Format, Offset);
}
void    STDMETHODCALLTYPE D3D10Device::GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppConstantBuffers)
{
	_orig->GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void    STDMETHODCALLTYPE D3D10Device::GSGetShader(ID3D10GeometryShader **ppGeometryShader)
{
	_orig->GSGetShader(ppGeometryShader);
}
void    STDMETHODCALLTYPE D3D10Device::IAGetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY *pTopology)
{
	_orig->IAGetPrimitiveTopology(pTopology);
}
void    STDMETHODCALLTYPE D3D10Device::VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView **ppShaderResourceViews)
{
	_orig->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void    STDMETHODCALLTYPE D3D10Device::VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState **ppSamplers)
{
	_orig->VSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void    STDMETHODCALLTYPE D3D10Device::GetPredication(ID3D10Predicate **ppPredicate, BOOL *pPredicateValue)
{
	_orig->GetPredication(ppPredicate, pPredicateValue);
}
void    STDMETHODCALLTYPE D3D10Device::GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView **ppShaderResourceViews)
{
	_orig->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void    STDMETHODCALLTYPE D3D10Device::GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState **ppSamplers)
{
	_orig->GSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void    STDMETHODCALLTYPE D3D10Device::OMGetRenderTargets(UINT NumViews, ID3D10RenderTargetView **ppRenderTargetViews, ID3D10DepthStencilView **ppDepthStencilView)
{
	_orig->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);
}
void    STDMETHODCALLTYPE D3D10Device::OMGetBlendState(ID3D10BlendState **ppBlendState, FLOAT BlendFactor[4], UINT *pSampleMask)
{
	_orig->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask);
}
void    STDMETHODCALLTYPE D3D10Device::OMGetDepthStencilState(ID3D10DepthStencilState **ppDepthStencilState, UINT *pStencilRef)
{
	_orig->OMGetDepthStencilState(ppDepthStencilState, pStencilRef);
}
void    STDMETHODCALLTYPE D3D10Device::SOGetTargets(UINT NumBuffers, ID3D10Buffer **ppSOTargets, UINT *pOffsets)
{
	_orig->SOGetTargets(NumBuffers, ppSOTargets, pOffsets);
}
void    STDMETHODCALLTYPE D3D10Device::RSGetState(ID3D10RasterizerState **ppRasterizerState)
{
	_orig->RSGetState(ppRasterizerState);
}
void    STDMETHODCALLTYPE D3D10Device::RSGetViewports(UINT *NumViewports, D3D10_VIEWPORT *pViewports)
{
	_orig->RSGetViewports(NumViewports, pViewports);
}
void    STDMETHODCALLTYPE D3D10Device::RSGetScissorRects(UINT *NumRects, D3D10_RECT *pRects)
{
	_orig->RSGetScissorRects(NumRects, pRects);
}
HRESULT STDMETHODCALLTYPE D3D10Device::GetDeviceRemovedReason()
{
	return _orig->GetDeviceRemovedReason();
}
HRESULT STDMETHODCALLTYPE D3D10Device::SetExceptionMode(UINT RaiseFlags)
{
	return _orig->SetExceptionMode(RaiseFlags);
}
UINT    STDMETHODCALLTYPE D3D10Device::GetExceptionMode()
{
	return _orig->GetExceptionMode();
}
HRESULT STDMETHODCALLTYPE D3D10Device::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D10Device::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D10Device::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
	return _orig->SetPrivateDataInterface(guid, pData);
}
void    STDMETHODCALLTYPE D3D10Device::ClearState()
{
	_orig->ClearState();
	// TODO: Call events with cleared state
}
void    STDMETHODCALLTYPE D3D10Device::Flush()
{
	_orig->Flush();
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateBuffer(const D3D10_BUFFER_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Buffer **ppBuffer)
{
	if (pDesc == nullptr)
		return E_INVALIDARG;

	D3D10_BUFFER_DESC new_desc = *pDesc;

	static_assert(sizeof(*pInitialData) == sizeof(reshade::api::subresource_data));

	HRESULT hr = E_FAIL;
	reshade::invoke_addon_event<reshade::addon_event::create_resource>(
		[this, &hr, &new_desc, ppBuffer](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
			if (desc.type != reshade::api::resource_type::buffer)
				return false;
			reshade::d3d10::convert_resource_desc(desc, new_desc);

			hr = _orig->CreateBuffer(&new_desc, reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data), ppBuffer);
			if (SUCCEEDED(hr))
			{
				if (ppBuffer != nullptr) // This can happen when application only wants to validate input parameters
					_resources.register_object(*ppBuffer);
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D10Device::CreateBuffer" << " failed with error code " << hr << '.';
#if RESHADE_VERBOSE_LOG
				LOG(DEBUG) << "> Dumping description:";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
				LOG(DEBUG) << "  | ByteWidth                               | " << std::setw(39) << new_desc.ByteWidth << " |";
				LOG(DEBUG) << "  | Usage                                   | " << std::setw(39) << new_desc.Usage << " |";
				LOG(DEBUG) << "  | BindFlags                               | " << std::setw(39) << std::hex << new_desc.BindFlags << std::dec << " |";
				LOG(DEBUG) << "  | CPUAccessFlags                          | " << std::setw(39) << std::hex << new_desc.CPUAccessFlags << std::dec << " |";
				LOG(DEBUG) << "  | MiscFlags                               | " << std::setw(39) << std::hex << new_desc.MiscFlags << std::dec << " |";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
#endif
				return false;
			}
		}, this, reshade::d3d10::convert_resource_desc(new_desc), reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::undefined);
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture1D(const D3D10_TEXTURE1D_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Texture1D **ppTexture1D)
{
	if (pDesc == nullptr)
		return E_INVALIDARG;

	D3D10_TEXTURE1D_DESC new_desc = *pDesc;

	HRESULT hr = E_FAIL;
	reshade::invoke_addon_event<reshade::addon_event::create_resource>(
		[this, &hr, &new_desc, ppTexture1D](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
			if (desc.type != reshade::api::resource_type::texture_1d)
				return false;
			reshade::d3d10::convert_resource_desc(desc, new_desc);

			hr = _orig->CreateTexture1D(&new_desc, reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data), ppTexture1D);
			if (SUCCEEDED(hr))
			{
				if (ppTexture1D != nullptr) // This can happen when application only wants to validate input parameters
					_resources.register_object(*ppTexture1D);
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D10Device::CreateTexture1D" << " failed with error code " << hr << '.';
#if RESHADE_VERBOSE_LOG
				LOG(DEBUG) << "> Dumping description:";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
				LOG(DEBUG) << "  | Width                                   | " << std::setw(39) << new_desc.Width << " |";
				LOG(DEBUG) << "  | MipLevels                               | " << std::setw(39) << new_desc.MipLevels << " |";
				LOG(DEBUG) << "  | ArraySize                               | " << std::setw(39) << new_desc.ArraySize << " |";
				LOG(DEBUG) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
				LOG(DEBUG) << "  | Usage                                   | " << std::setw(39) << new_desc.Usage << " |";
				LOG(DEBUG) << "  | BindFlags                               | " << std::setw(39) << std::hex << new_desc.BindFlags << std::dec << " |";
				LOG(DEBUG) << "  | CPUAccessFlags                          | " << std::setw(39) << std::hex << new_desc.CPUAccessFlags << std::dec << " |";
				LOG(DEBUG) << "  | MiscFlags                               | " << std::setw(39) << std::hex << new_desc.MiscFlags << std::dec << " |";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
#endif
				return false;
			}
		}, this, reshade::d3d10::convert_resource_desc(new_desc), reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::undefined);
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture2D(const D3D10_TEXTURE2D_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Texture2D **ppTexture2D)
{
	if (pDesc == nullptr)
		return E_INVALIDARG;

	D3D10_TEXTURE2D_DESC new_desc = *pDesc;

	HRESULT hr = E_FAIL;
	reshade::invoke_addon_event<reshade::addon_event::create_resource>(
		[this, &hr, &new_desc, ppTexture2D](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
			if (desc.type != reshade::api::resource_type::texture_2d)
				return false;
			reshade::d3d10::convert_resource_desc(desc, new_desc);

			hr = _orig->CreateTexture2D(&new_desc, reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data), ppTexture2D);
			if (SUCCEEDED(hr))
			{
				if (ppTexture2D != nullptr) // This can happen when application only wants to validate input parameters
					_resources.register_object(*ppTexture2D);
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D10Device::CreateTexture2D" << " failed with error code " << hr << '.';
#if RESHADE_VERBOSE_LOG
				LOG(DEBUG) << "> Dumping description:";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
				LOG(DEBUG) << "  | Width                                   | " << std::setw(39) << new_desc.Width << " |";
				LOG(DEBUG) << "  | Height                                  | " << std::setw(39) << new_desc.Height << " |";
				LOG(DEBUG) << "  | MipLevels                               | " << std::setw(39) << new_desc.MipLevels << " |";
				LOG(DEBUG) << "  | ArraySize                               | " << std::setw(39) << new_desc.ArraySize << " |";
				LOG(DEBUG) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
				LOG(DEBUG) << "  | SampleCount                             | " << std::setw(39) << new_desc.SampleDesc.Count << " |";
				LOG(DEBUG) << "  | SampleQuality                           | " << std::setw(39) << new_desc.SampleDesc.Quality << " |";
				LOG(DEBUG) << "  | Usage                                   | " << std::setw(39) << new_desc.Usage << " |";
				LOG(DEBUG) << "  | BindFlags                               | " << std::setw(39) << std::hex << new_desc.BindFlags << std::dec << " |";
				LOG(DEBUG) << "  | CPUAccessFlags                          | " << std::setw(39) << std::hex << new_desc.CPUAccessFlags << std::dec << " |";
				LOG(DEBUG) << "  | MiscFlags                               | " << std::setw(39) << std::hex << new_desc.MiscFlags << std::dec << " |";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
#endif
				return false;
			}
		}, this, reshade::d3d10::convert_resource_desc(new_desc), reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::undefined);
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture3D(const D3D10_TEXTURE3D_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Texture3D **ppTexture3D)
{
	if (pDesc == nullptr)
		return E_INVALIDARG;

	D3D10_TEXTURE3D_DESC new_desc = *pDesc;

	HRESULT hr = E_FAIL;
	reshade::invoke_addon_event<reshade::addon_event::create_resource>(
		[this, &hr, &new_desc, ppTexture3D](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
			if (desc.type != reshade::api::resource_type::texture_3d)
				return false;
			reshade::d3d10::convert_resource_desc(desc, new_desc);

			hr = _orig->CreateTexture3D(&new_desc, reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data), ppTexture3D);
			if (SUCCEEDED(hr))
			{
				if (ppTexture3D != nullptr) // This can happen when application only wants to validate input parameters
					_resources.register_object(*ppTexture3D);
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D10Device::CreateTexture3D" << " failed with error code " << hr << '.';
#if RESHADE_VERBOSE_LOG
				LOG(DEBUG) << "> Dumping description:";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
				LOG(DEBUG) << "  | Width                                   | " << std::setw(39) << new_desc.Width << " |";
				LOG(DEBUG) << "  | Height                                  | " << std::setw(39) << new_desc.Height << " |";
				LOG(DEBUG) << "  | Depth                                   | " << std::setw(39) << new_desc.Depth << " |";
				LOG(DEBUG) << "  | MipLevels                               | " << std::setw(39) << new_desc.MipLevels << " |";
				LOG(DEBUG) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
				LOG(DEBUG) << "  | Usage                                   | " << std::setw(39) << new_desc.Usage << " |";
				LOG(DEBUG) << "  | BindFlags                               | " << std::setw(39) << std::hex << new_desc.BindFlags << std::dec << " |";
				LOG(DEBUG) << "  | CPUAccessFlags                          | " << std::setw(39) << std::hex << new_desc.CPUAccessFlags << std::dec << " |";
				LOG(DEBUG) << "  | MiscFlags                               | " << std::setw(39) << std::hex << new_desc.MiscFlags << std::dec << " |";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
#endif
				return false;
			}
		}, this, reshade::d3d10::convert_resource_desc(new_desc), reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::undefined);
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateShaderResourceView(ID3D10Resource *pResource, const D3D10_SHADER_RESOURCE_VIEW_DESC *pDesc, ID3D10ShaderResourceView **ppSRView)
{
	if (pResource == nullptr)
		return E_INVALIDARG;

	D3D10_SHADER_RESOURCE_VIEW_DESC new_desc =
		pDesc != nullptr ? *pDesc : D3D10_SHADER_RESOURCE_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D10_SRV_DIMENSION_UNKNOWN };

	HRESULT hr = E_FAIL;
	reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
		[this, &hr, &new_desc, ppSRView](reshade::api::device *, reshade::api::resource resource, reshade::api::resource_usage usage_type, const reshade::api::resource_view_desc &desc) {
			if (usage_type != reshade::api::resource_usage::shader_resource)
				return false;
			reshade::d3d10::convert_resource_view_desc(desc, new_desc);

			hr = _orig->CreateShaderResourceView(reinterpret_cast<ID3D10Resource *>(resource.handle), new_desc.ViewDimension != D3D10_SRV_DIMENSION_UNKNOWN ? &new_desc : nullptr, ppSRView);
			if (SUCCEEDED(hr))
			{
				if (ppSRView != nullptr) // This can happen when application only wants to validate input parameters
					_views.register_object(*ppSRView);
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D10Device::CreateShaderResourceView" << " failed with error code " << hr << '.';
				return false;
			}
		}, this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::shader_resource, reshade::d3d10::convert_resource_view_desc(new_desc));
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateRenderTargetView(ID3D10Resource *pResource, const D3D10_RENDER_TARGET_VIEW_DESC *pDesc, ID3D10RenderTargetView **ppRTView)
{
	if (pResource == nullptr)
		return E_INVALIDARG;

	D3D10_RENDER_TARGET_VIEW_DESC new_desc =
		pDesc != nullptr ? *pDesc : D3D10_RENDER_TARGET_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D10_RTV_DIMENSION_UNKNOWN };

	HRESULT hr = E_FAIL;
	reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
		[this, &hr, &new_desc, ppRTView](reshade::api::device *, reshade::api::resource resource, reshade::api::resource_usage usage_type, const reshade::api::resource_view_desc &desc) {
			if (usage_type == reshade::api::resource_usage::render_target)
				return false;
			reshade::d3d10::convert_resource_view_desc(desc, new_desc);

			hr = _orig->CreateRenderTargetView(reinterpret_cast<ID3D10Resource *>(resource.handle), new_desc.ViewDimension != D3D10_RTV_DIMENSION_UNKNOWN ? &new_desc : nullptr, ppRTView);
			if (SUCCEEDED(hr))
			{
				if (ppRTView != nullptr) // This can happen when application only wants to validate input parameters
					_views.register_object(*ppRTView);
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D10Device::CreateRenderTargetView" << " failed with error code " << hr << '.';
				return false;
			}
		}, this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::render_target, reshade::d3d10::convert_resource_view_desc(new_desc));
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateDepthStencilView(ID3D10Resource *pResource, const D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D10DepthStencilView **ppDepthStencilView)
{
	if (pResource == nullptr)
		return E_INVALIDARG;

	D3D10_DEPTH_STENCIL_VIEW_DESC new_desc =
		pDesc != nullptr ? *pDesc : D3D10_DEPTH_STENCIL_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D10_DSV_DIMENSION_UNKNOWN };

	HRESULT hr = E_FAIL;
	reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
		[this, &hr, &new_desc, ppDepthStencilView](reshade::api::device *, reshade::api::resource resource, reshade::api::resource_usage usage_type, const reshade::api::resource_view_desc &desc) {
			if (usage_type != reshade::api::resource_usage::depth_stencil)
				return false;
			reshade::d3d10::convert_resource_view_desc(desc, new_desc);

			hr = _orig->CreateDepthStencilView(reinterpret_cast<ID3D10Resource *>(resource.handle), new_desc.ViewDimension != D3D10_DSV_DIMENSION_UNKNOWN ? &new_desc : nullptr, ppDepthStencilView);
			if (SUCCEEDED(hr))
			{
				if (ppDepthStencilView != nullptr) // This can happen when application only wants to validate input parameters
					_views.register_object(*ppDepthStencilView);
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D10Device::CreateDepthStencilView" << " failed with error code " << hr << '.';
				return false;
			}
		}, this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::depth_stencil, reshade::d3d10::convert_resource_view_desc(new_desc));
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateInputLayout(const D3D10_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements, const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D10InputLayout **ppInputLayout)
{
	return _orig->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateVertexShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10VertexShader **ppVertexShader)
{
	HRESULT hr = E_FAIL;
	reshade::invoke_addon_event<reshade::addon_event::create_shader_module>(
		[this, &hr, ppVertexShader](reshade::api::device *, reshade::api::shader_stage type, reshade::api::shader_format format, const char *, const void *code, size_t code_size) {
			if (type != reshade::api::shader_stage::vertex || format != reshade::api::shader_format::dxbc)
				return false;
			hr = _orig->CreateVertexShader(code, code_size, ppVertexShader);
			if (SUCCEEDED(hr))
			{
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D10Device::CreateVertexShader" << " failed with error code " << hr << '.';
				return false;
			}
		}, this, reshade::api::shader_stage::vertex, reshade::api::shader_format::dxbc, nullptr, pShaderBytecode, BytecodeLength);
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateGeometryShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10GeometryShader **ppGeometryShader)
{
	HRESULT hr = E_FAIL;
	reshade::invoke_addon_event<reshade::addon_event::create_shader_module>(
		[this, &hr, ppGeometryShader](reshade::api::device *, reshade::api::shader_stage type, reshade::api::shader_format format, const char *, const void *code, size_t code_size) {
			if (type != reshade::api::shader_stage::geometry || format != reshade::api::shader_format::dxbc)
				return false;
			hr = _orig->CreateGeometryShader(code, code_size, ppGeometryShader);
			if (SUCCEEDED(hr))
			{
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D10Device::CreateGeometryShader" << " failed with error code " << hr << '.';
				return false;
			}
		}, this, reshade::api::shader_stage::geometry, reshade::api::shader_format::dxbc, nullptr, pShaderBytecode, BytecodeLength);
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateGeometryShaderWithStreamOutput(const void *pShaderBytecode, SIZE_T BytecodeLength, const D3D10_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, UINT OutputStreamStride, ID3D10GeometryShader **ppGeometryShader)
{
	HRESULT hr = E_FAIL;
	reshade::invoke_addon_event<reshade::addon_event::create_shader_module>(
		[this, &hr, pSODeclaration, NumEntries, OutputStreamStride, ppGeometryShader](reshade::api::device *, reshade::api::shader_stage type, reshade::api::shader_format format, const char *, const void *code, size_t code_size) {
			if (type != reshade::api::shader_stage::geometry || format != reshade::api::shader_format::dxbc)
				return false;
			hr = _orig->CreateGeometryShaderWithStreamOutput(code, code_size, pSODeclaration, NumEntries, OutputStreamStride, ppGeometryShader);
			if (SUCCEEDED(hr))
			{
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D10Device::CreateGeometryShaderWithStreamOutput" << " failed with error code " << hr << '.';
				return false;
			}
		}, this, reshade::api::shader_stage::geometry, reshade::api::shader_format::dxbc, nullptr, pShaderBytecode, BytecodeLength);
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreatePixelShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10PixelShader **ppPixelShader)
{
	HRESULT hr = E_FAIL;
	reshade::invoke_addon_event<reshade::addon_event::create_shader_module>(
		[this, &hr, ppPixelShader](reshade::api::device *, reshade::api::shader_stage type, reshade::api::shader_format format, const char *, const void *code, size_t code_size) {
			if (type != reshade::api::shader_stage::pixel || format != reshade::api::shader_format::dxbc)
				return false;
			hr = _orig->CreatePixelShader(code, code_size, ppPixelShader);
			if (SUCCEEDED(hr))
			{
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D10Device::CreatePixelShader" << " failed with error code " << hr << '.';
				return false;
			}
		}, this, reshade::api::shader_stage::pixel, reshade::api::shader_format::dxbc, nullptr, pShaderBytecode, BytecodeLength);
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateBlendState(const D3D10_BLEND_DESC *pBlendStateDesc, ID3D10BlendState **ppBlendState)
{
	return _orig->CreateBlendState(pBlendStateDesc, ppBlendState);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateDepthStencilState(const D3D10_DEPTH_STENCIL_DESC *pDepthStencilDesc, ID3D10DepthStencilState **ppDepthStencilState)
{
	return _orig->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateRasterizerState(const D3D10_RASTERIZER_DESC *pRasterizerDesc, ID3D10RasterizerState **ppRasterizerState)
{
	return _orig->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateSamplerState(const D3D10_SAMPLER_DESC *pSamplerDesc, ID3D10SamplerState **ppSamplerState)
{
	return _orig->CreateSamplerState(pSamplerDesc, ppSamplerState);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateQuery(const D3D10_QUERY_DESC *pQueryDesc, ID3D10Query **ppQuery)
{
	return _orig->CreateQuery(pQueryDesc, ppQuery);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreatePredicate(const D3D10_QUERY_DESC *pPredicateDesc, ID3D10Predicate **ppPredicate)
{
	return _orig->CreatePredicate(pPredicateDesc, ppPredicate);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateCounter(const D3D10_COUNTER_DESC *pCounterDesc, ID3D10Counter **ppCounter)
{
	return _orig->CreateCounter(pCounterDesc, ppCounter);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CheckFormatSupport(DXGI_FORMAT Format, UINT *pFormatSupport)
{
	return _orig->CheckFormatSupport(Format, pFormatSupport);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount, UINT *pNumQualityLevels)
{
	return _orig->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
}
void    STDMETHODCALLTYPE D3D10Device::CheckCounterInfo(D3D10_COUNTER_INFO *pCounterInfo)
{
	_orig->CheckCounterInfo(pCounterInfo);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CheckCounter(const D3D10_COUNTER_DESC *pDesc, D3D10_COUNTER_TYPE *pType, UINT *pActiveCounters, LPSTR szName, UINT *pNameLength, LPSTR szUnits, UINT *pUnitsLength, LPSTR szDescription, UINT *pDescriptionLength)
{
	return _orig->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength, szDescription, pDescriptionLength);
}
UINT    STDMETHODCALLTYPE D3D10Device::GetCreationFlags()
{
	return _orig->GetCreationFlags();
}
HRESULT STDMETHODCALLTYPE D3D10Device::OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface, void **ppResource)
{
	return _orig->OpenSharedResource(hResource, ReturnedInterface, ppResource);
}
void    STDMETHODCALLTYPE D3D10Device::SetTextFilterSize(UINT Width, UINT Height)
{
	_orig->SetTextFilterSize(Width, Height);
}
void    STDMETHODCALLTYPE D3D10Device::GetTextFilterSize(UINT *pWidth, UINT *pHeight)
{
	_orig->GetTextFilterSize(pWidth, pHeight);
}

HRESULT STDMETHODCALLTYPE D3D10Device::CreateShaderResourceView1(ID3D10Resource *pResource, const D3D10_SHADER_RESOURCE_VIEW_DESC1 *pDesc, ID3D10ShaderResourceView1 **ppSRView)
{
	if (pResource == nullptr)
		return E_INVALIDARG;

	D3D10_SHADER_RESOURCE_VIEW_DESC1 new_desc =
		pDesc != nullptr ? *pDesc : D3D10_SHADER_RESOURCE_VIEW_DESC1 { DXGI_FORMAT_UNKNOWN, D3D10_1_SRV_DIMENSION_UNKNOWN };

	HRESULT hr = E_FAIL;
	reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
		[this, &hr, &new_desc, ppSRView](reshade::api::device *, reshade::api::resource resource, reshade::api::resource_usage usage_type, const reshade::api::resource_view_desc &desc) {
			if (usage_type != reshade::api::resource_usage::shader_resource)
				return false;
			reshade::d3d10::convert_resource_view_desc(desc, new_desc);

			hr = _orig->CreateShaderResourceView1(reinterpret_cast<ID3D10Resource *>(resource.handle), new_desc.ViewDimension != D3D10_1_SRV_DIMENSION_UNKNOWN ? &new_desc : nullptr, ppSRView);
			if (SUCCEEDED(hr))
			{
				if (ppSRView != nullptr) // This can happen when application only wants to validate input parameters
					_views.register_object(*ppSRView);
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D10Device1::CreateShaderResourceView1" << " failed with error code " << hr << '.';
				return false;
			}
		}, this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::shader_resource, reshade::d3d10::convert_resource_view_desc(new_desc));
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateBlendState1(const D3D10_BLEND_DESC1 *pBlendStateDesc, ID3D10BlendState1 **ppBlendState)
{
	return _orig->CreateBlendState1(pBlendStateDesc, ppBlendState);
}
D3D10_FEATURE_LEVEL1 STDMETHODCALLTYPE D3D10Device::GetFeatureLevel()
{
	return _orig->GetFeatureLevel();
}
