#include "Hook.hpp"

#include <assert.h>
#include <MinHook.h>

namespace reshade
{
	namespace
	{
		unsigned long s_reference_count = 0;
	}

	hook::hook() : target(nullptr), replacement(nullptr), trampoline(nullptr)
	{
	}
	hook::hook(address target, address replacement) : target(target), replacement(replacement), trampoline(nullptr)
	{
	}

	bool hook::valid() const
	{
		return target != nullptr && replacement != nullptr && target != replacement;
	}
	bool hook::enabled() const
	{
		if (!valid())
		{
			return false;
		}

		const MH_STATUS statuscode = MH_EnableHook(target);

		if (statuscode == MH_ERROR_ENABLED)
		{
			return true;
		}

		MH_DisableHook(target);

		return false;
	}
	bool hook::installed() const
	{
		return trampoline != nullptr;
	}

	bool hook::enable(bool enable) const
	{
		if (enable)
		{
			const MH_STATUS statuscode = MH_EnableHook(target);

			return statuscode == MH_OK || statuscode == MH_ERROR_ENABLED;
		}
		else
		{
			const MH_STATUS statuscode = MH_DisableHook(target);

			return statuscode == MH_OK || statuscode == MH_ERROR_DISABLED;
		}
	}
	hook::status hook::install()
	{
		if (!valid())
		{
			return status::unsupported_function;
		}

		if (s_reference_count++ == 0)
		{
			MH_Initialize();
		}

		const MH_STATUS statuscode = MH_CreateHook(target, replacement, &trampoline);

		if (statuscode == MH_OK || statuscode == MH_ERROR_ALREADY_CREATED)
		{
			enable();

			return status::success;
		}

		if (--s_reference_count == 0)
		{
			MH_Uninitialize();
		}

		return static_cast<status>(statuscode);
	}
	hook::status hook::uninstall()
	{
		if (!valid())
		{
			return status::unsupported_function;
		}

		const MH_STATUS statuscode = MH_RemoveHook(target);

		if (statuscode == MH_ERROR_NOT_CREATED)
		{
			return status::success;
		}
		else if (statuscode != MH_OK)
		{
			return static_cast<status>(statuscode);
		}

		trampoline = nullptr;

		if (--s_reference_count == 0)
		{
			MH_Uninitialize();
		}

		return status::success;
	}

	hook::address hook::call() const
	{
		assert(installed());

		return trampoline;
	}
}
