#pragma once

#include "Runtime.hpp"
#include "Effect.hpp"

#include <d3d9.h>
#include <vector>
#include <unordered_map>

namespace ReShade
{
	struct D3D9DepthStencilInfo
	{
		UINT Width, Height;
		FLOAT DrawCallCount, DrawVerticesCount;
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

		bool OnCreateInternal(const D3DPRESENT_PARAMETERS &params);
		void OnDeleteInternal();
		void OnDrawInternal(D3DPRIMITIVETYPE type, UINT count);
		void OnPresentInternal();

		virtual std::unique_ptr<Effect> CreateEffect(const EffectTree &ast, std::string &errors) const override;
		virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const override;

		void DetectBestDepthStencil();
		bool CreateDepthStencil(IDirect3DSurface9 *depthstencil);
		void ReplaceDepthStencil(IDirect3DSurface9 *&depthstencil);

		IDirect3DDevice9 *mDevice;
		IDirect3DSwapChain9 *mSwapChain;
		IDirect3D9 *mDirect3D;
		D3DCAPS9 mDeviceCaps;
		D3DPRESENT_PARAMETERS mPresentParams;
		IDirect3DStateBlock9 *mStateBlock;
		IDirect3DSurface9 *mBackBuffer, *mBackBufferResolved, *mBackBufferTextureSurface;
		IDirect3DTexture9 *mBackBufferTexture;
		IDirect3DSurface9 *mDepthStencil, *mDepthStencilReplacement;
		IDirect3DTexture9 *mDepthStencilTexture;
		std::unordered_map<IDirect3DSurface9 *, D3D9DepthStencilInfo> mDepthStencilTable;
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

		virtual const Texture *GetTexture(const std::string &name) const override;
		virtual std::vector<std::string> GetTextureNames() const override;
		virtual const Constant *GetConstant(const std::string &name) const override;
		virtual std::vector<std::string> GetConstantNames() const override;
		virtual const Technique *GetTechnique(const std::string &name) const override;
		virtual std::vector<std::string> GetTechniqueNames() const override;

		std::shared_ptr<const D3D9Runtime> mEffectContext;
		std::unordered_map<std::string, std::unique_ptr<D3D9Texture>> mTextures;
		std::vector<D3D9Sampler> mSamplers;
		std::unordered_map<std::string, std::unique_ptr<D3D9Constant>> mConstants;
		std::unordered_map<std::string, std::unique_ptr<D3D9Technique>> mTechniques;
		IDirect3DSurface9 *mDepthStencil;
		IDirect3DVertexBuffer9 *mVertexBuffer;
		IDirect3DVertexDeclaration9 *mVertexDeclaration;
		float *mConstantStorage;
		UINT mConstantRegisterCount;
	};
	struct D3D9Texture : public Effect::Texture
	{
		enum class Source
		{
			Memory,
			BackBuffer,
			DepthStencil
		};

		D3D9Texture(D3D9Effect *effect, const Description &desc);
		~D3D9Texture();

		inline bool AddAnnotation(const std::string &name, const Effect::Annotation &value)
		{
			return this->mAnnotations.emplace(name, value).second;
		}

		virtual bool Update(unsigned int level, const unsigned char *data, std::size_t size) override;
		bool UpdateSource(IDirect3DTexture9 *texture);

		D3D9Effect *mEffect;
		Source mSource;
		IDirect3DTexture9 *mTexture;
		IDirect3DSurface9 *mTextureSurface;
	};
	struct D3D9Sampler
	{
		DWORD mStates[12];
		D3D9Texture *mTexture;
	};
	struct D3D9Constant : public Effect::Constant
	{
		D3D9Constant(D3D9Effect *effect, const Description &desc);
		~D3D9Constant();

		inline bool AddAnnotation(const std::string &name, const Effect::Annotation &value)
		{
			return this->mAnnotations.emplace(name, value).second;
		}

		virtual void GetValue(unsigned char *data, std::size_t size) const override;
		virtual void SetValue(const unsigned char *data, std::size_t size) override;

		D3D9Effect *mEffect;
		UINT mStorageOffset;
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

		D3D9Effect *mEffect;
		std::vector<Pass> mPasses;
	};
}