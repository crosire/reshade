/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "filesystem.hpp"
#include "unicode.hpp"

#include <ShlObj.h>
#include <Shlwapi.h>

namespace reshade::filesystem
{
	bool path::operator==(const path &other) const
	{
		return _stricmp(_data.c_str(), other._data.c_str()) == 0;
	}
	bool path::operator!=(const path &other) const
	{
		return !operator==(other);
	}

	std::wstring path::wstring() const
	{
		return utf8_to_utf16(_data);
	}

	std::ostream &operator<<(std::ostream &stream, const path &path)
	{
		WCHAR username[257];
		DWORD username_length = _countof(username);
		GetUserNameW(username, &username_length);
		username_length -= 1;

		std::wstring result = utf8_to_utf16(path._data);

		for (size_t start_pos = 0; (start_pos = result.find(username, start_pos)) != std::wstring::npos; start_pos += username_length)
		{
			result.replace(start_pos, username_length, username_length, '*');
		}

		return stream << '\'' << utf16_to_utf8(result) << '\'';
	}

	bool path::is_absolute() const
	{
		return PathIsRelativeW(utf8_to_utf16(_data).c_str()) == FALSE;
	}

	path path::parent_path() const
	{
		WCHAR buffer[MAX_PATH] = { };
		utf8_to_utf16(_data, buffer);

		PathRemoveFileSpecW(buffer);
		return utf16_to_utf8(buffer);
	}
	path path::filename() const
	{
		WCHAR buffer[MAX_PATH] = { };
		utf8_to_utf16(_data, buffer);

		return utf16_to_utf8(PathFindFileNameW(buffer));
	}
	path path::filename_without_extension() const
	{
		WCHAR buffer[MAX_PATH] = { };
		utf8_to_utf16(_data, buffer);

		PathRemoveExtensionW(buffer);
		return utf16_to_utf8(PathFindFileNameW(buffer));
	}
	std::string path::extension() const
	{
		WCHAR buffer[MAX_PATH] = { };
		utf8_to_utf16(_data, buffer);

		return utf16_to_utf8(PathFindExtensionW(buffer));
	}

	path &path::replace_extension(const std::string &extension)
	{
		WCHAR buffer[MAX_PATH] = { };
		utf8_to_utf16(_data, buffer);

		PathRenameExtensionW(buffer, utf8_to_utf16(extension).c_str());
		return operator=(utf16_to_utf8(buffer));
	}

	path path::operator/(const path &more) const
	{
		WCHAR buffer[MAX_PATH] = { };
		utf8_to_utf16(_data, buffer);

		PathAppendW(buffer, utf8_to_utf16(more.string()).c_str());
		return utf16_to_utf8(buffer);
	}

	bool exists(const path &path)
	{
		return GetFileAttributesW(path.wstring().c_str()) != INVALID_FILE_ATTRIBUTES;
	}
	path resolve(const path &filename, const std::vector<path> &paths)
	{
		for (const auto &path : paths)
		{
			auto result = absolute(filename, path);

			if (exists(result))
			{
				return result;
			}
		}

		return filename;
	}
	path absolute(const path &filename, const path &parent_path)
	{
		if (filename.is_absolute())
		{
			return filename;
		}

		WCHAR result[MAX_PATH] = { };
		PathCombineW(result, utf8_to_utf16(parent_path.string()).c_str(), utf8_to_utf16(filename.string()).c_str());

		return utf16_to_utf8(result);
	}

	path get_module_path(void *handle)
	{
		WCHAR result[MAX_PATH] = { };
		GetModuleFileNameW(static_cast<HMODULE>(handle), result, MAX_PATH);

		return utf16_to_utf8(result);
	}
	path get_special_folder_path(special_folder id)
	{
		WCHAR result[MAX_PATH] = { };

		switch (id)
		{
			case special_folder::app_data:
				SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, result);
				break;
			case special_folder::system:
				GetSystemDirectoryW(result, MAX_PATH);
				break;
			case special_folder::windows:
				GetWindowsDirectoryW(result, MAX_PATH);
		}

		return utf16_to_utf8(result);
	}

	std::vector<path> list_files(const path &path, const std::string &mask, bool recursive)
	{
		if (!PathIsDirectoryW(path.wstring().c_str()))
		{
			return { };
		}

		WIN32_FIND_DATAW ffd;

		const HANDLE handle = FindFirstFileW((path / mask).wstring().c_str(), &ffd);

		if (handle == INVALID_HANDLE_VALUE)
		{
			return { };
		}

		std::vector<filesystem::path> result;

		do
		{
			const auto filename = utf16_to_utf8(ffd.cFileName);

			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (recursive)
				{
					const auto recursive_result = list_files(filename, mask, true);
					result.insert(result.end(), recursive_result.begin(), recursive_result.end());
				}
			}
			else
			{
				result.push_back(path / filename);
			}
		}
		while (FindNextFileW(handle, &ffd));

		FindClose(handle);

		return result;
	}
}
