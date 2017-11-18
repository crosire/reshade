/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <unordered_map>
#include "variant.hpp"
#include "filesystem.hpp"

namespace reshade
{
	class ini_file
	{
	public:
		explicit ini_file(const filesystem::path &path);
		~ini_file();

		variant get(const std::string &section, const std::string &key, const variant &default = variant()) const;
		void set(const std::string &section, const std::string &key, const variant &value);

	private:
		void load();
		void save() const;

		bool _modified = false;
		filesystem::path _path;
		using section = std::unordered_map<std::string, variant>;
		std::unordered_map<std::string, section> _sections;
	};
}
