/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "d3d11_device.hpp"
#include "d3d11_device_context.hpp"
#include "runtime_d3d11.hpp"

void D3D11DeviceContext::clear_drawcall_stats()
{
	_draw_call_tracker.reset();
}

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS

bool D3D11DeviceContext::save_depth_texture(ID3D11DepthStencilView *pDepthStencilView, bool cleared)
{
	if (_device->_runtimes.empty())
		return false;

	const auto runtime = _device->_runtimes.front();

	if (!runtime->depth_buffer_before_clear)
		return false;
	if (!cleared && !runtime->extended_depth_buffer_detection)
		return false;

	assert(pDepthStencilView != nullptr);

	// Retrieve texture from depth stencil
	com_ptr<ID3D11Resource> resource;
	pDepthStencilView->GetResource(&resource);

	com_ptr<ID3D11Texture2D> texture;
	if (FAILED(resource->QueryInterface(&texture)))
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC desc;
	texture->GetDesc(&desc);

	// Check if aspect ratio is similar to the back buffer one
	const float screen_aspect_ratio = float(runtime->frame_width()) / float(runtime->frame_height());
	const float texture_aspect_ratio = float(desc.Width) / float(desc.Height);

	if (fabs(texture_aspect_ratio - screen_aspect_ratio) > 0.1f)
	{
		return false;
	}

	if (desc.Width > runtime->frame_width())
	{
		return false;
	}	

	// In case the depth texture is retrieved, we make a copy of it and store it in an ordered map to use it later in the final rendering stage.
	if ((runtime->cleared_depth_buffer_index == 0 && cleared) || (_device->_clear_DSV_iter <= runtime->cleared_depth_buffer_index))
	{
		// Select an appropriate destination texture
		com_ptr<ID3D11Texture2D> depth_texture_save = runtime->select_depth_texture_save(desc);

		if (depth_texture_save == nullptr)
		{
			return false;
		}

		// Copy the depth texture. This is necessary because the content of the depth texture is cleared.
		// This way, we can retrieve this content in the final rendering stage
		this->CopyResource(depth_texture_save.get(), texture.get());

		// Store the saved texture in the ordered map.
		_draw_call_tracker.track_depth_texture(runtime->depth_buffer_texture_format, _device->_clear_DSV_iter, texture.get(), pDepthStencilView, depth_texture_save, cleared);
	}
	else
	{
		// Store a null depth texture in the ordered map in order to display it even if the user chose a previous cleared texture.
		// This way the texture will still be visible in the depth buffer selection window and the user can choose it.
		_draw_call_tracker.track_depth_texture(runtime->depth_buffer_texture_format, _device->_clear_DSV_iter, texture.get(), pDepthStencilView, nullptr, cleared);
	}

	// TODO: This is unsafe if multiple device contexts are used on multiple threads
	_device->_clear_DSV_iter++;

	return true;
}

void D3D11DeviceContext::track_active_rendertargets(UINT NumViews, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView)
{
	if (pDepthStencilView == nullptr)
		return;

	if (_device->_runtimes.empty())
		return;

	const auto runtime = _device->_runtimes.front();

	_draw_call_tracker.track_rendertargets(runtime->depth_buffer_texture_format, pDepthStencilView, NumViews, ppRenderTargetViews);

	save_depth_texture(pDepthStencilView, false);
}
void D3D11DeviceContext::track_cleared_depthstencil(ID3D11DepthStencilView *pDepthStencilView)
{
	if (pDepthStencilView == nullptr)
		return;

	save_depth_texture(pDepthStencilView, true);
}
#endif

// ID3D11DeviceContext
HRESULT STDMETHODCALLTYPE D3D11DeviceContext::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}
	else if (
		riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(ID3D11DeviceChild) ||
		riid == __uuidof(ID3D11DeviceContext) ||
		riid == __uuidof(ID3D11DeviceContext1) ||
		riid == __uuidof(ID3D11DeviceContext2) ||
		riid == __uuidof(ID3D11DeviceContext3))
	{
		#pragma region Update to ID3D11DeviceContext1 interface
		if (riid == __uuidof(ID3D11DeviceContext1) && _interface_version < 1)
		{
			ID3D11DeviceContext1 *devicecontext1 = nullptr;

			if (FAILED(_orig->QueryInterface(&devicecontext1)))
			{
				return E_NOINTERFACE;
			}

			_orig->Release();

#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgraded 'ID3D11DeviceContext' object " << this << " to 'ID3D11DeviceContext1'.";
#endif
			_orig = devicecontext1;
			_interface_version = 1;
		}
		#pragma endregion
		#pragma region Update to ID3D11DeviceContext2 interface
		if (riid == __uuidof(ID3D11DeviceContext2) && _interface_version < 2)
		{
			ID3D11DeviceContext2 *devicecontext2 = nullptr;

			if (FAILED(_orig->QueryInterface(&devicecontext2)))
			{
				return E_NOINTERFACE;
			}

			_orig->Release();

#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgraded 'ID3D11DeviceContext" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << "' object " << this << " to 'ID3D11DeviceContext2'.";
#endif
			_orig = devicecontext2;
			_interface_version = 2;
		}
		#pragma endregion
		#pragma region Update to ID3D11DeviceContext3 interface
		if (riid == __uuidof(ID3D11DeviceContext3) && _interface_version < 3)
		{
			ID3D11DeviceContext3 *devicecontext3 = nullptr;

			if (FAILED(_orig->QueryInterface(&devicecontext3)))
			{
				return E_NOINTERFACE;
			}

			_orig->Release();

#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgraded 'ID3D11DeviceContext" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << "' object " << this << " to 'ID3D11DeviceContext3'.";
#endif
			_orig = devicecontext3;
			_interface_version = 3;
		}
		#pragma endregion

		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE D3D11DeviceContext::AddRef()
{
	_ref++;

	return _orig->AddRef();
}
ULONG STDMETHODCALLTYPE D3D11DeviceContext::Release()
{
	ULONG ref = _orig->Release();

	if (--_ref == 0 && ref != 0)
	{
		LOG(WARNING) << "Reference count for 'ID3D11DeviceContext" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << "' object " << this << " is inconsistent: " << ref << ", but expected 0.";

		ref = 0;
	}

	if (ref == 0)
	{
		assert(_ref <= 0);

#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Destroyed 'ID3D11DeviceContext" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << "' object " << this << ".";
#endif
		delete this;
	}

	return ref;
}
void STDMETHODCALLTYPE D3D11DeviceContext::GetDevice(ID3D11Device **ppDevice)
{
	if (ppDevice == nullptr)
	{
		return;
	}

	_device->AddRef();

	*ppDevice = _device;
}
HRESULT STDMETHODCALLTYPE D3D11DeviceContext::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D11DeviceContext::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D11DeviceContext::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
	return _orig->SetPrivateDataInterface(guid, pData);
}
void STDMETHODCALLTYPE D3D11DeviceContext::VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers)
{
	_orig->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void STDMETHODCALLTYPE D3D11DeviceContext::PSSetShader(ID3D11PixelShader *pPixelShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
{
	_orig->PSSetShader(pPixelShader, ppClassInstances, NumClassInstances);
}
void STDMETHODCALLTYPE D3D11DeviceContext::PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers)
{
	_orig->PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::VSSetShader(ID3D11VertexShader *pVertexShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
{
	_orig->VSSetShader(pVertexShader, ppClassInstances, NumClassInstances);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	_orig->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
	_draw_call_tracker.on_draw(this, IndexCount);
}
void STDMETHODCALLTYPE D3D11DeviceContext::Draw(UINT VertexCount, UINT StartVertexLocation)
{
	_orig->Draw(VertexCount, StartVertexLocation);
	_draw_call_tracker.on_draw(this, VertexCount);
}
HRESULT STDMETHODCALLTYPE D3D11DeviceContext::Map(ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *pMappedResource)
{
#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
	_draw_call_tracker.on_map(pResource);
#endif

	return _orig->Map(pResource, Subresource, MapType, MapFlags, pMappedResource);
}
void STDMETHODCALLTYPE D3D11DeviceContext::Unmap(ID3D11Resource *pResource, UINT Subresource)
{
	_orig->Unmap(pResource, Subresource);
}
void STDMETHODCALLTYPE D3D11DeviceContext::PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers)
{
	_orig->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::IASetInputLayout(ID3D11InputLayout *pInputLayout)
{
	_orig->IASetInputLayout(pInputLayout);
}
void STDMETHODCALLTYPE D3D11DeviceContext::IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets)
{
	_orig->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}
void STDMETHODCALLTYPE D3D11DeviceContext::IASetIndexBuffer(ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset)
{
	_orig->IASetIndexBuffer(pIndexBuffer, Format, Offset);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
	_orig->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	_draw_call_tracker.on_draw(this, IndexCountPerInstance * InstanceCount);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
	_orig->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	_draw_call_tracker.on_draw(this, VertexCountPerInstance * InstanceCount);
}
void STDMETHODCALLTYPE D3D11DeviceContext::GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers)
{
	_orig->GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::GSSetShader(ID3D11GeometryShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
{
	_orig->GSSetShader(pShader, ppClassInstances, NumClassInstances);
}
void STDMETHODCALLTYPE D3D11DeviceContext::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology)
{
	_orig->IASetPrimitiveTopology(Topology);
}
void STDMETHODCALLTYPE D3D11DeviceContext::VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void STDMETHODCALLTYPE D3D11DeviceContext::VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers)
{
	_orig->VSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::Begin(ID3D11Asynchronous *pAsync)
{
	_orig->Begin(pAsync);
}
void STDMETHODCALLTYPE D3D11DeviceContext::End(ID3D11Asynchronous *pAsync)
{
	_orig->End(pAsync);
}
HRESULT STDMETHODCALLTYPE D3D11DeviceContext::GetData(ID3D11Asynchronous *pAsync, void *pData, UINT DataSize, UINT GetDataFlags)
{
	return _orig->GetData(pAsync, pData, DataSize, GetDataFlags);
}
void STDMETHODCALLTYPE D3D11DeviceContext::SetPredication(ID3D11Predicate *pPredicate, BOOL PredicateValue)
{
	_orig->SetPredication(pPredicate, PredicateValue);
}
void STDMETHODCALLTYPE D3D11DeviceContext::GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void STDMETHODCALLTYPE D3D11DeviceContext::GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers)
{
	_orig->GSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::OMSetRenderTargets(UINT NumViews, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView)
{
#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
	track_active_rendertargets(NumViews, ppRenderTargetViews, pDepthStencilView);
#endif
	_orig->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);
}
void STDMETHODCALLTYPE D3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews, const UINT *pUAVInitialCounts)
{
#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
	track_active_rendertargets(NumRTVs, ppRenderTargetViews, pDepthStencilView);
#endif
	_orig->OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, pDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}
void STDMETHODCALLTYPE D3D11DeviceContext::OMSetBlendState(ID3D11BlendState *pBlendState, const FLOAT BlendFactor[4], UINT SampleMask)
{
	_orig->OMSetBlendState(pBlendState, BlendFactor, SampleMask);
}
void STDMETHODCALLTYPE D3D11DeviceContext::OMSetDepthStencilState(ID3D11DepthStencilState *pDepthStencilState, UINT StencilRef)
{
	_orig->OMSetDepthStencilState(pDepthStencilState, StencilRef);
}
void STDMETHODCALLTYPE D3D11DeviceContext::SOSetTargets(UINT NumBuffers, ID3D11Buffer *const *ppSOTargets, const UINT *pOffsets)
{
	_orig->SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DrawAuto()
{
	_orig->DrawAuto();
	_draw_call_tracker.on_draw(this, 0);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DrawIndexedInstancedIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs)
{
	_orig->DrawIndexedInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
	_draw_call_tracker.on_draw(this, 0);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DrawInstancedIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs)
{
	_orig->DrawInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
	_draw_call_tracker.on_draw(this, 0);
}
void STDMETHODCALLTYPE D3D11DeviceContext::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
	_orig->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DispatchIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs)
{
	_orig->DispatchIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
}
void STDMETHODCALLTYPE D3D11DeviceContext::RSSetState(ID3D11RasterizerState *pRasterizerState)
{
	_orig->RSSetState(pRasterizerState);
}
void STDMETHODCALLTYPE D3D11DeviceContext::RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT *pViewports)
{
	_orig->RSSetViewports(NumViewports, pViewports);
}
void STDMETHODCALLTYPE D3D11DeviceContext::RSSetScissorRects(UINT NumRects, const D3D11_RECT *pRects)
{
	_orig->RSSetScissorRects(NumRects, pRects);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CopySubresourceRegion(ID3D11Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *pSrcResource, UINT SrcSubresource, const D3D11_BOX *pSrcBox)
{
	_orig->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CopyResource(ID3D11Resource *pDstResource, ID3D11Resource *pSrcResource)
{
	_orig->CopyResource(pDstResource, pSrcResource);
}
void STDMETHODCALLTYPE D3D11DeviceContext::UpdateSubresource(ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
{
	_orig->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CopyStructureCount(ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView *pSrcView)
{
	_orig->CopyStructureCount(pDstBuffer, DstAlignedByteOffset, pSrcView);
}
void STDMETHODCALLTYPE D3D11DeviceContext::ClearRenderTargetView(ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4])
{
	_orig->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
}
void STDMETHODCALLTYPE D3D11DeviceContext::ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView *pUnorderedAccessView, const UINT Values[4])
{
	_orig->ClearUnorderedAccessViewUint(pUnorderedAccessView, Values);
}
void STDMETHODCALLTYPE D3D11DeviceContext::ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView *pUnorderedAccessView, const FLOAT Values[4])
{
	_orig->ClearUnorderedAccessViewFloat(pUnorderedAccessView, Values);
}
void STDMETHODCALLTYPE D3D11DeviceContext::ClearDepthStencilView(ID3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
{
#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
	track_cleared_depthstencil(pDepthStencilView);
#endif
	_orig->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
}
void STDMETHODCALLTYPE D3D11DeviceContext::GenerateMips(ID3D11ShaderResourceView *pShaderResourceView)
{
	_orig->GenerateMips(pShaderResourceView);
}
void STDMETHODCALLTYPE D3D11DeviceContext::SetResourceMinLOD(ID3D11Resource *pResource, FLOAT MinLOD)
{
	_orig->SetResourceMinLOD(pResource, MinLOD);
}
FLOAT STDMETHODCALLTYPE D3D11DeviceContext::GetResourceMinLOD(ID3D11Resource *pResource)
{
	return _orig->GetResourceMinLOD(pResource);
}
void STDMETHODCALLTYPE D3D11DeviceContext::ResolveSubresource(ID3D11Resource *pDstResource, UINT DstSubresource, ID3D11Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)
{
	_orig->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}
void STDMETHODCALLTYPE D3D11DeviceContext::ExecuteCommandList(ID3D11CommandList *pCommandList, BOOL RestoreContextState)
{
	if (pCommandList != nullptr)
	{
		_device->merge_commandlist_trackers(pCommandList, _draw_call_tracker);
	}

	_orig->ExecuteCommandList(pCommandList, RestoreContextState);
}
void STDMETHODCALLTYPE D3D11DeviceContext::HSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->HSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void STDMETHODCALLTYPE D3D11DeviceContext::HSSetShader(ID3D11HullShader *pHullShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
{
	_orig->HSSetShader(pHullShader, ppClassInstances, NumClassInstances);
}
void STDMETHODCALLTYPE D3D11DeviceContext::HSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers)
{
	_orig->HSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers)
{
	_orig->HSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->DSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DSSetShader(ID3D11DomainShader *pDomainShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
{
	_orig->DSSetShader(pDomainShader, ppClassInstances, NumClassInstances);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers)
{
	_orig->DSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers)
{
	_orig->DSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->CSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CSSetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews, const UINT *pUAVInitialCounts)
{
	_orig->CSSetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CSSetShader(ID3D11ComputeShader *pComputeShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
{
	_orig->CSSetShader(pComputeShader, ppClassInstances, NumClassInstances);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers)
{
	_orig->CSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers)
{
	_orig->CSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers)
{
	_orig->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews)
{
	_orig->PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void STDMETHODCALLTYPE D3D11DeviceContext::PSGetShader(ID3D11PixelShader **ppPixelShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
{
	_orig->PSGetShader(ppPixelShader, ppClassInstances, pNumClassInstances);
}
void STDMETHODCALLTYPE D3D11DeviceContext::PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers)
{
	_orig->PSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::VSGetShader(ID3D11VertexShader **ppVertexShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
{
	_orig->VSGetShader(ppVertexShader, ppClassInstances, pNumClassInstances);
}
void STDMETHODCALLTYPE D3D11DeviceContext::PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers)
{
	_orig->PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::IAGetInputLayout(ID3D11InputLayout **ppInputLayout)
{
	_orig->IAGetInputLayout(ppInputLayout);
}
void STDMETHODCALLTYPE D3D11DeviceContext::IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppVertexBuffers, UINT *pStrides, UINT *pOffsets)
{
	_orig->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}
void STDMETHODCALLTYPE D3D11DeviceContext::IAGetIndexBuffer(ID3D11Buffer **pIndexBuffer, DXGI_FORMAT *Format, UINT *Offset)
{
	_orig->IAGetIndexBuffer(pIndexBuffer, Format, Offset);
}
void STDMETHODCALLTYPE D3D11DeviceContext::GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers)
{
	_orig->GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::GSGetShader(ID3D11GeometryShader **ppGeometryShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
{
	_orig->GSGetShader(ppGeometryShader, ppClassInstances, pNumClassInstances);
}
void STDMETHODCALLTYPE D3D11DeviceContext::IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY *pTopology)
{
	_orig->IAGetPrimitiveTopology(pTopology);
}
void STDMETHODCALLTYPE D3D11DeviceContext::VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews)
{
	_orig->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void STDMETHODCALLTYPE D3D11DeviceContext::VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers)
{
	_orig->VSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::GetPredication(ID3D11Predicate **ppPredicate, BOOL *pPredicateValue)
{
	_orig->GetPredication(ppPredicate, pPredicateValue);
}
void STDMETHODCALLTYPE D3D11DeviceContext::GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews)
{
	_orig->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void STDMETHODCALLTYPE D3D11DeviceContext::GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers)
{
	_orig->GSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::OMGetRenderTargets(UINT NumViews, ID3D11RenderTargetView **ppRenderTargetViews, ID3D11DepthStencilView **ppDepthStencilView)
{
	_orig->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);
}
void STDMETHODCALLTYPE D3D11DeviceContext::OMGetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView **ppRenderTargetViews, ID3D11DepthStencilView **ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews)
{
	_orig->OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, ppDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews);
}
void STDMETHODCALLTYPE D3D11DeviceContext::OMGetBlendState(ID3D11BlendState **ppBlendState, FLOAT BlendFactor[4], UINT *pSampleMask)
{
	_orig->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask);
}
void STDMETHODCALLTYPE D3D11DeviceContext::OMGetDepthStencilState(ID3D11DepthStencilState **ppDepthStencilState, UINT *pStencilRef)
{
	_orig->OMGetDepthStencilState(ppDepthStencilState, pStencilRef);
}
void STDMETHODCALLTYPE D3D11DeviceContext::SOGetTargets(UINT NumBuffers, ID3D11Buffer **ppSOTargets)
{
	_orig->SOGetTargets(NumBuffers, ppSOTargets);
}
void STDMETHODCALLTYPE D3D11DeviceContext::RSGetState(ID3D11RasterizerState **ppRasterizerState)
{
	_orig->RSGetState(ppRasterizerState);
}
void STDMETHODCALLTYPE D3D11DeviceContext::RSGetViewports(UINT *pNumViewports, D3D11_VIEWPORT *pViewports)
{
	_orig->RSGetViewports(pNumViewports, pViewports);
}
void STDMETHODCALLTYPE D3D11DeviceContext::RSGetScissorRects(UINT *pNumRects, D3D11_RECT *pRects)
{
	_orig->RSGetScissorRects(pNumRects, pRects);
}
void STDMETHODCALLTYPE D3D11DeviceContext::HSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews)
{
	_orig->HSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void STDMETHODCALLTYPE D3D11DeviceContext::HSGetShader(ID3D11HullShader **ppHullShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
{
	_orig->HSGetShader(ppHullShader, ppClassInstances, pNumClassInstances);
}
void STDMETHODCALLTYPE D3D11DeviceContext::HSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers)
{
	_orig->HSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers)
{
	_orig->HSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews)
{
	_orig->DSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DSGetShader(ID3D11DomainShader **ppDomainShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
{
	_orig->DSGetShader(ppDomainShader, ppClassInstances, pNumClassInstances);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers)
{
	_orig->DSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers)
{
	_orig->DSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews)
{
	_orig->CSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CSGetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews)
{
	_orig->CSGetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CSGetShader(ID3D11ComputeShader **ppComputeShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
{
	_orig->CSGetShader(ppComputeShader, ppClassInstances, pNumClassInstances);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers)
{
	_orig->CSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers)
{
	_orig->CSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void STDMETHODCALLTYPE D3D11DeviceContext::ClearState()
{
	_orig->ClearState();
}
void STDMETHODCALLTYPE D3D11DeviceContext::Flush()
{
	_orig->Flush();
}
UINT STDMETHODCALLTYPE D3D11DeviceContext::GetContextFlags()
{
	return _orig->GetContextFlags();
}
HRESULT STDMETHODCALLTYPE D3D11DeviceContext::FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList **ppCommandList)
{
	const HRESULT hr = _orig->FinishCommandList(RestoreDeferredContextState, ppCommandList);

	if (SUCCEEDED(hr) && ppCommandList != nullptr)
	{
		_device->add_commandlist_trackers(*ppCommandList, _draw_call_tracker);
	}

	_draw_call_tracker.reset();

	return hr;
}
D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE D3D11DeviceContext::GetType()
{
	return _orig->GetType();
}

// ID3D11DeviceContext1
void STDMETHODCALLTYPE D3D11DeviceContext::CopySubresourceRegion1(ID3D11Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *pSrcResource, UINT SrcSubresource, const D3D11_BOX *pSrcBox, UINT CopyFlags)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->CopySubresourceRegion1(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, CopyFlags);
}
void STDMETHODCALLTYPE D3D11DeviceContext::UpdateSubresource1(ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->UpdateSubresource1(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DiscardResource(ID3D11Resource *pResource)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->DiscardResource(pResource);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DiscardView(ID3D11View *pResourceView)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->DiscardView(pResourceView);
}
void STDMETHODCALLTYPE D3D11DeviceContext::VSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant, const UINT *pNumConstants)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->VSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}
void STDMETHODCALLTYPE D3D11DeviceContext::HSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant, const UINT *pNumConstants)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->HSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant, const UINT *pNumConstants)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->DSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}
void STDMETHODCALLTYPE D3D11DeviceContext::GSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant, const UINT *pNumConstants)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->GSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}
void STDMETHODCALLTYPE D3D11DeviceContext::PSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant, const UINT *pNumConstants)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->PSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant, const UINT *pNumConstants)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->CSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}
void STDMETHODCALLTYPE D3D11DeviceContext::VSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->VSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}
void STDMETHODCALLTYPE D3D11DeviceContext::HSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->HSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->DSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}
void STDMETHODCALLTYPE D3D11DeviceContext::GSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->GSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}
void STDMETHODCALLTYPE D3D11DeviceContext::PSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->PSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->CSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}
void STDMETHODCALLTYPE D3D11DeviceContext::SwapDeviceContextState(ID3DDeviceContextState *pState, ID3DDeviceContextState **ppPreviousState)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->SwapDeviceContextState(pState, ppPreviousState);
}
void STDMETHODCALLTYPE D3D11DeviceContext::ClearView(ID3D11View *pView, const FLOAT Color[4], const D3D11_RECT *pRect, UINT NumRects)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->ClearView(pView, Color, pRect, NumRects);
}
void STDMETHODCALLTYPE D3D11DeviceContext::DiscardView1(ID3D11View *pResourceView, const D3D11_RECT *pRects, UINT NumRects)
{
	assert(_interface_version >= 1);

	static_cast<ID3D11DeviceContext1 *>(_orig)->DiscardView1(pResourceView, pRects, NumRects);
}

// ID3D11DeviceContext2
HRESULT STDMETHODCALLTYPE D3D11DeviceContext::UpdateTileMappings(ID3D11Resource *pTiledResource, UINT NumTiledResourceRegions, const D3D11_TILED_RESOURCE_COORDINATE *pTiledResourceRegionStartCoordinates, const D3D11_TILE_REGION_SIZE *pTiledResourceRegionSizes, ID3D11Buffer *pTilePool, UINT NumRanges, const UINT *pRangeFlags, const UINT *pTilePoolStartOffsets, const UINT *pRangeTileCounts, UINT Flags)
{
	assert(_interface_version >= 2);

	return static_cast<ID3D11DeviceContext2 *>(_orig)->UpdateTileMappings(pTiledResource, NumTiledResourceRegions, pTiledResourceRegionStartCoordinates, pTiledResourceRegionSizes, pTilePool, NumRanges, pRangeFlags, pTilePoolStartOffsets, pRangeTileCounts, Flags);
}
HRESULT STDMETHODCALLTYPE D3D11DeviceContext::CopyTileMappings(ID3D11Resource *pDestTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pDestRegionStartCoordinate, ID3D11Resource *pSourceTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pSourceRegionStartCoordinate, const D3D11_TILE_REGION_SIZE *pTileRegionSize, UINT Flags)
{
	assert(_interface_version >= 2);

	return static_cast<ID3D11DeviceContext2 *>(_orig)->CopyTileMappings(pDestTiledResource, pDestRegionStartCoordinate, pSourceTiledResource, pSourceRegionStartCoordinate, pTileRegionSize, Flags);
}
void STDMETHODCALLTYPE D3D11DeviceContext::CopyTiles(ID3D11Resource *pTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate, const D3D11_TILE_REGION_SIZE *pTileRegionSize, ID3D11Buffer *pBuffer, UINT64 BufferStartOffsetInBytes, UINT Flags)
{
	assert(_interface_version >= 2);

	static_cast<ID3D11DeviceContext2 *>(_orig)->CopyTiles(pTiledResource, pTileRegionStartCoordinate, pTileRegionSize, pBuffer, BufferStartOffsetInBytes, Flags);
}
void STDMETHODCALLTYPE D3D11DeviceContext::UpdateTiles(ID3D11Resource *pDestTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pDestTileRegionStartCoordinate, const D3D11_TILE_REGION_SIZE *pDestTileRegionSize, const void *pSourceTileData, UINT Flags)
{
	assert(_interface_version >= 2);

	static_cast<ID3D11DeviceContext2 *>(_orig)->UpdateTiles(pDestTiledResource, pDestTileRegionStartCoordinate, pDestTileRegionSize, pSourceTileData, Flags);
}
HRESULT STDMETHODCALLTYPE D3D11DeviceContext::ResizeTilePool(ID3D11Buffer *pTilePool, UINT64 NewSizeInBytes)
{
	assert(_interface_version >= 2);

	return static_cast<ID3D11DeviceContext2 *>(_orig)->ResizeTilePool(pTilePool, NewSizeInBytes);
}
void STDMETHODCALLTYPE D3D11DeviceContext::TiledResourceBarrier(ID3D11DeviceChild *pTiledResourceOrViewAccessBeforeBarrier, ID3D11DeviceChild *pTiledResourceOrViewAccessAfterBarrier)
{
	assert(_interface_version >= 2);

	static_cast<ID3D11DeviceContext2 *>(_orig)->TiledResourceBarrier(pTiledResourceOrViewAccessBeforeBarrier, pTiledResourceOrViewAccessAfterBarrier);
}
BOOL STDMETHODCALLTYPE D3D11DeviceContext::IsAnnotationEnabled()
{
	assert(_interface_version >= 2);

	return static_cast<ID3D11DeviceContext2 *>(_orig)->IsAnnotationEnabled();
}
void STDMETHODCALLTYPE D3D11DeviceContext::SetMarkerInt(LPCWSTR pLabel, INT Data)
{
	assert(_interface_version >= 2);

	static_cast<ID3D11DeviceContext2 *>(_orig)->SetMarkerInt(pLabel, Data);
}
void STDMETHODCALLTYPE D3D11DeviceContext::BeginEventInt(LPCWSTR pLabel, INT Data)
{
	assert(_interface_version >= 2);

	static_cast<ID3D11DeviceContext2 *>(_orig)->BeginEventInt(pLabel, Data);
}
void STDMETHODCALLTYPE D3D11DeviceContext::EndEvent()
{
	assert(_interface_version >= 2);

	static_cast<ID3D11DeviceContext2 *>(_orig)->EndEvent();
}

// ID3D11DeviceContext3
void STDMETHODCALLTYPE D3D11DeviceContext::Flush1(D3D11_CONTEXT_TYPE ContextType, HANDLE hEvent)
{
	assert(_interface_version >= 3);

	static_cast<ID3D11DeviceContext3 *>(_orig)->Flush1(ContextType, hEvent);
}
void STDMETHODCALLTYPE D3D11DeviceContext::SetHardwareProtectionState(BOOL HwProtectionEnable)
{
	assert(_interface_version >= 3);

	static_cast<ID3D11DeviceContext3 *>(_orig)->SetHardwareProtectionState(HwProtectionEnable);
}
void STDMETHODCALLTYPE D3D11DeviceContext::GetHardwareProtectionState(BOOL *pHwProtectionEnable)
{
	assert(_interface_version >= 3);

	static_cast<ID3D11DeviceContext3 *>(_orig)->GetHardwareProtectionState(pHwProtectionEnable);
}
