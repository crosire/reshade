#pragma once
#include <d3d11.h>

#include "com_ptr.hpp"
#include "openvr.h"
#include "d3d11/runtime_d3d11.hpp"

namespace reshade::openvr
{
	class openvr_runtime_d3d11
	{
	public:
		openvr_runtime_d3d11(const vr::Texture_t * pTexture, const vr::VRTextureBounds_t * pBounds);
	private:
		com_ptr<ID3D11Device> _device;
		com_ptr<ID3D11DeviceContext> _context;
		std::unique_ptr<d3d11::state_tracking_context> _state;
		std::unique_ptr<d3d11::runtime_d3d11> _runtime;
	};	
}
