#pragma once

#include "Runtime.hpp"

#include <algorithm>
#include <d3d10_1.h>

namespace ReShade
{
	namespace Runtimes
	{
		class D3D10Runtime : public Runtime
		{
		public:
			D3D10Runtime(ID3D10Device *device, IDXGISwapChain *swapchain);
			~D3D10Runtime();

			bool OnInit(const DXGI_SWAP_CHAIN_DESC &desc);
			void OnReset() override;
			void OnResetEffect() override;
			void OnPresent() override;
			void OnDrawCall(UINT vertices) override;
			void OnApplyEffect() override;
			void OnApplyEffectTechnique(const Technique *technique) override;
			void OnCreateDepthStencilView(ID3D10Resource *resource, ID3D10DepthStencilView *depthstencil);
			void OnDeleteDepthStencilView(ID3D10DepthStencilView *depthstencil);
			void OnSetDepthStencilView(ID3D10DepthStencilView *&depthstencil);
			void OnGetDepthStencilView(ID3D10DepthStencilView *&depthstencil);
			void OnClearDepthStencilView(ID3D10DepthStencilView *&depthstencil);
			void OnCopyResource(ID3D10Resource *&dest, ID3D10Resource *&source);

			Texture *GetTexture(const std::string &name) const
			{
				const auto it = std::find_if(_textures.cbegin(), _textures.cend(), [name](const std::unique_ptr<Texture> &it) { return it->Name == name; });

				return it != _textures.cend() ? it->get() : nullptr;
			}
			const std::vector<std::unique_ptr<Technique>> &GetTechniques() const
			{
				return _techniques;
			}
			void EnlargeConstantStorage()
			{
				_uniformDataStorage.resize(_uniformDataStorage.size() + 128);
			}
			unsigned char *GetConstantStorage()
			{
				return _uniformDataStorage.data();
			}
			size_t GetConstantStorageSize() const
			{
				return _uniformDataStorage.size();
			}
			void AddTexture(Texture *texture)
			{
				_textures.push_back(std::unique_ptr<Texture>(texture));
			}
			void AddConstant(Uniform *constant)
			{
				_uniforms.push_back(std::unique_ptr<Uniform>(constant));
			}
			void AddTechnique(Technique *technique)
			{
				_techniques.push_back(std::unique_ptr<Technique>(technique));
			}

			ID3D10Device *_device;
			IDXGISwapChain *_swapchain;

			ID3D10Texture2D *_backbufferTexture;
			ID3D10RenderTargetView *_backbufferTargets[3];
			ID3D10ShaderResourceView *_backbufferTextureSRV[2];
			ID3D10ShaderResourceView *_depthStencilTextureSRV;
			std::vector<ID3D10SamplerState *> _effectSamplerStates;
			std::vector<ID3D10ShaderResourceView *> _effectShaderResources;
			ID3D10Buffer *_constantBuffer;
			UINT _constantBufferSize;

		private:
			struct DepthSourceInfo
			{
				UINT Width, Height;
				FLOAT DrawCallCount, DrawVerticesCount;
			};

			void Screenshot(unsigned char *buffer) const override;
			bool UpdateEffect(const FX::NodeTree &ast, const std::vector<std::string> &pragmas, std::string &errors) override;
			bool UpdateTexture(Texture *texture, const unsigned char *data, size_t size) override;

			void DetectDepthSource();
			bool CreateDepthStencilReplacement(ID3D10DepthStencilView *depthstencil);

			bool _multisamplingEnabled;
			DXGI_FORMAT _backbufferFormat;
			std::unique_ptr<class D3D10StateBlock> _stateBlock;
			ID3D10Texture2D *_backbuffer, *_backbufferResolved;
			ID3D10DepthStencilView *_depthStencil, *_depthStencilReplacement;
			ID3D10Texture2D *_depthStencilTexture;
			ID3D10DepthStencilView *_defaultDepthStencil;
			std::unordered_map<ID3D10DepthStencilView *, DepthSourceInfo> _depthSourceTable;
			ID3D10VertexShader *_copyVS;
			ID3D10PixelShader *_copyPS;
			ID3D10SamplerState *_copySampler;
			ID3D10RasterizerState *_effectRasterizerState;
		};
	}
}