#pragma once

#include <memory>
#include <vector>
#include <boost\filesystem\path.hpp>

class FileWatcher
{
public:
	explicit FileWatcher(const boost::filesystem::path &path);
	~FileWatcher();

	bool Check(std::vector<boost::filesystem::path> &modifications);

private:
	boost::filesystem::path _path;
	std::unique_ptr<unsigned char[]> _buffer;
	void *_fileHandle, *_completionHandle;
};
