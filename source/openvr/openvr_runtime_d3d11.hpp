#pragma once
#include <d3d11.h>

#include "com_ptr.hpp"
#include <openvr.h>
#include "d3d11/d3d11_device.hpp"
#include "d3d11/runtime_d3d11.hpp"

namespace reshade::openvr
{
	struct eye_texture_d3d11
	{
		uint32_t width = 0;
		uint32_t height = 0;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		com_ptr<ID3D11Texture2D> color;
		bool flipped_x = false;
		bool flipped_y = false;

		void reset()
		{
			*this = eye_texture_d3d11();
		}
	};

	class openvr_runtime_d3d11
	{
	public:
	    typedef vr::EVRCompositorError(* const submit_function)(vr::IVRCompositor *, vr::EVREye, const vr::Texture_t *, const vr::VRTextureBounds_t *, vr::EVRSubmitFlags);
		openvr_runtime_d3d11(D3D11Device *device, submit_function orig_submit, vr::IVRCompositor *orig_compositor);

	    vr::EVRCompositorError on_submit(vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds, vr::EVRSubmitFlags nSubmitFlags);
	private:
		bool apply_effects(vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds);
		bool update_eye_texture(vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds);
		
		submit_function _orig_submit;
		vr::IVRCompositor *_orig_compositor;
		com_ptr<ID3D11Device> _device;
		com_ptr<ID3D11DeviceContext> _context;
		std::unique_ptr<d3d11::runtime_impl> _runtime;
		eye_texture_d3d11 _eye_texture;
	};	
}
