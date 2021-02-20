#include "openvr_runtime_d3d11.hpp"

namespace reshade::openvr
{

	openvr_runtime_d3d11::openvr_runtime_d3d11(const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds)
	{
		ID3D11Texture2D *texture = static_cast<ID3D11Texture2D *>(pTexture->handle);
		texture->GetDevice(&_device);
		_device->GetImmediateContext(&_context);

		_state.reset(new d3d11::state_tracking_context(_context.get()));
		_runtime.reset(new d3d11::runtime_d3d11(_device.get(), nullptr, _state.get()));
	}
}
