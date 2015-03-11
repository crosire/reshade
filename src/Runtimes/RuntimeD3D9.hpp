#pragma once

#include "Runtime.hpp"

#include <d3d9.h>

namespace ReShade
{
	namespace Runtimes
	{
		struct D3D9Runtime : public Runtime, public std::enable_shared_from_this<D3D9Runtime>
		{
			friend struct D3D9Technique;

			struct DepthSourceInfo
			{
				UINT Width, Height;
				FLOAT DrawCallCount, DrawVerticesCount;
			};

			D3D9Runtime(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain);
			~D3D9Runtime();

			bool OnCreateInternal(const D3DPRESENT_PARAMETERS &params);
			void OnDeleteInternal();
			void OnDrawInternal(D3DPRIMITIVETYPE type, UINT count);
			void OnPresentInternal();
			void OnDeleteDepthStencilSurface(IDirect3DSurface9 *depthstencil);
			void OnSetDepthStencilSurface(IDirect3DSurface9 *&depthstencil);
			void OnGetDepthStencilSurface(IDirect3DSurface9 *&depthstencil);

			void DetectDepthSource();
			bool CreateDepthStencilReplacement(IDirect3DSurface9 *depthstencil);

			virtual std::unique_ptr<FX::Effect> CompileEffect(const FX::Tree &ast, std::string &errors) const override;
			virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const override;

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
			IDirect3DSurface9 *mDefaultDepthStencil;
			std::unordered_map<IDirect3DSurface9 *, DepthSourceInfo> mDepthSourceTable;
			bool mLost;
		};

		struct D3D9Effect : public FX::Effect
		{
			friend struct D3D9Runtime;

			D3D9Effect(std::shared_ptr<const D3D9Runtime> runtime);
			~D3D9Effect();

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

			virtual void Enter() const override;
			virtual void Leave() const override;

			std::shared_ptr<const D3D9Runtime> mRuntime;
			IDirect3DVertexBuffer9 *mVertexBuffer;
			IDirect3DVertexDeclaration9 *mVertexDeclaration;
			float *mConstantStorage;
			UINT mConstantRegisterCount;
		};
		struct D3D9Texture : public FX::Effect::Texture
		{
			enum class Source
			{
				Memory,
				BackBuffer,
				DepthStencil
			};

			D3D9Texture(D3D9Effect *effect, const Description &desc);
			~D3D9Texture();

			inline bool AddAnnotation(const std::string &name, const FX::Effect::Annotation &value)
			{
				return this->mAnnotations.emplace(name, value).second;
			}

			virtual bool Update(unsigned int level, const unsigned char *data, std::size_t size) override;
			void ChangeSource(IDirect3DTexture9 *texture);

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
		struct D3D9Constant : public FX::Effect::Constant
		{
			D3D9Constant(D3D9Effect *effect, const Description &desc);
			~D3D9Constant();

			inline bool AddAnnotation(const std::string &name, const FX::Effect::Annotation &value)
			{
				return this->mAnnotations.emplace(name, value).second;
			}

			virtual void GetValue(unsigned char *data, std::size_t size) const override;
			virtual void SetValue(const unsigned char *data, std::size_t size) override;

			D3D9Effect *mEffect;
		};
		struct D3D9Technique : public FX::Effect::Technique
		{
			struct Pass
			{
				IDirect3DVertexShader9 *VS;
				IDirect3DPixelShader9 *PS;
				D3D9Sampler Samplers[16];
				DWORD SamplerCount;
				IDirect3DStateBlock9 *Stateblock;
				IDirect3DSurface9 *RT[8];
			};

			D3D9Technique(D3D9Effect *effect, const Description &desc);
			~D3D9Technique();

			inline bool AddAnnotation(const std::string &name, const FX::Effect::Annotation &value)
			{
				return this->mAnnotations.emplace(name, value).second;
			}
			inline void AddPass(const Pass &pass)
			{
				this->mPasses.push_back(pass);
			}

			virtual void RenderPass(unsigned int index) const override;

			D3D9Effect *mEffect;
			std::vector<Pass> mPasses;
		};
	}
}