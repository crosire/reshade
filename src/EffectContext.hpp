#pragma once

#include <memory>
#include <boost\filesystem\path.hpp>

namespace ReShade
{
	class														Effect;

	class														EffectContext
	{
	public:
		virtual void											GetDimension(unsigned int &width, unsigned int &height) const = 0;

		inline std::unique_ptr<Effect>							Compile(const boost::filesystem::path &path, const std::vector<std::pair<std::string, std::string>> &defines)
		{
			std::string errors;
			return Compile(path, defines, errors);
		}
		std::unique_ptr<Effect>									Compile(const boost::filesystem::path &path, const std::vector<std::pair<std::string, std::string>> &defines, std::string &errors);

	protected:
		virtual std::unique_ptr<Effect>							Compile(const struct EffectTree &ast, std::string &errors) = 0;
	};
}