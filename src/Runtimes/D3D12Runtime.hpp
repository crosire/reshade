#pragma once

#include "Runtime.hpp"

#include <d3d12.h>

namespace ReShade
{
	namespace Runtimes
	{
		class D3D12Runtime : public Runtime
		{
		public:
			D3D12Runtime(ID3D12Device *device, ID3D12CommandQueue *queue, IDXGISwapChain *swapchain);
			~D3D12Runtime();

			bool OnInit(const DXGI_SWAP_CHAIN_DESC &desc);
			void OnReset() override;
			void OnResetEffect() override;
			void OnPresent() override;
			void OnDrawCall(ID3D12CommandList *list, UINT vertices);
			void OnApplyEffect() override;
			void OnApplyEffectTechnique(const Technique *technique) override;

			void Screenshot(unsigned char *buffer) const override;
			bool UpdateEffect(const FX::NodeTree &ast, const std::vector<std::string> &pragmas, std::string &errors) override;
			bool UpdateTexture(Texture *texture, const unsigned char *data, std::size_t size) override;

			ID3D12Device *mDevice;
			ID3D12CommandQueue *mCommandQueue;
			IDXGISwapChain *mSwapChain;

		private:
		};
	}
}
