#pragma once

#include <vector>
#include <string>
#include <algorithm>

namespace stdext
{
	template <class InputIt, class ForwardIt, class BinOp>
	inline void for_each_token(InputIt first, InputIt last, ForwardIt d_first, ForwardIt d_last, BinOp op)
	{
		while (first != last)
		{
			const auto pos = std::find_first_of(first, last, d_first, d_last);

			op(first, pos);

			if (pos == last)
			{
				break;
			}

			first = std::next(pos);
		}
	}

	inline std::vector<std::string> split(const std::string &str, char delim)
	{
		const char delims[1] = { delim };
		std::vector<std::string> result;
		for_each_token(str.cbegin(), str.cend(), std::cbegin(delims), std::cend(delims),
			[&result](auto first, auto second) {
			if (first != second)
				result.emplace_back(first, second);
		});
		return result;
	}
}
