#pragma once

#include "Runtime.hpp"
#include "Effect.hpp"

#include <d3d11_1.h>
#include <vector>
#include <unordered_map>

namespace ReShade { namespace Runtimes
{
	struct D3D11_SAMPLER_DESC_HASHER
	{
		inline std::size_t operator()(const D3D11_SAMPLER_DESC &s) const 
		{
			const unsigned char *p = reinterpret_cast<const unsigned char *>(&s);
			std::size_t h = 2166136261;

			for (std::size_t i = 0; i < sizeof(D3D11_SAMPLER_DESC); ++i)
			{
				h = (h * 16777619) ^ p[i];
			}

			return h;
		}
	};

	struct D3D11DepthStencilInfo
	{
		UINT Width, Height;
		FLOAT DrawCallCount, DrawVerticesCount;
	};
	struct D3D11Runtime : public Runtime, public std::enable_shared_from_this<D3D11Runtime>
	{
		friend struct D3D11Effect;
		friend struct D3D11Texture;
		friend struct D3D11Constant;
		friend struct D3D11Technique;

		D3D11Runtime(ID3D11Device *device, IDXGISwapChain *swapchain);
		~D3D11Runtime();

		bool OnCreateInternal(const DXGI_SWAP_CHAIN_DESC &desc);
		void OnDeleteInternal();
		void OnDrawInternal(ID3D11DeviceContext *context, unsigned int vertices);
		void OnPresentInternal();
		void OnCreateDepthStencil(ID3D11Resource *resource, ID3D11DepthStencilView *depthstencil);

		virtual std::unique_ptr<Effect> CompileEffect(const EffectTree &ast, std::string &errors) const override;
		virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const override;

		void DetectBestDepthStencil();
		bool CreateBackBuffer(ID3D11Texture2D *backbuffer, const DXGI_SAMPLE_DESC &samples);
		bool CreateDepthStencil(ID3D11DepthStencilView *depthstencil);
		void ReplaceBackBuffer(ID3D11Texture2D *&backbuffer);
		void ReplaceDepthStencil(ID3D11DepthStencilView *&depthstencil);
		void ReplaceDepthStencilResource(ID3D11Resource *&depthstencil);

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
		std::unordered_map<ID3D11DepthStencilView *, D3D11DepthStencilInfo> mDepthStencilTable;
		CRITICAL_SECTION mCS;
		bool mLost;
	};
	struct D3D11Effect : public Effect
	{
		friend struct D3D11Texture;
		friend struct D3D11Constant;
		friend struct D3D11Technique;

		D3D11Effect(std::shared_ptr<const D3D11Runtime> context);
		~D3D11Effect();

		virtual const Texture *GetTexture(const std::string &name) const override;
		virtual std::vector<std::string> GetTextureNames() const override;
		virtual const Constant *GetConstant(const std::string &name) const override;
		virtual std::vector<std::string> GetConstantNames() const override;
		virtual const Technique *GetTechnique(const std::string &name) const override;
		virtual std::vector<std::string> GetTechniqueNames() const override;

		void UpdateConstants() const;

		std::shared_ptr<const D3D11Runtime> mEffectContext;
		std::unordered_map<std::string, std::unique_ptr<D3D11Texture>> mTextures;
		std::unordered_map<std::string, std::unique_ptr<D3D11Constant>> mConstants;
		std::unordered_map<std::string, std::unique_ptr<D3D11Technique>> mTechniques;
		ID3D11RasterizerState *mRasterizerState;
		std::unordered_map<D3D11_SAMPLER_DESC, size_t, D3D11_SAMPLER_DESC_HASHER> mSamplerDescs;
		std::vector<ID3D11SamplerState *> mSamplerStates;
		std::vector<ID3D11ShaderResourceView *> mShaderResources;
		std::vector<ID3D11Buffer *> mConstantBuffers;
		std::vector<unsigned char *> mConstantStorages;
		mutable bool mConstantsDirty;
	};
	struct D3D11Texture : public Effect::Texture
	{
		enum class Source
		{
			Memory,
			BackBuffer,
			DepthStencil
		};

		D3D11Texture(D3D11Effect *effect, const Description &desc);
		~D3D11Texture();

		inline bool AddAnnotation(const std::string &name, const Effect::Annotation &value)
		{
			return this->mAnnotations.emplace(name, value).second;
		}

		virtual bool Update(unsigned int level, const unsigned char *data, std::size_t size) override;
		bool UpdateSource(ID3D11ShaderResourceView *srv, ID3D11ShaderResourceView *srvSRGB);

		D3D11Effect *mEffect;
		Source mSource;
		unsigned int mRegister;
		ID3D11Texture2D *mTexture;
		ID3D11ShaderResourceView *mShaderResourceView[2];
		ID3D11RenderTargetView *mRenderTargetView[2];
	};
	struct D3D11Constant : public Effect::Constant
	{
		D3D11Constant(D3D11Effect *effect, const Description &desc);
		~D3D11Constant();

		inline bool AddAnnotation(const std::string &name, const Effect::Annotation &value)
		{
			return this->mAnnotations.emplace(name, value).second;
		}

		virtual void GetValue(unsigned char *data, std::size_t size) const override;
		virtual void SetValue(const unsigned char *data, std::size_t size) override;

		D3D11Effect *mEffect;
		std::size_t mBuffer, mBufferOffset;
	};
	struct D3D11Technique : public Effect::Technique
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

		D3D11Technique(D3D11Effect *effect);
		~D3D11Technique();

		inline bool AddAnnotation(const std::string &name, const Effect::Annotation &value)
		{
			return this->mAnnotations.emplace(name, value).second;
		}
		inline void AddPass(const Pass &pass)
		{
			this->mPasses.push_back(pass);
		}

		virtual bool Begin(unsigned int &passes) const override;
		virtual void End() const override;
		virtual void RenderPass(unsigned int index) const override;

		D3D11Effect *mEffect;
		std::vector<Pass> mPasses;
	};
} }