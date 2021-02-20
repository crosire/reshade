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
	    typedef vr::EVRCompositorError(* const submit_function)(void *, vr::EVREye, const vr::Texture_t *, const vr::VRTextureBounds_t *, vr::EVRSubmitFlags);
		openvr_runtime_d3d11(const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds, submit_function orig_submit, void * orig_compositor);

		bool on_submit(vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds);
	private:
		submit_function _orig_submit;
		void *_orig_compositor;
		com_ptr<ID3D11Device> _device;
		com_ptr<ID3D11DeviceContext> _context;
		uint32_t _width;
		uint32_t _height;
		std::unique_ptr<d3d11::state_tracking_context> _state;
		std::unique_ptr<d3d11::runtime_d3d11> _runtime;
		uint32_t _num_submitted;
		bool _left_flipped_horizontally;
		bool _left_flipped_vertically;
		bool _right_flipped_horizontally;
		bool _right_flipped_vertically;
	};	
}
