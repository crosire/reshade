#pragma once

#include "Runtime.hpp"
#include "Effect.hpp"

#include <d3d9.h>
#include <vector>
#include <unordered_map>

namespace ReShade
{
	struct DepthStencilInfo
	{
		UINT Width, Height;
		UINT DrawCallCount;
	};
	struct D3D9Runtime : public Runtime, public std::enable_shared_from_this<D3D9Runtime>
	{
		friend struct D3D9Effect;
		friend struct D3D9Texture;
		friend struct D3D9Sampler;
		friend struct D3D9Constant;
		friend struct D3D9Technique;

		D3D9Runtime(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain);
		~D3D9Runtime();

		virtual bool OnCreate(unsigned int width, unsigned int height) override;
		virtual void OnDelete() override;
		virtual void OnPresent() override;
		void OnDraw(UINT primitives);

		virtual std::unique_ptr<Effect> CreateEffect(const EffectTree &ast, std::string &errors) const override;
		virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const override;

		void DetectBestDepthStencil();
		void ReplaceDepthStencil(IDirect3DSurface9 **depthstencil);

		IDirect3DDevice9 *mDevice;
		IDirect3DSwapChain9 *mSwapChain;
		IDirect3DStateBlock9 *mStateBlock;
		IDirect3DSurface9 *mBackBuffer;
		IDirect3DSurface9 *mBackBufferNotMultisampled;
		IDirect3DTexture9 *mDepthStencilTexture;
		std::unordered_map<IDirect3DSurface9 *, DepthStencilInfo> mDepthStencilTable;
		IDirect3DSurface9 *mBestDepthStencil, *mBestDepthStencilReplacement;
		bool mLost;
	};
	struct D3D9Effect : public Effect
	{
		friend struct D3D9Texture;
		friend struct D3D9Sampler;
		friend struct D3D9Constant;
		friend struct D3D9Technique;

		D3D9Effect(std::shared_ptr<const D3D9Runtime> context);
		~D3D9Effect();

		const Texture *GetTexture(const std::string &name) const;
		std::vector<std::string> GetTextureNames() const;
		const Constant *GetConstant(const std::string &name) const;
		std::vector<std::string> GetConstantNames() const;
		const Technique *GetTechnique(const std::string &name) const;
		std::vector<std::string> GetTechniqueNames() const;

		std::shared_ptr<const D3D9Runtime> mEffectContext;
		std::unordered_map<std::string,std::unique_ptr<D3D9Texture>> mTextures;
		std::vector<D3D9Sampler> mSamplers;
		std::unordered_map<std::string, std::unique_ptr<D3D9Constant>> mConstants;
		std::unordered_map<std::string, std::unique_ptr<D3D9Technique>> mTechniques;
		IDirect3DSurface9 *mDepthStencil;
		IDirect3DVertexDeclaration9 *mVertexDeclaration;
		IDirect3DVertexBuffer9 *mVertexBuffer;
		float *mConstantStorage;
		UINT mConstantRegisterCount;
	};
	struct D3D9Texture : public Effect::Texture
	{
		D3D9Texture(D3D9Effect *effect);
		~D3D9Texture();

		const Description GetDescription() const;
		const Effect::Annotation GetAnnotation(const std::string &name) const;

		void Update(unsigned int level, const unsigned char *data, std::size_t size);
		void UpdateFromColorBuffer();
		void UpdateFromDepthBuffer();

		D3D9Effect *mEffect;
		Description mDesc;
		std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
		IDirect3DTexture9 *mTexture;
		IDirect3DSurface9 *mSurface;
	};
	struct D3D9Sampler
	{
		DWORD mStates[12];
		D3D9Texture *mTexture;
	};
	struct D3D9Constant : public Effect::Constant
	{
		D3D9Constant(D3D9Effect *effect);
		~D3D9Constant();

		const Description GetDescription() const;
		const Effect::Annotation GetAnnotation(const std::string &name) const;
		void GetValue(unsigned char *data, std::size_t size) const;
		void SetValue(const unsigned char *data, std::size_t size);

		D3D9Effect *mEffect;
		Description mDesc;
		UINT mRegisterOffset;
		std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
	};
	struct D3D9Technique : public Effect::Technique
	{
		struct Pass
		{
			IDirect3DVertexShader9 *VS;
			IDirect3DPixelShader9 *PS;
			IDirect3DStateBlock9 *Stateblock;
			IDirect3DSurface9 *RT[8];
		};

		D3D9Technique(D3D9Effect *effect);
		~D3D9Technique();

		const Effect::Annotation GetAnnotation(const std::string &name) const;

		bool Begin(unsigned int &passes) const;
		void End() const;
		void RenderPass(unsigned int index) const;

		D3D9Effect *mEffect;
		std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
		std::vector<Pass> mPasses;
	};
}