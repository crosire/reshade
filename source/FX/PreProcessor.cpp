#include "PreProcessor.hpp"

#include <fpp.h>
#include <array>
#include <boost\algorithm\string\trim.hpp>
#include <boost\filesystem\operations.hpp>

namespace ReShade
{
	namespace FX
	{
		struct PreProcessor::Impl
		{
			static void OnOutput(Impl *impl, char ch)
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
			static void OnPrintError(Impl *impl, const char *format, va_list args)
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

		PreProcessor::PreProcessor() : _impl(new Impl())
		{
			_impl->_scratchCursor = 0;
			_impl->_lastPragma = std::string::npos;

			_impl->_tags.resize(7);
			_impl->_tags[0].tag = FPPTAG_USERDATA;
			_impl->_tags[0].data = static_cast<void *>(_impl.get());
			_impl->_tags[1].tag = FPPTAG_OUTPUT;
			_impl->_tags[1].data = reinterpret_cast<void *>(&Impl::OnOutput);
			_impl->_tags[2].tag = FPPTAG_ERROR;
			_impl->_tags[2].data = reinterpret_cast<void *>(&Impl::OnPrintError);
			_impl->_tags[3].tag = FPPTAG_IGNOREVERSION;
			_impl->_tags[3].data = reinterpret_cast<void *>(true);
			_impl->_tags[4].tag = FPPTAG_OUTPUTLINE;
			_impl->_tags[4].data = reinterpret_cast<void *>(true);
			_impl->_tags[5].tag = FPPTAG_OUTPUTSPACE;
			_impl->_tags[5].data = reinterpret_cast<void *>(true);
			_impl->_tags[6].tag = FPPTAG_OUTPUTINCLUDES;
			_impl->_tags[6].data = reinterpret_cast<void *>(true);
		}
		PreProcessor::~PreProcessor()
		{
		}

		void PreProcessor::AddDefine(const std::string &name, const std::string &value)
		{
			const std::string define = name + (value.empty() ? "" : "=" + value);
			const size_t size = define.length() + 1;

			assert(_impl->_scratchCursor + size < _impl->_scratch.size());

			fppTag tag;
			tag.tag = FPPTAG_DEFINE;
			tag.data = std::memcpy(_impl->_scratch.data() + _impl->_scratchCursor, define.c_str(), size);
			_impl->_tags.push_back(tag);
			_impl->_scratchCursor += size;
		}
		void PreProcessor::AddIncludePath(const boost::filesystem::path &path)
		{
			const std::string directory = path.string() + '\\';
			const size_t size = directory.length() + 1;

			assert(_impl->_scratchCursor + size < _impl->_scratch.size());

			fppTag tag;
			tag.tag = FPPTAG_INCLUDE_DIR;
			tag.data = std::memcpy(_impl->_scratch.data() + _impl->_scratchCursor, directory.c_str(), size);
			_impl->_tags.push_back(tag);
			_impl->_scratchCursor += size;
		}

		std::string PreProcessor::Run(const boost::filesystem::path &path, std::string &errors, std::vector<std::string> &pragmas, std::vector<boost::filesystem::path> &includes)
		{
			_impl->_output.clear();
			_impl->_errors.clear();

			fppTag tag;
			std::vector<fppTag> tags = _impl->_tags;
			const std::string name = path.string();

			tag.tag = FPPTAG_INPUT_NAME;
			tag.data = const_cast<void *>(static_cast<const void *>(name.c_str()));
			tags.push_back(tag);

			tag.tag = FPPTAG_END;
			tag.data = nullptr;
			tags.push_back(tag);

			// Run preprocessor
			const bool success = fppPreProcess(tags.data()) == 0;

			// Add pragmas
			pragmas.insert(pragmas.end(), _impl->_pragmas.begin(), _impl->_pragmas.end());

			// Add included files
			size_t pos = 0;

			while ((pos = _impl->_errors.find("Included", pos)) != std::string::npos)
			{
				const size_t begin = _impl->_errors.find_first_of('"', pos) + 1, end = _impl->_errors.find_first_of('"', begin);
				const boost::filesystem::path include = boost::filesystem::canonical(_impl->_errors.substr(begin, end - begin)).make_preferred();

				_impl->_errors.erase(pos, 12 + end - begin);
				includes.push_back(include);
			}

			std::sort(includes.begin(), includes.end());
			includes.erase(std::unique(includes.begin(), includes.end()), includes.end());

			// Return preprocessed source
			if (!success)
			{
				errors += _impl->_errors;

				_impl->_output.clear();
			}

			return _impl->_output;
		}
	}
}
