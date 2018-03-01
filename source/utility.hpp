#pragma once

#include <d3d11.h>
#include "filesystem.hpp"
#include "com_ptr.hpp"

namespace reshade::utility
{
	bool findStringIC(std::vector<std::string> vec, const std::string toSearch);
	void getHostApp(wchar_t* wszProcOut);
	void capture_depthstencil(const com_ptr<ID3D11Device> &device, const com_ptr<ID3D11DeviceContext> &devicecontext, const com_ptr<ID3D11Texture2D> &texture, D3D11_TEXTURE2D_DESC texture_desc, uint8_t *buffer, bool greyscale);
	void save_depthstencil(filesystem::path parent_path, filesystem::path filename_without_extension, D3D11_TEXTURE2D_DESC texture_desc, uint8_t *buffer);
}