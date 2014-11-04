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

	struct D3D10Runtime : public Runtime, public std::enable_shared_from_this<D3D10Runtime>
	{
		friend struct D3D10Effect;
		friend struct D3D10Texture;
		friend struct D3D10Constant;
		friend struct D3D10Technique;

		D3D10Runtime(ID3D10Device *device, IDXGISwapChain *swapchain);
		~D3D10Runtime();

		virtual bool OnCreate(unsigned int width, unsigned int height) override;
		virtual void OnDelete() override;
		virtual void OnPresent() override;

		virtual std::unique_ptr<Effect> CreateEffect(const EffectTree &ast, std::string &errors) const override;
		virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const override;

		ID3D10Device *mDevice;
		IDXGISwapChain *mSwapChain;
		ID3D10StateBlock *mStateBlock;
		ID3D10Texture2D *mBackBuffer;
		ID3D10Texture2D *mBackBufferTexture;
		ID3D10RenderTargetView *mBackBufferTargets[2];
		bool mLost;
	};
	struct D3D10Effect : public Effect
	{
		friend struct D3D10Texture;
		friend struct D3D10Constant;
		friend struct D3D10Technique;

		D3D10Effect(std::shared_ptr<const D3D10Runtime> context);
		~D3D10Effect();

		const Texture *GetTexture(const std::string &name) const;
		std::vector<std::string> GetTextureNames() const;
		const Constant *GetConstant(const std::string &name) const;
		std::vector<std::string> GetConstantNames() const;
		const Technique *GetTechnique(const std::string &name) const;
		std::vector<std::string> GetTechniqueNames() const;

		void ApplyConstants() const;

		std::shared_ptr<const D3D10Runtime> mEffectContext;
		std::unordered_map<std::string, std::unique_ptr<D3D10Texture>> mTextures;
		std::unordered_map<std::string, std::unique_ptr<D3D10Constant>> mConstants;
		std::unordered_map<std::string, std::unique_ptr<D3D10Technique>> mTechniques;
		ID3D10Texture2D *mDepthStencilTexture;
		ID3D10ShaderResourceView *mDepthStencilView;
		ID3D10DepthStencilView *mDepthStencil;
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
		D3D10Texture(D3D10Effect *effect);
		~D3D10Texture();

		const Description GetDescription() const;
		const Effect::Annotation GetAnnotation(const std::string &name) const;

		void Update(unsigned int level, const unsigned char *data, std::size_t size);
		void UpdateFromColorBuffer();
		void UpdateFromDepthBuffer();

		D3D10Effect *mEffect;
		Description mDesc;
		unsigned int mRegister;
		std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
		ID3D10Texture2D *mTexture;
		ID3D10ShaderResourceView *mShaderResourceView[2];
		ID3D10RenderTargetView *mRenderTargetView[2];
	};
	struct D3D10Constant : public Effect::Constant
	{
		D3D10Constant(D3D10Effect *effect);
		~D3D10Constant();

		const Description GetDescription() const;
		const Effect::Annotation GetAnnotation(const std::string &name) const;
		void GetValue(unsigned char *data, std::size_t size) const;
		void SetValue(const unsigned char *data, std::size_t size);

		D3D10Effect *mEffect;
		Description mDesc;
		std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
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
			std::vector<ID3D10ShaderResourceView *> SR;
		};

		D3D10Technique(D3D10Effect *effect);
		~D3D10Technique();

		const Effect::Annotation GetAnnotation(const std::string &name) const;

		bool Begin(unsigned int &passes) const;
		void End() const;
		void RenderPass(unsigned int index) const;

		D3D10Effect *mEffect;
		std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
		std::vector<Pass> mPasses;
	};
}