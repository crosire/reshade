#pragma once

#include <string>
#include <vector>
#include <ostream>

namespace reshade
{
	namespace filesystem
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
			path();
			path(const char *data);
			path(const std::string &data);

			inline operator std::string &() { return _data; }
			inline operator const std::string &() const { return _data; }

			// Comparison
			bool operator==(const path &other) const;
			bool operator!=(const path &other) const;

			// Decomposition
			path parent_path() const;
			path filename() const;
			path filename_without_extension() const;
			std::string extension() const;

			// Modification
			inline path &remove_filename() { return operator=(parent_path()); }
			path &replace_extension(const std::string &extension);

			path operator/(const path &more) const;

		private:
			std::string _data;
		};

		path get_module_path(void *handle);
		path get_special_folder_path(special_folder id);
		path current_directory();

		std::vector<path> list_files(const path &path, const std::string &mask = "*", bool recursive = false);

		bool exists(const path &path);
		bool is_symlink(const path &path);
		bool is_directory(const path &path);

		path resolve(const path &filename, const std::vector<path> &paths);
		path resolve(const path &filename, const std::vector<std::string> &paths);
		path absolute(const path &filename, const path &parent = current_directory());

		bool create_directory(const path &path);
	}
}

std::ostream &operator<<(std::ostream &stream, const reshade::filesystem::path &path);
