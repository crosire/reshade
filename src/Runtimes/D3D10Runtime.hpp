#pragma once

#include "Runtime.hpp"

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

			ID3D10Device *mDevice;
			IDXGISwapChain *mSwapChain;

			ID3D10Texture2D *mBackBufferTexture;
			ID3D10RenderTargetView *mBackBufferTargets[3];
			ID3D10ShaderResourceView *mBackBufferTextureSRV[2];
			ID3D10ShaderResourceView *mDepthStencilTextureSRV;
			std::vector<ID3D10SamplerState *> mEffectSamplerStates;
			std::vector<ID3D10ShaderResourceView *> mEffectShaderResources;
			ID3D10Buffer *mConstantBuffer;
			UINT mConstantBufferSize;

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
			bool CreateDepthStencilReplacement(ID3D10DepthStencilView *depthstencil);

			bool mMultisamplingEnabled;
			DXGI_FORMAT mBackBufferFormat;
			std::unique_ptr<class D3D10StateBlock> mStateBlock;
			ID3D10Texture2D *mBackBuffer, *mBackBufferResolved;
			ID3D10DepthStencilView *mDepthStencil, *mDepthStencilReplacement;
			ID3D10Texture2D *mDepthStencilTexture;
			ID3D10DepthStencilView *mDefaultDepthStencil;
			std::unordered_map<ID3D10DepthStencilView *, DepthSourceInfo> mDepthSourceTable;
			ID3D10VertexShader *mCopyVS;
			ID3D10PixelShader *mCopyPS;
			ID3D10SamplerState *mCopySampler;
			ID3D10RasterizerState *mEffectRasterizerState;
		};
	}
}