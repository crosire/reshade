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

			bool OnInit(const D3DPRESENT_PARAMETERS &pp);
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

			inline unsigned int GetWidth() const
			{
				return this->mWidth;
			}
			inline unsigned int GetHeight() const
			{
				return this->mHeight;
			}
			inline Texture *GetTexture(const std::string &name) const
			{
				const auto it = std::find_if(this->mTextures.cbegin(), this->mTextures.cend(), [name](const std::unique_ptr<Texture> &it) { return it->Name == name; });

				return it != this->mTextures.cend() ? it->get() : nullptr;
			}
			inline void EnlargeConstantStorage()
			{
				this->mUniformDataStorage.resize(this->mUniformDataStorage.size() + 64 * sizeof(float));
			}
			inline unsigned char *GetConstantStorage()
			{
				return this->mUniformDataStorage.data();
			}
			inline size_t GetConstantStorageSize() const
			{
				return this->mUniformDataStorage.size();
			}
			inline void AddTexture(Texture *texture)
			{
				this->mTextures.push_back(std::unique_ptr<Texture>(texture));
			}
			inline void AddConstant(Uniform *constant)
			{
				this->mUniforms.push_back(std::unique_ptr<Uniform>(constant));
			}
			inline void AddTechnique(Technique *technique)
			{
				this->mTechniques.push_back(std::unique_ptr<Technique>(technique));
			}

			IDirect3D9 *mDirect3D;
			IDirect3DDevice9 *mDevice;
			IDirect3DSwapChain9 *mSwapChain;

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
			bool mMultisamplingEnabled;
			D3DFORMAT mBackBufferFormat;
			IDirect3DStateBlock9 *mStateBlock;
			IDirect3DSurface9 *mDepthStencil, *mDepthStencilReplacement;
			IDirect3DSurface9 *mDefaultDepthStencil;
			std::unordered_map<IDirect3DSurface9 *, DepthSourceInfo> mDepthSourceTable;

			IDirect3DVertexBuffer9 *mEffectTriangleBuffer;
			IDirect3DVertexDeclaration9 *mEffectTriangleLayout;
		};
	}
}
