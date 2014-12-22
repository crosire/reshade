#pragma once

/*
 * Adapted from http://veridium.net/programming-tutorials/asynchronous-file-system-monitor/
 */

#include <vector>
#include <memory>
#include <boost\filesystem\path.hpp>
#include <windows.h>

class FileWatcher
{
public:
	FileWatcher(const boost::filesystem::path &path, bool subtree = true);
	~FileWatcher();

	bool GetModifications(std::vector<boost::filesystem::path> &modifications, DWORD timeout = 0);

private:
	boost::filesystem::path mPath;
	bool mSubTree;
	DWORD mBufferSize;
	std::unique_ptr<unsigned char[]> mBuffer;
	HANDLE mFileHandle;
	HANDLE mFileCompletionPortHandle;
	OVERLAPPED mOverlapped;
};