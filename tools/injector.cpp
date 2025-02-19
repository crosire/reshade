/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <Windows.h>
#include <sddl.h>
#include <AclAPI.h>
#include <TlHelp32.h>

#define RESHADE_LOADING_THREAD_FUNC 1

struct loading_data
{
	WCHAR load_path[MAX_PATH] = L"";
	decltype(&GetLastError) GetLastError = nullptr;
	decltype(&LoadLibraryW) LoadLibraryW = nullptr;
	const WCHAR env_var_name[30] = L"RESHADE_DISABLE_LOADING_CHECK";
	const WCHAR env_var_value[2] = L"1";
	decltype(&SetEnvironmentVariableW) SetEnvironmentVariableW = nullptr;
};

struct scoped_handle
{
	HANDLE handle;

	scoped_handle() :
		handle(INVALID_HANDLE_VALUE) {}
	scoped_handle(HANDLE handle) :
		handle(handle) {}
	scoped_handle(scoped_handle &&other) :
		handle(other.handle) {
		other.handle = NULL;
	}
	~scoped_handle() { if (handle != NULL && handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }

	operator HANDLE() const { return handle; }

	HANDLE *operator&() { return &handle; }
	const HANDLE *operator&() const { return &handle; }
};

static void update_acl_for_uwp(LPWSTR path)
{
	OSVERSIONINFOEX verinfo_windows7 = { sizeof(OSVERSIONINFOEX), 6, 1 };
	const bool is_windows7 = VerifyVersionInfo(&verinfo_windows7, VER_MAJORVERSION | VER_MINORVERSION,
		VerSetConditionMask(VerSetConditionMask(0, VER_MAJORVERSION, VER_EQUAL), VER_MINORVERSION, VER_EQUAL)) != FALSE;
	if (is_windows7)
		return; // There is no UWP on Windows 7, so no need to update DACL

	PACL old_acl = nullptr, new_acl = nullptr;
	PSECURITY_DESCRIPTOR sd = nullptr;
	SECURITY_INFORMATION siInfo = DACL_SECURITY_INFORMATION;

	// Get the existing DACL for the file
	if (GetNamedSecurityInfo(path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &old_acl, nullptr, &sd) != ERROR_SUCCESS)
		return;

	// Get the SID for 'ALL_APPLICATION_PACKAGES'
	PSID sid = nullptr;
	if (ConvertStringSidToSid(TEXT("S-1-15-2-1"), &sid))
	{
		EXPLICIT_ACCESS access = {};
		access.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
		access.grfAccessMode = SET_ACCESS;
		access.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
		access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
		access.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
		access.Trustee.ptstrName = reinterpret_cast<LPTCH>(sid);

		// Update the DACL with the new entry
		if (SetEntriesInAcl(1, &access, old_acl, &new_acl) == ERROR_SUCCESS)
		{
			SetNamedSecurityInfo(path, SE_FILE_OBJECT, siInfo, NULL, NULL, new_acl, NULL);
			LocalFree(new_acl);
		}

		LocalFree(sid);
	}

	LocalFree(sd);
}

#if RESHADE_LOADING_THREAD_FUNC
static DWORD WINAPI loading_thread_func(loading_data *arg)
{
	arg->SetEnvironmentVariableW(arg->env_var_name, arg->env_var_value);
	if (arg->LoadLibraryW(arg->load_path) == NULL)
		return arg->GetLastError();
	return ERROR_SUCCESS;
}
#endif

int wmain(int argc, wchar_t *argv[])
{
	if (argc != 2)
	{
		wprintf(L"usage: %s <exe name>\n", argv[0]);
		return 0;
	}

	const wchar_t *name = wcsrchr(argv[1], L'\\');
	if (name)
		name++; // Path separator in the argument, skip to the file name part
	else
		name = argv[1]; // No path separator in the argument, this is a file name

	wprintf(L"Waiting for a '%s' process to spawn ...\n", name);

	DWORD pid = 0;

	// Wait for a process with the target name to spawn
	while (!pid)
	{
		const scoped_handle snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		PROCESSENTRY32W process = { sizeof(process) };
		for (BOOL next = Process32FirstW(snapshot, &process); next; next = Process32NextW(snapshot, &process))
		{
			if (_wcsicmp(process.szExeFile, name) == 0)
			{
				pid = process.th32ProcessID;
				break;
			}
		}

		Sleep(1); // Sleep a bit to not overburden the CPU
	}

	wprintf(L"Found a matching process with PID %lu! Injecting ReShade ... ", pid);

	// Wait just a little bit for the application to initialize
	Sleep(50);

	// Open target application process
	const scoped_handle remote_process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, pid);
	if (remote_process == nullptr)
	{
		wprintf(L"\nFailed to open target application process!\n");
		return GetLastError();
	}

	// Check process architecture
	BOOL remote_is_wow64 = FALSE;
	IsWow64Process(remote_process, &remote_is_wow64);
#ifndef _WIN64
	if (remote_is_wow64 == FALSE)
#else
	if (remote_is_wow64 != FALSE)
#endif
	{
		wprintf(L"\nProcess architecture does not match injection tool! Cannot continue.\n");
		return ERROR_IMAGE_MACHINE_TYPE_MISMATCH;
	}

	loading_data arg;
#if 0
	GetCurrentDirectoryW(MAX_PATH, arg.load_path);
#else
	// Use module directory, since current directory points to target executable directory when drag'n'dropping a target executable onto the injector
	GetModuleFileNameW(nullptr, arg.load_path, MAX_PATH);
	if (wchar_t *const load_path_filename = wcsrchr(arg.load_path, L'\\'))
		*load_path_filename = '\0';
#endif
	wcscat_s(arg.load_path, L"\\");
	wcscat_s(arg.load_path, remote_is_wow64 ? L"ReShade32.dll" : L"ReShade64.dll");

	if (GetFileAttributesW(arg.load_path) == INVALID_FILE_ATTRIBUTES)
	{
		wprintf(L"\nFailed to find ReShade at \"%s\"!\n", arg.load_path);
		return ERROR_FILE_NOT_FOUND;
	}

	// Make sure the DLL has permissions set up for 'ALL_APPLICATION_PACKAGES'
	update_acl_for_uwp(arg.load_path);

	// This happens to work because kernel32.dll is always loaded to the same base address, so the address of 'LoadLibrary' is the same in the target application and the current one
	arg.GetLastError = GetLastError;
	arg.LoadLibraryW = LoadLibraryW;
	arg.SetEnvironmentVariableW = SetEnvironmentVariableW;

#if RESHADE_LOADING_THREAD_FUNC
	const auto loading_thread_func_size = 256; // An estimate of the size of the 'loading_thread_func' function
#else
	const auto loading_thread_func_size = 0;
#endif
	const auto load_param = VirtualAllocEx(remote_process, nullptr, loading_thread_func_size + sizeof(arg), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#if RESHADE_LOADING_THREAD_FUNC
	const auto loading_thread_func_address = static_cast<LPBYTE>(load_param) + sizeof(arg);
#else
	const auto loading_thread_func_address = arg.LoadLibraryW;
#endif

	// Write thread entry point function and 'LoadLibrary' call argument to target application
	if (load_param == nullptr
		|| !WriteProcessMemory(remote_process, load_param, &arg, sizeof(arg), nullptr)
#if RESHADE_LOADING_THREAD_FUNC
		|| !WriteProcessMemory(remote_process, loading_thread_func_address, loading_thread_func, loading_thread_func_size, nullptr)
#endif
		)
	{
		wprintf(L"\nFailed to allocate and write 'LoadLibrary' argument in target application!\n");
		return GetLastError();
	}

	// Execute 'LoadLibrary' in target application
	const scoped_handle load_thread = CreateRemoteThread(remote_process, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(loading_thread_func_address), load_param, 0, nullptr);
	if (load_thread == nullptr)
	{
		wprintf(L"\nFailed to execute 'LoadLibrary' in target application!\n");
		return GetLastError();
	}

	// Wait for loading to finish and clean up parameter memory afterwards
	WaitForSingleObject(load_thread, INFINITE);
	VirtualFreeEx(remote_process, load_param, 0, MEM_RELEASE);

	// Thread thread exit code will contain the module handle
	if (DWORD exit_code;
		GetExitCodeThread(load_thread, &exit_code) &&
#if RESHADE_LOADING_THREAD_FUNC
		exit_code == ERROR_SUCCESS)
#else
		exit_code != NULL)
#endif
	{
		wprintf(L"Succeeded!\n");
		return 0;
	}
	else
	{
#if RESHADE_LOADING_THREAD_FUNC
		wprintf(L"\nFailed to load ReShade in target application! Error code is 0x%x.\n", exit_code);
		return exit_code;
#else
		wprintf(L"\nFailed to load ReShade in target application!\n");
		return ERROR_MOD_NOT_FOUND;
#endif
	}
}
