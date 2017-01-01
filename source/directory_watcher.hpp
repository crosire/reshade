/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "filesystem.hpp"

namespace reshade::filesystem
{
	class directory_watcher
	{
	public:
		explicit directory_watcher(const path &path);
		~directory_watcher();

		bool check(std::vector<path> &modifications);

	private:
		path _path;
		std::vector<uint8_t> _buffer;
		void *_file_handle, *_completion_handle;
	};
}
