#pragma once

#include "runtime_objects.hpp"
#include <boost/filesystem/path.hpp>

namespace reshade
{
	namespace utils
	{
		class ini_file
		{
		public:
			ini_file();
			explicit ini_file(const boost::filesystem::path &path);
			~ini_file();

			void load(const boost::filesystem::path &path);
			void save() const;

			annotation get(const std::string &section, const std::string &key, const annotation &default = "") const;
			void set(const std::string &section, const std::string &key, const annotation &value);

		private:
			boost::filesystem::path _path;
			std::unordered_map<std::string, std::unordered_map<std::string, annotation>> _data;
		};
	}
}
