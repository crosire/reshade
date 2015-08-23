#pragma once

#include "Runtime.hpp"

#include <d3d9.h>

namespace ReShade
{
	namespace Runtimes
	{
		class D3D9Runtime : public Runtime
		{
		public:
			D3D9Runtime(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain);
			~D3D9Runtime();

			bool OnInit(const D3DPRESENT_PARAMETERS &params);
			void OnReset() override;
			void OnPresent() override;
			void OnDrawCall(D3DPRIMITIVETYPE type, UINT count);
			void OnApplyEffect() override;
			void OnApplyEffectTechnique(const Technique *technique) override;
			void OnSetDepthStencilSurface(IDirect3DSurface9 *&depthstencil);
			void OnGetDepthStencilSurface(IDirect3DSurface9 *&depthstencil);

			void Screenshot(unsigned char *buffer) const override;
			bool UpdateEffect(const FX::Tree &ast, const std::vector<std::string> &pragmas, std::string &errors) override;
			bool UpdateTexture(Texture *texture, const unsigned char *data, std::size_t size) override;

			void DetectDepthSource();
			bool CreateDepthStencilReplacement(IDirect3DSurface9 *depthstencil);

			inline Texture *GetTexture(const std::string &name) const
			{
				const auto it = std::find_if(this->mTextures.cbegin(), this->mTextures.cend(), [name](const std::unique_ptr<Texture> &it) { return it->Name == name; });

				return it != this->mTextures.cend() ? it->get() : nullptr;
			}
			inline void EnlargeConstantStorage()
			{
				this->mConstantStorage = static_cast<unsigned char *>(::realloc(this->mConstantStorage, this->mConstantStorageSize += sizeof(float) * 64));
			}
			inline unsigned char *GetConstantStorage() const
			{
				return this->mConstantStorage;
			}
			inline size_t GetConstantStorageSize() const
			{
				return this->mConstantStorageSize;
			}
			inline void AddTexture(Texture *texture)
			{
				this->mTextures.push_back(std::unique_ptr<Texture>(texture));
			}
			inline void AddConstant(Constant *constant)
			{
				this->mConstants.push_back(std::unique_ptr<Constant>(constant));
			}
			inline void AddTechnique(Technique *technique)
			{
				this->mTechniques.push_back(std::unique_ptr<Technique>(technique));
			}

			IDirect3D9 *mDirect3D;
			IDirect3DDevice9 *mDevice;
			IDirect3DSwapChain9 *mSwapChain;

			D3DPRESENT_PARAMETERS mPresentParams;
			IDirect3DSurface9 *mBackBuffer, *mBackBufferResolved, *mBackBufferTextureSurface;
			IDirect3DTexture9 *mBackBufferTexture;
			IDirect3DTexture9 *mDepthStencilTexture;
			UINT mConstantRegisterCount;

		private:
			struct DepthSourceInfo
			{
				UINT Width, Height;
				FLOAT DrawCallCount, DrawVerticesCount;
			};

			UINT mBehaviorFlags, mNumSimultaneousRTs;

			IDirect3DStateBlock9 *mStateBlock;
			IDirect3DSurface9 *mDepthStencil, *mDepthStencilReplacement;
			IDirect3DSurface9 *mDefaultDepthStencil;
			std::unordered_map<IDirect3DSurface9 *, DepthSourceInfo> mDepthSourceTable;

			IDirect3DVertexBuffer9 *mEffectVertexBuffer;
			IDirect3DVertexDeclaration9 *mEffectVertexDeclaration;
		};
	}
}
