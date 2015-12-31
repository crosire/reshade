#pragma once

#include "Runtime.hpp"
#include "Utils\CriticalSection.hpp"

#include <algorithm>
#include <d3d11_3.h>

namespace ReShade
{
	namespace Runtimes
	{
		class D3D11Runtime : public Runtime
		{
		public:
			D3D11Runtime(ID3D11Device *device, IDXGISwapChain *swapchain);
			~D3D11Runtime();

			bool OnInit(const DXGI_SWAP_CHAIN_DESC &desc);
			void OnReset() override;
			void OnResetEffect() override;
			void OnPresent() override;
			void OnDrawCall(ID3D11DeviceContext *context, unsigned int vertices);
			void OnApplyEffect() override;
			void OnApplyEffectTechnique(const Technique *technique) override;
			void OnCreateDepthStencilView(ID3D11Resource *resource, ID3D11DepthStencilView *depthstencil);
			void OnDeleteDepthStencilView(ID3D11DepthStencilView *depthstencil);
			void OnSetDepthStencilView(ID3D11DepthStencilView *&depthstencil);
			void OnGetDepthStencilView(ID3D11DepthStencilView *&depthstencil);
			void OnClearDepthStencilView(ID3D11DepthStencilView *&depthstencil);
			void OnCopyResource(ID3D11Resource *&dest, ID3D11Resource *&source);

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

			ID3D11Device *_device;
			ID3D11DeviceContext *_immediateContext;
			IDXGISwapChain *_swapchain;

			ID3D11Texture2D *_backbufferTexture;
			ID3D11ShaderResourceView *_backbufferTextureSRV[2];
			ID3D11RenderTargetView *_backbufferTargets[3];
			ID3D11ShaderResourceView *_depthStencilTextureSRV;
			std::vector<ID3D11SamplerState *> _effectSamplerStates;
			std::vector<ID3D11ShaderResourceView *> _effectShaderResources;
			ID3D11Buffer *_constantBuffer;

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
			bool CreateDepthStencilReplacement(ID3D11DepthStencilView *depthstencil);

			bool _multisamplingEnabled;
			DXGI_FORMAT _backbufferFormat;
			std::unique_ptr<class D3D11StateBlock> _stateBlock;
			ID3D11Texture2D *_backbuffer, *_backbufferResolved;
			ID3D11DepthStencilView *_depthStencil, *_depthStencilReplacement;
			ID3D11Texture2D *_depthStencilTexture;
			ID3D11DepthStencilView *_defaultDepthStencil;
			std::unordered_map<ID3D11DepthStencilView *, DepthSourceInfo> _depthSourceTable;
			ID3D11VertexShader *_copyVS;
			ID3D11PixelShader *_copyPS;
			ID3D11SamplerState *_copySampler;
			Utils::CriticalSection _cs;
			ID3D11RasterizerState *_effectRasterizerState;
		};
	}
}