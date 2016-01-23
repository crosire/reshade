#pragma once

#include "runtime.hpp"

#include <algorithm>
#include <d3d9.h>

namespace reshade
{
	namespace runtimes
	{
		class d3d9_runtime : public runtime
		{
		public:
			d3d9_runtime(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain);
			~d3d9_runtime();

			bool on_init(const D3DPRESENT_PARAMETERS &pp);
			void on_reset() override;
			void on_present() override;
			void on_draw_call(D3DPRIMITIVETYPE type, UINT count);
			void on_apply_effect() override;
			void on_apply_effect_technique(const technique *technique) override;
			void on_set_depthstencil_surface(IDirect3DSurface9 *&depthstencil);
			void on_get_depthstencil_surface(IDirect3DSurface9 *&depthstencil);

			void enlarge_uniform_data_storage()
			{
				_uniform_data_storage.resize(_uniform_data_storage.size() + 64 * sizeof(float));
			}
			unsigned char *get_uniform_data_storage()
			{
				return _uniform_data_storage.data();
			}
			size_t get_uniform_data_storage_size() const
			{
				return _uniform_data_storage.size();
			}
			void add_texture(texture *x)
			{
				_textures.push_back(std::unique_ptr<texture>(x));
			}
			void add_uniform(uniform *x)
			{
				_uniforms.push_back(std::unique_ptr<uniform>(x));
			}
			void add_technique(technique *x)
			{
				_techniques.push_back(std::unique_ptr<technique>(x));
			}

			IDirect3D9 *_d3d;
			IDirect3DDevice9 *_device;
			IDirect3DSwapChain9 *_swapchain;

			IDirect3DSurface9 *_backbuffer, *_backbuffer_resolved, *_backbuffer_texture_surface;
			IDirect3DTexture9 *_backbuffer_texture;
			IDirect3DTexture9 *_depthstencil_texture;
			UINT _constant_register_count;

		private:
			struct depth_source_info
			{
				UINT width, height;
				FLOAT drawcall_count, vertices_count;
			};

			void screenshot(unsigned char *buffer) const override;
			bool update_effect(const fx::nodetree &ast, const std::vector<std::string> &pragmas, std::string &errors) override;
			bool update_texture(texture *texture, const unsigned char *data, size_t size) override;

			void detect_depth_source();
			bool create_depthstencil_replacement(IDirect3DSurface9 *depthstencil);

			UINT _behavior_flags, _num_simultaneous_rendertargets;
			bool _is_multisampling_enabled;
			D3DFORMAT _backbuffer_format;
			IDirect3DStateBlock9 *_stateblock;
			IDirect3DSurface9 *_depthstencil, *_depthstencil_replacement;
			IDirect3DSurface9 *_default_depthstencil;
			std::unordered_map<IDirect3DSurface9 *, depth_source_info> _depth_source_table;

			IDirect3DVertexBuffer9 *_effect_triangle_buffer;
			IDirect3DVertexDeclaration9 *_effect_triangle_layout;
		};
	}
}
