#pragma once

#include <memory>
#include <vector>
#include <string>

namespace reshade
{
	namespace utils
	{
		class file_watcher
		{
		public:
			explicit file_watcher(const wchar_t *path);
			~file_watcher();

			bool check(std::vector<std::wstring> &modifications);

		private:
			std::unique_ptr<uint8_t[]> _buffer;
			void *_file_handle, *_completion_handle;
		};
	}
}
