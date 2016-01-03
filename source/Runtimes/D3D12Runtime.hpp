#pragma once

#include "Runtime.hpp"

#include <d3d12.h>
#include <dxgi1_4.h>

namespace ReShade
{
	namespace Runtimes
	{
		class D3D12Runtime : public Runtime
		{
		public:
			D3D12Runtime(ID3D12Device *device, ID3D12CommandQueue *queue, IDXGISwapChain3 *swapchain);
			~D3D12Runtime();

			bool OnInit(const DXGI_SWAP_CHAIN_DESC &desc);
			void OnReset() override;
			void OnResetEffect() override;
			void OnPresent() override;
			void OnApplyEffect() override;
			void OnApplyEffectTechnique(const Technique *technique) override;

			void Screenshot(unsigned char *buffer) const override;
			bool UpdateEffect(const FX::nodetree &ast, const std::vector<std::string> &pragmas, std::string &errors) override;
			bool UpdateTexture(Texture *texture, const unsigned char *data, size_t size) override;

			ID3D12Device *_device;
			ID3D12CommandQueue *_commandQueue;
			IDXGISwapChain3 *_swapchain;
		};
	}
}
