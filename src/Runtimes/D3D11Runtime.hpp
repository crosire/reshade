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
			void OnDrawCall(ID3D11DeviceContext *context, UINT vertices);
			void OnApplyEffect() override;
			void OnApplyEffectTechnique(const Technique *technique) override;
			void OnCreateDepthStencilView(ID3D11Resource *resource, ID3D11DepthStencilView *depthstencil);
			void OnDeleteDepthStencilView(ID3D11DepthStencilView *depthstencil);
			void OnSetDepthStencilView(ID3D11DepthStencilView *&depthstencil);
			void OnGetDepthStencilView(ID3D11DepthStencilView *&depthstencil);
			void OnClearDepthStencilView(ID3D11DepthStencilView *&depthstencil);
			void OnCopyResource(ID3D11Resource *&dest, ID3D11Resource *&source);

			inline Texture *GetTexture(const std::string &name) const
			{
				const auto it = std::find_if(this->mTextures.cbegin(), this->mTextures.cend(), [name](const std::unique_ptr<Texture> &it) { return it->Name == name; });

				return it != this->mTextures.cend() ? it->get() : nullptr;
			}
			inline const std::vector<std::unique_ptr<Technique>> &GetTechniques() const
			{
				return this->mTechniques;
			}
			inline void EnlargeConstantStorage()
			{
				this->mUniformDataStorage.resize(this->mUniformDataStorage.size() + 128);
			}
			inline unsigned char *GetConstantStorage()
			{
				return this->mUniformDataStorage.data();
			}
			inline size_t GetConstantStorageSize() const
			{
				return this->mUniformDataStorage.size();
			}
			inline void AddTexture(Texture *texture)
			{
				this->mTextures.push_back(std::unique_ptr<Texture>(texture));
			}
			inline void AddConstant(Uniform *constant)
			{
				this->mUniforms.push_back(std::unique_ptr<Uniform>(constant));
			}
			inline void AddTechnique(Technique *technique)
			{
				this->mTechniques.push_back(std::unique_ptr<Technique>(technique));
			}

			ID3D11Device *mDevice;
			ID3D11DeviceContext *mImmediateContext;
			IDXGISwapChain *mSwapChain;

			ID3D11Texture2D *mBackBufferTexture;
			ID3D11ShaderResourceView *mBackBufferTextureSRV[2];
			ID3D11RenderTargetView *mBackBufferTargets[3];
			ID3D11ShaderResourceView *mDepthStencilTextureSRV;
			std::vector<ID3D11SamplerState *> mEffectSamplerStates;
			std::vector<ID3D11ShaderResourceView *> mEffectShaderResources;
			ID3D11Buffer *mConstantBuffer;

		private:
			struct DepthSourceInfo
			{
				UINT Width, Height;
				FLOAT DrawCallCount, DrawVerticesCount;
			};

			void Screenshot(unsigned char *buffer) const override;
			bool UpdateEffect(const FX::NodeTree &ast, const std::vector<std::string> &pragmas, std::string &errors) override;
			bool UpdateTexture(Texture *texture, const unsigned char *data, std::size_t size) override;

			void DetectDepthSource();
			bool CreateDepthStencilReplacement(ID3D11DepthStencilView *depthstencil);

			bool mMultisamplingEnabled;
			DXGI_FORMAT mBackBufferFormat;
			std::unique_ptr<class D3D11StateBlock> mStateBlock;
			ID3D11Texture2D *mBackBuffer, *mBackBufferResolved;
			ID3D11DepthStencilView *mDepthStencil, *mDepthStencilReplacement;
			ID3D11Texture2D *mDepthStencilTexture;
			ID3D11DepthStencilView *mDefaultDepthStencil;
			std::unordered_map<ID3D11DepthStencilView *, DepthSourceInfo> mDepthSourceTable;
			ID3D11VertexShader *mCopyVS;
			ID3D11PixelShader *mCopyPS;
			ID3D11SamplerState *mCopySampler;
			Utils::CriticalSection mCS;
			ID3D11RasterizerState *mEffectRasterizerState;
		};
	}
}