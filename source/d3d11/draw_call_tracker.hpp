#pragma once

#include <d3d11.h>
#include <unordered_map>
#include "runtime.hpp"
#include "com_ptr.hpp"

namespace reshade::d3d11
{
	class draw_call_tracker
	{
	public:
		struct depthstencil_counter_info
		{
			UINT vertices = 0;
			UINT drawcalls = 0;
		};

		UINT vertices() const { return _counters.vertices; }
		UINT drawcalls() const { return _counters.drawcalls; }

		void merge(const draw_call_tracker& source);
		void reset(bool ball);

		void track_depthstencil(ID3D11DepthStencilView* to_track);
		void draw_call_tracker::set_depth_texture(ID3D11Texture2D* depth_texture);
		void log_drawcalls(UINT drawcalls, UINT vertices);
		void log_drawcalls(ID3D11DepthStencilView* depthstencil, UINT drawcalls, UINT vertices);

		ID3D11DepthStencilView* get_best_depth_stencil(std::string host_process_name, std::unordered_map<std::string, runtime::game> game_list, UINT drawcalls, const com_ptr<ID3D11Device> device, const com_ptr<ID3D11DeviceContext> devicecontext, UINT width, UINT height);
		ID3D11Texture2D* get_depth_texture();
		depthstencil_counter_info get_counters(ID3D11DepthStencilView* depthstencil);

	private:
		depthstencil_counter_info _counters;
		std::unordered_map<com_ptr<ID3D11DepthStencilView>, depthstencil_counter_info> _counters_per_used_depthstencil;
		com_ptr<ID3D11Texture2D> _active_depth_texture;
	};
}
