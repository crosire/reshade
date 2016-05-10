#pragma once

#include "runtime_objects.hpp"

namespace reshade
{
	namespace utils
	{
		class ini_file
		{
		public:
			ini_file();
			explicit ini_file(const std::wstring &path);
			~ini_file();

			void load(const std::wstring &path);
			void save() const;

			annotation get(const std::string &section, const std::string &key, const annotation &default = "") const;
			void set(const std::string &section, const std::string &key, const annotation &value);

		private:
			std::wstring _path;
			std::unordered_map<std::string, std::unordered_map<std::string, annotation>> _data;
		};
	}
}
