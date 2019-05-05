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
		struct intermediate_snapshot_info
		{
			ID3D11DepthStencilView *depthstencil = nullptr; // No need to use a 'com_ptr' here since '_counters_per_used_depthstencil' already keeps a reference
			draw_stats stats;
			com_ptr<ID3D11Texture2D> texture;
			std::map<ID3D11RenderTargetView *, draw_stats> additional_views;
		};
#endif

		UINT total_vertices() const { return _global_counter.vertices; }
		UINT total_drawcalls() const { return _global_counter.drawcalls; }

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		const auto &depth_buffer_counters() const { return _counters_per_used_depthstencil; }
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
		void track_rendertargets(int format_index, ID3D11DepthStencilView *depthstencil, UINT num_views, ID3D11RenderTargetView *const *views);
		void track_depth_texture(int format_index, UINT index, com_ptr<ID3D11Texture2D> src_texture, com_ptr<ID3D11DepthStencilView> src_depthstencil, com_ptr<ID3D11Texture2D> dest_texture, bool cleared);

		void keep_cleared_depth_textures();

		intermediate_snapshot_info find_best_snapshot(UINT width, UINT height);
		ID3D11Texture2D *find_best_cleared_depth_buffer_texture(UINT clear_index);
#endif

	private:
		struct depth_texture_save_info
		{
			com_ptr<ID3D11Texture2D> src_texture;
			com_ptr<ID3D11DepthStencilView> src_depthstencil;
			D3D11_TEXTURE2D_DESC src_texture_desc;
			com_ptr<ID3D11Texture2D> dest_texture;
			bool cleared = false;
		};

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		bool check_depthstencil(ID3D11DepthStencilView *depthstencil) const;
		bool check_depth_texture_format(int format_index, ID3D11DepthStencilView *depthstencil);
#endif

		draw_stats _global_counter;
#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<com_ptr<ID3D11DepthStencilView>, intermediate_snapshot_info> _counters_per_used_depthstencil;
		std::map<UINT, depth_texture_save_info> _cleared_depth_textures;
#endif
#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
		std::map<com_ptr<ID3D11Buffer>, draw_stats> _counters_per_constant_buffer;
#endif
	};
}
