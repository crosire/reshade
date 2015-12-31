#pragma once

#include "Runtime.hpp"

#include <algorithm>
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

			Texture *GetTexture(const std::string &name) const
			{
				const auto it = std::find_if(_textures.cbegin(), _textures.cend(), [name](const std::unique_ptr<Texture> &it) { return it->Name == name; });

				return it != _textures.cend() ? it->get() : nullptr;
			}
			void EnlargeConstantStorage()
			{
				_uniformDataStorage.resize(_uniformDataStorage.size() + 64 * sizeof(float));
			}
			unsigned char *GetConstantStorage()
			{
				return _uniformDataStorage.data();
			}
			size_t GetConstantStorageSize() const
			{
				return _uniformDataStorage.size();
			}
			void AddTexture(Texture *texture)
			{
				_textures.push_back(std::unique_ptr<Texture>(texture));
			}
			void AddConstant(Uniform *constant)
			{
				_uniforms.push_back(std::unique_ptr<Uniform>(constant));
			}
			void AddTechnique(Technique *technique)
			{
				_techniques.push_back(std::unique_ptr<Technique>(technique));
			}

			IDirect3D9 *_d3d;
			IDirect3DDevice9 *_device;
			IDirect3DSwapChain9 *_swapchain;

			IDirect3DSurface9 *_backbuffer, *_backbufferResolved, *_backbufferTextureSurface;
			IDirect3DTexture9 *_backbufferTexture;
			IDirect3DTexture9 *_depthStencilTexture;
			UINT _constantRegisterCount;

		private:
			struct DepthSourceInfo
			{
				UINT Width, Height;
				FLOAT DrawCallCount, DrawVerticesCount;
			};

			void Screenshot(unsigned char *buffer) const override;
			bool UpdateEffect(const FX::NodeTree &ast, const std::vector<std::string> &pragmas, std::string &errors) override;
			bool UpdateTexture(Texture *texture, const unsigned char *data, size_t size) override;

			void DetectDepthSource();
			bool CreateDepthStencilReplacement(IDirect3DSurface9 *depthstencil);

			UINT _behaviorFlags, _numSimultaneousRTs;
			bool _multisamplingEnabled;
			D3DFORMAT _backbufferFormat;
			IDirect3DStateBlock9 *_stateBlock;
			IDirect3DSurface9 *_depthStencil, *_depthStencilReplacement;
			IDirect3DSurface9 *_defaultDepthStencil;
			std::unordered_map<IDirect3DSurface9 *, DepthSourceInfo> _depthSourceTable;

			IDirect3DVertexBuffer9 *_effectTriangleBuffer;
			IDirect3DVertexDeclaration9 *_effectTriangleLayout;
		};
	}
}
