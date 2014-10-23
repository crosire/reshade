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
	struct Change
	{
		DWORD Action;
		boost::filesystem::path Filename;
	};

public:
	FileWatcher(const boost::filesystem::path &path, bool subtree = true);
	~FileWatcher();

	bool GetChanges(std::vector<Change> &changes, DWORD timeout);

private:
	boost::filesystem::path mPath;
	bool mSubTree;
	DWORD mBufferSize;
	std::unique_ptr<unsigned char[]> mBuffer;
	HANDLE mFileHandle;
	HANDLE mFileCompletionPortHandle;
	OVERLAPPED mOverlapped;
};