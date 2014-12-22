#include "FileWatcher.hpp"

FileWatcher::FileWatcher(const boost::filesystem::path &path, bool subtree) : mPath(path), mSubTree(subtree), mBufferSize(sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH * sizeof(WCHAR)), mBuffer(new unsigned char[this->mBufferSize])
{
	ZeroMemory(&this->mOverlapped, sizeof(OVERLAPPED));

	this->mFileHandle = CreateFile(path.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
	this->mFileCompletionPortHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 2);

	CreateIoCompletionPort(this->mFileHandle, this->mFileCompletionPortHandle, reinterpret_cast<ULONG_PTR>(this->mFileHandle), 0);
	ReadDirectoryChangesW(this->mFileHandle, this->mBuffer.get(), this->mBufferSize, this->mSubTree, FILE_NOTIFY_CHANGE_LAST_WRITE, nullptr, &this->mOverlapped, nullptr);
}
FileWatcher::~FileWatcher()
{
	CancelIo(this->mFileHandle);

	CloseHandle(this->mFileHandle);
	CloseHandle(this->mFileCompletionPortHandle);
}

bool FileWatcher::GetModifications(std::vector<boost::filesystem::path> &modifications, DWORD timeout)
{
	DWORD transferred = 0;
	ULONG_PTR key = 0;
	LPOVERLAPPED overlapped;

	if (!GetQueuedCompletionStatus(this->mFileCompletionPortHandle, &transferred, &key, &overlapped, timeout))
	{
		return false;
	}

	const FILE_NOTIFY_INFORMATION *record = reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(this->mBuffer.get());
	static boost::filesystem::path sLastFilename;
	static std::time_t sLastTime = 0;

	while (record != nullptr)
	{
		const boost::filesystem::path filename = this->mPath / std::wstring(record->FileName, record->FileNameLength / sizeof(WCHAR));

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

	ReadDirectoryChangesW(this->mFileHandle, this->mBuffer.get(), this->mBufferSize, this->mSubTree, FILE_NOTIFY_CHANGE_LAST_WRITE, nullptr, &this->mOverlapped, nullptr);

	return true;
}