/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "hook.hpp"
#include <assert.h>
#include <MinHook.h>

namespace reshade
{
	static unsigned long s_reference_count = 0;

	void hook::enable(bool enable) const
	{
		if (enable)
		{
			MH_QueueEnableHook(target);
		}
		else
		{
			MH_QueueDisableHook(target);
		}
	}

	hook::status hook::install()
	{
		if (!valid())
		{
			return hook::status::unsupported_function;
		}

		if (s_reference_count++ == 0)
		{
			MH_Initialize();
		}

		const MH_STATUS statuscode = MH_CreateHook(target, replacement, &trampoline);

		if (statuscode == MH_OK || statuscode == MH_ERROR_ALREADY_CREATED)
		{
			enable(true);

			return hook::status::success;
		}

		if (--s_reference_count == 0)
		{
			MH_Uninitialize();
		}

		return static_cast<hook::status>(statuscode);
	}
	hook::status hook::uninstall()
	{
		if (!valid())
		{
			return hook::status::unsupported_function;
		}

		const MH_STATUS statuscode = MH_RemoveHook(target);

		if (statuscode == MH_ERROR_NOT_CREATED)
		{
			return hook::status::success;
		}
		else if (statuscode != MH_OK)
		{
			return static_cast<hook::status>(statuscode);
		}

		trampoline = nullptr;

		if (--s_reference_count == 0)
		{
			MH_Uninitialize();
		}

		return hook::status::success;
	}

	hook::address hook::call() const
	{
		assert(installed());

		return trampoline;
	}

	bool hook::apply_queued_actions()
	{
		return MH_ApplyQueued() == MH_OK;
	}
}
