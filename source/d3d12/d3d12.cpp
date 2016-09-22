#include "log.hpp"
#include "hook_manager.hpp"
#include "d3d12_device.hpp"

// D3D12
HOOK_EXPORT HRESULT WINAPI D3D12CreateDevice(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void **ppDevice)
{
	OLECHAR riidString[40];
	StringFromGUID2(riid, riidString, ARRAYSIZE(riidString));

	LOG(INFO) << "Redirecting '" << "D3D12CreateDevice" << "(" << pAdapter << ", " << std::hex << MinimumFeatureLevel << std::dec << ", " << riidString << ", " << ppDevice << ")' ...";

	const HRESULT hr = reshade::hooks::call(&D3D12CreateDevice)(pAdapter, MinimumFeatureLevel, riid, ppDevice);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'D3D12CreateDevice' failed with error code " << std::hex << hr << std::dec << "!";

		return hr;
	}

	if (ppDevice != nullptr)
	{
		assert(*ppDevice != nullptr);

		ID3D12Device *device = nullptr;

		if (SUCCEEDED(static_cast<IUnknown *>(*ppDevice)->QueryInterface(&device)))
		{
			const auto device_proxy = new D3D12Device(device);

			device->Release();

			*ppDevice = device_proxy;

			LOG(INFO) << "> Returned device objects: " << device_proxy;
		}
		else
		{
			LOG(WARNING) << "> Skipping device because it is missing support for the 'ID3D12Device' interface.";
		}
	}

	return hr;
}
