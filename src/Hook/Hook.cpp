#include "Hook.hpp"

#include <assert.h>
#include <MinHook.h>

namespace ReShade
{
	namespace
	{
		unsigned long sCounter = 0;
	}

	Hook::Hook() : Target(nullptr), Replacement(nullptr), Trampoline(nullptr)
	{
	}
	Hook::Hook(Function target, Function replacement) : Target(target), Replacement(replacement), Trampoline(nullptr)
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
	Hook::Status Hook::Install()
	{
		if (!IsValid())
		{
			return Status::UnsupportedFunction;
		}

		if (sCounter++ == 0)
		{
			MH_Initialize();
		}

		const MH_STATUS status = MH_CreateHook(this->Target, this->Replacement, &this->Trampoline);

		if (status == MH_OK)
		{
			if (MH_EnableHook(this->Target) == MH_OK)
			{
				return Status::Success;
			}
		}
		else if (status != MH_ERROR_ALREADY_CREATED)
		{
			if (--sCounter == 0)
			{
				MH_Uninitialize();
			}
		}

		return static_cast<Status>(status);
	}
	Hook::Status Hook::Uninstall()
	{
		if (!IsValid())
		{
			return Status::UnsupportedFunction;
		}

		const MH_STATUS status = MH_RemoveHook(this->Target);

		if (status == MH_ERROR_NOT_CREATED)
		{
			return Status::Success;
		}
		else if (status != MH_OK)
		{
			return static_cast<Status>(status);
		}

		this->Trampoline = nullptr;

		if (--sCounter == 0)
		{
			MH_Uninitialize();
		}

		return Status::Success;
	}

	Hook::Function Hook::Call() const
	{
		assert(IsInstalled());

		return this->Trampoline;
	}
}
