/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d10_device.hpp"
#include "d3d10_resource.hpp"
#include "d3d10_impl_type_convert.hpp"
#include "dll_log.hpp" // Include late to get 'hr_to_string' helper function
#include "com_utils.hpp"
#include "hook_manager.hpp"
#include "addon_manager.hpp"

using reshade::d3d10::to_handle;

D3D10Device::D3D10Device(IDXGIAdapter *adapter, IDXGIDevice1 *original_dxgi_device, ID3D10Device1 *original) :
	DXGIDevice(adapter, original_dxgi_device), device_impl(original)
{
	assert(_orig != nullptr);

	// Add proxy object to the private data of the device, so that it can be retrieved again when only the original device is available
	D3D10Device *const device_proxy = this;
	_orig->SetPrivateData(__uuidof(D3D10Device), sizeof(device_proxy), &device_proxy);

#if RESHADE_ADDON
	reshade::load_addons();

	reshade::invoke_addon_event<reshade::addon_event::init_device>(this);

	const reshade::api::pipeline_layout_param global_pipeline_layout_params[3] = {
		reshade::api::descriptor_range { 0, 0, 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT, reshade::api::shader_stage::all, 1, reshade::api::descriptor_type::sampler },
		reshade::api::descriptor_range { 0, 0, 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, reshade::api::shader_stage::all, 1, reshade::api::descriptor_type::shader_resource_view },
		reshade::api::descriptor_range { 0, 0, 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, reshade::api::shader_stage::all, 1, reshade::api::descriptor_type::constant_buffer },
	};
	device_impl::create_pipeline_layout(static_cast<uint32_t>(std::size(global_pipeline_layout_params)), global_pipeline_layout_params, &_global_pipeline_layout);
	reshade::invoke_addon_event<reshade::addon_event::init_pipeline_layout>(this, static_cast<uint32_t>(std::size(global_pipeline_layout_params)), global_pipeline_layout_params, _global_pipeline_layout);

	reshade::invoke_addon_event<reshade::addon_event::init_command_list>(this);
	reshade::invoke_addon_event<reshade::addon_event::init_command_queue>(this);
#endif
}
D3D10Device::~D3D10Device()
{
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_command_queue>(this);
	reshade::invoke_addon_event<reshade::addon_event::destroy_command_list>(this);

	// Ensure all objects referenced by the device are destroyed before the 'destroy_device' event is called
	_orig->ClearState();
	_orig->Flush();

	reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline_layout>(this, _global_pipeline_layout);
	device_impl::destroy_pipeline_layout(_global_pipeline_layout);

	reshade::invoke_addon_event<reshade::addon_event::destroy_device>(this);

	reshade::unload_addons();
#endif

	// Remove pointer to this proxy object from the private data of the device (in case the device unexpectedly survives)
	_orig->SetPrivateData(__uuidof(D3D10Device), 0, nullptr);
}

bool D3D10Device::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(ID3D10Device) ||
		riid == __uuidof(ID3D10Device1))
		return true;

	return false;
}

HRESULT STDMETHODCALLTYPE D3D10Device::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == __uuidof(this))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	if (check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = static_cast<ID3D10Device *>(this);
		return S_OK;
	}

	// Note: Objects must have an identity, so use DXGIDevice for IID_IUnknown
	// See https://docs.microsoft.com/windows/desktop/com/rules-for-implementing-queryinterface
	if (DXGIDevice::check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = static_cast<IDXGIDevice1 *>(this);
		return S_OK;
	}

	// Interface ID to query the original object from a proxy object
	constexpr GUID IID_UnwrappedObject = { 0x7f2c9a11, 0x3b4e, 0x4d6a, { 0x81, 0x2f, 0x5e, 0x9c, 0xd3, 0x7a, 0x1b, 0x42 } }; // {7F2C9A11-3B4E-4D6A-812F-5E9CD37A1B42}
	if (riid == IID_UnwrappedObject)
	{
		_orig->AddRef();
		*ppvObj = _orig;
		return S_OK;
	}

	// Unimplemented interfaces:
	//   ID3D10Debug       {9B7E4E01-342C-4106-A19F-4F2704F689F0}
	//   ID3D10InfoQueue   {1B940B17-2642-4D1F-AB1F-B99BAD0C395F}
	//   ID3D10Multithread {9B7E4E00-342C-4106-A19F-4F2704F689F0}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D10Device::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE D3D10Device::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
	{
		_orig->Release();
		return ref;
	}

	const auto orig = _orig;
#if RESHADE_VERBOSE_LOG
	reshade::log::message(
		reshade::log::level::debug,
		"Destroying ID3D10Device1 object %p (%p) and IDXGIDevice%hu object %p (%p).",
		static_cast<ID3D10Device *>(this), orig, DXGIDevice::_interface_version, static_cast<IDXGIDevice1 *>(this), DXGIDevice::_orig);
#endif
	this->~D3D10Device();

	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		reshade::log::message(reshade::log::level::warning, "Reference count for ID3D10Device1 object %p (%p) is inconsistent (%lu).", static_cast<ID3D10Device *>(this), orig, ref_orig);
	else
		operator delete(this, sizeof(D3D10Device));
	return 0;
}

void    STDMETHODCALLTYPE D3D10Device::VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppConstantBuffers)
{
	_orig->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
#if RESHADE_ADDON >= 2
	invoke_bind_constant_buffers_event(reshade::api::shader_stage::vertex, StartSlot, NumBuffers, ppConstantBuffers);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
#if RESHADE_ADDON >= 2
	invoke_bind_shader_resource_views_event(reshade::api::shader_stage::pixel, StartSlot, NumViews, ppShaderResourceViews);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::PSSetShader(ID3D10PixelShader *pPixelShader)
{
	_orig->PSSetShader(pPixelShader);
#if RESHADE_ADDON >= 2
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::pixel_shader, to_handle(pPixelShader));
#endif
}
void    STDMETHODCALLTYPE D3D10Device::PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState *const *ppSamplers)
{
	_orig->PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
#if RESHADE_ADDON >= 2
	invoke_bind_samplers_event(reshade::api::shader_stage::pixel, StartSlot, NumSamplers, ppSamplers);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::VSSetShader(ID3D10VertexShader *pVertexShader)
{
	_orig->VSSetShader(pVertexShader);
#if RESHADE_ADDON >= 2
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::vertex_shader, to_handle(pVertexShader));
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
#if RESHADE_ADDON >= 2
	invoke_bind_constant_buffers_event(reshade::api::shader_stage::pixel, StartSlot, NumBuffers, ppConstantBuffers);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::IASetInputLayout(ID3D10InputLayout *pInputLayout)
{
	_orig->IASetInputLayout(pInputLayout);
#if RESHADE_ADDON >= 2
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::input_assembler, to_handle(pInputLayout));
#endif
}
void    STDMETHODCALLTYPE D3D10Device::IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets)
{
	_orig->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);

#if RESHADE_ADDON >= 2
	if (!reshade::has_addon_event<reshade::addon_event::bind_vertex_buffers>())
		return;

#ifndef _WIN64
	temp_mem<reshade::api::resource, D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> buffer_handles_mem(NumBuffers);
	for (UINT i = 0; i < NumBuffers; ++i)
		buffer_handles_mem[i] = to_handle(ppVertexBuffers[i]);
	const auto buffer_handles = buffer_handles_mem.p;
#else
	static_assert(sizeof(*ppVertexBuffers) == sizeof(reshade::api::resource));
	const auto buffer_handles = reinterpret_cast<const reshade::api::resource *>(ppVertexBuffers);
#endif

	temp_mem<uint64_t, D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> offsets_64(NumBuffers);
	for (UINT i = 0; i < NumBuffers; ++i)
		offsets_64[i] = pOffsets[i];

	reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(this, StartSlot, NumBuffers, buffer_handles, offsets_64.p, pStrides);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::IASetIndexBuffer(ID3D10Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset)
{
	_orig->IASetIndexBuffer(pIndexBuffer, Format, Offset);
#if RESHADE_ADDON >= 2
	reshade::invoke_addon_event<reshade::addon_event::bind_index_buffer>(this, to_handle(pIndexBuffer), Offset, Format == DXGI_FORMAT_R16_UINT ? 2 : 4);
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
#if RESHADE_ADDON >= 2
	invoke_bind_constant_buffers_event(reshade::api::shader_stage::geometry, StartSlot, NumBuffers, ppConstantBuffers);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::GSSetShader(ID3D10GeometryShader *pShader)
{
	_orig->GSSetShader(pShader);
#if RESHADE_ADDON >= 2
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::geometry_shader | reshade::api::pipeline_stage::stream_output, to_handle(pShader));
#endif
}
void    STDMETHODCALLTYPE D3D10Device::IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY Topology)
{
	_orig->IASetPrimitiveTopology(Topology);

#if RESHADE_ADDON >= 2
	const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::primitive_topology };
	const uint32_t values[1] = { static_cast<uint32_t>(reshade::d3d10::convert_primitive_topology(Topology)) };

	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, static_cast<uint32_t>(std::size(states)), states, values);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
#if RESHADE_ADDON >= 2
	invoke_bind_shader_resource_views_event(reshade::api::shader_stage::vertex, StartSlot, NumViews, ppShaderResourceViews);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState *const *ppSamplers)
{
	_orig->VSSetSamplers(StartSlot, NumSamplers, ppSamplers);
#if RESHADE_ADDON >= 2
	invoke_bind_samplers_event(reshade::api::shader_stage::vertex, StartSlot, NumSamplers, ppSamplers);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::SetPredication(ID3D10Predicate *pPredicate, BOOL PredicateValue)
{
	_orig->SetPredication(pPredicate, PredicateValue);
}
void    STDMETHODCALLTYPE D3D10Device::GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
#if RESHADE_ADDON >= 2
	invoke_bind_shader_resource_views_event(reshade::api::shader_stage::geometry, StartSlot, NumViews, ppShaderResourceViews);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState *const *ppSamplers)
{
	_orig->GSSetSamplers(StartSlot, NumSamplers, ppSamplers);
#if RESHADE_ADDON >= 2
	invoke_bind_samplers_event(reshade::api::shader_stage::geometry, StartSlot, NumSamplers, ppSamplers);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::OMSetRenderTargets(UINT NumViews, ID3D10RenderTargetView *const *ppRenderTargetViews, ID3D10DepthStencilView *pDepthStencilView)
{
	_orig->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);

#if RESHADE_ADDON
	if (!reshade::has_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>())
		return;

#ifndef _WIN64
	temp_mem<reshade::api::resource_view, D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT> rtvs_mem(NumViews);
	for (UINT i = 0; i < NumViews; ++i)
		rtvs_mem[i] = to_handle(ppRenderTargetViews[i]);
	const auto rtvs = rtvs_mem.p;
#else
	static_assert(sizeof(*ppRenderTargetViews) == sizeof(reshade::api::resource_view));
	const auto rtvs = reinterpret_cast<const reshade::api::resource_view *>(ppRenderTargetViews);
#endif

	reshade::invoke_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(this, NumViews, rtvs, to_handle(pDepthStencilView));
#endif
}
void    STDMETHODCALLTYPE D3D10Device::OMSetBlendState(ID3D10BlendState *pBlendState, const FLOAT BlendFactor[4], UINT SampleMask)
{
	_orig->OMSetBlendState(pBlendState, BlendFactor, SampleMask);

#if RESHADE_ADDON >= 2
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::output_merger, to_handle(pBlendState));

	const reshade::api::dynamic_state states[2] = { reshade::api::dynamic_state::blend_constant, reshade::api::dynamic_state::sample_mask };
	const uint32_t values[2] = {
		(BlendFactor == nullptr) ? 0xFFFFFFFF : // Default blend factor is { 1, 1, 1, 1 }, see D3D10_DEFAULT_BLEND_FACTOR_*
			((static_cast<uint32_t>(BlendFactor[0] * 255.f) & 0xFF)) |
			((static_cast<uint32_t>(BlendFactor[1] * 255.f) & 0xFF) << 8) |
			((static_cast<uint32_t>(BlendFactor[2] * 255.f) & 0xFF) << 16) |
			((static_cast<uint32_t>(BlendFactor[3] * 255.f) & 0xFF) << 24), SampleMask };

	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, static_cast<uint32_t>(std::size(states)), states, values);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::OMSetDepthStencilState(ID3D10DepthStencilState *pDepthStencilState, UINT StencilRef)
{
	_orig->OMSetDepthStencilState(pDepthStencilState, StencilRef);

#if RESHADE_ADDON >= 2
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::depth_stencil, to_handle(pDepthStencilState));

	const reshade::api::dynamic_state states[2] = { reshade::api::dynamic_state::front_stencil_reference_value, reshade::api::dynamic_state::back_stencil_reference_value };
	const uint32_t values[2] = { StencilRef, StencilRef };

	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, static_cast<uint32_t>(std::size(states)), states, values);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::SOSetTargets(UINT NumBuffers, ID3D10Buffer *const *ppSOTargets, const UINT *pOffsets)
{
	_orig->SOSetTargets(NumBuffers, ppSOTargets, pOffsets);

#if RESHADE_ADDON >= 2
	if (!reshade::has_addon_event<reshade::addon_event::bind_stream_output_buffers>())
		return;

#ifndef _WIN64
	temp_mem<reshade::api::resource, D3D10_SO_BUFFER_SLOT_COUNT> buffer_handles_mem(NumBuffers);
	for (UINT i = 0; i < NumBuffers; ++i)
		buffer_handles_mem[i] = to_handle(ppSOTargets[i]);
	const auto buffer_handles = buffer_handles_mem.p;
#else
	static_assert(sizeof(*ppSOTargets) == sizeof(reshade::api::resource));
	const auto buffer_handles = reinterpret_cast<const reshade::api::resource *>(ppSOTargets);
#endif

	temp_mem<uint64_t, D3D10_SO_BUFFER_SLOT_COUNT> offsets_64(NumBuffers);
	for (UINT i = 0; i < NumBuffers; ++i)
		offsets_64[i] = pOffsets[i];

	reshade::invoke_addon_event<reshade::addon_event::bind_stream_output_buffers>(this, 0, NumBuffers, buffer_handles, offsets_64.p, nullptr, nullptr, nullptr);
#endif
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

#if RESHADE_ADDON >= 2
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::rasterizer, to_handle(pRasterizerState));
#endif
}
void    STDMETHODCALLTYPE D3D10Device::RSSetViewports(UINT NumViewports, const D3D10_VIEWPORT *pViewports)
{
	_orig->RSSetViewports(NumViewports, pViewports);

#if RESHADE_ADDON
	if (!reshade::has_addon_event<reshade::addon_event::bind_viewports>())
		return;

	temp_mem<reshade::api::viewport, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewport_data(NumViewports);
	for (UINT i = 0; i < NumViewports; ++i)
	{
		viewport_data[i].x = static_cast<float>(pViewports[i].TopLeftX);
		viewport_data[i].y = static_cast<float>(pViewports[i].TopLeftY);
		viewport_data[i].width = static_cast<float>(pViewports[i].Width);
		viewport_data[i].height = static_cast<float>(pViewports[i].Height);
		viewport_data[i].min_depth = pViewports[i].MinDepth;
		viewport_data[i].max_depth = pViewports[i].MaxDepth;
	}

	reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(this, 0, NumViewports, viewport_data.p);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::RSSetScissorRects(UINT NumRects, const D3D10_RECT *pRects)
{
	_orig->RSSetScissorRects(NumRects, pRects);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(this, 0, NumRects, reinterpret_cast<const reshade::api::rect *>(pRects));
#endif
}
void    STDMETHODCALLTYPE D3D10Device::CopySubresourceRegion(ID3D10Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D10Resource *pSrcResource, UINT SrcSubresource, const D3D10_BOX *pSrcBox)
{
	assert(pDstResource != nullptr && pSrcResource != nullptr);

#if RESHADE_ADDON >= 2
	if (reshade::has_addon_event<reshade::addon_event::copy_buffer_region>() ||
		reshade::has_addon_event<reshade::addon_event::copy_texture_region>())
	{
		D3D10_RESOURCE_DIMENSION type;
		pDstResource->GetType(&type);

		if (type == D3D10_RESOURCE_DIMENSION_BUFFER)
		{
			assert(SrcSubresource == 0 && DstSubresource == 0);

			if (reshade::invoke_addon_event<reshade::addon_event::copy_buffer_region>(
					this,
					to_handle(pSrcResource),
					pSrcBox != nullptr ? pSrcBox->left : 0,
					to_handle(pDstResource),
					DstX,
					pSrcBox != nullptr ? pSrcBox->right - pSrcBox->left : UINT64_MAX))
				return;
		}
		else
		{
			const bool use_dst_box = DstX != 0 || DstY != 0 || DstZ != 0;
			reshade::api::subresource_box dst_box;

			if (use_dst_box)
			{
				dst_box.left = DstX;
				dst_box.top = DstY;
				dst_box.front = DstZ;

				if (pSrcBox != nullptr)
				{
					dst_box.right = dst_box.left + pSrcBox->right - pSrcBox->left;
					dst_box.bottom = dst_box.top + pSrcBox->bottom - pSrcBox->top;
					dst_box.back = dst_box.front + pSrcBox->back - pSrcBox->front;
				}
				else
				{
					dst_box.right = dst_box.left;
					dst_box.bottom = dst_box.top;
					dst_box.back = dst_box.front;

					switch (type)
					{
						case D3D10_RESOURCE_DIMENSION_TEXTURE1D:
						{
							D3D10_TEXTURE1D_DESC desc;
							static_cast<ID3D10Texture1D *>(pSrcResource)->GetDesc(&desc);
							dst_box.right += desc.Width;
							dst_box.bottom += 1;
							dst_box.back += 1;
							break;
						}
						case D3D10_RESOURCE_DIMENSION_TEXTURE2D:
						{
							D3D10_TEXTURE2D_DESC desc;
							static_cast<ID3D10Texture2D *>(pSrcResource)->GetDesc(&desc);
							dst_box.right += desc.Width;
							dst_box.bottom += desc.Height;
							dst_box.back += 1;
							break;
						}
						case D3D10_RESOURCE_DIMENSION_TEXTURE3D:
						{
							D3D10_TEXTURE3D_DESC desc;
							static_cast<ID3D10Texture3D *>(pSrcResource)->GetDesc(&desc);
							dst_box.right += desc.Width;
							dst_box.bottom += desc.Height;
							dst_box.back += desc.Depth;
							break;
						}
					}
				}
			}

			if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(
					this,
					to_handle(pSrcResource),
					SrcSubresource,
					reinterpret_cast<const reshade::api::subresource_box *>(pSrcBox),
					to_handle(pDstResource),
					DstSubresource,
					use_dst_box ? &dst_box : nullptr,
					reshade::api::filter_mode::min_mag_mip_point))
				return;
		}
	}
#endif

	_orig->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
}
void    STDMETHODCALLTYPE D3D10Device::CopyResource(ID3D10Resource *pDstResource, ID3D10Resource *pSrcResource)
{
	assert(pDstResource != nullptr && pSrcResource != nullptr);

#if RESHADE_ADDON >= 2
	if (reshade::invoke_addon_event<reshade::addon_event::copy_resource>(this, to_handle(pSrcResource), to_handle(pDstResource)))
		return;
#endif
	_orig->CopyResource(pDstResource, pSrcResource);
}
void    STDMETHODCALLTYPE D3D10Device::UpdateSubresource(ID3D10Resource *pDstResource, UINT DstSubresource, const D3D10_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
{
	assert(pDstResource != nullptr);

#if RESHADE_ADDON >= 2
	if (reshade::has_addon_event<reshade::addon_event::update_buffer_region>() ||
		reshade::has_addon_event<reshade::addon_event::update_texture_region>())
	{
		D3D10_RESOURCE_DIMENSION type;
		pDstResource->GetType(&type);

		if (type == D3D10_RESOURCE_DIMENSION_BUFFER)
		{
			assert(DstSubresource == 0);

			if (reshade::invoke_addon_event<reshade::addon_event::update_buffer_region>(
					this,
					pSrcData,
					to_handle(pDstResource),
					pDstBox != nullptr ? pDstBox->left : 0,
					pDstBox != nullptr ? pDstBox->right - pDstBox->left : SrcRowPitch))
				return;
		}
		else
		{
			if (reshade::invoke_addon_event<reshade::addon_event::update_texture_region>(
					this,
					reshade::api::subresource_data { const_cast<void *>(pSrcData), SrcRowPitch, SrcDepthPitch },
					to_handle(pDstResource),
					DstSubresource,
					reinterpret_cast<const reshade::api::subresource_box *>(pDstBox)))
				return;
		}
	}
#endif

	_orig->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
}
void    STDMETHODCALLTYPE D3D10Device::ClearRenderTargetView(ID3D10RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4])
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(this, to_handle(pRenderTargetView), ColorRGBA, 0, nullptr))
		return;
#endif
	_orig->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
}
void    STDMETHODCALLTYPE D3D10Device::ClearDepthStencilView(ID3D10DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(this, to_handle(pDepthStencilView), (ClearFlags & D3D10_CLEAR_DEPTH) ? &Depth : nullptr, (ClearFlags & D3D10_CLEAR_STENCIL) ? &Stencil : nullptr, 0, nullptr))
		return;
#endif
	_orig->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
}
void    STDMETHODCALLTYPE D3D10Device::GenerateMips(ID3D10ShaderResourceView *pShaderResourceView)
{
#if RESHADE_ADDON >= 2
	if (reshade::invoke_addon_event<reshade::addon_event::generate_mipmaps>(this, to_handle(pShaderResourceView)))
		return;
#endif
	_orig->GenerateMips(pShaderResourceView);
}
void    STDMETHODCALLTYPE D3D10Device::ResolveSubresource(ID3D10Resource *pDstResource, UINT DstSubresource, ID3D10Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)
{
#if RESHADE_ADDON >= 2
	if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(this, to_handle(pSrcResource), SrcSubresource, nullptr, to_handle(pDstResource), DstSubresource, 0, 0, 0, reshade::d3d10::convert_format(Format)))
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

#if RESHADE_ADDON >= 2
	// Call events with cleared state
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::all, reshade::api::pipeline {});

	const reshade::api::dynamic_state states[5] = { reshade::api::dynamic_state::primitive_topology, reshade::api::dynamic_state::blend_constant, reshade::api::dynamic_state::sample_mask, reshade::api::dynamic_state::front_stencil_reference_value, reshade::api::dynamic_state::back_stencil_reference_value };
	const uint32_t values[5] = { static_cast<uint32_t>(reshade::api::primitive_topology::undefined), 0xFFFFFFFF, D3D10_DEFAULT_SAMPLE_MASK, D3D10_DEFAULT_STENCIL_REFERENCE, D3D10_DEFAULT_STENCIL_REFERENCE };
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, static_cast<uint32_t>(std::size(states)), states, values);

	constexpr size_t max_null_objects = std::max(D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT, std::max(D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, std::max(D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, std::max(D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT))));
	void *const null_objects[max_null_objects] = {};
	invoke_bind_samplers_event(reshade::api::shader_stage::all, 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT, reinterpret_cast<ID3D10SamplerState *const *>(null_objects));
	invoke_bind_shader_resource_views_event(reshade::api::shader_stage::all, 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, reinterpret_cast<ID3D10ShaderResourceView *const *>(null_objects));
	invoke_bind_constant_buffers_event(reshade::api::shader_stage::all, 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, reinterpret_cast<ID3D10Buffer *const *>(null_objects));

	reshade::invoke_addon_event<reshade::addon_event::bind_index_buffer>(this, reshade::api::resource {}, 0, 0);
	reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(this, 0, D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, reinterpret_cast<const reshade::api::resource *>(null_objects), reinterpret_cast<const uint64_t *>(null_objects), reinterpret_cast<const uint32_t *>(null_objects));

	reshade::invoke_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(this, D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, reinterpret_cast<const reshade::api::resource_view *>(null_objects), reshade::api::resource_view {});

	reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(this, 0, 0, nullptr);
	reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(this, 0, 0, nullptr);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::Flush()
{
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::execute_command_list>(this, this);
#endif

	_orig->Flush();
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateBuffer(const D3D10_BUFFER_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Buffer **ppBuffer)
{
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppBuffer == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateBuffer(pDesc, pInitialData, ppBuffer);

#if RESHADE_ADDON
	D3D10_BUFFER_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d10::convert_resource_desc(internal_desc);

	std::vector<reshade::api::subresource_data> initial_data;
	if (pInitialData != nullptr)
	{
		initial_data.resize(1);
		initial_data[0] = *reinterpret_cast<const reshade::api::subresource_data *>(pInitialData);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, initial_data.data(), reshade::api::resource_usage::general))
	{
		reshade::d3d10::convert_resource_desc(desc, internal_desc);
		pDesc = &internal_desc;
		pInitialData = reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(&initial_data);
	}
#endif

	const HRESULT hr = _orig->CreateBuffer(pDesc, pInitialData, ppBuffer);
	if (SUCCEEDED(hr))
	{
		ID3D10Buffer *const resource = *ppBuffer;

		reshade::hooks::install("ID3D10Buffer::GetDevice", reshade::hooks::vtable_from_instance(resource), 3, &ID3D10Resource_GetDevice);

#if RESHADE_ADDON >= 2
		if (reshade::has_addon_event<reshade::addon_event::map_buffer_region>())
			reshade::hooks::install("ID3D10Buffer::Map", reshade::hooks::vtable_from_instance(resource), 10, &ID3D10Buffer_Map);
		if (reshade::has_addon_event<reshade::addon_event::unmap_buffer_region>())
			reshade::hooks::install("ID3D10Buffer::Unmap", reshade::hooks::vtable_from_instance(resource), 11, &ID3D10Buffer_Unmap);
#endif

#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::general, to_handle(resource));

		if (reshade::has_addon_event<reshade::addon_event::destroy_resource>())
		{
			register_destruction_callback_d3dx(resource, [this, resource]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreateBuffer failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture1D(const D3D10_TEXTURE1D_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Texture1D **ppTexture1D)
{
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppTexture1D == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateTexture1D(pDesc, pInitialData, ppTexture1D);

#if RESHADE_ADDON
	D3D10_TEXTURE1D_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d10::convert_resource_desc(internal_desc);

	std::vector<reshade::api::subresource_data> initial_data;
	if (pInitialData != nullptr)
	{
		// Allocate sufficient space in the array, in case an add-on changes the texture description, but wants to upload initial data still
		initial_data.resize(D3D10_REQ_MIP_LEVELS * D3D10_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION);
		for (UINT i = 0; i < pDesc->MipLevels * pDesc->ArraySize; ++i)
			initial_data[i] = *reinterpret_cast<const reshade::api::subresource_data *>(pInitialData + i);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, initial_data.data(), reshade::api::resource_usage::general))
	{
		reshade::d3d10::convert_resource_desc(desc, internal_desc);
		pDesc = &internal_desc;
		pInitialData = reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data.data());
	}
#endif

	const HRESULT hr = _orig->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
	if (SUCCEEDED(hr))
	{
		ID3D10Texture1D *const resource = *ppTexture1D;

		reshade::hooks::install("ID3D10Texture1D::GetDevice", reshade::hooks::vtable_from_instance(resource), 3, &ID3D10Resource_GetDevice);

#if RESHADE_ADDON >= 2
		if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
			reshade::hooks::install("ID3D10Texture1D::Map", reshade::hooks::vtable_from_instance(resource), 10, &ID3D10Texture1D_Map);
		if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
			reshade::hooks::install("ID3D10Texture1D::Unmap", reshade::hooks::vtable_from_instance(resource), 11, &ID3D10Texture1D_Unmap);
#endif

#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::general, to_handle(resource));

		if (reshade::has_addon_event<reshade::addon_event::destroy_resource>())
		{
			register_destruction_callback_d3dx(resource, [this, resource]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreateTexture1D failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture2D(const D3D10_TEXTURE2D_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Texture2D **ppTexture2D)
{
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppTexture2D == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateTexture2D(pDesc, pInitialData, ppTexture2D);

#if RESHADE_ADDON
	D3D10_TEXTURE2D_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d10::convert_resource_desc(internal_desc);

	std::vector<reshade::api::subresource_data> initial_data;
	if (pInitialData != nullptr)
	{
		initial_data.resize(D3D10_REQ_MIP_LEVELS * D3D10_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION);
		for (UINT i = 0; i < pDesc->MipLevels * pDesc->ArraySize; ++i)
			initial_data[i] = *reinterpret_cast<const reshade::api::subresource_data *>(pInitialData + i);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, initial_data.data(), reshade::api::resource_usage::general))
	{
		reshade::d3d10::convert_resource_desc(desc, internal_desc);
		pDesc = &internal_desc;
		pInitialData = reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data.data());
	}
#endif

	const HRESULT hr = _orig->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
	if (SUCCEEDED(hr))
	{
		ID3D10Texture2D *const resource = *ppTexture2D;

		reshade::hooks::install("ID3D10Texture2D::GetDevice", reshade::hooks::vtable_from_instance(resource), 3, &ID3D10Resource_GetDevice);

#if RESHADE_ADDON >= 2
		if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
			reshade::hooks::install("ID3D10Texture2D::Map", reshade::hooks::vtable_from_instance(resource), 10, &ID3D10Texture2D_Map);
		if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
			reshade::hooks::install("ID3D10Texture2D::Unmap", reshade::hooks::vtable_from_instance(resource), 11, &ID3D10Texture2D_Unmap);
#endif

#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::general, to_handle(resource));

		if (reshade::has_addon_event<reshade::addon_event::destroy_resource>())
		{
			register_destruction_callback_d3dx(resource, [this, resource]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreateTexture2D failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture3D(const D3D10_TEXTURE3D_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Texture3D **ppTexture3D)
{
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppTexture3D == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateTexture3D(pDesc, pInitialData, ppTexture3D);

#if RESHADE_ADDON
	D3D10_TEXTURE3D_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d10::convert_resource_desc(internal_desc);

	std::vector<reshade::api::subresource_data> initial_data;
	if (pInitialData != nullptr)
	{
		initial_data.resize(D3D10_REQ_MIP_LEVELS);
		for (UINT i = 0; i < pDesc->MipLevels; ++i)
			initial_data[i] = *reinterpret_cast<const reshade::api::subresource_data *>(pInitialData + i);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, initial_data.data(), reshade::api::resource_usage::general))
	{
		reshade::d3d10::convert_resource_desc(desc, internal_desc);
		pDesc = &internal_desc;
		pInitialData = reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data.data());
	}
#endif

	const HRESULT hr = _orig->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
	if (SUCCEEDED(hr))
	{
		ID3D10Texture3D *const resource = *ppTexture3D;

		reshade::hooks::install("ID3D10Texture3D::GetDevice", reshade::hooks::vtable_from_instance(resource), 3, &ID3D10Resource_GetDevice);

#if RESHADE_ADDON >= 2
		if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
			reshade::hooks::install("ID3D10Texture3D::Map", reshade::hooks::vtable_from_instance(resource), 10, &ID3D10Texture3D_Map);
		if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
			reshade::hooks::install("ID3D10Texture3D::Unmap", reshade::hooks::vtable_from_instance(resource), 11, &ID3D10Texture3D_Unmap);
#endif

#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::general, to_handle(resource));

		if (reshade::has_addon_event<reshade::addon_event::destroy_resource>())
		{
			register_destruction_callback_d3dx(resource, [this, resource]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreateTexture3D failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateShaderResourceView(ID3D10Resource *pResource, const D3D10_SHADER_RESOURCE_VIEW_DESC *pDesc, ID3D10ShaderResourceView **ppShaderResourceView)
{
#if RESHADE_ADDON
	if (pResource == nullptr)
		return E_INVALIDARG;
	if (ppShaderResourceView == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateShaderResourceView(pResource, pDesc, ppShaderResourceView);

	D3D10_SHADER_RESOURCE_VIEW_DESC internal_desc = (pDesc != nullptr) ? *pDesc : D3D10_SHADER_RESOURCE_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D10_SRV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d10::convert_resource_view_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::shader_resource, desc))
	{
		reshade::d3d10::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateShaderResourceView(pResource, pDesc, ppShaderResourceView);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		ID3D10ShaderResourceView *const resource_view = *ppShaderResourceView;

		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::shader_resource, desc, to_handle(resource_view));

		if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
		{
			register_destruction_callback_d3dx(resource_view, [this, resource_view]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(resource_view));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreateShaderResourceView failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateRenderTargetView(ID3D10Resource *pResource, const D3D10_RENDER_TARGET_VIEW_DESC *pDesc, ID3D10RenderTargetView **ppRenderTargetView)
{
#if RESHADE_ADDON
	if (pResource == nullptr)
		return E_INVALIDARG;
	if (ppRenderTargetView == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateRenderTargetView(pResource, pDesc, ppRenderTargetView);

	D3D10_RENDER_TARGET_VIEW_DESC internal_desc = (pDesc != nullptr) ? *pDesc : D3D10_RENDER_TARGET_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D10_RTV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d10::convert_resource_view_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::render_target, desc))
	{
		reshade::d3d10::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateRenderTargetView(pResource, pDesc, ppRenderTargetView);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		ID3D10RenderTargetView *const resource_view = *ppRenderTargetView;

		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::render_target, desc, to_handle(resource_view));

		if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
		{
			register_destruction_callback_d3dx(resource_view, [this, resource_view]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(resource_view));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreateRenderTargetView failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateDepthStencilView(ID3D10Resource *pResource, const D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D10DepthStencilView **ppDepthStencilView)
{
#if RESHADE_ADDON
	if (pResource == nullptr)
		return E_INVALIDARG;
	if (ppDepthStencilView == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);

	D3D10_DEPTH_STENCIL_VIEW_DESC internal_desc = (pDesc != nullptr) ? *pDesc : D3D10_DEPTH_STENCIL_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D10_DSV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d10::convert_resource_view_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::depth_stencil, desc))
	{
		reshade::d3d10::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		ID3D10DepthStencilView *const resource_view = *ppDepthStencilView;

		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::depth_stencil, desc, to_handle(resource_view));

		if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
		{
			register_destruction_callback_d3dx(resource_view, [this, resource_view]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(resource_view));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreateDepthStencilView failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateInputLayout(const D3D10_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements, const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D10InputLayout **ppInputLayout)
{
#if RESHADE_ADDON
	if (ppInputLayout == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);

	std::vector<D3D10_INPUT_ELEMENT_DESC> internal_desc; std::vector<reshade::api::input_element> desc;
	desc.reserve(NumElements);
	for (UINT i = 0; i < NumElements; ++i)
		desc.push_back(reshade::d3d10::convert_input_element(pInputElementDescs[i]));

	reshade::api::shader_desc signature_desc = {};
	signature_desc.code = pShaderBytecodeWithInputSignature;
	signature_desc.code_size = BytecodeLength;

	const reshade::api::pipeline_subobject subobjects[] = {
		{ reshade::api::pipeline_subobject_type::input_layout, static_cast<uint32_t>(desc.size()), desc.data() },
		{ reshade::api::pipeline_subobject_type::vertex_shader, 1, &signature_desc }
	};

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects))
	{
		internal_desc.reserve(desc.size());
		for (size_t i = 0; i < desc.size(); ++i)
			reshade::d3d10::convert_input_element(desc[i], internal_desc.emplace_back());

		pInputElementDescs = internal_desc.data();
		NumElements = static_cast<UINT>(internal_desc.size());
		pShaderBytecodeWithInputSignature = signature_desc.code;
		BytecodeLength = signature_desc.code_size;
	}
#endif

	const HRESULT hr = _orig->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		ID3D10InputLayout *const pipeline = *ppInputLayout;

		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(pipeline));

		if (reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
		{
			register_destruction_callback_d3dx(pipeline, [this, pipeline]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, to_handle(pipeline));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreateInputLayout failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateVertexShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10VertexShader **ppVertexShader)
{
#if RESHADE_ADDON
	if (ppVertexShader == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateVertexShader(pShaderBytecode, BytecodeLength, ppVertexShader);

	reshade::api::shader_desc desc = {};
	desc.code = pShaderBytecode;
	desc.code_size = BytecodeLength;

	const reshade::api::pipeline_subobject subobjects[] = {
		{ reshade::api::pipeline_subobject_type::vertex_shader, 1, &desc }
	};

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects))
	{
		pShaderBytecode = desc.code;
		BytecodeLength = desc.code_size;
	}
#endif

	const HRESULT hr = _orig->CreateVertexShader(pShaderBytecode, BytecodeLength, ppVertexShader);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		ID3D10VertexShader *const pipeline = *ppVertexShader;

		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(pipeline));

		if (reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
		{
			register_destruction_callback_d3dx(pipeline, [this, pipeline]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, to_handle(pipeline));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreateVertexShader failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateGeometryShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10GeometryShader **ppGeometryShader)
{
#if RESHADE_ADDON
	if (ppGeometryShader == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateGeometryShader(pShaderBytecode, BytecodeLength, ppGeometryShader);

	reshade::api::shader_desc desc = {};
	desc.code = pShaderBytecode;
	desc.code_size = BytecodeLength;

	const reshade::api::pipeline_subobject subobjects[] = {
		{ reshade::api::pipeline_subobject_type::geometry_shader, 1, &desc }
	};

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects))
	{
		pShaderBytecode = desc.code;
		BytecodeLength = desc.code_size;
	}
#endif

	const HRESULT hr = _orig->CreateGeometryShader(pShaderBytecode, BytecodeLength, ppGeometryShader);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		ID3D10GeometryShader *const pipeline = *ppGeometryShader;

		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(pipeline));

		if (reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
		{
			register_destruction_callback_d3dx(pipeline, [this, pipeline]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, to_handle(pipeline));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreateGeometryShader failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateGeometryShaderWithStreamOutput(const void *pShaderBytecode, SIZE_T BytecodeLength, const D3D10_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, UINT OutputStreamStride, ID3D10GeometryShader **ppGeometryShader)
{
#if RESHADE_ADDON
	if (ppGeometryShader == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, OutputStreamStride, ppGeometryShader);

	reshade::api::shader_desc desc = {};
	desc.code = pShaderBytecode;
	desc.code_size = BytecodeLength;

	reshade::api::stream_output_desc stream_output_desc = {};
	stream_output_desc.rasterized_stream = 0;

	const reshade::api::pipeline_subobject subobjects[] = {
		{ reshade::api::pipeline_subobject_type::geometry_shader, 1, &desc },
		{ reshade::api::pipeline_subobject_type::stream_output_state, 1, &stream_output_desc }
	};

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects))
	{
		pShaderBytecode = desc.code;
		BytecodeLength = desc.code_size;
		assert(stream_output_desc.rasterized_stream == 0);
	}
#endif

	const HRESULT hr = _orig->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, OutputStreamStride, ppGeometryShader);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		ID3D10GeometryShader *const pipeline = *ppGeometryShader;

		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(pipeline));

		if (reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
		{
			register_destruction_callback_d3dx(pipeline, [this, pipeline]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, to_handle(pipeline));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreateGeometryShaderWithStreamOutput failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreatePixelShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10PixelShader **ppPixelShader)
{
#if RESHADE_ADDON
	if (ppPixelShader == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreatePixelShader(pShaderBytecode, BytecodeLength, ppPixelShader);

	reshade::api::shader_desc desc = {};
	desc.code = pShaderBytecode;
	desc.code_size = BytecodeLength;

	const reshade::api::pipeline_subobject subobjects[] = {
		{ reshade::api::pipeline_subobject_type::pixel_shader, 1, &desc }
	};

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects))
	{
		pShaderBytecode = desc.code;
		BytecodeLength = desc.code_size;
	}
#endif

	const HRESULT hr = _orig->CreatePixelShader(pShaderBytecode, BytecodeLength, ppPixelShader);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		ID3D10PixelShader *const pipeline = *ppPixelShader;

		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(pipeline));

		if (reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
		{
			register_destruction_callback_d3dx(pipeline, [this, pipeline]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, to_handle(pipeline));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreatePixelShader failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateBlendState(const D3D10_BLEND_DESC *pBlendStateDesc, ID3D10BlendState **ppBlendState)
{
#if RESHADE_ADDON
	if (ppBlendState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateBlendState(pBlendStateDesc, ppBlendState);

	D3D10_BLEND_DESC internal_desc = {};
	auto desc = reshade::d3d10::convert_blend_desc(pBlendStateDesc);
	reshade::api::dynamic_state dynamic_states[2] = { reshade::api::dynamic_state::blend_constant, reshade::api::dynamic_state::sample_mask };

	const reshade::api::pipeline_subobject subobjects[] = {
		{ reshade::api::pipeline_subobject_type::blend_state, 1, &desc },
		{ reshade::api::pipeline_subobject_type::dynamic_pipeline_states, static_cast<uint32_t>(std::size(dynamic_states)), dynamic_states }
	};

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects))
	{
		reshade::d3d10::convert_blend_desc(desc, internal_desc);
		pBlendStateDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateBlendState(pBlendStateDesc, ppBlendState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		ID3D10BlendState *const pipeline = *ppBlendState;

		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(pipeline));

		if (reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
		{
			register_destruction_callback_d3dx(pipeline, [this, pipeline]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, to_handle(pipeline));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreateBlendState failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateDepthStencilState(const D3D10_DEPTH_STENCIL_DESC *pDepthStencilDesc, ID3D10DepthStencilState **ppDepthStencilState)
{
#if RESHADE_ADDON
	if (ppDepthStencilState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);

	D3D10_DEPTH_STENCIL_DESC internal_desc = {};
	auto desc = reshade::d3d10::convert_depth_stencil_desc(pDepthStencilDesc);
	reshade::api::dynamic_state dynamic_states[2] = { reshade::api::dynamic_state::front_stencil_reference_value, reshade::api::dynamic_state::back_stencil_reference_value };

	const reshade::api::pipeline_subobject subobjects[] = {
		{ reshade::api::pipeline_subobject_type::depth_stencil_state, 1, &desc },
		{ reshade::api::pipeline_subobject_type::dynamic_pipeline_states, static_cast<uint32_t>(std::size(dynamic_states)), dynamic_states }
	};

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects))
	{
		reshade::d3d10::convert_depth_stencil_desc(desc, internal_desc);
		pDepthStencilDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		ID3D10DepthStencilState *const pipeline = *ppDepthStencilState;

		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(pipeline));

		if (reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
		{
			register_destruction_callback_d3dx(pipeline, [this, pipeline]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, to_handle(pipeline));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreateDepthStencilState failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateRasterizerState(const D3D10_RASTERIZER_DESC *pRasterizerDesc, ID3D10RasterizerState **ppRasterizerState)
{
#if RESHADE_ADDON
	if (ppRasterizerState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);

	D3D10_RASTERIZER_DESC internal_desc = {};
	auto desc = reshade::d3d10::convert_rasterizer_desc(pRasterizerDesc);

	const reshade::api::pipeline_subobject subobjects[] = {
		{ reshade::api::pipeline_subobject_type::rasterizer_state, 1, &desc }
	};

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects))
	{
		reshade::d3d10::convert_rasterizer_desc(desc, internal_desc);
		pRasterizerDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		ID3D10RasterizerState *const pipeline = *ppRasterizerState;

		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(pipeline));

		if (reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
		{
			register_destruction_callback_d3dx(pipeline, [this, pipeline]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, to_handle(pipeline));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreateRasterizerState failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateSamplerState(const D3D10_SAMPLER_DESC *pSamplerDesc, ID3D10SamplerState **ppSamplerState)
{
#if RESHADE_ADDON
	if (pSamplerDesc == nullptr)
		return E_INVALIDARG;
	if (ppSamplerState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateSamplerState(pSamplerDesc, ppSamplerState);

	D3D10_SAMPLER_DESC internal_desc = *pSamplerDesc;
	auto desc = reshade::d3d10::convert_sampler_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_sampler>(this, desc))
	{
		reshade::d3d10::convert_sampler_desc(desc, internal_desc);
		pSamplerDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateSamplerState(pSamplerDesc, ppSamplerState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		ID3D10SamplerState *const sampler = *ppSamplerState;

		reshade::invoke_addon_event<reshade::addon_event::init_sampler>(this, desc, to_handle(sampler));

		if (reshade::has_addon_event<reshade::addon_event::destroy_sampler>())
		{
			register_destruction_callback_d3dx(sampler, [this, sampler]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_sampler>(this, to_handle(sampler));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::CreateSamplerState failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
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
	const HRESULT hr = _orig->OpenSharedResource(hResource, ReturnedInterface, ppResource);
	if (SUCCEEDED(hr))
	{
		assert(ppResource != nullptr);

#if RESHADE_ADDON
		// The returned interface IID may be 'IDXGIResource', which is a different pointer than 'ID3D10Resource', so need to query it first
		ID3D10Resource *resource = nullptr;
		reshade::api::resource_desc desc;

		if (com_ptr<ID3D10Buffer> buffer_resource;
			SUCCEEDED(static_cast<IUnknown *>(*ppResource)->QueryInterface(&buffer_resource)))
		{
			D3D10_BUFFER_DESC internal_desc;
			buffer_resource->GetDesc(&internal_desc);
			desc = reshade::d3d10::convert_resource_desc(internal_desc);
			resource = buffer_resource.get();
		}
		else
		if (com_ptr<ID3D10Texture1D> texture1d_resource;
			SUCCEEDED(static_cast<IUnknown *>(*ppResource)->QueryInterface(&texture1d_resource)))
		{
			D3D10_TEXTURE1D_DESC internal_desc;
			texture1d_resource->GetDesc(&internal_desc);
			desc = reshade::d3d10::convert_resource_desc(internal_desc);
			resource = texture1d_resource.get();
		}
		else
		if (com_ptr<ID3D10Texture2D> texture2d_resource;
			SUCCEEDED(static_cast<IUnknown *>(*ppResource)->QueryInterface(&texture2d_resource)))
		{
			D3D10_TEXTURE2D_DESC internal_desc;
			texture2d_resource->GetDesc(&internal_desc);
			desc = reshade::d3d10::convert_resource_desc(internal_desc);
			resource = texture2d_resource.get();
		}
		else
		if (com_ptr<ID3D10Texture3D> texture3d_resource;
			SUCCEEDED(static_cast<IUnknown *>(*ppResource)->QueryInterface(&texture3d_resource)))
		{
			D3D10_TEXTURE3D_DESC internal_desc;
			texture3d_resource->GetDesc(&internal_desc);
			desc = reshade::d3d10::convert_resource_desc(internal_desc);
			resource = texture3d_resource.get();
		}

		if (resource != nullptr)
		{
			assert((desc.flags & reshade::api::resource_flags::shared) == reshade::api::resource_flags::shared);

			reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, reshade::api::resource_usage::general, to_handle(resource));

			if (reshade::has_addon_event<reshade::addon_event::destroy_resource>())
			{
				register_destruction_callback_d3dx(resource, [this, resource]() {
					reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
				});
			}
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device::OpenSharedResource failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
void    STDMETHODCALLTYPE D3D10Device::SetTextFilterSize(UINT Width, UINT Height)
{
	_orig->SetTextFilterSize(Width, Height);
}
void    STDMETHODCALLTYPE D3D10Device::GetTextFilterSize(UINT *pWidth, UINT *pHeight)
{
	_orig->GetTextFilterSize(pWidth, pHeight);
}

HRESULT STDMETHODCALLTYPE D3D10Device::CreateShaderResourceView1(ID3D10Resource *pResource, const D3D10_SHADER_RESOURCE_VIEW_DESC1 *pDesc, ID3D10ShaderResourceView1 **ppShaderResourceView)
{
#if RESHADE_ADDON
	if (pResource == nullptr)
		return E_INVALIDARG;
	if (ppShaderResourceView == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateShaderResourceView1(pResource, pDesc, ppShaderResourceView);

	D3D10_SHADER_RESOURCE_VIEW_DESC1 internal_desc = (pDesc != nullptr) ? *pDesc : D3D10_SHADER_RESOURCE_VIEW_DESC1 { DXGI_FORMAT_UNKNOWN, D3D10_1_SRV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d10::convert_resource_view_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::shader_resource, desc))
	{
		reshade::d3d10::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateShaderResourceView1(pResource, pDesc, ppShaderResourceView);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		ID3D10ShaderResourceView1 *const resource_view = *ppShaderResourceView;

		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::shader_resource, desc, to_handle(resource_view));

		if (reshade::has_addon_event<reshade::addon_event::destroy_resource_view>())
		{
			register_destruction_callback_d3dx(resource_view, [this, resource_view]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(resource_view));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device1::CreateShaderResourceView1 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateBlendState1(const D3D10_BLEND_DESC1 *pBlendStateDesc, ID3D10BlendState1 **ppBlendState)
{
#if RESHADE_ADDON
	if (ppBlendState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateBlendState1(pBlendStateDesc, ppBlendState);

	D3D10_BLEND_DESC1 internal_desc = {};
	auto desc = reshade::d3d10::convert_blend_desc(pBlendStateDesc);
	reshade::api::dynamic_state dynamic_states[2] = { reshade::api::dynamic_state::blend_constant, reshade::api::dynamic_state::sample_mask };

	const reshade::api::pipeline_subobject subobjects[] = {
		{ reshade::api::pipeline_subobject_type::blend_state, 1, &desc },
		{ reshade::api::pipeline_subobject_type::dynamic_pipeline_states, static_cast<uint32_t>(std::size(dynamic_states)), dynamic_states }
	};

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects))
	{
		reshade::d3d10::convert_blend_desc(desc, internal_desc);
		pBlendStateDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateBlendState1(pBlendStateDesc, ppBlendState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		ID3D10BlendState1 *const pipeline = *ppBlendState;

		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, _global_pipeline_layout, static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(pipeline));

		if (reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
		{
			register_destruction_callback_d3dx(pipeline, [this, pipeline]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, to_handle(pipeline));
			});
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D10Device1::CreateBlendState1 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
D3D10_FEATURE_LEVEL1 STDMETHODCALLTYPE D3D10Device::GetFeatureLevel()
{
	return _orig->GetFeatureLevel();
}

#if RESHADE_ADDON >= 2
void D3D10Device::invoke_bind_samplers_event(reshade::api::shader_stage stage, UINT first, UINT count, ID3D10SamplerState *const *objects)
{
	assert(objects != nullptr || count == 0);

	if (!reshade::has_addon_event<reshade::addon_event::push_descriptors>())
		return;

#ifndef _WIN64
	temp_mem<reshade::api::sampler, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT> descriptors_mem(count);
	for (UINT i = 0; i < count; ++i)
		descriptors_mem[i] = to_handle(objects[i]);
	const auto descriptors = descriptors_mem.p;
#else
	static_assert(sizeof(*objects) == sizeof(reshade::api::sampler));
	const auto descriptors = reinterpret_cast<const reshade::api::sampler *>(objects);
#endif

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
		this,
		stage,
		_global_pipeline_layout, 0,
		reshade::api::descriptor_table_update { {}, first, 0, count, reshade::api::descriptor_type::sampler, descriptors });
}
void D3D10Device::invoke_bind_shader_resource_views_event(reshade::api::shader_stage stage, UINT first, UINT count, ID3D10ShaderResourceView *const *objects)
{
	assert(objects != nullptr || count == 0);

	if (!reshade::has_addon_event<reshade::addon_event::push_descriptors>())
		return;

#ifndef _WIN64
	temp_mem<reshade::api::resource_view, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> descriptors_mem(count);
	for (UINT i = 0; i < count; ++i)
		descriptors_mem[i] = to_handle(objects[i]);
	const auto descriptors = descriptors_mem.p;
#else
	static_assert(sizeof(*objects) == sizeof(reshade::api::resource_view));
	const auto descriptors = reinterpret_cast<const reshade::api::resource_view *>(objects);
#endif

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
		this,
		stage,
		_global_pipeline_layout, 1,
		reshade::api::descriptor_table_update { {}, first, 0, count, reshade::api::descriptor_type::shader_resource_view, descriptors });
}
void D3D10Device::invoke_bind_constant_buffers_event(reshade::api::shader_stage stage, UINT first, UINT count, ID3D10Buffer *const *objects)
{
	assert(objects != nullptr || count == 0);

	if (!reshade::has_addon_event<reshade::addon_event::push_descriptors>())
		return;

	temp_mem<reshade::api::buffer_range, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> descriptors_mem(count);
	for (UINT i = 0; i < count; ++i)
		descriptors_mem[i] = { to_handle(objects[i]), 0, UINT64_MAX };
	const auto descriptors = descriptors_mem.p;

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
		this,
		stage,
		_global_pipeline_layout, 2,
		reshade::api::descriptor_table_update { {}, first, 0, count, reshade::api::descriptor_type::constant_buffer, descriptors });
}
#endif
