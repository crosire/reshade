#include "openvr_runtime_d3d11.hpp"

#include "dll_log.hpp"
#include "d3d11/d3d11_device_context.hpp"
#include "dxgi/format_utils.hpp"

namespace reshade::openvr
{
	openvr_runtime_d3d11::openvr_runtime_d3d11(D3D11Device *device, submit_function orig_submit, vr::IVRCompositor *orig_compositor) :
		_orig_submit(orig_submit), _orig_compositor(orig_compositor), _device(device->_orig)
	{
		_device->GetImmediateContext(&_context);
		_runtime.reset(new d3d11::runtime_impl(device, device->_immediate_context, nullptr));

		LOG(DEBUG) << "Created OpenVR D3D11 runtime";
	}

	vr::EVRCompositorError openvr_runtime_d3d11::on_submit(vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds, vr::EVRSubmitFlags nSubmitFlags)
	{
		assert(pTexture->eType == vr::TextureType_DirectX);

		if (!apply_effects(eEye, pTexture, pBounds))
		{
			LOG(DEBUG) << "VR: failed to apply effects, submitting unmodified";
			return _orig_submit(_orig_compositor, eEye, pTexture, pBounds, nSubmitFlags);
		}

		vr::Texture_t tex;
		tex.eType = vr::TextureType_DirectX;
		tex.eColorSpace = pTexture->eColorSpace; // FIXME: is this appropriate, or could effects change the outcome?
		tex.handle = _eye_texture.color.get();
		vr::VRTextureBounds_t bounds { 0, 0, 1, 1 };
		if (_eye_texture.flipped_x)
			std::swap(bounds.uMin, bounds.uMax);
		if (_eye_texture.flipped_y)
			std::swap(bounds.vMin, bounds.vMax);

		// TODO: deal with additional submit flags, i.e. pose, depth
		return _orig_submit(_orig_compositor, eEye, &tex, &bounds, vr::Submit_Default);
	}

	bool openvr_runtime_d3d11::apply_effects(vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds)
	{
		if (!update_eye_texture(eEye, pTexture, pBounds))
			return false;

		_runtime->on_present(_eye_texture.color.get());

		return true;
	}

	bool openvr_runtime_d3d11::update_eye_texture(vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t *pBounds)
	{
		ID3D11Texture2D *texture = static_cast<ID3D11Texture2D*>(pTexture->handle);
		D3D11_TEXTURE2D_DESC td;
		texture->GetDesc(&td);
		if (td.SampleDesc.Count > 1)
		{
			LOG(DEBUG) << "VR: Multisampled textures are not supported.";
			return false;
		}
		
		D3D11_BOX region;
		region.left = static_cast<UINT>(td.Width * std::min(pBounds->uMin, pBounds->uMax));
		region.right = static_cast<UINT>(td.Width * std::max(pBounds->uMin, pBounds->uMax));
		region.top = static_cast<UINT>(td.Height * std::min(pBounds->vMin, pBounds->vMax));
		region.bottom = static_cast<UINT>(td.Height * std::max(pBounds->vMin, pBounds->vMax));
		region.front = 0;
		region.back = 1;
		uint32_t tex_width = region.right - region.left;
		uint32_t tex_height = region.bottom -region.top;

		if (_eye_texture.width != tex_width || _eye_texture.height != tex_height || _eye_texture.format != td.Format)
		{
			// (re-)create our eye texture
			_eye_texture.reset();

			D3D11_TEXTURE2D_DESC tex_desc = {};
			tex_desc.Width = tex_width;
			tex_desc.Height = tex_height;
			tex_desc.MipLevels = 1;
			tex_desc.ArraySize = 1;
			tex_desc.Format = make_dxgi_format_typeless(td.Format);
			tex_desc.SampleDesc = { 1, 0 };
			tex_desc.Usage = D3D11_USAGE_DEFAULT;
			tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET;
			if (FAILED(_device->CreateTexture2D(&tex_desc, nullptr, &_eye_texture.color)))
			{
				LOG(ERROR) << "VR: Failed to create eye texture.";
				return false;
			}

			_eye_texture.width = tex_desc.Width;
			_eye_texture.height = tex_desc.Height;
			_eye_texture.format = tex_desc.Format;
		}

		_eye_texture.flipped_x = pBounds->uMax < pBounds->uMin;
		_eye_texture.flipped_y = pBounds->vMax < pBounds->vMin;

		_context->CopySubresourceRegion(_eye_texture.color.get(), 0, 0, 0, 0, texture, 0, &region);
		return true;
	}
}
