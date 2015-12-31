#include "FileWatcher.hpp"

FileWatcher::FileWatcher(const boost::filesystem::path &path, bool subtree) : _path(path), _subTree(subtree), _bufferSize(sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH * sizeof(WCHAR)), _buffer(new unsigned char[_bufferSize])
{
	ZeroMemory(&_overlapped, sizeof(OVERLAPPED));

	_fileHandle = CreateFile(path.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
	_fileCompletionPortHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 2);

	CreateIoCompletionPort(_fileHandle, _fileCompletionPortHandle, reinterpret_cast<ULONG_PTR>(_fileHandle), 0);
	ReadDirectoryChangesW(_fileHandle, _buffer.get(), _bufferSize, _subTree, FILE_NOTIFY_CHANGE_LAST_WRITE, nullptr, &_overlapped, nullptr);
}
FileWatcher::~FileWatcher()
{
	CancelIo(_fileHandle);

	CloseHandle(_fileHandle);
	CloseHandle(_fileCompletionPortHandle);
}

bool FileWatcher::GetModifications(std::vector<boost::filesystem::path> &modifications, DWORD timeout)
{
	DWORD transferred = 0;
	ULONG_PTR key = 0;
	LPOVERLAPPED overlapped;

	if (!GetQueuedCompletionStatus(_fileCompletionPortHandle, &transferred, &key, &overlapped, timeout))
	{
		return false;
	}

	auto record = reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(_buffer.get());
	static boost::filesystem::path sLastFilename;
	static std::time_t sLastTime = 0;

	while (record != nullptr)
	{
		const boost::filesystem::path filename = _path / std::wstring(record->FileName, record->FileNameLength / sizeof(WCHAR));

		if (filename != sLastFilename || sLastTime + 2 < std::time(nullptr))
		{
			sLastFilename = filename;
			sLastTime = std::time(nullptr);
		
			modifications.push_back(std::move(filename));
		}

		if (record->NextEntryOffset == 0)
		{
			break;
		}

		record = reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(reinterpret_cast<const unsigned char *>(record) + record->NextEntryOffset);
	}

	ReadDirectoryChangesW(_fileHandle, _buffer.get(), _bufferSize, _subTree, FILE_NOTIFY_CHANGE_LAST_WRITE, nullptr, &_overlapped, nullptr);

	return true;
}
