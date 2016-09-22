#include "log.hpp"
#include "dxgi_device.hpp"
#include "dxgi_swapchain.hpp"

// IDXGIDevice
HRESULT STDMETHODCALLTYPE DXGIDevice::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}
	else if (
		riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDXGIObject) ||
		riid == __uuidof(IDXGIDevice) ||
		riid == __uuidof(IDXGIDevice1) ||
		riid == __uuidof(IDXGIDevice2) ||
		riid == __uuidof(IDXGIDevice3))
	{
		#pragma region Update to IDXGIDevice2 interface
		if (riid == __uuidof(IDXGIDevice2) && _interface_version < 2)
		{
			IDXGIDevice2 *device2 = nullptr;

			if (FAILED(_orig->QueryInterface(&device2)))
			{
				return E_NOINTERFACE;
			}

			_orig->Release();

			LOG(TRACE) << "Upgraded 'IDXGIDevice" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << "' object " << this << " to 'IDXGIDevice2'.";

			_orig = device2;
			_interface_version = 2;
		}
		#pragma endregion
		#pragma region Update to IDXGIDevice3 interface
		if (riid == __uuidof(IDXGIDevice3) && _interface_version < 3)
		{
			IDXGIDevice3 *device3 = nullptr;

			if (FAILED(_orig->QueryInterface(&device3)))
			{
				return E_NOINTERFACE;
			}

			_orig->Release();

			LOG(TRACE) << "Upgraded 'IDXGIDevice" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << "' object " << this << " to 'IDXGIDevice3'.";

			_orig = device3;
			_interface_version = 3;
		}
		#pragma endregion

		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _direct3d_device->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE DXGIDevice::AddRef()
{
	_ref++;

	return _orig->AddRef();
}
ULONG STDMETHODCALLTYPE DXGIDevice::Release()
{
	ULONG ref = _orig->Release();

	if (--_ref == 0 && ref != 1)
	{
		LOG(WARNING) << "Reference count for 'IDXGIDevice" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << "' object " << this << " is inconsistent: " << ref << ", but expected 1.";

		ref = 1;
	}

	if (ref == 1)
	{
		assert(_ref <= 0);

		LOG(TRACE) << "Destroyed 'IDXGIDevice" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << "' object " << this << ".";

		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE DXGIDevice::SetPrivateData(REFGUID Name, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(Name, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown)
{
	return _orig->SetPrivateDataInterface(Name, pUnknown);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(Name, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::GetParent(REFIID riid, void **ppParent)
{
	return _orig->GetParent(riid, ppParent);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::GetAdapter(IDXGIAdapter **pAdapter)
{
	return _orig->GetAdapter(pAdapter);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::CreateSurface(const DXGI_SURFACE_DESC *pDesc, UINT NumSurfaces, DXGI_USAGE Usage, const DXGI_SHARED_RESOURCE *pSharedResource, IDXGISurface **ppSurface)
{
	return _orig->CreateSurface(pDesc, NumSurfaces, Usage, pSharedResource, ppSurface);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::QueryResourceResidency(IUnknown *const *ppResources, DXGI_RESIDENCY *pResidencyStatus, UINT NumResources)
{
	return _orig->QueryResourceResidency(ppResources, pResidencyStatus, NumResources);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::SetGPUThreadPriority(INT Priority)
{
	return _orig->SetGPUThreadPriority(Priority);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::GetGPUThreadPriority(INT *pPriority)
{
	return _orig->GetGPUThreadPriority(pPriority);
}

// IDXGIDevice1
HRESULT STDMETHODCALLTYPE DXGIDevice::SetMaximumFrameLatency(UINT MaxLatency)
{
	return _orig->SetMaximumFrameLatency(MaxLatency);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::GetMaximumFrameLatency(UINT *pMaxLatency)
{
	return _orig->GetMaximumFrameLatency(pMaxLatency);
}

// IDXGIDevice2
HRESULT STDMETHODCALLTYPE DXGIDevice::OfferResources(UINT NumResources, IDXGIResource *const *ppResources, DXGI_OFFER_RESOURCE_PRIORITY Priority)
{
	assert(_interface_version >= 2);

	return static_cast<IDXGIDevice2 *>(_orig)->OfferResources(NumResources, ppResources, Priority);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::ReclaimResources(UINT NumResources, IDXGIResource *const *ppResources, BOOL *pDiscarded)
{
	assert(_interface_version >= 2);

	return static_cast<IDXGIDevice2 *>(_orig)->ReclaimResources(NumResources, ppResources, pDiscarded);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::EnqueueSetEvent(HANDLE hEvent)
{
	assert(_interface_version >= 2);

	return static_cast<IDXGIDevice2 *>(_orig)->EnqueueSetEvent(hEvent);
}

// IDXGIDevice3
void STDMETHODCALLTYPE DXGIDevice::Trim()
{
	assert(_interface_version >= 3);

	return static_cast<IDXGIDevice3 *>(_orig)->Trim();
}
