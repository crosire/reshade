#pragma once

#include <memory>
#include <vector>
#include <string>
#include "filesystem.hpp"

namespace reshade
{
	namespace utils
	{
		class file_watcher
		{
		public:
			explicit file_watcher(const filesystem::path &path);
			~file_watcher();

			bool check(std::vector<filesystem::path> &modifications);

		private:
			std::unique_ptr<uint8_t[]> _buffer;
			void *_file_handle, *_completion_handle;
		};
	}
}
