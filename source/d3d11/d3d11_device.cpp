/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "d3d11_device.hpp"
#include "d3d11_device_context.hpp"
#include "dxgi/dxgi_device.hpp"
#include "dxgi/format_utils.hpp"

D3D11Device::D3D11Device(IDXGIDevice1 *dxgi_device, ID3D11Device *original) :
	_orig(original),
	_interface_version(0),
	_dxgi_device(new DXGIDevice(dxgi_device, this))
{
	assert(_orig != nullptr);
}

bool D3D11Device::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this))
		// IUnknown is handled by DXGIDevice
		return true;

	static const IID iid_lookup[] = {
		__uuidof(ID3D11Device),
		__uuidof(ID3D11Device1),
		__uuidof(ID3D11Device2),
		__uuidof(ID3D11Device3),
		__uuidof(ID3D11Device4),
		__uuidof(ID3D11Device5),
	};

	for (unsigned int version = 0; version < ARRAYSIZE(iid_lookup); ++version)
	{
		if (riid != iid_lookup[version])
			continue;

		if (version > _interface_version)
		{
			IUnknown *new_interface = nullptr;
			if (FAILED(_orig->QueryInterface(riid, reinterpret_cast<void **>(&new_interface))))
				return false;
#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgraded ID3D11Device" << _interface_version << " object " << this << " to ID3D11Device" << version << '.';
#endif
			_orig->Release();
			_orig = static_cast<ID3D11Device *>(new_interface);
			_interface_version = version;
		}

		return true;
	}

	return false;
}

HRESULT STDMETHODCALLTYPE D3D11Device::QueryInterface(REFIID riid, void **ppvObj)
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
ULONG   STDMETHODCALLTYPE D3D11Device::AddRef()
{
	_orig->AddRef();

	// Add references to other objects that are coupled with the device
	_dxgi_device->AddRef();
	_immediate_context->AddRef();

	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE D3D11Device::Release()
{
	// Release references to other objects that are coupled with the device
	_immediate_context->Release(); // Release context before device since it may hold a reference to it
	_dxgi_device->Release();

	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
		return _orig->Release(), ref;

	// Note: At this point the immediate context should have been deleted by the release above (so do not access it)
	const ULONG ref_orig = _orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		LOG(WARN) << "Reference count for ID3D11Device" << _interface_version << " object " << this << " is inconsistent.";

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroyed ID3D11Device" << _interface_version << " object " << this << '.';
#endif
	delete this;

	return 0;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateBuffer(const D3D11_BUFFER_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Buffer **ppBuffer)
{
	return _orig->CreateBuffer(pDesc, pInitialData, ppBuffer);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture1D(const D3D11_TEXTURE1D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture1D **ppTexture1D)
{
	return _orig->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture2D(const D3D11_TEXTURE2D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture2D **ppTexture2D)
{
	assert(pDesc != nullptr);

	// Add D3D11_BIND_SHADER_RESOURCE flag to all depth-stencil textures so that we can access them in post-processing shaders
	D3D11_TEXTURE2D_DESC new_desc = *pDesc;
	if (new_desc.SampleDesc.Count == 1 && // Skip MSAA textures
		0 != (new_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) &&
		0 == (new_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE))
	{
		new_desc.Format = make_dxgi_format_typeless(new_desc.Format);
		new_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
	}

	const HRESULT hr = _orig->CreateTexture2D(&new_desc, pInitialData, ppTexture2D);
	if (FAILED(hr))
	{
		LOG(WARN) << "ID3D11Device::CreateTexture2D" << " failed with error code " << hr << '.';
		LOG(INFO) << "> Dumping description:";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(INFO) << "  | Width                                   | " << std::setw(39) << new_desc.Width << " |";
		LOG(INFO) << "  | Height                                  | " << std::setw(39) << new_desc.Height << " |";
		LOG(INFO) << "  | MipLevels                               | " << std::setw(39) << new_desc.MipLevels << " |";
		LOG(INFO) << "  | ArraySize                               | " << std::setw(39) << new_desc.ArraySize << " |";
		LOG(INFO) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
		LOG(INFO) << "  | SampleCount                             | " << std::setw(39) << new_desc.SampleDesc.Count << " |";
		LOG(INFO) << "  | SampleQuality                           | " << std::setw(39) << new_desc.SampleDesc.Quality << " |";
		LOG(INFO) << "  | Usage                                   | " << std::setw(39) << new_desc.Usage << " |";
		LOG(INFO) << "  | BindFlags                               | " << std::setw(39) << std::hex << new_desc.BindFlags << std::dec << " |";
		LOG(INFO) << "  | CPUAccessFlags                          | " << std::setw(39) << std::hex << new_desc.CPUAccessFlags << std::dec << " |";
		LOG(INFO) << "  | MiscFlags                               | " << std::setw(39) << std::hex << new_desc.MiscFlags << std::dec << " |";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture3D(const D3D11_TEXTURE3D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture3D **ppTexture3D)
{
	return _orig->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateShaderResourceView(ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc, ID3D11ShaderResourceView **ppSRView)
{
	if (pResource == nullptr) // This can happen if the passed resource failed creation previously, but application did not do error checking to catch that
		return E_INVALIDARG;

	D3D11_SHADER_RESOURCE_VIEW_DESC new_desc =
		pDesc != nullptr ? *pDesc : D3D11_SHADER_RESOURCE_VIEW_DESC { DXGI_FORMAT_UNKNOWN };

	// A view cannot be created with a typeless format (which was set in 'CreateTexture2D'), so fix it
	if (pDesc == nullptr || pDesc->Format == DXGI_FORMAT_UNKNOWN)
	{
		D3D11_TEXTURE2D_DESC texture_desc;
		if (com_ptr<ID3D11Texture2D> texture;
			SUCCEEDED(pResource->QueryInterface(&texture)))
		{
			texture->GetDesc(&texture_desc);

			// Only non-MSAA textures with the depth-stencil bind flag where modified, so skip all others
			if (texture_desc.SampleDesc.Count == 1 &&
				0 != (texture_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL))
			{
				new_desc.Format = make_dxgi_format_normal(texture_desc.Format);

				if (pDesc == nullptr) // Only need to set the rest of the fields if the application did not pass in a valid description already
				{
					new_desc.ViewDimension = texture_desc.ArraySize > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DARRAY : D3D11_SRV_DIMENSION_TEXTURE2D;
					new_desc.Texture2DArray.MipLevels = static_cast<UINT>(-1); // All the mipmap levels from 'MostDetailedMip' on down to least detailed
					new_desc.Texture2DArray.MostDetailedMip = 0;
					new_desc.Texture2DArray.ArraySize = texture_desc.ArraySize;
					new_desc.Texture2DArray.FirstArraySlice = 0;
				}

				pDesc = &new_desc;
			}
		}
	}

	const HRESULT hr = _orig->CreateShaderResourceView(pResource, pDesc, ppSRView);
	if (FAILED(hr))
	{
		LOG(WARN) << "ID3D11Device::CreateShaderResourceView" << " failed with error code " << hr << '.';
		LOG(INFO) << "> Dumping description:";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(INFO) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
		LOG(INFO) << "  | ViewDimension                           | " << std::setw(39) << new_desc.ViewDimension << " |";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateUnorderedAccessView(ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc, ID3D11UnorderedAccessView **ppUAView)
{
	return _orig->CreateUnorderedAccessView(pResource, pDesc, ppUAView);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateRenderTargetView(ID3D11Resource *pResource, const D3D11_RENDER_TARGET_VIEW_DESC *pDesc, ID3D11RenderTargetView **ppRTView)
{
	return _orig->CreateRenderTargetView(pResource, pDesc, ppRTView);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateDepthStencilView(ID3D11Resource *pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D11DepthStencilView **ppDepthStencilView)
{
	if (pResource == nullptr)
		return E_INVALIDARG;

	D3D11_DEPTH_STENCIL_VIEW_DESC new_desc =
		pDesc != nullptr ? *pDesc : D3D11_DEPTH_STENCIL_VIEW_DESC { DXGI_FORMAT_UNKNOWN };

	// A view cannot be created with a typeless format (which was set in 'CreateTexture2D'), so fix it
	if (pDesc == nullptr || pDesc->Format == DXGI_FORMAT_UNKNOWN)
	{
		D3D11_TEXTURE2D_DESC texture_desc;
		if (com_ptr<ID3D11Texture2D> texture;
			SUCCEEDED(pResource->QueryInterface(&texture)))
		{
			texture->GetDesc(&texture_desc);

			// Only non-MSAA textures where modified, so skip all others
			if (texture_desc.SampleDesc.Count == 1)
			{
				assert((texture_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0);

				new_desc.Format = make_dxgi_format_dsv(texture_desc.Format);

				if (pDesc == nullptr) // Only need to set the rest of the fields if the application did not pass in a valid description already
				{
					new_desc.ViewDimension = texture_desc.ArraySize > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DARRAY : D3D11_DSV_DIMENSION_TEXTURE2D;
					new_desc.Texture2DArray.MipSlice = 0;
					new_desc.Texture2DArray.FirstArraySlice = 0;
					new_desc.Texture2DArray.ArraySize = texture_desc.ArraySize;
				}

				pDesc = &new_desc;
			}
		}
	}

	const HRESULT hr = _orig->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
	if (FAILED(hr))
	{
		LOG(WARN) << "ID3D11Device::CreateDepthStencilView" << " failed with error code " << hr << '.';
		LOG(INFO) << "> Dumping description:";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(INFO) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
		LOG(INFO) << "  | ViewDimension                           | " << std::setw(39) << new_desc.ViewDimension << " |";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements, const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout **ppInputLayout)
{
	return _orig->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateVertexShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11VertexShader **ppVertexShader)
{
	return _orig->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateGeometryShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11GeometryShader **ppGeometryShader)
{
	return _orig->CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateGeometryShaderWithStreamOutput(const void *pShaderBytecode, SIZE_T BytecodeLength, const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, const UINT *pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage *pClassLinkage, ID3D11GeometryShader **ppGeometryShader)
{
	return _orig->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreatePixelShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11PixelShader **ppPixelShader)
{
	return _orig->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateHullShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11HullShader **ppHullShader)
{
	return _orig->CreateHullShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateDomainShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11DomainShader **ppDomainShader)
{
	return _orig->CreateDomainShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateComputeShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11ComputeShader **ppComputeShader)
{
	return _orig->CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateClassLinkage(ID3D11ClassLinkage **ppLinkage)
{
	return _orig->CreateClassLinkage(ppLinkage);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateBlendState(const D3D11_BLEND_DESC *pBlendStateDesc, ID3D11BlendState **ppBlendState)
{
	return _orig->CreateBlendState(pBlendStateDesc, ppBlendState);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc, ID3D11DepthStencilState **ppDepthStencilState)
{
	return _orig->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateRasterizerState(const D3D11_RASTERIZER_DESC *pRasterizerDesc, ID3D11RasterizerState **ppRasterizerState)
{
	return _orig->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateSamplerState(const D3D11_SAMPLER_DESC *pSamplerDesc, ID3D11SamplerState **ppSamplerState)
{
	return _orig->CreateSamplerState(pSamplerDesc, ppSamplerState);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateQuery(const D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery)
{
	return _orig->CreateQuery(pQueryDesc, ppQuery);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreatePredicate(const D3D11_QUERY_DESC *pPredicateDesc, ID3D11Predicate **ppPredicate)
{
	return _orig->CreatePredicate(pPredicateDesc, ppPredicate);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateCounter(const D3D11_COUNTER_DESC *pCounterDesc, ID3D11Counter **ppCounter)
{
	return _orig->CreateCounter(pCounterDesc, ppCounter);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeferredContext(UINT ContextFlags, ID3D11DeviceContext **ppDeferredContext)
{
	LOG(INFO) << "Redirecting " << "ID3D11Device::CreateDeferredContext" << '(' << "this = " << this << ", ContextFlags = " << ContextFlags << ", ppDeferredContext = " << ppDeferredContext << ')' << " ...";

	if (ppDeferredContext == nullptr)
		return E_INVALIDARG;

	const HRESULT hr = _orig->CreateDeferredContext(ContextFlags, ppDeferredContext);
	if (FAILED(hr))
	{
		LOG(WARN) << "ID3D11Device::CreateDeferredContext" << " failed with error code " << hr << '.';
		return hr;
	}

	const auto device_context_proxy = new D3D11DeviceContext(this, *ppDeferredContext);
	*ppDeferredContext = device_context_proxy;

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "> Returning ID3D11DeviceContext object " << device_context_proxy << '.';
#endif
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface, void **ppResource)
{
	return _orig->OpenSharedResource(hResource, ReturnedInterface, ppResource);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CheckFormatSupport(DXGI_FORMAT Format, UINT *pFormatSupport)
{
	return _orig->CheckFormatSupport(Format, pFormatSupport);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount, UINT *pNumQualityLevels)
{
	return _orig->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
}
void    STDMETHODCALLTYPE D3D11Device::CheckCounterInfo(D3D11_COUNTER_INFO *pCounterInfo)
{
	_orig->CheckCounterInfo(pCounterInfo);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CheckCounter(const D3D11_COUNTER_DESC *pDesc, D3D11_COUNTER_TYPE *pType, UINT *pActiveCounters, LPSTR szName, UINT *pNameLength, LPSTR szUnits, UINT *pUnitsLength, LPSTR szDescription, UINT *pDescriptionLength)
{
	return _orig->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength, szDescription, pDescriptionLength);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CheckFeatureSupport(D3D11_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize)
{
	return _orig->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}
HRESULT STDMETHODCALLTYPE D3D11Device::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D11Device::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D11Device::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
	return _orig->SetPrivateDataInterface(guid, pData);
}
UINT    STDMETHODCALLTYPE D3D11Device::GetCreationFlags()
{
	return _orig->GetCreationFlags();
}
HRESULT STDMETHODCALLTYPE D3D11Device::GetDeviceRemovedReason()
{
	return _orig->GetDeviceRemovedReason();
}
void    STDMETHODCALLTYPE D3D11Device::GetImmediateContext(ID3D11DeviceContext **ppImmediateContext)
{
	assert(ppImmediateContext != nullptr);

	_immediate_context->AddRef();
	*ppImmediateContext = _immediate_context;
}
HRESULT STDMETHODCALLTYPE D3D11Device::SetExceptionMode(UINT RaiseFlags)
{
	return _orig->SetExceptionMode(RaiseFlags);
}
UINT    STDMETHODCALLTYPE D3D11Device::GetExceptionMode()
{
	return _orig->GetExceptionMode();
}
D3D_FEATURE_LEVEL STDMETHODCALLTYPE D3D11Device::GetFeatureLevel()
{
	return _orig->GetFeatureLevel();
}

void    STDMETHODCALLTYPE D3D11Device::GetImmediateContext1(ID3D11DeviceContext1 **ppImmediateContext)
{
	assert(ppImmediateContext != nullptr);
	assert(_interface_version >= 1);

	// Upgrade immediate context to interface version 1
	_immediate_context->check_and_upgrade_interface(__uuidof(**ppImmediateContext));

	_immediate_context->AddRef();
	*ppImmediateContext = _immediate_context;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeferredContext1(UINT ContextFlags, ID3D11DeviceContext1 **ppDeferredContext)
{
	LOG(INFO) << "Redirecting " << "ID3D11Device1::CreateDeferredContext1" << '(' << "this = " << this << ", ContextFlags = " << ContextFlags << ", ppDeferredContext = " << ppDeferredContext << ')' << " ...";

	if (ppDeferredContext == nullptr)
		return E_INVALIDARG;

	assert(_interface_version >= 1);

	const HRESULT hr = static_cast<ID3D11Device1 *>(_orig)->CreateDeferredContext1(ContextFlags, ppDeferredContext);
	if (FAILED(hr))
	{
		LOG(WARN) << "ID3D11Device1::CreateDeferredContext1" << " failed with error code " << hr << '.';
		return hr;
	}

	const auto device_context_proxy = new D3D11DeviceContext(this, *ppDeferredContext);
	*ppDeferredContext = device_context_proxy;

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "> Returning ID3D11DeviceContext1 object " << device_context_proxy << '.';
#endif
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateBlendState1(const D3D11_BLEND_DESC1 *pBlendStateDesc, ID3D11BlendState1 **ppBlendState)
{
	assert(_interface_version >= 1);
	return static_cast<ID3D11Device1 *>(_orig)->CreateBlendState1(pBlendStateDesc, ppBlendState);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateRasterizerState1(const D3D11_RASTERIZER_DESC1 *pRasterizerDesc, ID3D11RasterizerState1 **ppRasterizerState)
{
	assert(_interface_version >= 1);
	return static_cast<ID3D11Device1 *>(_orig)->CreateRasterizerState1(pRasterizerDesc, ppRasterizerState);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeviceContextState(UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, REFIID EmulatedInterface, D3D_FEATURE_LEVEL *pChosenFeatureLevel, ID3DDeviceContextState **ppContextState)
{
	assert(_interface_version >= 1);
	return static_cast<ID3D11Device1 *>(_orig)->CreateDeviceContextState(Flags, pFeatureLevels, FeatureLevels, SDKVersion, EmulatedInterface, pChosenFeatureLevel, ppContextState);
}
HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedResource1(HANDLE hResource, REFIID returnedInterface, void **ppResource)
{
	assert(_interface_version >= 1);
	return static_cast<ID3D11Device1 *>(_orig)->OpenSharedResource1(hResource, returnedInterface, ppResource);
}
HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedResourceByName(LPCWSTR lpName, DWORD dwDesiredAccess, REFIID returnedInterface, void **ppResource)
{
	assert(_interface_version >= 1);
	return static_cast<ID3D11Device1 *>(_orig)->OpenSharedResourceByName(lpName, dwDesiredAccess, returnedInterface, ppResource);
}

void    STDMETHODCALLTYPE D3D11Device::GetImmediateContext2(ID3D11DeviceContext2 **ppImmediateContext)
{
	assert(ppImmediateContext != nullptr);
	assert(_interface_version >= 2);

	// Upgrade immediate context to interface version 2
	_immediate_context->check_and_upgrade_interface(__uuidof(**ppImmediateContext));

	_immediate_context->AddRef();
	*ppImmediateContext = _immediate_context;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeferredContext2(UINT ContextFlags, ID3D11DeviceContext2 **ppDeferredContext)
{
	LOG(INFO) << "Redirecting " << "ID3D11Device2::CreateDeferredContext2" << '(' << "this = " << this << ", ContextFlags = " << ContextFlags << ", ppDeferredContext = " << ppDeferredContext << ')' << " ...";

	if (ppDeferredContext == nullptr)
		return E_INVALIDARG;

	assert(_interface_version >= 2);

	const HRESULT hr = static_cast<ID3D11Device2 *>(_orig)->CreateDeferredContext2(ContextFlags, ppDeferredContext);
	if (FAILED(hr))
	{
		LOG(WARN) << "ID3D11Device1::CreateDeferredContext2" << " failed with error code " << hr << '.';
		return hr;
	}

	const auto device_context_proxy = new D3D11DeviceContext(this, *ppDeferredContext);
	*ppDeferredContext = device_context_proxy;

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "> Returning ID3D11DeviceContext2 object " << device_context_proxy << '.';
#endif
	return hr;
}
void    STDMETHODCALLTYPE D3D11Device::GetResourceTiling(ID3D11Resource *pTiledResource, UINT *pNumTilesForEntireResource, D3D11_PACKED_MIP_DESC *pPackedMipDesc, D3D11_TILE_SHAPE *pStandardTileShapeForNonPackedMips, UINT *pNumSubresourceTilings, UINT FirstSubresourceTilingToGet, D3D11_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips)
{
	assert(_interface_version >= 2);
	static_cast<ID3D11Device2 *>(_orig)->GetResourceTiling(pTiledResource, pNumTilesForEntireResource, pPackedMipDesc, pStandardTileShapeForNonPackedMips, pNumSubresourceTilings, FirstSubresourceTilingToGet, pSubresourceTilingsForNonPackedMips);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CheckMultisampleQualityLevels1(DXGI_FORMAT Format, UINT SampleCount, UINT Flags, UINT *pNumQualityLevels)
{
	assert(_interface_version >= 2);
	return static_cast<ID3D11Device2 *>(_orig)->CheckMultisampleQualityLevels1(Format, SampleCount, Flags, pNumQualityLevels);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture2D1(const D3D11_TEXTURE2D_DESC1 *pDesc1, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture2D1 **ppTexture2D)
{
	assert(pDesc1 != nullptr);

	D3D11_TEXTURE2D_DESC1 new_desc = *pDesc1;
	if (new_desc.SampleDesc.Count == 1 &&
		0 != (new_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) &&
		0 == (new_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE))
	{
		new_desc.Format = make_dxgi_format_typeless(new_desc.Format);
		new_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
	}

	assert(_interface_version >= 3);
	const HRESULT hr = static_cast<ID3D11Device3 *>(_orig)->CreateTexture2D1(&new_desc, pInitialData, ppTexture2D);
	if (FAILED(hr))
	{
		LOG(WARN) << "ID3D11Device3::CreateTexture2D1" << " failed with error code " << hr << '.';
		LOG(INFO) << "> Dumping description:";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(INFO) << "  | Width                                   | " << std::setw(39) << new_desc.Width << " |";
		LOG(INFO) << "  | Height                                  | " << std::setw(39) << new_desc.Height << " |";
		LOG(INFO) << "  | MipLevels                               | " << std::setw(39) << new_desc.MipLevels << " |";
		LOG(INFO) << "  | ArraySize                               | " << std::setw(39) << new_desc.ArraySize << " |";
		LOG(INFO) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
		LOG(INFO) << "  | SampleCount                             | " << std::setw(39) << new_desc.SampleDesc.Count << " |";
		LOG(INFO) << "  | SampleQuality                           | " << std::setw(39) << new_desc.SampleDesc.Quality << " |";
		LOG(INFO) << "  | Usage                                   | " << std::setw(39) << new_desc.Usage << " |";
		LOG(INFO) << "  | BindFlags                               | " << std::setw(39) << std::hex << new_desc.BindFlags << std::dec << " |";
		LOG(INFO) << "  | CPUAccessFlags                          | " << std::setw(39) << std::hex << new_desc.CPUAccessFlags << std::dec << " |";
		LOG(INFO) << "  | MiscFlags                               | " << std::setw(39) << std::hex << new_desc.MiscFlags << std::dec << " |";
		LOG(INFO) << "  | TextureLayout                           | " << std::setw(39) << new_desc.TextureLayout << " |";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture3D1(const D3D11_TEXTURE3D_DESC1 *pDesc1, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture3D1 **ppTexture3D)
{
	assert(_interface_version >= 3);
	return static_cast<ID3D11Device3 *>(_orig)->CreateTexture3D1(pDesc1, pInitialData, ppTexture3D);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateRasterizerState2(const D3D11_RASTERIZER_DESC2 *pRasterizerDesc, ID3D11RasterizerState2 **ppRasterizerState)
{
	assert(_interface_version >= 3);
	return static_cast<ID3D11Device3 *>(_orig)->CreateRasterizerState2(pRasterizerDesc, ppRasterizerState);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateShaderResourceView1(ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc1, ID3D11ShaderResourceView1 **ppSRView1)
{
	if (pResource == nullptr)
		return E_INVALIDARG;

	D3D11_SHADER_RESOURCE_VIEW_DESC1 new_desc =
		pDesc1 != nullptr ? *pDesc1 : D3D11_SHADER_RESOURCE_VIEW_DESC1 { DXGI_FORMAT_UNKNOWN };

	if (pDesc1 == nullptr || pDesc1->Format == DXGI_FORMAT_UNKNOWN)
	{
		D3D11_TEXTURE2D_DESC texture_desc;
		if (com_ptr<ID3D11Texture2D> texture;
			SUCCEEDED(pResource->QueryInterface(&texture)))
		{
			texture->GetDesc(&texture_desc);

			if (texture_desc.SampleDesc.Count == 1 &&
				0 != (texture_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL))
			{
				new_desc.Format = make_dxgi_format_normal(texture_desc.Format);

				if (pDesc1 == nullptr)
				{
					if (texture_desc.ArraySize > 1)
					{
						new_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
						new_desc.Texture2DArray.MipLevels = static_cast<UINT>(-1);
						new_desc.Texture2DArray.MostDetailedMip = 0;
						new_desc.Texture2DArray.ArraySize = texture_desc.ArraySize;
						new_desc.Texture2DArray.FirstArraySlice = 0;
						new_desc.Texture2DArray.PlaneSlice = 0;
					}
					else
					{
						new_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
						new_desc.Texture2D.MipLevels = static_cast<UINT>(-1);
						new_desc.Texture2D.MostDetailedMip = 0;
						new_desc.Texture2D.PlaneSlice = 0;
					}
				}

				pDesc1 = &new_desc;
			}
		}
	}

	assert(_interface_version >= 3);
	const HRESULT hr = static_cast<ID3D11Device3 *>(_orig)->CreateShaderResourceView1(pResource, pDesc1, ppSRView1);
	if (FAILED(hr))
	{
		LOG(WARN) << "ID3D11Device3::CreateShaderResourceView1" << " failed with error code " << hr << '.';
		LOG(INFO) << "> Dumping description:";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(INFO) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
		LOG(INFO) << "  | ViewDimension                           | " << std::setw(39) << new_desc.ViewDimension << " |";
		LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateUnorderedAccessView1(ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc1, ID3D11UnorderedAccessView1 **ppUAView1)
{
	assert(_interface_version >= 3);
	return static_cast<ID3D11Device3 *>(_orig)->CreateUnorderedAccessView1(pResource, pDesc1, ppUAView1);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateRenderTargetView1(ID3D11Resource *pResource, const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc1, ID3D11RenderTargetView1 **ppRTView1)
{
	assert(_interface_version >= 3);
	return static_cast<ID3D11Device3 *>(_orig)->CreateRenderTargetView1(pResource, pDesc1, ppRTView1);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateQuery1(const D3D11_QUERY_DESC1 *pQueryDesc1, ID3D11Query1 **ppQuery1)
{
	assert(_interface_version >= 3);
	return static_cast<ID3D11Device3 *>(_orig)->CreateQuery1(pQueryDesc1, ppQuery1);
}
void    STDMETHODCALLTYPE D3D11Device::GetImmediateContext3(ID3D11DeviceContext3 **ppImmediateContext)
{
	assert(_interface_version >= 3);
	static_cast<ID3D11Device3 *>(_orig)->GetImmediateContext3(ppImmediateContext);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeferredContext3(UINT ContextFlags, ID3D11DeviceContext3 **ppDeferredContext)
{
	LOG(INFO) << "Redirecting " << "ID3D11Device3::CreateDeferredContext3" << '(' << "this = " << this << ", ContextFlags = " << ContextFlags << ", ppDeferredContext = " << ppDeferredContext << ')' << " ...";

	if (ppDeferredContext == nullptr)
		return E_INVALIDARG;

	assert(_interface_version >= 3);

	const HRESULT hr = static_cast<ID3D11Device3 *>(_orig)->CreateDeferredContext3(ContextFlags, ppDeferredContext);
	if (FAILED(hr))
	{
		LOG(WARN) << "ID3D11Device1::CreateDeferredContext3" << " failed with error code " << hr << '.';
		return hr;
	}

	const auto device_context_proxy = new D3D11DeviceContext(this, *ppDeferredContext);
	*ppDeferredContext = device_context_proxy;

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "> Returning ID3D11DeviceContext3 object " << device_context_proxy << '.';
#endif
	return hr;
}
void    STDMETHODCALLTYPE D3D11Device::WriteToSubresource(ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
{
	assert(_interface_version >= 3);
	static_cast<ID3D11Device3 *>(_orig)->WriteToSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
}
void    STDMETHODCALLTYPE D3D11Device::ReadFromSubresource(void *pDstData, UINT DstRowPitch, UINT DstDepthPitch, ID3D11Resource *pSrcResource, UINT SrcSubresource, const D3D11_BOX *pSrcBox)
{
	assert(_interface_version >= 3);

	static_cast<ID3D11Device3 *>(_orig)->ReadFromSubresource(pDstData, DstRowPitch, DstDepthPitch, pSrcResource, SrcSubresource, pSrcBox);
}

HRESULT STDMETHODCALLTYPE D3D11Device::RegisterDeviceRemovedEvent(HANDLE hEvent, DWORD *pdwCookie)
{
	assert(_interface_version >= 4);
	return static_cast<ID3D11Device4 *>(_orig)->RegisterDeviceRemovedEvent(hEvent, pdwCookie);
}
void    STDMETHODCALLTYPE D3D11Device::UnregisterDeviceRemoved(DWORD dwCookie)
{
	assert(_interface_version >= 4);
	static_cast<ID3D11Device4 *>(_orig)->UnregisterDeviceRemoved(dwCookie);
}

HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedFence(HANDLE hFence, REFIID ReturnedInterface, void **ppFence)
{
	assert(_interface_version >= 5);
	return static_cast<ID3D11Device5 *>(_orig)->OpenSharedFence(hFence, ReturnedInterface, ppFence);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateFence(UINT64 InitialValue, D3D11_FENCE_FLAG Flags, REFIID ReturnedInterface, void **ppFence)
{
	assert(_interface_version >= 5);
	return static_cast<ID3D11Device5 *>(_orig)->CreateFence(InitialValue, Flags, ReturnedInterface, ppFence);
}
