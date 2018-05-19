/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <string>
#include <vector>
#include <ostream>

namespace reshade::filesystem
{
	enum class special_folder
	{
		app_data,
		system,
		windows,
	};

	class path
	{
	public:
		path() { }
		path(const char *data) : path(std::string(data)) { }
		path(const wchar_t *data) : path(std::wstring(data)) { }
		path(const std::string &data);
		path(const std::wstring &data);

		bool operator==(const path &other) const;
		bool operator!=(const path &other) const;

		std::string &string() { return _data; }
		const std::string &string() const { return _data; }
		std::wstring wstring() const;

		friend std::ostream &operator<<(std::ostream &stream, const path &path);

		bool empty() const { return _data.empty(); }
		bool is_absolute() const;

		path parent_path() const;
		path filename() const;
		path filename_without_extension() const;
		path extension() const;

		path &remove_filename() { return operator=(parent_path()); }
		path &replace_extension(const path &extension);

		path operator/(const path &more) const;
		path operator+(char c) const { return _data + c; }
		path operator+(const path &more) const { return _data + more._data; }

	private:
		std::string _data;
	};

	bool exists(const path &path);
	path resolve(const path &filename, const std::vector<path> &paths);
	path absolute(const path &filename, const path &parent_path);

	path get_module_path(void *handle);
	path get_special_folder_path(special_folder id);

	std::vector<path> list_files(const path &path, const std::string &mask = "*", bool recursive = false);
}
