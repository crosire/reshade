#pragma once

#include "Runtime.hpp"

#include <d3d10_1.h>

namespace ReShade { namespace Runtimes
{
	struct D3D10Runtime : public Runtime, public std::enable_shared_from_this<D3D10Runtime>
	{
		friend struct D3D10Technique;

		struct DepthSourceInfo
		{
			UINT Width, Height;
			FLOAT DrawCallCount, DrawVerticesCount;
		};

		D3D10Runtime(ID3D10Device *device, IDXGISwapChain *swapchain);
		~D3D10Runtime();

		bool OnCreateInternal(const DXGI_SWAP_CHAIN_DESC &desc);
		void OnDeleteInternal();
		void OnDrawInternal(unsigned int vertices);
		void OnPresentInternal();
		void OnGetBackBuffer(ID3D10Texture2D *&buffer);
		void OnCreateDepthStencilView(ID3D10Resource *resource, ID3D10DepthStencilView *depthstencil);
		void OnDeleteDepthStencilView(ID3D10DepthStencilView *depthstencil);
		void OnSetDepthStencilView(ID3D10DepthStencilView *&depthstencil);
		void OnGetDepthStencilView(ID3D10DepthStencilView *&depthstencil);
		void OnClearDepthStencilView(ID3D10DepthStencilView *&depthstencil);
		void OnCopyResource(ID3D10Resource *&dest, ID3D10Resource *&source);

		void DetectDepthSource();
		bool CreateBackBufferReplacement(ID3D10Texture2D *backbuffer, const DXGI_SAMPLE_DESC &samples);
		bool CreateDepthStencilReplacement(ID3D10DepthStencilView *depthstencil);

		virtual std::unique_ptr<Effect> CompileEffect(const EffectTree &ast, std::string &errors) const override;
		virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const override;

		ID3D10Device *mDevice;
		IDXGISwapChain *mSwapChain;
		DXGI_SWAP_CHAIN_DESC mSwapChainDesc;
		ID3D10StateBlock *mStateBlock;
		ID3D10Texture2D *mBackBuffer, *mBackBufferReplacement;
		ID3D10Texture2D *mBackBufferTexture;
		ID3D10ShaderResourceView *mBackBufferTextureSRV[2];
		ID3D10RenderTargetView *mBackBufferTargets[2];
		ID3D10DepthStencilView *mDepthStencil, *mDepthStencilReplacement;
		ID3D10Texture2D *mDepthStencilTexture;
		ID3D10ShaderResourceView *mDepthStencilTextureSRV;
		ID3D10DepthStencilView *mDefaultDepthStencil;
		std::unordered_map<ID3D10DepthStencilView *, DepthSourceInfo> mDepthSourceTable;
		bool mLost;
	};

	struct D3D10Effect : public Effect
	{
		friend struct D3D10Runtime;
		friend struct D3D10Texture;

		D3D10Effect(std::shared_ptr<const D3D10Runtime> runtime);
		~D3D10Effect();

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

		std::shared_ptr<const D3D10Runtime> mRuntime;
		ID3D10RasterizerState *mRasterizerState;
		std::vector<ID3D10SamplerState *> mSamplerStates;
		std::vector<ID3D10ShaderResourceView *> mShaderResources;
		std::vector<ID3D10Buffer *> mConstantBuffers;
		std::vector<unsigned char *> mConstantStorages;
		mutable bool mConstantsDirty;
	};
	struct D3D10Texture : public Effect::Texture
	{
		enum class Source
		{
			Memory,
			BackBuffer,
			DepthStencil
		};

		D3D10Texture(D3D10Effect *effect, const Description &desc);
		~D3D10Texture();

		inline bool AddAnnotation(const std::string &name, const Effect::Annotation &value)
		{
			return this->mAnnotations.emplace(name, value).second;
		}

		virtual bool Update(unsigned int level, const unsigned char *data, std::size_t size) override;
		void ChangeSource(ID3D10ShaderResourceView *srv, ID3D10ShaderResourceView *srvSRGB);

		D3D10Effect *mEffect;
		Source mSource;
		std::size_t mRegister;
		ID3D10Texture2D *mTexture;
		ID3D10ShaderResourceView *mShaderResourceView[2];
		ID3D10RenderTargetView *mRenderTargetView[2];
	};
	struct D3D10Constant : public Effect::Constant
	{
		D3D10Constant(D3D10Effect *effect, const Description &desc);
		~D3D10Constant();

		inline bool AddAnnotation(const std::string &name, const Effect::Annotation &value)
		{
			return this->mAnnotations.emplace(name, value).second;
		}

		virtual void GetValue(unsigned char *data, std::size_t size) const override;
		virtual void SetValue(const unsigned char *data, std::size_t size) override;

		D3D10Effect *mEffect;
		std::size_t mBufferIndex, mBufferOffset;
	};
	struct D3D10Technique : public Effect::Technique
	{
		struct Pass
		{
			ID3D10VertexShader *VS;
			ID3D10PixelShader *PS;
			ID3D10BlendState *BS;
			ID3D10DepthStencilState *DSS;
			UINT StencilRef;
			ID3D10RenderTargetView *RT[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
			ID3D10ShaderResourceView *RTSRV[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
			D3D10_VIEWPORT Viewport;
			std::vector<ID3D10ShaderResourceView *> SRV;
		};

		D3D10Technique(D3D10Effect *effect, const Description &desc);
		~D3D10Technique();

		inline bool AddAnnotation(const std::string &name, const Effect::Annotation &value)
		{
			return this->mAnnotations.emplace(name, value).second;
		}
		inline void AddPass(const Pass &pass)
		{
			this->mPasses.push_back(pass);
		}

		virtual void RenderPass(unsigned int index) const override;

		D3D10Effect *mEffect;
		std::vector<Pass> mPasses;
	};
} }