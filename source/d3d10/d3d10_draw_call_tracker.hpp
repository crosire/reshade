#pragma once

#include <d3d10.h>
#include <map>
#include "com_ptr.hpp"

namespace reshade::d3d10
{
	class d3d10_draw_call_tracker
	{
	public:
		UINT vertices() const { return _counters.vertices; }
		UINT drawcalls() const { return _counters.drawcalls; }

		const auto &depthstencil_counters() const { return _counters_per_used_depthstencil; }
		const auto &cleared_depth_textures() const { return _cleared_depth_textures; }

		void merge(const d3d10_draw_call_tracker &source);
		void reset();

		void log_drawcalls(UINT drawcalls, UINT vertices);
		void log_drawcalls(ID3D10DepthStencilView *depthstencil, UINT drawcalls, UINT vertices);

		bool check_depthstencil(ID3D10DepthStencilView *depthstencil) const;
		void track_depthstencil(ID3D10DepthStencilView *depthstencil, com_ptr<ID3D10Texture2D> texture = nullptr);

		void track_depth_texture(UINT index, com_ptr<ID3D10Texture2D> src_texture, com_ptr<ID3D10DepthStencilView> src_depthstencil, com_ptr<ID3D10Texture2D> dest_texture, bool cleared);
		void keep_cleared_depth_textures();

		struct depth_texture_save_info
		{
			com_ptr<ID3D10Texture2D> src_texture;
			com_ptr<ID3D10DepthStencilView> src_depthstencil;
			D3D10_TEXTURE2D_DESC src_texture_desc;
			com_ptr<ID3D10Texture2D> dest_texture;
			bool cleared = false;
		};

		std::pair<ID3D10DepthStencilView *, ID3D10Texture2D *> find_best_depth_stencil(UINT width, UINT height, DXGI_FORMAT format);
		ID3D10Texture2D *find_best_cleared_depth_buffer_texture(DXGI_FORMAT format, UINT depth_buffer_clearing_number);

	private:
		struct depthstencil_counter_info
		{
			UINT vertices = 0;
			UINT drawcalls = 0;
			com_ptr<ID3D10Texture2D> texture;
		};

		depthstencil_counter_info _counters;
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<com_ptr<ID3D10DepthStencilView>, depthstencil_counter_info> _counters_per_used_depthstencil;
		std::map<UINT, depth_texture_save_info> _cleared_depth_textures;
	};
}
