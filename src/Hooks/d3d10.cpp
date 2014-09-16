#include "Log.hpp"
#include "HookManager.hpp"

#include <d3d10_1.h>

// -----------------------------------------------------------------------------------------------------

EXPORT HRESULT WINAPI											D3D10CreateStateBlock(ID3D10Device *pDevice, D3D10_STATE_BLOCK_MASK *pStateBlockMask, ID3D10StateBlock **ppStateBlock)
{
	return ReHook::Call(&D3D10CreateStateBlock)(pDevice, pStateBlockMask, ppStateBlock);
}
EXPORT HRESULT WINAPI											D3D10StateBlockMaskUnion(D3D10_STATE_BLOCK_MASK *pA, D3D10_STATE_BLOCK_MASK *pB, D3D10_STATE_BLOCK_MASK *pResult)
{
	return ReHook::Call(&D3D10StateBlockMaskUnion)(pA, pB, pResult);
}
EXPORT HRESULT WINAPI											D3D10StateBlockMaskIntersect(D3D10_STATE_BLOCK_MASK *pA, D3D10_STATE_BLOCK_MASK *pB, D3D10_STATE_BLOCK_MASK *pResult)
{
	return ReHook::Call(&D3D10StateBlockMaskIntersect)(pA, pB, pResult);
}
EXPORT HRESULT WINAPI											D3D10StateBlockMaskDifference(D3D10_STATE_BLOCK_MASK *pA, D3D10_STATE_BLOCK_MASK *pB, D3D10_STATE_BLOCK_MASK *pResult)
{
	return ReHook::Call(&D3D10StateBlockMaskDifference)(pA, pB, pResult);
}
EXPORT HRESULT WINAPI											D3D10StateBlockMaskEnableCapture(D3D10_STATE_BLOCK_MASK *pMask, D3D10_DEVICE_STATE_TYPES StateType, UINT RangeStart, UINT RangeLength)
{
	return ReHook::Call(&D3D10StateBlockMaskEnableCapture)(pMask, StateType, RangeStart, RangeLength);
}
EXPORT HRESULT WINAPI											D3D10StateBlockMaskDisableCapture(D3D10_STATE_BLOCK_MASK *pMask, D3D10_DEVICE_STATE_TYPES StateType, UINT RangeStart, UINT RangeLength)
{
	return ReHook::Call(&D3D10StateBlockMaskDisableCapture)(pMask, StateType, RangeStart, RangeLength);
}
EXPORT HRESULT WINAPI											D3D10StateBlockMaskEnableAll(D3D10_STATE_BLOCK_MASK *pMask)
{
	return ReHook::Call(&D3D10StateBlockMaskEnableAll)(pMask);
}
EXPORT HRESULT WINAPI											D3D10StateBlockMaskDisableAll(D3D10_STATE_BLOCK_MASK *pMask)
{
	return ReHook::Call(&D3D10StateBlockMaskDisableAll)(pMask);
}
EXPORT BOOL WINAPI												D3D10StateBlockMaskGetSetting(D3D10_STATE_BLOCK_MASK *pMask, D3D10_DEVICE_STATE_TYPES StateType, UINT Entry)
{
	return ReHook::Call(&D3D10StateBlockMaskGetSetting)(pMask, StateType, Entry);
}

EXPORT HRESULT WINAPI											D3D10CreateDevice(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, ID3D10Device **ppDevice)
{
	LOG(INFO) << "Redirecting '" << "D3D10CreateDevice" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << Flags << ", " << SDKVersion << ", " << ppDevice << ")' ...";

#ifdef _DEBUG
	Flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	return ReHook::Call(&D3D10CreateDevice)(pAdapter, DriverType, Software, Flags, SDKVersion, ppDevice);
}
EXPORT HRESULT WINAPI											D3D10CreateDevice1(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, D3D10_FEATURE_LEVEL1 HardwareLevel, UINT SDKVersion, ID3D10Device1 **ppDevice)
{
	LOG(INFO) << "Redirecting '" << "D3D10CreateDevice1" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << Flags << ", " << HardwareLevel << ", " << SDKVersion << ", " << ppDevice << ")' ...";

#ifdef _DEBUG
	Flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	return ReHook::Call(&D3D10CreateDevice1)(pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, ppDevice);
}
EXPORT HRESULT WINAPI											D3D10CreateDeviceAndSwapChain(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device **ppDevice)
{
	LOG(INFO) << "Redirecting '" << "D3D10CreateDeviceAndSwapChain" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << Flags << ", " << SDKVersion << ", " << pSwapChainDesc << ", " << ppSwapChain << ", " << ppDevice << ")' ...";

#ifdef _DEBUG
	Flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	return ReHook::Call(&D3D10CreateDeviceAndSwapChain)(pAdapter, DriverType, Software, Flags, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice);
}
EXPORT HRESULT WINAPI											D3D10CreateDeviceAndSwapChain1(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, D3D10_FEATURE_LEVEL1 HardwareLevel, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device1 **ppDevice)
{
	LOG(INFO) << "Redirecting '" << "D3D10CreateDeviceAndSwapChain1" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << Flags << ", " << HardwareLevel << ", " << SDKVersion << ", " << pSwapChainDesc << ", " << ppSwapChain << ", " << ppDevice << ")' ...";

#ifdef _DEBUG
	Flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	return ReHook::Call(&D3D10CreateDeviceAndSwapChain1)(pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice);
}