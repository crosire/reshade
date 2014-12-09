#pragma once

#include "Runtime.hpp"
#include "Effect.hpp"

#include <d3d10_1.h>
#include <vector>
#include <unordered_map>

namespace ReShade
{
	struct D3D10_SAMPLER_DESC_HASHER
	{
		inline std::size_t operator()(const D3D10_SAMPLER_DESC &s) const 
		{
			const unsigned char *p = reinterpret_cast<const unsigned char *>(&s);
			std::size_t h = 2166136261;

			for (std::size_t i = 0; i < sizeof(D3D10_SAMPLER_DESC); ++i)
			{
				h = (h * 16777619) ^ p[i];
			}

			return h;
		}
	};

	struct D3D10DepthStencilInfo
	{
		UINT Width, Height;
		FLOAT DrawCallCount, DrawVerticesCount;
	};
	struct D3D10Runtime : public Runtime, public std::enable_shared_from_this<D3D10Runtime>
	{
		friend struct D3D10Effect;
		friend struct D3D10Texture;
		friend struct D3D10Constant;
		friend struct D3D10Technique;

		D3D10Runtime(ID3D10Device *device, IDXGISwapChain *swapchain);
		~D3D10Runtime();

		bool OnCreateInternal(const DXGI_SWAP_CHAIN_DESC &desc);
		void OnDeleteInternal();
		void OnDrawInternal(unsigned int vertices);
		void OnPresentInternal();
		void OnCreateDepthStencil(ID3D10Resource *resource, ID3D10DepthStencilView *depthstencil);

		virtual std::unique_ptr<Effect> CompileEffect(const EffectTree &ast, std::string &errors) const override;
		virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const override;

		void DetectBestDepthStencil();
		bool CreateBackBuffer(ID3D10Texture2D *backbuffer, const DXGI_SAMPLE_DESC &samples);
		bool CreateDepthStencil(ID3D10DepthStencilView *depthstencil);
		void ReplaceBackBuffer(ID3D10Texture2D *&backbuffer);
		void ReplaceDepthStencil(ID3D10DepthStencilView *&depthstencil);
		void ReplaceDepthStencilResource(ID3D10Resource *&depthstencil);

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
		std::unordered_map<ID3D10DepthStencilView *, D3D10DepthStencilInfo> mDepthStencilTable;
		bool mLost;
	};
	struct D3D10Effect : public Effect
	{
		friend struct D3D10Texture;
		friend struct D3D10Constant;
		friend struct D3D10Technique;

		D3D10Effect(std::shared_ptr<const D3D10Runtime> context);
		~D3D10Effect();

		virtual const Texture *GetTexture(const std::string &name) const override;
		virtual std::vector<std::string> GetTextureNames() const override;
		virtual const Constant *GetConstant(const std::string &name) const override;
		virtual std::vector<std::string> GetConstantNames() const override;
		virtual const Technique *GetTechnique(const std::string &name) const override;
		virtual std::vector<std::string> GetTechniqueNames() const override;

		void UpdateConstants() const;

		std::shared_ptr<const D3D10Runtime> mEffectContext;
		std::unordered_map<std::string, std::unique_ptr<D3D10Texture>> mTextures;
		std::unordered_map<std::string, std::unique_ptr<D3D10Constant>> mConstants;
		std::unordered_map<std::string, std::unique_ptr<D3D10Technique>> mTechniques;
		ID3D10RasterizerState *mRasterizerState;
		std::unordered_map<D3D10_SAMPLER_DESC, size_t, D3D10_SAMPLER_DESC_HASHER> mSamplerDescs;
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
		bool UpdateSource(ID3D10ShaderResourceView *srv, ID3D10ShaderResourceView *srvSRGB);

		D3D10Effect *mEffect;
		Source mSource;
		unsigned int mRegister;
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
		std::size_t mBuffer, mBufferOffset;
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

		D3D10Technique(D3D10Effect *effect);
		~D3D10Technique();

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

		D3D10Effect *mEffect;
		std::vector<Pass> mPasses;
	};
}