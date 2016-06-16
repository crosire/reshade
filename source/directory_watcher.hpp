#pragma once

#include "filesystem.hpp"

namespace reshade
{
	namespace filesystem
	{
		class directory_watcher
		{
		public:
			directory_watcher(const path &path, bool recursive);
			~directory_watcher();

			bool check(std::vector<path> &modifications);

		private:
			path _path;
			bool _recursive;
			std::vector<uint8_t> _buffer;
			void *_file_handle, *_completion_handle;
		};
	}
}
