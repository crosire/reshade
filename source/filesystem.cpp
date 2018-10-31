/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "filesystem.hpp"
#include <utf8/unchecked.h>
#include <ShlObj.h>
#include <Shlwapi.h>

namespace reshade::filesystem
{
	path::path(const std::string &data) : _data(data)
	{
	}
	path::path(const std::wstring &data) 
	{
		utf8::unchecked::utf16to8(data.begin(), data.end(), std::back_inserter(_data));
	}

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
		std::wstring data;
		utf8::unchecked::utf8to16(_data.begin(), _data.end(), std::back_inserter(data));
		return data;
	}

	std::ostream &operator<<(std::ostream &stream, const path &path)
	{
		WCHAR username[257];
		DWORD username_length = _countof(username);
		GetUserNameW(username, &username_length);
		username_length -= 1;

		std::wstring resultw = path.wstring();

		for (size_t start_pos = 0; (start_pos = resultw.find(username, start_pos)) != std::wstring::npos; start_pos += username_length)
		{
			resultw.replace(start_pos, username_length, username_length, '*');
		}

		std::string result;
		result.reserve(resultw.capacity());
		utf8::unchecked::utf16to8(resultw.begin(), resultw.end(), std::back_inserter(result));

		return stream << '\'' << result << '\'';
	}

	bool path::is_absolute() const
	{
		return PathIsRelativeW(wstring().c_str()) == FALSE;
	}

	path path::parent_path() const
	{
		WCHAR buffer[MAX_PATH] = { };
		utf8::unchecked::utf8to16(_data.begin(), _data.end(), buffer);
		PathRemoveFileSpecW(buffer);
		return buffer;
	}
	path path::filename() const
	{
		WCHAR buffer[MAX_PATH] = { };
		utf8::unchecked::utf8to16(_data.begin(), _data.end(), buffer);
		return PathFindFileNameW(buffer);
	}
	path path::filename_without_extension() const
	{
		WCHAR buffer[MAX_PATH] = { };
		utf8::unchecked::utf8to16(_data.begin(), _data.end(), buffer);
		PathRemoveExtensionW(buffer);
		return PathFindFileNameW(buffer);
	}
	path path::extension() const
	{
		WCHAR buffer[MAX_PATH] = { };
		utf8::unchecked::utf8to16(_data.begin(), _data.end(), buffer);
		return PathFindExtensionW(buffer);
	}

	path &path::replace_extension(const path &extension)
	{
		WCHAR buffer[MAX_PATH] = { };
		utf8::unchecked::utf8to16(_data.begin(), _data.end(), buffer);
		PathRenameExtensionW(buffer, extension.wstring().c_str());
		return operator=(buffer);
	}

	path path::operator/(const path &more) const
	{
		path result(*this);
		if (!_data.empty() && (_data.back() != '\\' && _data.back() != '/'))
			result._data += '\\';
		result._data += more._data;
		return result;
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
			return filename;

		WCHAR result[MAX_PATH] = { };
		PathCombineW(result, parent_path.wstring().c_str(), filename.wstring().c_str());

		return result;
	}

	path get_module_path(void *handle)
	{
		WCHAR result[MAX_PATH] = { };
		GetModuleFileNameW(static_cast<HMODULE>(handle), result, MAX_PATH);

		return result;
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

		return result;
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
			const filesystem::path filename(ffd.cFileName);

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
