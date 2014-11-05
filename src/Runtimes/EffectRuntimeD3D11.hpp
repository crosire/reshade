#pragma once

#include "Runtime.hpp"
#include "Effect.hpp"

#include <d3d11_1.h>
#include <vector>
#include <unordered_map>

namespace ReShade
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
		UINT DrawCallCount;
	};
	struct D3D11Runtime : public Runtime, public std::enable_shared_from_this<D3D11Runtime>
	{
		friend struct D3D11Effect;
		friend struct D3D11Texture;
		friend struct D3D11Constant;
		friend struct D3D11Technique;

		D3D11Runtime(ID3D11Device *device, IDXGISwapChain *swapchain);
		~D3D11Runtime();

		virtual bool OnCreate(unsigned int width, unsigned int height) override;
		virtual void OnDelete() override;
		virtual void OnPresent() override;
		void OnDraw(ID3D11DeviceContext *context, UINT vertices);

		virtual std::unique_ptr<Effect> CreateEffect(const EffectTree &ast, std::string &errors) const override;
		virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const override;

		void DetectBestDepthStencil();
		void CreateDepthStencil(ID3D11Resource *resource, ID3D11DepthStencilView *depthstencil);
		void ReplaceDepthStencil(ID3D11DeviceContext *context, ID3D11DepthStencilView **depthstencil);

		ID3D11Device *mDevice;
		ID3D11DeviceContext *mImmediateContext;
		ID3D11DeviceContext *mDeferredContext;
		IDXGISwapChain *mSwapChain;
		ID3D11Texture2D *mBackBuffer;
		ID3D11Texture2D *mBackBufferTexture;
		ID3D11RenderTargetView *mBackBufferTargets[2];
		std::unordered_map<ID3D11DepthStencilView *, D3D11DepthStencilInfo> mDepthStencilTable;
		ID3D11DepthStencilView *mBestDepthStencil, *mBestDepthStencilReplacement;
		ID3D11ShaderResourceView *mDepthStencilShaderResourceView;
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

		const Texture *GetTexture(const std::string &name) const;
		std::vector<std::string> GetTextureNames() const;
		const Constant *GetConstant(const std::string &name) const;
		std::vector<std::string> GetConstantNames() const;
		const Technique *GetTechnique(const std::string &name) const;
		std::vector<std::string> GetTechniqueNames() const;

		void ApplyConstants() const;

		std::shared_ptr<const D3D11Runtime> mEffectContext;
		std::unordered_map<std::string, std::unique_ptr<D3D11Texture>> mTextures;
		std::unordered_map<std::string, std::unique_ptr<D3D11Constant>> mConstants;
		std::unordered_map<std::string, std::unique_ptr<D3D11Technique>> mTechniques;
		ID3D11Texture2D *mDepthStencilTexture;
		ID3D11ShaderResourceView *mDepthStencilView;
		ID3D11DepthStencilView *mDepthStencil;
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
		D3D11Texture(D3D11Effect *effect);
		~D3D11Texture();

		const Description GetDescription() const;
		const Effect::Annotation GetAnnotation(const std::string &name) const;

		void Update(unsigned int level, const unsigned char *data, std::size_t size);
		void UpdateFromColorBuffer();
		void UpdateFromDepthBuffer();

		D3D11Effect *mEffect;
		Description mDesc;
		unsigned int mRegister;
		std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
		ID3D11Texture2D *mTexture;
		ID3D11ShaderResourceView *mShaderResourceView[2];
		ID3D11RenderTargetView *mRenderTargetView[2];
	};
	struct D3D11Constant : public Effect::Constant
	{
		D3D11Constant(D3D11Effect *effect);
		~D3D11Constant();

		const Description GetDescription() const;
		const Effect::Annotation GetAnnotation(const std::string &name) const;
		void GetValue(unsigned char *data, std::size_t size) const;
		void SetValue(const unsigned char *data, std::size_t size);

		D3D11Effect *mEffect;
		Description mDesc;
		std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
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
			std::vector<ID3D11ShaderResourceView *> SR;
		};

		D3D11Technique(D3D11Effect *effect);
		~D3D11Technique();

		const Effect::Annotation GetAnnotation(const std::string &name) const;

		bool Begin(unsigned int &passes) const;
		void End() const;
		void RenderPass(unsigned int index) const;

		D3D11Effect *mEffect;
		std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
		std::vector<Pass> mPasses;
	};
}