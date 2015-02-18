#pragma once

#include "Runtime.hpp"

#include <d3d11_1.h>

namespace ReShade
{
	namespace Runtimes
	{
		struct D3D11Runtime : public Runtime, public std::enable_shared_from_this<D3D11Runtime>
		{
			friend struct D3D11Technique;

			struct DepthSourceInfo
			{
				UINT Width, Height;
				FLOAT DrawCallCount, DrawVerticesCount;
			};

			D3D11Runtime(ID3D11Device *device, IDXGISwapChain *swapchain);
			~D3D11Runtime();

			bool OnCreateInternal(const DXGI_SWAP_CHAIN_DESC &desc);
			void OnDeleteInternal();
			void OnDrawInternal(ID3D11DeviceContext *context, unsigned int vertices);
			void OnPresentInternal();
			void OnGetBackBuffer(ID3D11Texture2D *&buffer);
			void OnCreateDepthStencilView(ID3D11Resource *resource, ID3D11DepthStencilView *depthstencil);
			void OnDeleteDepthStencilView(ID3D11DepthStencilView *depthstencil);
			void OnSetDepthStencilView(ID3D11DepthStencilView *&depthstencil);
			void OnGetDepthStencilView(ID3D11DepthStencilView *&depthstencil);
			void OnClearDepthStencilView(ID3D11DepthStencilView *&depthstencil);
			void OnCopyResource(ID3D11Resource *&dest, ID3D11Resource *&source);

			void DetectDepthSource();
			bool CreateBackBufferReplacement(ID3D11Texture2D *backbuffer, const DXGI_SAMPLE_DESC &samples);
			bool CreateDepthStencilReplacement(ID3D11DepthStencilView *depthstencil);

			virtual std::unique_ptr<FX::Effect> CompileEffect(const FX::Tree &ast, std::string &errors) const override;
			virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const override;

			ID3D11Device *mDevice;
			ID3D11DeviceContext *mImmediateContext;
			IDXGISwapChain *mSwapChain;
			DXGI_SWAP_CHAIN_DESC mSwapChainDesc;
			std::unique_ptr<class D3D11StateBlock> mStateBlock;
			ID3D11Texture2D *mBackBuffer, *mBackBufferReplacement;
			ID3D11Texture2D *mBackBufferTexture;
			ID3D11ShaderResourceView *mBackBufferTextureSRV[2];
			ID3D11RenderTargetView *mBackBufferTargets[2];
			ID3D11DepthStencilView *mDepthStencil, *mDepthStencilReplacement;
			ID3D11Texture2D *mDepthStencilTexture;
			ID3D11ShaderResourceView *mDepthStencilTextureSRV;
			ID3D11DepthStencilView *mDefaultDepthStencil;
			std::unordered_map<ID3D11DepthStencilView *, DepthSourceInfo> mDepthSourceTable;
			CRITICAL_SECTION mCS;
			bool mLost;
		};

		struct D3D11Effect : public FX::Effect
		{
			friend struct D3D11Runtime;
			friend struct D3D11Texture;

			D3D11Effect(std::shared_ptr<const D3D11Runtime> runtime);
			~D3D11Effect();

			inline bool AddTexture(const std::string &name, Texture *texture)
			{
				return this->mTextures.emplace(name, std::unique_ptr<Texture>(texture)).second;
			}
			inline bool AddConstant(const std::string &name, Constant *constant)
			{
				return this->mConstants.emplace(name, std::unique_ptr<Constant>(constant)).second;
			}
			inline bool AddTechnique(const std::string &name, Technique *technique)
			{
				return this->mTechniques.emplace(name, std::unique_ptr<Technique>(technique)).second;
			}

			virtual void Begin() const override;
			virtual void End() const override;

			std::shared_ptr<const D3D11Runtime> mRuntime;
			ID3D11RasterizerState *mRasterizerState;
			std::vector<ID3D11SamplerState *> mSamplerStates;
			std::vector<ID3D11ShaderResourceView *> mShaderResources;
			std::vector<ID3D11Buffer *> mConstantBuffers;
			std::vector<unsigned char *> mConstantStorages;
			mutable bool mConstantsDirty;
		};
		struct D3D11Texture : public FX::Effect::Texture
		{
			enum class Source
			{
				Memory,
				BackBuffer,
				DepthStencil
			};

			D3D11Texture(D3D11Effect *effect, const Description &desc);
			~D3D11Texture();

			inline bool AddAnnotation(const std::string &name, const FX::Effect::Annotation &value)
			{
				return this->mAnnotations.emplace(name, value).second;
			}

			virtual bool Update(unsigned int level, const unsigned char *data, std::size_t size) override;
			void ChangeSource(ID3D11ShaderResourceView *srv, ID3D11ShaderResourceView *srvSRGB);

			D3D11Effect *mEffect;
			Source mSource;
			std::size_t mRegister;
			ID3D11Texture2D *mTexture;
			ID3D11ShaderResourceView *mShaderResourceView[2];
			ID3D11RenderTargetView *mRenderTargetView[2];
		};
		struct D3D11Constant : public FX::Effect::Constant
		{
			D3D11Constant(D3D11Effect *effect, const Description &desc);
			~D3D11Constant();

			inline bool AddAnnotation(const std::string &name, const FX::Effect::Annotation &value)
			{
				return this->mAnnotations.emplace(name, value).second;
			}

			virtual void GetValue(unsigned char *data, std::size_t size) const override;
			virtual void SetValue(const unsigned char *data, std::size_t size) override;

			D3D11Effect *mEffect;
			std::size_t mBufferIndex, mBufferOffset;
		};
		struct D3D11Technique : public FX::Effect::Technique
		{
			struct Pass
			{
				ID3D11VertexShader *VS;
				ID3D11PixelShader *PS;
				ID3D11BlendState *BS;
				ID3D11DepthStencilState *DSS;
				UINT StencilRef;
				ID3D11RenderTargetView *RT[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
				ID3D11ShaderResourceView *RTSRV[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
				D3D11_VIEWPORT Viewport;
				std::vector<ID3D11ShaderResourceView *> SRV;
			};

			D3D11Technique(D3D11Effect *effect, const Description &desc);
			~D3D11Technique();

			inline bool AddAnnotation(const std::string &name, const FX::Effect::Annotation &value)
			{
				return this->mAnnotations.emplace(name, value).second;
			}
			inline void AddPass(const Pass &pass)
			{
				this->mPasses.push_back(pass);
			}

			virtual void RenderPass(unsigned int index) const override;

			D3D11Effect *mEffect;
			std::vector<Pass> mPasses;
		};
	}
}