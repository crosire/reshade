#include "Log.hpp"
#include "D3D12Runtime.hpp"
#include "WindowWatcher.hpp"

#include <assert.h>

// ---------------------------------------------------------------------------------------------------

namespace ReShade
{
	namespace Runtimes
	{
		D3D12Runtime::D3D12Runtime(ID3D12Device *device, ID3D12CommandQueue *queue, IDXGISwapChain3 *swapchain) : mDevice(device), mCommandQueue(queue), mSwapChain(swapchain)
		{
			assert(queue != nullptr);
			assert(device != nullptr);
			assert(swapchain != nullptr);

			this->mDevice->AddRef();
			this->mCommandQueue->AddRef();
			this->mSwapChain->AddRef();
		}
		D3D12Runtime::~D3D12Runtime()
		{
			this->mDevice->Release();
			this->mCommandQueue->Release();
			this->mSwapChain->Release();
		}

		bool D3D12Runtime::OnInit(const DXGI_SWAP_CHAIN_DESC &desc)
		{
			this->mWidth = desc.BufferDesc.Width;
			this->mHeight = desc.BufferDesc.Height;
			this->mWindow.reset(new WindowWatcher(desc.OutputWindow));

			return Runtime::OnInit();
		}
		void D3D12Runtime::OnReset()
		{
			if (!this->mIsInitialized)
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
			if (!this->mIsInitialized)
			{
				LOG(TRACE) << "Failed to present! Runtime is in a lost state.";
				return;
			}
			else if (this->mStats.DrawCalls == 0)
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
		bool D3D12Runtime::UpdateTexture(Texture *texture, const unsigned char *data, std::size_t size)
		{
			return false;
		}
	}
}
