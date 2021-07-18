/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "d3d11_device.hpp"
#include "d3d11_device_context.hpp"
#include "dxgi/dxgi_device.hpp"
#include "d3d11_impl_type_convert.hpp"

D3D11Device::D3D11Device(IDXGIDevice1 *dxgi_device, ID3D11Device *original) :
	device_impl(original),
	_dxgi_device(new DXGIDevice(dxgi_device, this))
{
	assert(_orig != nullptr);

	// Add proxy object to the private data of the device, so that it can be retrieved again when only the original device is available (as is the case in the OpenVR hooks)
	D3D11Device *const device_proxy = this;
	_orig->SetPrivateData(__uuidof(D3D11Device), sizeof(device_proxy), &device_proxy);
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

	const auto orig = _orig;
	const auto interface_version = _interface_version;
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroying " << "ID3D11Device" << interface_version << " object " << this << " (" << orig << ").";
#endif
	delete this;

	// Note: At this point the immediate context should have been deleted by the release above (so do not access it)
	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		LOG(WARN) << "Reference count for " << "ID3D11Device" << interface_version << " object " << this << " (" << orig << ") is inconsistent (" << ref_orig << ").";
	return 0;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateBuffer(const D3D11_BUFFER_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Buffer **ppBuffer)
{
#if RESHADE_ADDON
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppBuffer == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateBuffer(pDesc, pInitialData, nullptr);

	D3D11_BUFFER_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d11::convert_resource_desc(internal_desc);

	std::vector<reshade::api::subresource_data> initial_data;
	if (pInitialData != nullptr)
	{
		static_assert(sizeof(*pInitialData) == sizeof(reshade::api::subresource_data));

		initial_data.resize(1);
		initial_data[0] = *reinterpret_cast<const reshade::api::subresource_data *>(pInitialData);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, initial_data.data(), reshade::api::resource_usage::general))
	{
		reshade::d3d11::convert_resource_desc(internal_desc);
		pDesc = &internal_desc;
		pInitialData = reinterpret_cast<const D3D11_SUBRESOURCE_DATA *>(&initial_data);
	}
#endif

	const HRESULT hr = _orig->CreateBuffer(pDesc, pInitialData, ppBuffer);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this, desc, reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::general, reshade::api::resource { reinterpret_cast<uintptr_t>(*ppBuffer) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_resource, reshade::api::resource>(this, *ppBuffer);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateBuffer" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture1D(const D3D11_TEXTURE1D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture1D **ppTexture1D)
{
#if RESHADE_ADDON
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppTexture1D == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateTexture1D(pDesc, pInitialData, nullptr);

	D3D11_TEXTURE1D_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d11::convert_resource_desc(internal_desc);

	std::vector<reshade::api::subresource_data> initial_data;
	if (pInitialData != nullptr)
	{
		// Allocate sufficient space in the array, in case an add-on changes the texture description, but wants to upload initial data still
		initial_data.resize(D3D11_REQ_MIP_LEVELS * D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION);
		for (UINT i = 0; i < pDesc->MipLevels * pDesc->ArraySize; ++i)
			initial_data[i] = *reinterpret_cast<const reshade::api::subresource_data *>(pInitialData + i);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, initial_data.data(), reshade::api::resource_usage::general))
	{
		reshade::d3d11::convert_resource_desc(internal_desc);
		pDesc = &internal_desc;
		pInitialData = reinterpret_cast<const D3D11_SUBRESOURCE_DATA *>(initial_data.data());
	}
#endif

	const HRESULT hr = _orig->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this, desc, reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::general, reshade::api::resource { reinterpret_cast<uintptr_t>(*ppTexture1D) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_resource, reshade::api::resource>(this, *ppTexture1D);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateTexture1D" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture2D(const D3D11_TEXTURE2D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture2D **ppTexture2D)
{
#if RESHADE_ADDON
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppTexture2D == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateTexture2D(pDesc, pInitialData, nullptr);

	D3D11_TEXTURE2D_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d11::convert_resource_desc(internal_desc);

	std::vector<reshade::api::subresource_data> initial_data;
	if (pInitialData != nullptr)
	{
		initial_data.resize(D3D11_REQ_MIP_LEVELS * D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION);
		for (UINT i = 0; i < pDesc->MipLevels * pDesc->ArraySize; ++i)
			initial_data[i] = *reinterpret_cast<const reshade::api::subresource_data *>(pInitialData + i);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, initial_data.data(), reshade::api::resource_usage::general))
	{
		reshade::d3d11::convert_resource_desc(internal_desc);
		pDesc = &internal_desc;
		pInitialData = reinterpret_cast<const D3D11_SUBRESOURCE_DATA *>(initial_data.data());
	}
#endif

	const HRESULT hr = _orig->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this, desc, reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::general, reshade::api::resource { reinterpret_cast<uintptr_t>(*ppTexture2D) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_resource, reshade::api::resource>(this, *ppTexture2D);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateTexture2D" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture3D(const D3D11_TEXTURE3D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture3D **ppTexture3D)
{
#if RESHADE_ADDON
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppTexture3D == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateTexture3D(pDesc, pInitialData, nullptr);

	D3D11_TEXTURE3D_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d11::convert_resource_desc(internal_desc);

	std::vector<reshade::api::subresource_data> initial_data;
	if (pInitialData != nullptr)
	{
		initial_data.resize(D3D11_REQ_MIP_LEVELS);
		for (UINT i = 0; i < pDesc->MipLevels; ++i)
			initial_data[i] = *reinterpret_cast<const reshade::api::subresource_data *>(pInitialData + i);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, initial_data.data(), reshade::api::resource_usage::general))
	{
		reshade::d3d11::convert_resource_desc(internal_desc);
		pDesc = &internal_desc;
		pInitialData = reinterpret_cast<const D3D11_SUBRESOURCE_DATA *>(initial_data.data());
	}
#endif

	const HRESULT hr = _orig->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this, desc, reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::general, reshade::api::resource { reinterpret_cast<uintptr_t>(*ppTexture3D) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_resource, reshade::api::resource>(this, *ppTexture3D);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateTexture3D" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateShaderResourceView(ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc, ID3D11ShaderResourceView **ppShaderResourceView)
{
#if RESHADE_ADDON
	if (pResource == nullptr) // This can happen if the passed resource failed creation previously, but application did not do error checking to catch that
		return E_INVALIDARG;
	if (ppShaderResourceView == nullptr) // This can happen when application only wants to validate input parameter
		return _orig->CreateShaderResourceView(pResource, pDesc, nullptr);

	D3D11_SHADER_RESOURCE_VIEW_DESC internal_desc = pDesc != nullptr ? *pDesc : D3D11_SHADER_RESOURCE_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D11_SRV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d11::convert_resource_view_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::shader_resource, desc))
	{
		reshade::d3d11::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateShaderResourceView(pResource, pDesc, ppShaderResourceView);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::shader_resource, desc, reshade::api::resource_view { reinterpret_cast<uintptr_t>(*ppShaderResourceView) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_resource_view, reshade::api::resource_view>(this, *ppShaderResourceView);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateShaderResourceView" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateUnorderedAccessView(ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc, ID3D11UnorderedAccessView **ppUnorderedAccessView)
{
#if RESHADE_ADDON
	if (pResource == nullptr)
		return E_INVALIDARG;
	if (ppUnorderedAccessView == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateUnorderedAccessView(pResource, pDesc, nullptr);

	D3D11_UNORDERED_ACCESS_VIEW_DESC internal_desc = pDesc != nullptr ? *pDesc : D3D11_UNORDERED_ACCESS_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D11_UAV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d11::convert_resource_view_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::unordered_access, desc))
	{
		reshade::d3d11::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateUnorderedAccessView(pResource, pDesc, ppUnorderedAccessView);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::unordered_access, desc, reshade::api::resource_view { reinterpret_cast<uintptr_t>(*ppUnorderedAccessView) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_resource_view, reshade::api::resource_view>(this, *ppUnorderedAccessView);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateUnorderedAccessView" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateRenderTargetView(ID3D11Resource *pResource, const D3D11_RENDER_TARGET_VIEW_DESC *pDesc, ID3D11RenderTargetView **ppRenderTargetView)
{
#if RESHADE_ADDON
	if (pResource == nullptr)
		return E_INVALIDARG;
	if (ppRenderTargetView == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateRenderTargetView(pResource, pDesc, nullptr);

	D3D11_RENDER_TARGET_VIEW_DESC internal_desc = pDesc != nullptr ? *pDesc : D3D11_RENDER_TARGET_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D11_RTV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d11::convert_resource_view_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::render_target, desc))
	{
		reshade::d3d11::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateRenderTargetView(pResource, pDesc, ppRenderTargetView);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::render_target, desc, reshade::api::resource_view { reinterpret_cast<uintptr_t>(*ppRenderTargetView) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_resource_view, reshade::api::resource_view>(this, *ppRenderTargetView);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateRenderTargetView" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateDepthStencilView(ID3D11Resource *pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D11DepthStencilView **ppDepthStencilView)
{
#if RESHADE_ADDON
	if (pResource == nullptr)
		return E_INVALIDARG;
	if (ppDepthStencilView == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateDepthStencilView(pResource, pDesc, nullptr);

	D3D11_DEPTH_STENCIL_VIEW_DESC internal_desc = pDesc != nullptr ? *pDesc : D3D11_DEPTH_STENCIL_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D11_DSV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d11::convert_resource_view_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::depth_stencil, desc))
	{
		reshade::d3d11::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::depth_stencil, desc, reshade::api::resource_view { reinterpret_cast<uintptr_t>(*ppDepthStencilView) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_resource_view, reshade::api::resource_view>(this, *ppDepthStencilView);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateDepthStencilView" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements, const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout **ppInputLayout)
{
#if RESHADE_ADDON
	if (ppInputLayout == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, nullptr);

	reshade::api::pipeline_desc desc = reshade::d3d11::convert_pipeline_desc(pInputElementDescs, NumElements);
	desc.graphics.vertex_shader.code = pShaderBytecodeWithInputSignature;
	desc.graphics.vertex_shader.code_size = BytecodeLength;

	std::vector<D3D11_INPUT_ELEMENT_DESC> internal_elements;

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		reshade::d3d11::convert_pipeline_desc(desc, internal_elements);
		pInputElementDescs = internal_elements.data();
		NumElements = static_cast<UINT>(internal_elements.size());
		pShaderBytecodeWithInputSignature = desc.graphics.vertex_shader.code;
		BytecodeLength = desc.graphics.vertex_shader.code_size;
	}
#endif

	const HRESULT hr = _orig->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppInputLayout) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_pipeline, reshade::api::pipeline>(this, *ppInputLayout);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateInputLayout" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateVertexShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11VertexShader **ppVertexShader)
{
#if RESHADE_ADDON
	if (ppVertexShader == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, nullptr);

	reshade::api::pipeline_desc desc = { reshade::api::pipeline_stage::vertex_shader };
	desc.graphics.vertex_shader.code = pShaderBytecode;
	desc.graphics.vertex_shader.code_size = BytecodeLength;
	desc.graphics.vertex_shader.format = reshade::api::shader_format::dxbc;

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		assert(desc.graphics.vertex_shader.format == reshade::api::shader_format::dxbc);
		pShaderBytecode = desc.graphics.vertex_shader.code;
		BytecodeLength  = desc.graphics.vertex_shader.code_size;
	}
#endif

	const HRESULT hr = _orig->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppVertexShader) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_pipeline, reshade::api::pipeline>(this, *ppVertexShader);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateVertexShader" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateGeometryShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11GeometryShader **ppGeometryShader)
{
#if RESHADE_ADDON
	if (ppGeometryShader == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, nullptr);

	reshade::api::pipeline_desc desc = { reshade::api::pipeline_stage::geometry_shader };
	desc.graphics.geometry_shader.code = pShaderBytecode;
	desc.graphics.geometry_shader.code_size = BytecodeLength;
	desc.graphics.geometry_shader.format = reshade::api::shader_format::dxbc;

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		assert(desc.graphics.geometry_shader.format == reshade::api::shader_format::dxbc);
		pShaderBytecode = desc.graphics.geometry_shader.code;
		BytecodeLength  = desc.graphics.geometry_shader.code_size;
	}
#endif

	const HRESULT hr = _orig->CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppGeometryShader) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_pipeline, reshade::api::pipeline>(this, *ppGeometryShader);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateGeometryShader" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateGeometryShaderWithStreamOutput(const void *pShaderBytecode, SIZE_T BytecodeLength, const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, const UINT *pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage *pClassLinkage, ID3D11GeometryShader **ppGeometryShader)
{
#if RESHADE_ADDON
	if (ppGeometryShader == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, nullptr);

	reshade::api::pipeline_desc desc = { reshade::api::pipeline_stage::geometry_shader };
	desc.graphics.geometry_shader.code = pShaderBytecode;
	desc.graphics.geometry_shader.code_size = BytecodeLength;
	desc.graphics.geometry_shader.format = reshade::api::shader_format::dxbc;

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		assert(desc.graphics.vertex_shader.format == reshade::api::shader_format::dxbc);
		pShaderBytecode = desc.graphics.geometry_shader.code;
		BytecodeLength  = desc.graphics.geometry_shader.code_size;
	}
#endif

	const HRESULT hr = _orig->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppGeometryShader) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_pipeline, reshade::api::pipeline>(this, *ppGeometryShader);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateGeometryShaderWithStreamOutput" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreatePixelShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11PixelShader **ppPixelShader)
{
#if RESHADE_ADDON
	if (ppPixelShader == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, nullptr);

	reshade::api::pipeline_desc desc = { reshade::api::pipeline_stage::pixel_shader };
	desc.graphics.pixel_shader.code = pShaderBytecode;
	desc.graphics.pixel_shader.code_size = BytecodeLength;
	desc.graphics.pixel_shader.format = reshade::api::shader_format::dxbc;

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		assert(desc.graphics.pixel_shader.format == reshade::api::shader_format::dxbc);
		pShaderBytecode = desc.graphics.pixel_shader.code;
		BytecodeLength  = desc.graphics.pixel_shader.code_size;
	}
#endif

	const HRESULT hr = _orig->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppPixelShader) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_pipeline, reshade::api::pipeline>(this, *ppPixelShader);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreatePixelShader" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateHullShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11HullShader **ppHullShader)
{
#if RESHADE_ADDON
	if (ppHullShader == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateHullShader(pShaderBytecode, BytecodeLength, pClassLinkage, nullptr);

	reshade::api::pipeline_desc desc = { reshade::api::pipeline_stage::hull_shader };
	desc.graphics.hull_shader.code = pShaderBytecode;
	desc.graphics.hull_shader.code_size = BytecodeLength;
	desc.graphics.hull_shader.format = reshade::api::shader_format::dxbc;

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		assert(desc.graphics.hull_shader.format == reshade::api::shader_format::dxbc);
		pShaderBytecode = desc.graphics.hull_shader.code;
		BytecodeLength  = desc.graphics.hull_shader.code_size;
	}
#endif

	const HRESULT hr = _orig->CreateHullShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppHullShader) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_pipeline, reshade::api::pipeline>(this, *ppHullShader);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateHullShader" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateDomainShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11DomainShader **ppDomainShader)
{
#if RESHADE_ADDON
	if (ppDomainShader == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateDomainShader(pShaderBytecode, BytecodeLength, pClassLinkage, nullptr);

	reshade::api::pipeline_desc desc = { reshade::api::pipeline_stage::domain_shader };
	desc.graphics.domain_shader.code = pShaderBytecode;
	desc.graphics.domain_shader.code_size = BytecodeLength;
	desc.graphics.domain_shader.format = reshade::api::shader_format::dxbc;

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		assert(desc.graphics.domain_shader.format == reshade::api::shader_format::dxbc);
		pShaderBytecode = desc.graphics.domain_shader.code;
		BytecodeLength  = desc.graphics.domain_shader.code_size;
	}
#endif

	const HRESULT hr = _orig->CreateDomainShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppDomainShader) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_pipeline, reshade::api::pipeline>(this, *ppDomainShader);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateDomainShader" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateComputeShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11ComputeShader **ppComputeShader)
{
#if RESHADE_ADDON
	if (ppComputeShader == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, nullptr);

	reshade::api::pipeline_desc desc = { reshade::api::pipeline_stage::compute_shader };
	desc.compute.shader.code = pShaderBytecode;
	desc.compute.shader.code_size = BytecodeLength;
	desc.compute.shader.format = reshade::api::shader_format::dxbc;

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		assert(desc.compute.shader.format == reshade::api::shader_format::dxbc);
		pShaderBytecode = desc.compute.shader.code;
		BytecodeLength  = desc.compute.shader.code_size;
	}
#endif

	const HRESULT hr = _orig->CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppComputeShader) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_pipeline, reshade::api::pipeline>(this, *ppComputeShader);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateComputeShader" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateClassLinkage(ID3D11ClassLinkage **ppLinkage)
{
	return _orig->CreateClassLinkage(ppLinkage);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateBlendState(const D3D11_BLEND_DESC *pBlendStateDesc, ID3D11BlendState **ppBlendState)
{
#if RESHADE_ADDON
	if (ppBlendState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateBlendState(pBlendStateDesc, nullptr);

	D3D11_BLEND_DESC internal_desc = {};
	auto desc = reshade::d3d11::convert_pipeline_desc(pBlendStateDesc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		reshade::d3d11::convert_pipeline_desc(desc, internal_desc);
		pBlendStateDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateBlendState(pBlendStateDesc, ppBlendState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppBlendState) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_pipeline, reshade::api::pipeline>(this, *ppBlendState);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateBlendState" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc, ID3D11DepthStencilState **ppDepthStencilState)
{
#if RESHADE_ADDON
	if (ppDepthStencilState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateDepthStencilState(pDepthStencilDesc, nullptr);

	D3D11_DEPTH_STENCIL_DESC internal_desc;
	auto desc = reshade::d3d11::convert_pipeline_desc(pDepthStencilDesc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		reshade::d3d11::convert_pipeline_desc(desc, internal_desc);
		pDepthStencilDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppDepthStencilState) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_pipeline, reshade::api::pipeline>(this, *ppDepthStencilState);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateDepthStencilState" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateRasterizerState(const D3D11_RASTERIZER_DESC *pRasterizerDesc, ID3D11RasterizerState **ppRasterizerState)
{
#if RESHADE_ADDON
	if (ppRasterizerState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateRasterizerState(pRasterizerDesc, nullptr);

	D3D11_RASTERIZER_DESC internal_desc = {};
	auto desc = reshade::d3d11::convert_pipeline_desc(pRasterizerDesc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		reshade::d3d11::convert_pipeline_desc(desc, internal_desc);
		pRasterizerDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppRasterizerState) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_pipeline, reshade::api::pipeline>(this, *ppRasterizerState);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateRasterizerState" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateSamplerState(const D3D11_SAMPLER_DESC *pSamplerDesc, ID3D11SamplerState **ppSamplerState)
{
#if RESHADE_ADDON
	if (pSamplerDesc == nullptr)
		return E_INVALIDARG;
	if (ppSamplerState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateSamplerState(pSamplerDesc, nullptr);

	D3D11_SAMPLER_DESC internal_desc = *pSamplerDesc;
	auto desc = reshade::d3d11::convert_sampler_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_sampler>(this, desc))
	{
		reshade::d3d11::convert_sampler_desc(desc, internal_desc);
		pSamplerDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateSamplerState(pSamplerDesc, ppSamplerState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_sampler>(this, desc, reshade::api::sampler { reinterpret_cast<uintptr_t>(*ppSamplerState) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_sampler, reshade::api::sampler>(this, *ppSamplerState);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::CreateSamplerState" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
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
	if (SUCCEEDED(hr))
	{
		const auto device_context_proxy = new D3D11DeviceContext(this, *ppDeferredContext);
		*ppDeferredContext = device_context_proxy;

#if RESHADE_VERBOSE_LOG
		LOG(INFO) << "> Returning ID3D11DeviceContext object " << device_context_proxy << '.';
#endif
	}
	else
	{
		LOG(WARN) << "ID3D11Device::CreateDeferredContext" << " failed with error code " << hr << '.';
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface, void **ppResource)
{
	const HRESULT hr = _orig->OpenSharedResource(hResource, ReturnedInterface, ppResource);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		const auto resource = static_cast<ID3D11Resource *>(*ppResource);
		reshade::api::resource_desc desc;

		if (com_ptr<ID3D11Buffer> resource_impl;
			SUCCEEDED(resource->QueryInterface(&resource_impl)))
		{
			D3D11_BUFFER_DESC internal_desc;
			resource_impl->GetDesc(&internal_desc);
			desc = reshade::d3d11::convert_resource_desc(internal_desc);
		}
		if (com_ptr<ID3D11Texture1D> resource_impl;
			SUCCEEDED(resource->QueryInterface(&resource_impl)))
		{
			D3D11_TEXTURE1D_DESC internal_desc;
			resource_impl->GetDesc(&internal_desc);
			desc = reshade::d3d11::convert_resource_desc(internal_desc);
		}
		if (com_ptr<ID3D11Texture2D> resource_impl;
			SUCCEEDED(resource->QueryInterface(&resource_impl)))
		{
			D3D11_TEXTURE2D_DESC internal_desc;
			resource_impl->GetDesc(&internal_desc);
			desc = reshade::d3d11::convert_resource_desc(internal_desc);
		}
		if (com_ptr<ID3D11Texture3D> resource_impl;
			SUCCEEDED(resource->QueryInterface(&resource_impl)))
		{
			D3D11_TEXTURE3D_DESC internal_desc;
			resource_impl->GetDesc(&internal_desc);
			desc = reshade::d3d11::convert_resource_desc(internal_desc);
		}

		assert((desc.flags & reshade::api::resource_flags::shared) == reshade::api::resource_flags::shared);

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, reshade::api::resource_usage::general, reshade::api::resource { reinterpret_cast<uintptr_t>(resource) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_resource, reshade::api::resource>(this, resource);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device::OpenSharedResource" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
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
	if (SUCCEEDED(hr))
	{
		const auto device_context_proxy = new D3D11DeviceContext(this, *ppDeferredContext);
		*ppDeferredContext = device_context_proxy;

#if RESHADE_VERBOSE_LOG
		LOG(INFO) << "> Returning ID3D11DeviceContext1 object " << device_context_proxy << '.';
#endif
	}
	else
	{
		LOG(WARN) << "ID3D11Device1::CreateDeferredContext1" << " failed with error code " << hr << '.';
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateBlendState1(const D3D11_BLEND_DESC1 *pBlendStateDesc, ID3D11BlendState1 **ppBlendState)
{
#if RESHADE_ADDON
	if (ppBlendState == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D11Device1 *>(_orig)->CreateBlendState1(pBlendStateDesc, nullptr);

	D3D11_BLEND_DESC1 internal_desc = {};
	auto desc = reshade::d3d11::convert_pipeline_desc(pBlendStateDesc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		reshade::d3d11::convert_pipeline_desc(desc, internal_desc);
		pBlendStateDesc = &internal_desc;
	}
#endif

	assert(_interface_version >= 1);
	const HRESULT hr = static_cast<ID3D11Device1 *>(_orig)->CreateBlendState1(pBlendStateDesc, ppBlendState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppBlendState) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_pipeline, reshade::api::pipeline>(this, *ppBlendState);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device1::CreateBlendState1" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateRasterizerState1(const D3D11_RASTERIZER_DESC1 *pRasterizerDesc, ID3D11RasterizerState1 **ppRasterizerState)
{
#if RESHADE_ADDON
	if (ppRasterizerState == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D11Device1 *>(_orig)->CreateRasterizerState1(pRasterizerDesc, nullptr);

	D3D11_RASTERIZER_DESC1 internal_desc = {};
	auto desc = reshade::d3d11::convert_pipeline_desc(pRasterizerDesc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		reshade::d3d11::convert_pipeline_desc(desc, internal_desc);
		pRasterizerDesc = &internal_desc;
	}
#endif

	assert(_interface_version >= 1);
	const HRESULT hr = static_cast<ID3D11Device1 *>(_orig)->CreateRasterizerState1(pRasterizerDesc, ppRasterizerState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppRasterizerState) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_pipeline, reshade::api::pipeline>(this, *ppRasterizerState);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device1::CreateRasterizerState1" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
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
	if (SUCCEEDED(hr))
	{
		const auto device_context_proxy = new D3D11DeviceContext(this, *ppDeferredContext);
		*ppDeferredContext = device_context_proxy;

#if RESHADE_VERBOSE_LOG
		LOG(INFO) << "> Returning ID3D11DeviceContext2 object " << device_context_proxy << '.';
#endif
	}
	else
	{
		LOG(WARN) << "ID3D11Device1::CreateDeferredContext2" << " failed with error code " << hr << '.';
	}

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
#if RESHADE_ADDON
	if (pDesc1 == nullptr)
		return E_INVALIDARG;
	if (ppTexture2D == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D11Device3 *>(_orig)->CreateTexture2D1(pDesc1, pInitialData, nullptr);

	D3D11_TEXTURE2D_DESC1 internal_desc = *pDesc1;
	auto desc = reshade::d3d11::convert_resource_desc(internal_desc);

	std::vector<reshade::api::subresource_data> initial_data;
	if (pInitialData != nullptr)
	{
		initial_data.resize(D3D11_REQ_MIP_LEVELS * D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION);
		for (UINT i = 0; i < pDesc1->MipLevels * pDesc1->ArraySize; ++i)
			initial_data[i] = *reinterpret_cast<const reshade::api::subresource_data *>(pInitialData + i);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, initial_data.data(), reshade::api::resource_usage::general))
	{
		reshade::d3d11::convert_resource_desc(internal_desc);
		pDesc1 = &internal_desc;
		pInitialData = reinterpret_cast<const D3D11_SUBRESOURCE_DATA *>(initial_data.data());
	}
#endif

	assert(_interface_version >= 3);
	const HRESULT hr = static_cast<ID3D11Device3 *>(_orig)->CreateTexture2D1(pDesc1, pInitialData, ppTexture2D);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this, desc, reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::general, reshade::api::resource { reinterpret_cast<uintptr_t>(*ppTexture2D) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_resource, reshade::api::resource>(this, *ppTexture2D);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device3::CreateTexture2D1" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture3D1(const D3D11_TEXTURE3D_DESC1 *pDesc1, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture3D1 **ppTexture3D)
{
#if RESHADE_ADDON
	if (pDesc1 == nullptr)
		return E_INVALIDARG;
	if (ppTexture3D == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D11Device3 *>(_orig)->CreateTexture3D1(pDesc1, pInitialData, nullptr);

	D3D11_TEXTURE3D_DESC1 internal_desc = *pDesc1;
	auto desc = reshade::d3d11::convert_resource_desc(internal_desc);

	std::vector<reshade::api::subresource_data> initial_data;
	if (pInitialData != nullptr)
	{
		initial_data.resize(D3D11_REQ_MIP_LEVELS);
		for (UINT i = 0; i < pDesc1->MipLevels; ++i)
			initial_data[i] = *reinterpret_cast<const reshade::api::subresource_data *>(pInitialData + i);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, initial_data.data(), reshade::api::resource_usage::general))
	{
		reshade::d3d11::convert_resource_desc(internal_desc);
		pDesc1 = &internal_desc;
		pInitialData = reinterpret_cast<const D3D11_SUBRESOURCE_DATA *>(initial_data.data());
	}
#endif

	assert(_interface_version >= 3);
	const HRESULT hr = static_cast<ID3D11Device3 *>(_orig)->CreateTexture3D1(pDesc1, pInitialData, ppTexture3D);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this, desc, reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::general, reshade::api::resource { reinterpret_cast<uintptr_t>(*ppTexture3D) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_resource, reshade::api::resource>(this, *ppTexture3D);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device3::CreateTexture3D1" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateRasterizerState2(const D3D11_RASTERIZER_DESC2 *pRasterizerDesc, ID3D11RasterizerState2 **ppRasterizerState)
{
#if RESHADE_ADDON
	if (ppRasterizerState == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D11Device3 *>(_orig)->CreateRasterizerState2(pRasterizerDesc, nullptr);

	D3D11_RASTERIZER_DESC2 internal_desc = {};
	auto desc = reshade::d3d11::convert_pipeline_desc(pRasterizerDesc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		reshade::d3d11::convert_pipeline_desc(desc, internal_desc);
		pRasterizerDesc = &internal_desc;
	}
#endif

	assert(_interface_version >= 3);
	const HRESULT hr = static_cast<ID3D11Device3 *>(_orig)->CreateRasterizerState2(pRasterizerDesc, ppRasterizerState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppRasterizerState) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_pipeline, reshade::api::pipeline>(this, *ppRasterizerState);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device3::CreateRasterizerState2" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateShaderResourceView1(ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc1, ID3D11ShaderResourceView1 **ppShaderResourceView1)
{
#if RESHADE_ADDON
	if (pResource == nullptr)
		return E_INVALIDARG;
	if (ppShaderResourceView1 == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D11Device3 *>(_orig)->CreateShaderResourceView1(pResource, pDesc1, nullptr);

	D3D11_SHADER_RESOURCE_VIEW_DESC1 internal_desc = pDesc1 != nullptr ? *pDesc1 : D3D11_SHADER_RESOURCE_VIEW_DESC1 { DXGI_FORMAT_UNKNOWN, D3D11_SRV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d11::convert_resource_view_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::shader_resource, desc))
	{
		reshade::d3d11::convert_resource_view_desc(desc, internal_desc);
		pDesc1 = &internal_desc;
	}
#endif

	assert(_interface_version >= 3);
	const HRESULT hr = static_cast<ID3D11Device3 *>(_orig)->CreateShaderResourceView1(pResource, pDesc1, ppShaderResourceView1);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::shader_resource, desc, reshade::api::resource_view { reinterpret_cast<uintptr_t>(*ppShaderResourceView1) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_resource_view, reshade::api::resource_view>(this, *ppShaderResourceView1);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device3::CreateShaderResourceView1" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateUnorderedAccessView1(ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc1, ID3D11UnorderedAccessView1 **ppUnorderedAccessView1)
{
#if RESHADE_ADDON
	if (pResource == nullptr)
		return E_INVALIDARG;
	if (ppUnorderedAccessView1 == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D11Device3 *>(_orig)->CreateUnorderedAccessView1(pResource, pDesc1, nullptr);

	D3D11_UNORDERED_ACCESS_VIEW_DESC1 internal_desc = pDesc1 != nullptr ? *pDesc1 : D3D11_UNORDERED_ACCESS_VIEW_DESC1 { DXGI_FORMAT_UNKNOWN, D3D11_UAV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d11::convert_resource_view_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::unordered_access, desc))
	{
		reshade::d3d11::convert_resource_view_desc(desc, internal_desc);
		pDesc1 = &internal_desc;
	}
#endif

	assert(_interface_version >= 3);
	const HRESULT hr = static_cast<ID3D11Device3 *>(_orig)->CreateUnorderedAccessView1(pResource, pDesc1, ppUnorderedAccessView1);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::unordered_access, desc, reshade::api::resource_view { reinterpret_cast<uintptr_t>(*ppUnorderedAccessView1) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_resource_view, reshade::api::resource_view>(this, *ppUnorderedAccessView1);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device3::CreateUnorderedAccessView1" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateRenderTargetView1(ID3D11Resource *pResource, const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc1, ID3D11RenderTargetView1 **ppRenderTargetView1)
{
#if RESHADE_ADDON
	if (pResource == nullptr)
		return E_INVALIDARG;
	if (ppRenderTargetView1 == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D11Device3 *>(_orig)->CreateRenderTargetView1(pResource, pDesc1, nullptr);

	D3D11_RENDER_TARGET_VIEW_DESC1 internal_desc = pDesc1 != nullptr ? *pDesc1 : D3D11_RENDER_TARGET_VIEW_DESC1 { DXGI_FORMAT_UNKNOWN, D3D11_RTV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d11::convert_resource_view_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::render_target, desc))
	{
		reshade::d3d11::convert_resource_view_desc(desc, internal_desc);
		pDesc1 = &internal_desc;
	}
#endif

	assert(_interface_version >= 3);
	const HRESULT hr = static_cast<ID3D11Device3 *>(_orig)->CreateRenderTargetView1(pResource, pDesc1, ppRenderTargetView1);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::render_target, desc, reshade::api::resource_view { reinterpret_cast<uintptr_t>(*ppRenderTargetView1) });

		reshade::invoke_addon_event_on_destruction<reshade::addon_event::destroy_resource_view, reshade::api::resource_view>(this, *ppRenderTargetView1);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D11Device3::CreateRenderTargetView1" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
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
	if (SUCCEEDED(hr))
	{
		const auto device_context_proxy = new D3D11DeviceContext(this, *ppDeferredContext);
		*ppDeferredContext = device_context_proxy;

#if RESHADE_VERBOSE_LOG
		LOG(INFO) << "> Returning ID3D11DeviceContext3 object " << device_context_proxy << '.';
#endif
	}
	else
	{
		LOG(WARN) << "ID3D11Device1::CreateDeferredContext3" << " failed with error code " << hr << '.';
	}

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
