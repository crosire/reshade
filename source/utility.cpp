#include <Windows.h>
#include <WinUser.h>
#include <Strsafe.h>
#include <stb_image.h>
#include <stb_image_dds.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>
#include <stb_image.h>
#include <stb_image_dds.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
#include <cctype>
#include "log.hpp"
#include "utility.hpp"
#include "imgui.cpp"

namespace reshade::utility
{

	bool utility::findStringIC(std::vector<std::string> vec, const std::string toSearch)
	{
		auto itr = std::find_if(vec.begin(), vec.end(),
			[&](auto &s) {
			if (s.size() != toSearch.size())
				return false;
			for (size_t i = 0; i < s.size(); ++i)
				if (::tolower(s[i]) == ::tolower(toSearch[i]))
					return true;
			return false;
		}
		);
		if (itr != vec.end()) {
			return true;
		}

		return false;
	}


	void utility::getHostApp(wchar_t* wszProcOut)
	{
		DWORD   dwProcessSize = MAX_PATH * 2;
		wchar_t wszProcessName[MAX_PATH * 2] = {};

		HANDLE hProc =
			GetCurrentProcess();

		QueryFullProcessImageName(
			hProc,
			0,
			wszProcessName,
			&dwProcessSize);

		int      len = lstrlenW(wszProcessName);
		wchar_t* pwszShortName = wszProcessName;

		for (int i = 0; i < len; i++)
			pwszShortName = CharNextW(pwszShortName);

		while (pwszShortName > wszProcessName)
		{
			wchar_t* wszPrev =
				CharPrevW(wszProcessName, pwszShortName);

			if (wszPrev < wszProcessName)
				break;

			if (*wszPrev != L'\\' && *wszPrev != L'/')
			{
				pwszShortName = wszPrev;
				continue;
			}

			break;
		}

		StringCchCopyW(
			wszProcOut,
			MAX_PATH * 2 - 1,
			pwszShortName
		);
	}

	void utility::capture_depthstencil(const com_ptr<ID3D11Device> &device, const com_ptr<ID3D11DeviceContext> &devicecontext, const com_ptr<ID3D11Texture2D> &texture, D3D11_TEXTURE2D_DESC texture_desc, uint8_t *buffer, bool greyscale)
	{
		D3D11_TEXTURE2D_DESC textdesc = {};
		textdesc.Width = texture_desc.Width;
		textdesc.Height = texture_desc.Height;
		textdesc.ArraySize = 1;
		textdesc.MipLevels = 1;
		textdesc.Format = texture_desc.Format;
		textdesc.SampleDesc.Count = 1;
		textdesc.Usage = D3D11_USAGE_STAGING;
		textdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		com_ptr<ID3D11Texture2D> texture_staging;

		HRESULT hr = device->CreateTexture2D(&textdesc, nullptr, &texture_staging);

		if (FAILED(hr))
		{
			LOG(ERROR) << "Failed to create staging resource for screenshot capture! HRESULT is '" << std::hex << hr << std::dec << "'.";
			return;
		}

		devicecontext->CopyResource(texture_staging.get(), texture.get());

		D3D11_MAPPED_SUBRESOURCE mapped;
		hr = devicecontext->Map(texture_staging.get(), 0, D3D11_MAP_READ, 0, &mapped);

		if (FAILED(hr))
		{
			LOG(ERROR) << "Failed to map staging resource with screenshot capture! HRESULT is '" << std::hex << hr << std::dec << "'.";
			return;
		}

		auto mapped_data = static_cast<BYTE *>(mapped.pData);
		const UINT pitch = texture_desc.Width * 4;

		for (UINT y = 0; y < texture_desc.Height; y++)
		{
			CopyMemory(buffer, mapped_data, std::min(pitch, static_cast<UINT>(mapped.RowPitch)));

			for (UINT x = 0; x < pitch; x += 4)
			{
				if (greyscale == true)
				{
					buffer[x + 1] = buffer[x];
					buffer[x + 2] = buffer[x];
				}
				buffer[x + 3] = 0xFF;
			}

			buffer += pitch;
			mapped_data += mapped.RowPitch;
		}

		devicecontext->Unmap(texture_staging.get(), 0);
	}
	void save_depthstencil(filesystem::path parent_path, filesystem::path filename_without_extension, D3D11_TEXTURE2D_DESC texture_desc, uint8_t *buffer)
	{
		// Get current time and date
		SYSTEMTIME time;
		GetLocalTime(&time);

		const char level_names[][6] = { "INFO ", "ERROR", "WARN " };

		int date[4] = {};
		date[0] = time.wYear;
		date[1] = time.wMonth;
		date[2] = time.wDay;
		date[3] = time.wHour * 3600 + time.wMinute * 60 + time.wSecond;

		const int hour = date[3] / 3600;
		const int minute = (date[3] - hour * 3600) / 60;
		const int second = date[3] - hour * 3600 - minute * 60;

		char filename[25];
		ImFormatString(filename, sizeof(filename), " %.4d-%.2d-%.2d %.2d-%.2d-%.2d%s", date[0], date[1], date[2], hour, minute, second, ".png");
		const auto path = parent_path / (filename_without_extension + filename);

		LOG(INFO) << "Saving screenshot to " << path << " ...";

		FILE *file;
		bool success = false;

		if (_wfopen_s(&file, path.wstring().c_str(), L"wb") == 0)
		{
			stbi_write_func *const func =
				[](void *context, void *data, int size) {
				fwrite(data, 1, size, static_cast<FILE *>(context));
			};

			success = stbi_write_png_to_func(func, file, texture_desc.Width, texture_desc.Height, 4, buffer, 0) != 0;

			fclose(file);
		}

		if (!success)
		{
			LOG(ERROR) << "Failed to write screenshot to " << path << "!";
		}
	}

}