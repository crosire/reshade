#include "Hook.hpp"

#include <assert.h>
#include <MinHook.h>

namespace ReHook
{
	namespace
	{
		unsigned long											sCounter = 0;
	}

	Hook														Hook::Install(Hook::Function target, const Hook::Function replacement)
	{
		assert(target != nullptr);
		assert(replacement != nullptr);

		Hook hook;

		if (sCounter++ == 0)
		{
			MH_Initialize();
		}
		
		const MH_STATUS status = MH_CreateHook(hook.mTarget = target, replacement, &hook.mTrampoline);

		if (status == MH_OK)
		{
			MH_EnableHook(target);
		}
		else if (status != MH_ERROR_ALREADY_CREATED)
		{
			if (--sCounter == 0)
			{
				MH_Uninitialize();
			}

			return Hook();
		}

		return hook;
	}
	bool														Hook::Uninstall(Hook &hook)
	{
		assert(hook.IsInstalled());

		const MH_STATUS status = MH_RemoveHook(hook.mTarget);

		if (status == MH_ERROR_NOT_CREATED)
		{
			return true;
		}
		else if (status != MH_OK)
		{
			return false;
		}

		hook.mTrampoline = nullptr;

		if (--sCounter == 0)
		{
			MH_Uninitialize();
		}

		return true;
	}

	bool														Hook::IsEnabled(void) const
	{
		assert(IsInstalled());

		const MH_STATUS status = MH_EnableHook(this->mTarget);

		if (status == MH_ERROR_ENABLED)
		{
			return true;
		}
		else
		{
			MH_DisableHook(this->mTarget);

			return false;
		}
	}
	bool														Hook::IsInstalled(void) const
	{
		return this->mTrampoline != nullptr;
	}

	Hook::Function												Hook::Call(void) const
	{
		assert(IsInstalled());

		return this->mTrampoline;
	}

	bool														Hook::Enable(bool enable)
	{
		if (enable)
		{
			const MH_STATUS status = MH_EnableHook(this->mTarget);

			return (status == MH_OK || status == MH_ERROR_ENABLED);
		}
		else
		{
			const MH_STATUS status = MH_DisableHook(this->mTarget);

			return (status == MH_OK || status == MH_ERROR_DISABLED);
		}
	}
}