#include "ini_file.hpp"
#include "string_utils.hpp"

#include <fstream>

namespace reshade
{
	namespace utils
	{
		ini_file::ini_file()
		{
		}
		ini_file::ini_file(const std::wstring &path)
		{
			load(path);
		}
		ini_file::~ini_file()
		{
			save();
		}

		void ini_file::load(const std::wstring &path)
		{
			_path = path;

			std::string line, section;
			std::ifstream file(_path);

			while (std::getline(file, line))
			{
				stdext::trim(line);

				if (line.empty() || line[0] == ';' || line[0] == '/')
				{
					continue;
				}

				// Read section name
				if (line[0] == '[')
				{
					section = stdext::trim(line.substr(0, line.find(']')), " \t[]");
					continue;
				}

				// Read section content
				const auto assign_index = line.find('=');

				if (assign_index != std::string::npos)
				{
					const auto key = stdext::trim(line.substr(0, assign_index));
					const auto value = stdext::trim(line.substr(assign_index + 1));

					_data[section][key] = stdext::split(value, ',');
				}
				else
				{
					_data[section][line] = 0;
				}
			}
		}
		void ini_file::save() const
		{
			std::ofstream file(_path.c_str());

			for (const auto &section : _data)
			{
				file << '[' << section.first << ']' << std::endl;

				for (const auto &section_line : section.second)
				{
					file << section_line.first << '=';

					size_t i = 0;

					for (const auto &item : section_line.second.data())
					{
						if (i++ != 0)
						{
							file << ',';
						}

						file << item;
					}

					file << std::endl;
				}

				file << std::endl;
			}
		}

		annotation ini_file::get(const std::string &section, const std::string &key, const annotation &default) const
		{
			const auto it1 = _data.find(section);

			if (it1 == _data.end())
			{
				return default;
			}

			const auto it2 = it1->second.find(key);

			if (it2 == it1->second.end())
			{
				return default;
			}

			return it2->second;
		}
		void ini_file::set(const std::string &section, const std::string &key, const annotation &value)
		{
			_data[section][key] = value;
		}
	}
}
