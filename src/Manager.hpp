#pragma once

#include <boost\filesystem\path.hpp>

namespace ReShade
{
	class														Manager
	{
	public:
		static bool												Initialize(const boost::filesystem::path &executable, const boost::filesystem::path &injector, const boost::filesystem::path &system);
		static void												Exit(void);
	};
}