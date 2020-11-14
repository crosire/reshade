/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "hook.hpp"
#include <cassert>
#include <MinHook.h>

// Verify status codes match the ones from MinHook
static_assert(static_cast<int>(reshade::hook::status::unknown) == MH_UNKNOWN);
static_assert(static_cast<int>(reshade::hook::status::success) == MH_OK);
static_assert(static_cast<int>(reshade::hook::status::not_executable) == MH_ERROR_NOT_EXECUTABLE);
static_assert(static_cast<int>(reshade::hook::status::unsupported_function) == MH_ERROR_UNSUPPORTED_FUNCTION);
static_assert(static_cast<int>(reshade::hook::status::allocation_failure) == MH_ERROR_MEMORY_ALLOC);
static_assert(static_cast<int>(reshade::hook::status::memory_protection_failure) == MH_ERROR_MEMORY_PROTECT);

static unsigned long s_reference_count = 0;

void reshade::hook::enable() const
{
	MH_QueueEnableHook(target);
}
void reshade::hook::disable() const
{
	MH_QueueDisableHook(target);
}

reshade::hook::status reshade::hook::install()
{
	if (!valid())
		return hook::status::unsupported_function;

	// Only leave MinHook active as long as any hooks exist
	if (s_reference_count++ == 0)
		MH_Initialize();

	const MH_STATUS status_code = MH_CreateHook(target, replacement, &trampoline);
	if (status_code == MH_OK || status_code == MH_ERROR_ALREADY_CREATED)
	{
		// Installation was successful, so enable the hook and return
		enable();

		return hook::status::success;
	}

	if (--s_reference_count == 0)
		MH_Uninitialize();

	return static_cast<hook::status>(status_code);
}
reshade::hook::status reshade::hook::uninstall()
{
	if (!valid())
		return hook::status::unsupported_function;

	const MH_STATUS status_code = MH_RemoveHook(target);
	if (status_code == MH_ERROR_NOT_CREATED)
		return hook::status::success; // If the hook was never created, then uninstallation is implicitly successful
	else if (status_code != MH_OK)
		return static_cast<hook::status>(status_code);

	trampoline = nullptr;

	// MinHook can be uninitialized after all hooks were uninstalled
	if (--s_reference_count == 0)
		MH_Uninitialize();

	return hook::status::success;
}

reshade::hook::address reshade::hook::call() const
{
	assert(installed());

	return trampoline;
}

bool reshade::hook::apply_queued_actions()
{
	return MH_ApplyQueued() == MH_OK;
}
