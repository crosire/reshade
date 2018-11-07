/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "hook.hpp"
#include <assert.h>
#include <MinHook.h>

static unsigned long s_reference_count = 0;

void reshade::hook::enable(bool enable) const
{
	if (enable)
		MH_QueueEnableHook(target);
	else
		MH_QueueDisableHook(target);
}

reshade::hook::status reshade::hook::install()
{
	if (!valid())
		return hook::status::unsupported_function;

	// Only leave MinHook active as long as any hooks exist
	if (s_reference_count++ == 0)
		MH_Initialize();

	const MH_STATUS statuscode = MH_CreateHook(target, replacement, &trampoline);

	if (statuscode == MH_OK || statuscode == MH_ERROR_ALREADY_CREATED)
	{
		// Installation was successful, so enable the hook and return
		enable(true);

		return hook::status::success;
	}

	if (--s_reference_count == 0)
		MH_Uninitialize();

	return static_cast<hook::status>(statuscode);
}
reshade::hook::status reshade::hook::uninstall()
{
	if (!valid())
		return hook::status::unsupported_function;

	const MH_STATUS statuscode = MH_RemoveHook(target);

	if (statuscode == MH_ERROR_NOT_CREATED)
		return hook::status::success; // If the hook was never created, then uninstallation is implicitly successful
	else if (statuscode != MH_OK)
		return static_cast<hook::status>(statuscode);

	trampoline = nullptr;

	if (--s_reference_count == 0)
		MH_Uninitialize(); // If this was the last active hook, MinHook can now be uninialized, since no more are active

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
