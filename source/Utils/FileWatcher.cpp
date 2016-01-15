#include "FileWatcher.hpp"

#include <Windows.h>

const DWORD bufferSize = sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH * sizeof(WCHAR);

FileWatcher::FileWatcher(const boost::filesystem::path &path) : _path(path), _buffer(new unsigned char[bufferSize])
{
	_fileHandle = CreateFileW(path.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
	_completionHandle = CreateIoCompletionPort(_fileHandle, nullptr, reinterpret_cast<ULONG_PTR>(_fileHandle), 1);

	OVERLAPPED overlapped = { };
	ReadDirectoryChangesW(_fileHandle, _buffer.get(), bufferSize, TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE, nullptr, &overlapped, nullptr);
}
FileWatcher::~FileWatcher()
{
	CancelIo(_fileHandle);

	CloseHandle(_fileHandle);
	CloseHandle(_completionHandle);
}

bool FileWatcher::Check(std::vector<boost::filesystem::path> &modifications)
{
	DWORD transferred;
	ULONG_PTR key;
	OVERLAPPED *overlapped;

	if (!GetQueuedCompletionStatus(_completionHandle, &transferred, &key, &overlapped, 0))
	{
		return false;
	}

	static std::time_t sLastTime = 0;
	static boost::filesystem::path sLastFilename;

	auto record = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(_buffer.get());

	while (true)
	{
		record->FileNameLength /= sizeof(WCHAR);

		const boost::filesystem::path filename = _path / std::wstring(record->FileName, record->FileNameLength);

		if (filename != sLastFilename || sLastTime + 2 < std::time(nullptr))
		{
			sLastTime = std::time(nullptr);
			sLastFilename = filename;
		
			modifications.push_back(std::move(filename));
		}

		if (record->NextEntryOffset == 0)
		{
			break;
		}

		record = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(reinterpret_cast<BYTE *>(record) + record->NextEntryOffset);
	}

	overlapped->hEvent = nullptr;

	ReadDirectoryChangesW(_fileHandle, _buffer.get(), bufferSize, TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE, nullptr, overlapped, nullptr);

	return true;
}
