#include "Log.hpp"
#include "D3D12Runtime.hpp"
#include "WindowWatcher.hpp"

#include <assert.h>

// ---------------------------------------------------------------------------------------------------

namespace ReShade
{
	namespace Runtimes
	{
		D3D12Runtime::D3D12Runtime(ID3D12Device *device, ID3D12CommandQueue *queue, IDXGISwapChain3 *swapchain) : Runtime(D3D_FEATURE_LEVEL_12_0), _device(device), _commandQueue(queue), _swapchain(swapchain)
		{
			assert(queue != nullptr);
			assert(device != nullptr);
			assert(swapchain != nullptr);

			_device->AddRef();
			_commandQueue->AddRef();
			_swapchain->AddRef();
		}
		D3D12Runtime::~D3D12Runtime()
		{
			_device->Release();
			_commandQueue->Release();
			_swapchain->Release();
		}

		bool D3D12Runtime::OnInit(const DXGI_SWAP_CHAIN_DESC &desc)
		{
			_width = desc.BufferDesc.Width;
			_height = desc.BufferDesc.Height;
			_window.reset(new WindowWatcher(desc.OutputWindow));

			return Runtime::OnInit();
		}
		void D3D12Runtime::OnReset()
		{
			if (!_isInitialized)
			{
				return;
			}

			Runtime::OnReset();
		}
		void D3D12Runtime::OnResetEffect()
		{
			Runtime::OnResetEffect();
		}
		void D3D12Runtime::OnPresent()
		{
			if (!_isInitialized)
			{
				LOG(TRACE) << "Failed to present! Runtime is in a lost state.";
				return;
			}
			else if (_stats.DrawCalls == 0)
			{
				return;
			}

			Runtime::OnPresent();
		}
		void D3D12Runtime::OnApplyEffect()
		{
			Runtime::OnApplyEffect();
		}
		void D3D12Runtime::OnApplyEffectTechnique(const Technique *technique)
		{
			Runtime::OnApplyEffectTechnique(technique);
		}

		void D3D12Runtime::Screenshot(unsigned char *buffer) const
		{
		}
		bool D3D12Runtime::UpdateEffect(const FX::NodeTree &ast, const std::vector<std::string> &pragmas, std::string &errors)
		{
			return false;
		}
		bool D3D12Runtime::UpdateTexture(Texture *texture, const unsigned char *data, size_t size)
		{
			return false;
		}
	}
}
