/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "version.h"
#include "runtime.hpp"
#include <Windows.h>
#include <WinInet.h>

struct scoped_handle
{
	scoped_handle(HINTERNET handle) : handle(handle) {}
	~scoped_handle() { InternetCloseHandle(handle); }

	inline operator HINTERNET() const { return handle; }

private:
	HINTERNET handle;
};

bool reshade::runtime::check_for_update(unsigned long latest_version[3])
{
	std::memset(latest_version, 0, 3 * sizeof(unsigned long));

#ifdef NDEBUG
	const scoped_handle handle = InternetOpen(TEXT("reshade"), INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
	if (handle == nullptr)
		return false;

	constexpr auto api_url = TEXT("https://api.github.com/repos/crosire/reshade/tags");

	const scoped_handle request = InternetOpenUrl(handle, api_url, nullptr, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_NO_CACHE_WRITE, 0);
	if (request == nullptr)
		return false;

	// Set some timeouts to avoid stalling startup because of a broken Internet connection
	DWORD timeout = 2000; // 2 seconds
	InternetSetOption(request, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
	InternetSetOption(request, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

	char response_data[32];
	if (DWORD len = 0; InternetReadFile(request, response_data, sizeof(response_data) - 1, &len) && len > 0)
	{
		response_data[len] = '\0';

		const char *version_major_offset = std::strchr(response_data, 'v');
		if (version_major_offset == nullptr) return false; else version_major_offset++;
		const char *version_minor_offset = std::strchr(version_major_offset, '.');
		if (version_minor_offset == nullptr) return false; else version_minor_offset++;
		const char *version_revision_offset = std::strchr(version_minor_offset, '.');
		if (version_revision_offset == nullptr) return false; else version_revision_offset++;

		latest_version[0] = std::strtoul(version_major_offset, nullptr, 10);
		latest_version[1] = std::strtoul(version_minor_offset, nullptr, 10);
		latest_version[2] = std::strtoul(version_revision_offset, nullptr, 10);

		return (latest_version[0] > VERSION_MAJOR) ||
			(latest_version[0] == VERSION_MAJOR && latest_version[1] > VERSION_MINOR) ||
			(latest_version[0] == VERSION_MAJOR && latest_version[1] == VERSION_MINOR && latest_version[2] > VERSION_REVISION);
	}
#endif

	return false;
}
