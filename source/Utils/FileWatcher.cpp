#include "FileWatcher.hpp"

#include <Windows.h>

namespace reshade
{
	namespace utils
	{
		namespace
		{
			const DWORD buffer_size = sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH * sizeof(WCHAR);
		}

		file_watcher::file_watcher(const boost::filesystem::path &path) : _path(path), _buffer(new unsigned char[buffer_size])
		{
			_file_handle = CreateFileW(path.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
			_completion_handle = CreateIoCompletionPort(_file_handle, nullptr, reinterpret_cast<ULONG_PTR>(_file_handle), 1);

			OVERLAPPED overlapped = { };
			ReadDirectoryChangesW(_file_handle, _buffer.get(), buffer_size, TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE, nullptr, &overlapped, nullptr);
		}
		file_watcher::~file_watcher()
		{
			CancelIo(_file_handle);

			CloseHandle(_file_handle);
			CloseHandle(_completion_handle);
		}

		bool file_watcher::check(std::vector<boost::filesystem::path> &modifications)
		{
			DWORD transferred;
			ULONG_PTR key;
			OVERLAPPED *overlapped;

			if (!GetQueuedCompletionStatus(_completion_handle, &transferred, &key, &overlapped, 0))
			{
				return false;
			}

			static std::time_t s_last_time = 0;
			static boost::filesystem::path s_last_filename;

			auto record = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(_buffer.get());

			while (true)
			{
				record->FileNameLength /= sizeof(WCHAR);

				const boost::filesystem::path filename = _path / std::wstring(record->FileName, record->FileNameLength);

				if (filename != s_last_filename || s_last_time + 2 < std::time(nullptr))
				{
					s_last_time = std::time(nullptr);
					s_last_filename = filename;

					modifications.push_back(std::move(filename));
				}

				if (record->NextEntryOffset == 0)
				{
					break;
				}

				record = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(reinterpret_cast<BYTE *>(record) + record->NextEntryOffset);
			}

			overlapped->hEvent = nullptr;

			ReadDirectoryChangesW(_file_handle, _buffer.get(), buffer_size, TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE, nullptr, overlapped, nullptr);

			return true;
		}
	}
}
