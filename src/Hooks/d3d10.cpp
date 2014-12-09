#include "Log.hpp"
#include "HookManager.hpp"

#include <d3d10_1.h>

// -----------------------------------------------------------------------------------------------------

EXPORT HRESULT WINAPI D3D10CreateDevice(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, ID3D10Device **ppDevice)
{
	LOG(INFO) << "Redirecting '" << "D3D10CreateDevice" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << Flags << ", " << SDKVersion << ", " << ppDevice << ")' ...";

#ifdef _DEBUG
	Flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	return ReShade::Hooks::Call(&D3D10CreateDevice)(pAdapter, DriverType, Software, Flags, SDKVersion, ppDevice);
}
EXPORT HRESULT WINAPI D3D10CreateDevice1(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, D3D10_FEATURE_LEVEL1 HardwareLevel, UINT SDKVersion, ID3D10Device1 **ppDevice)
{
	LOG(INFO) << "Redirecting '" << "D3D10CreateDevice1" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << Flags << ", " << HardwareLevel << ", " << SDKVersion << ", " << ppDevice << ")' ...";

#ifdef _DEBUG
	Flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	return ReShade::Hooks::Call(&D3D10CreateDevice1)(pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, ppDevice);
}
EXPORT HRESULT WINAPI D3D10CreateDeviceAndSwapChain(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device **ppDevice)
{
	LOG(INFO) << "Redirecting '" << "D3D10CreateDeviceAndSwapChain" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << Flags << ", " << SDKVersion << ", " << pSwapChainDesc << ", " << ppSwapChain << ", " << ppDevice << ")' ...";

#ifdef _DEBUG
	Flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	return ReShade::Hooks::Call(&D3D10CreateDeviceAndSwapChain)(pAdapter, DriverType, Software, Flags, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice);
}
EXPORT HRESULT WINAPI D3D10CreateDeviceAndSwapChain1(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, D3D10_FEATURE_LEVEL1 HardwareLevel, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device1 **ppDevice)
{
	LOG(INFO) << "Redirecting '" << "D3D10CreateDeviceAndSwapChain1" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << Flags << ", " << HardwareLevel << ", " << SDKVersion << ", " << pSwapChainDesc << ", " << ppSwapChain << ", " << ppDevice << ")' ...";

#ifdef _DEBUG
	Flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	return ReShade::Hooks::Call(&D3D10CreateDeviceAndSwapChain1)(pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice);
}