#pragma once

#include <string>
#include <boost\algorithm\string.hpp>

namespace ReShade
{
	namespace Utils
	{
		void EscapeString(std::string &buffer)
		{
			for (auto it = buffer.begin(); it != buffer.end(); ++it)
			{
				if (it[0] == '\\')
				{
					switch (it[1])
					{
						case '"':
							it[0] = '"';
							break;
						case '\'':
							it[0] = '\'';
							break;
						case '\\':
							it[0] = '\\';
							break;
						case 'a':
							it[0] = '\a';
							break;
						case 'b':
							it[0] = '\b';
							break;
						case 'f':
							it[0] = '\f';
							break;
						case 'n':
							it[0] = '\n';
							break;
						case 'r':
							it[0] = '\r';
							break;
						case 't':
							it[0] = '\t';
							break;
						case 'v':
							it[0] = '\v';
							break;
						default:
							it[0] = it[1];
							break;
					}

					it = std::prev(buffer.erase(it + 1));
				}
			}
		}
	}
}
