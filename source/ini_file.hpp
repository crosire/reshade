#pragma once

#include "filesystem.hpp"
#include "runtime_objects.hpp"

namespace reshade
{
	namespace utils
	{
		class ini_file
		{
		public:
			explicit ini_file(const filesystem::path &path);
			~ini_file();

			void load();
			void save() const;

			annotation get(const std::string &section, const std::string &key, const annotation &default = "") const;
			void set(const std::string &section, const std::string &key, const annotation &value);

		private:
			filesystem::path _path;
			std::unordered_map<std::string, std::unordered_map<std::string, annotation>> _data;
		};
	}
}
