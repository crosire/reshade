#pragma once

#include <memory>
#include <vector>
#include <boost\filesystem\path.hpp>

namespace reshade
{
	namespace utils
	{
		class file_watcher
		{
		public:
			explicit file_watcher(const boost::filesystem::path &path);
			~file_watcher();

			bool check(std::vector<boost::filesystem::path> &modifications);

		private:
			boost::filesystem::path _path;
			std::unique_ptr<unsigned char[]> _buffer;
			void *_file_handle, *_completion_handle;
		};
	}
}
