#include "filesystem.hpp"
#include "string_utils.hpp"

#include <ShlObj.h>
#include <Shlwapi.h>

namespace reshade
{
	namespace filesystem
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
			return stdext::utf8_to_utf16(_data);
		}

		std::ostream &operator<<(std::ostream &stream, const path &path)
		{
			WCHAR username[257];
			DWORD username_length = _countof(username);
			GetUserNameW(username, &username_length);
			username_length -= 1;

			std::wstring result = stdext::utf8_to_utf16(path._data);

			for (size_t start_pos = 0; (start_pos = result.find(username, start_pos)) != std::wstring::npos; start_pos += username_length)
			{
				result.replace(start_pos, username_length, username_length, '*');
			}

			return stream << '\'' << stdext::utf16_to_utf8(result) << '\'';
		}

		bool path::is_absolute() const
		{
			return PathIsRelativeW(stdext::utf8_to_utf16(_data).c_str()) == FALSE;
		}

		path path::parent_path() const
		{
			WCHAR buffer[MAX_PATH] = { };
			stdext::utf8_to_utf16(_data, buffer);

			PathRemoveFileSpecW(buffer);
			return stdext::utf16_to_utf8(buffer);
		}
		path path::filename() const
		{
			WCHAR buffer[MAX_PATH] = { };
			stdext::utf8_to_utf16(_data, buffer);

			return stdext::utf16_to_utf8(PathFindFileNameW(buffer));
		}
		path path::filename_without_extension() const
		{
			WCHAR buffer[MAX_PATH] = { };
			stdext::utf8_to_utf16(_data, buffer);

			PathRemoveExtensionW(buffer);
			return stdext::utf16_to_utf8(PathFindFileNameW(buffer));
		}
		std::string path::extension() const
		{
			WCHAR buffer[MAX_PATH] = { };
			stdext::utf8_to_utf16(_data, buffer);

			return stdext::utf16_to_utf8(PathFindExtensionW(buffer));
		}

		path &path::remove_filename()
		{
			return operator=(parent_path());
		}
		path &path::replace_extension(const std::string &extension)
		{
			WCHAR buffer[MAX_PATH] = { };
			stdext::utf8_to_utf16(_data, buffer);

			PathRenameExtensionW(buffer, stdext::utf8_to_utf16(extension).c_str());
			return operator=(stdext::utf16_to_utf8(buffer));
		}

		path path::operator/(const path &more) const
		{
			WCHAR buffer[MAX_PATH] = { };
			stdext::utf8_to_utf16(_data, buffer);

			PathAppendW(buffer, stdext::utf8_to_utf16(more.string()).c_str());
			return stdext::utf16_to_utf8(buffer);
		}
		path path::operator+(char c) const
		{
			return _data + c;
		}
		path path::operator+(const path &more) const
		{
			return _data + more._data;
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
			PathCombineW(result, stdext::utf8_to_utf16(parent_path.string()).c_str(), stdext::utf8_to_utf16(filename.string()).c_str());

			return stdext::utf16_to_utf8(result);
		}

		bool exists(const path &path)
		{
			return GetFileAttributesW(path.wstring().c_str()) != INVALID_FILE_ATTRIBUTES;
		}
		bool is_symlink(const path &path)
		{
			return (GetFileAttributesW(path.wstring().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
		}
		bool is_directory(const path &path)
		{
			return PathIsDirectoryW(path.wstring().c_str()) != FALSE;
		}

		path current_directory()
		{
			WCHAR result[MAX_PATH] = { };
			GetCurrentDirectoryW(MAX_PATH, result);

			return stdext::utf16_to_utf8(result);
		}
		path get_module_path(void *handle)
		{
			WCHAR result[MAX_PATH] = { };
			GetModuleFileNameW(static_cast<HMODULE>(handle), result, MAX_PATH);

			return stdext::utf16_to_utf8(result);
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

			return stdext::utf16_to_utf8(result);
		}

		std::vector<path> list_files(const path &path, const std::string &mask, bool recursive)
		{
			if (!is_directory(path))
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
				const auto filename = stdext::utf16_to_utf8(ffd.cFileName);

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

		bool create_directory(const path &path)
		{
			return SHCreateDirectoryExW(nullptr, path.wstring().c_str(), nullptr) == ERROR_SUCCESS;
		}
	}
}
