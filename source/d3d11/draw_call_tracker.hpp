#pragma once

#include <map>
#include <d3d11.h>
#include "com_ptr.hpp"

#define RESHADE_DX11_CAPTURE_DEPTH_BUFFERS 1
#define RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS 0

namespace reshade::d3d11
{
	class draw_call_tracker
	{
	public:
		struct draw_stats
		{
			UINT vertices = 0;
			UINT drawcalls = 0;
			UINT mapped = 0;
			UINT vs_uses = 0;
			UINT ps_uses = 0;
		};

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		struct cleared_depthstencil_info
		{
			com_ptr<ID3D11Texture2D> dsv_texture;
			com_ptr<ID3D11Texture2D> backup_texture;
		};
		struct intermediate_snapshot_info
		{
			draw_stats stats;
			std::map<ID3D11RenderTargetView *, draw_stats> additional_views;
		};

		static bool preserve_depth_buffers;
		static bool preserve_stencil_buffers;
		static unsigned int depth_stencil_clear_index;
		static unsigned int filter_depth_texture_format;
#endif

		UINT total_vertices() const { return _global_counter.vertices; }
		UINT total_drawcalls() const { return _global_counter.drawcalls; }

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		const auto &depth_buffer_counters() const { return _counters_per_used_depth_texture; }
		const auto &cleared_depth_textures() const { return _cleared_depth_textures; }
#endif
#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
		const auto &constant_buffer_counters() const { return _counters_per_constant_buffer; }
#endif

		void merge(const draw_call_tracker &source);
		void reset();

		void on_map(ID3D11Resource *pResource);
		void on_draw(ID3D11DeviceContext *context, UINT vertices);

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		void track_render_targets(UINT num_views, ID3D11RenderTargetView *const *views, ID3D11DepthStencilView *dsv);
		void track_cleared_depthstencil(ID3D11DeviceContext *context, UINT clear_flags, ID3D11DepthStencilView *dsv, UINT clear_index, class runtime_d3d11 *runtime);

		static bool check_aspect_ratio(const D3D11_TEXTURE2D_DESC &desc, UINT width, UINT height);
		static bool check_texture_format(const D3D11_TEXTURE2D_DESC &desc);

		com_ptr<ID3D11Texture2D> find_best_depth_texture(UINT width, UINT height);
#endif

	private:
		draw_stats _global_counter;
#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		std::map<UINT, cleared_depthstencil_info> _cleared_depth_textures;
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<com_ptr<ID3D11Texture2D>, intermediate_snapshot_info> _counters_per_used_depth_texture;
#endif
#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
		std::map<com_ptr<ID3D11Buffer>, draw_stats> _counters_per_constant_buffer;
#endif
	};
}
