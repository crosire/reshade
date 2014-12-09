#include "Hook.hpp"

#include <assert.h>
#include <MinHook.h>

namespace ReShade
{
	namespace
	{
		unsigned long sCounter = 0;
	}

	bool Hook::Install(Hook &hook)
	{
		if (!hook.IsValid())
		{
			return false;
		}

		if (sCounter++ == 0)
		{
			MH_Initialize();
		}
		
		const MH_STATUS status = MH_CreateHook(hook.Target, hook.Replacement, &hook.Trampoline);

		if (status == MH_OK)
		{
			hook.Enable();
		}
		else if (status != MH_ERROR_ALREADY_CREATED)
		{
			if (--sCounter == 0)
			{
				MH_Uninitialize();
			}

			return false;
		}

		return true;
	}
	bool Hook::Uninstall(Hook &hook)
	{
		if (!hook.IsValid())
		{
			return false;
		}

		const MH_STATUS status = MH_RemoveHook(hook.Target);

		if (status == MH_ERROR_NOT_CREATED)
		{
			return true;
		}
		else if (status != MH_OK)
		{
			return false;
		}

		hook.Trampoline = nullptr;

		if (--sCounter == 0)
		{
			MH_Uninitialize();
		}

		return true;
	}

	Hook::Hook() : Target(nullptr), Replacement(nullptr), Trampoline(nullptr)
	{
	}
	Hook::Hook(Function target, const Function replacement) : Target(target), Replacement(replacement), Trampoline(nullptr)
	{
	}

	bool Hook::IsValid() const
	{
		return (this->Target != nullptr && this->Replacement != nullptr) && (this->Target != this->Replacement);
	}
	bool Hook::IsEnabled() const
	{
		if (!IsValid())
		{
			return false;
		}

		const MH_STATUS status = MH_EnableHook(this->Target);

		if (status == MH_ERROR_ENABLED)
		{
			return true;
		}
		else
		{
			MH_DisableHook(this->Target);

			return false;
		}
	}
	bool Hook::IsInstalled() const
	{
		return this->Trampoline != nullptr;
	}

	Hook::Function Hook::Call() const
	{
		assert(IsInstalled());

		return this->Trampoline;
	}

	bool Hook::Enable(bool enable)
	{
		if (enable)
		{
			const MH_STATUS status = MH_EnableHook(this->Target);

			return (status == MH_OK || status == MH_ERROR_ENABLED);
		}
		else
		{
			const MH_STATUS status = MH_DisableHook(this->Target);

			return (status == MH_OK || status == MH_ERROR_DISABLED);
		}
	}
}