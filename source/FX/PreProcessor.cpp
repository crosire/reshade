#include "PreProcessor.hpp"

#include <fpp.h>
#include <array>
#include <memory>
#include <boost\algorithm\string\trim.hpp>
#include <boost\filesystem\operations.hpp>

namespace ReShade
{
	namespace FX
	{
		struct preprocess_data
		{
			static void OnOutput(preprocess_data *impl, char ch)
			{
				if (impl->_lastPragma != std::string::npos)
				{
					if (ch == '\n')
					{
						std::string pragma = impl->_output.substr(impl->_lastPragma);
						boost::algorithm::trim(pragma);

						impl->_pragmas.push_back(pragma);
						impl->_lastPragma = std::string::npos;
					}
				}
				else
				{
					const size_t length = impl->_output.size();

					if (length > 7 && impl->_output.substr(length - 7) == "#pragma")
					{
						impl->_lastPragma = length;
					}
				}

				impl->_output += ch;
			}
			static void OnPrintError(preprocess_data *impl, const char *format, va_list args)
			{
				char buffer[1024];
				vsprintf_s(buffer, format, args);

				impl->_errors += buffer;
			}

			std::vector<fppTag> _tags;
			std::vector<std::string> _pragmas;
			std::string _output, _errors;
			size_t _scratchCursor, _lastPragma;
			std::array<char, 16384> _scratch;
		};

		bool preprocess(const boost::filesystem::path &path, const std::vector<std::pair<std::string, std::string>> &macros, std::vector<boost::filesystem::path> &include_paths, std::vector<std::string> &pragmas, std::string &output, std::string &errors)
		{
			const std::string path_string = path.string();
			std::unique_ptr<preprocess_data> data(new preprocess_data());

			data->_scratchCursor = 0;
			data->_lastPragma = std::string::npos;

			data->_tags.resize(8);
			data->_tags[0].tag = FPPTAG_USERDATA;
			data->_tags[0].data = static_cast<void *>(data.get());
			data->_tags[1].tag = FPPTAG_OUTPUT;
			data->_tags[1].data = reinterpret_cast<void *>(&preprocess_data::OnOutput);
			data->_tags[2].tag = FPPTAG_ERROR;
			data->_tags[2].data = reinterpret_cast<void *>(&preprocess_data::OnPrintError);
			data->_tags[3].tag = FPPTAG_IGNOREVERSION;
			data->_tags[3].data = reinterpret_cast<void *>(true);
			data->_tags[4].tag = FPPTAG_OUTPUTLINE;
			data->_tags[4].data = reinterpret_cast<void *>(true);
			data->_tags[5].tag = FPPTAG_OUTPUTSPACE;
			data->_tags[5].data = reinterpret_cast<void *>(true);
			data->_tags[6].tag = FPPTAG_OUTPUTINCLUDES;
			data->_tags[6].data = reinterpret_cast<void *>(true);
			data->_tags[7].tag = FPPTAG_INPUT_NAME;
			data->_tags[7].data = const_cast<void *>(static_cast<const void *>(path_string.c_str()));

			fppTag tag;

			for (const auto &macro : macros)
			{
				const auto define = macro.first + (macro.second.empty() ? "" : "=" + macro.second);
				const size_t size = define.size() + 1;

				assert(data->_scratchCursor + size < data->_scratch.size());

				tag.tag = FPPTAG_DEFINE;
				tag.data = std::memcpy(data->_scratch.data() + data->_scratchCursor, define.c_str(), size);
				data->_tags.push_back(tag);
				data->_scratchCursor += size;
			}
			for (const auto &include_path : include_paths)
			{
				const auto directory = include_path.string() + '\\';
				const size_t size = directory.size() + 1;

				assert(data->_scratchCursor + size < data->_scratch.size());

				tag.tag = FPPTAG_INCLUDE_DIR;
				tag.data = std::memcpy(data->_scratch.data() + data->_scratchCursor, directory.c_str(), size);
				data->_tags.push_back(tag);
				data->_scratchCursor += size;
			}

			tag.tag = FPPTAG_END;
			tag.data = nullptr;
			data->_tags.push_back(tag);

			// Run preprocessor
			const bool success = fppPreProcess(data->_tags.data()) == 0;

			// Add pragmas
			pragmas = data->_pragmas;

			// Add included files
			include_paths.clear();

			size_t pos = 0;

			while ((pos = data->_errors.find("Included", pos)) != std::string::npos)
			{
				const size_t begin = data->_errors.find_first_of('"', pos) + 1, end = data->_errors.find_first_of('"', begin);
				const auto include_path = boost::filesystem::canonical(data->_errors.substr(begin, end - begin)).make_preferred();

				data->_errors.erase(pos, 12 + end - begin);
				include_paths.push_back(include_path);
			}

			std::sort(include_paths.begin(), include_paths.end());
			include_paths.erase(std::unique(include_paths.begin(), include_paths.end()), include_paths.end());

			// Return preprocessed source
			if (!success)
			{
				errors += data->_errors;

				data->_output.clear();
			}

			output = data->_output;

			return success;
		}
	}
}
