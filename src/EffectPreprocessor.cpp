#include "EffectPreprocessor.hpp"

#include <fstream>
#include <fpp.h>

namespace ReShade
{
	class EffectPreprocessor::Impl
	{
	public:
		Impl::Impl() : mTags(7), mScratch(16384), mScratchCursor(0)
		{
			this->mTags[0].tag = FPPTAG_USERDATA;
			this->mTags[0].data = static_cast<void *>(this);
			this->mTags[1].tag = FPPTAG_INPUT;
			this->mTags[1].data = reinterpret_cast<void *>(&OnInput);
			this->mTags[2].tag = FPPTAG_OUTPUT;
			this->mTags[2].data = reinterpret_cast<void *>(&OnOutput);
			this->mTags[3].tag = FPPTAG_ERROR;
			this->mTags[3].data = reinterpret_cast<void *>(&OnError);
			this->mTags[4].tag = FPPTAG_IGNOREVERSION;
			this->mTags[4].data = reinterpret_cast<void *>(TRUE);
			this->mTags[5].tag = FPPTAG_OUTPUTLINE;
			this->mTags[5].data = reinterpret_cast<void *>(TRUE);
			this->mTags[6].tag = FPPTAG_OUTPUTSPACE;
			this->mTags[6].data = reinterpret_cast<void *>(TRUE);
		}

		void AddDefine(const std::string &name, const std::string &value)
		{
			const std::string define = name + (value.empty() ? "" : "=" + value);
			const std::size_t size = define.length() + 1;

			assert(this->mScratchCursor + size < this->mScratch.size());

			fppTag tag;
			tag.tag = FPPTAG_DEFINE;
			tag.data = std::memcpy(this->mScratch.data() + this->mScratchCursor, define.c_str(), size);
			this->mTags.push_back(tag);
			this->mScratchCursor += size;
		}
		void AddIncludePath(const boost::filesystem::path &path)
		{
			const std::string directory = path.string() + '\\';
			const std::size_t size = directory.length() + 1;

			assert(this->mScratchCursor + size < this->mScratch.size());

			fppTag tag;
			tag.tag = FPPTAG_INCLUDE_DIR;
			tag.data = std::memcpy(this->mScratch.data() + this->mScratchCursor, directory.c_str(), size);
			this->mTags.push_back(tag);
			this->mScratchCursor += size;
		}

		std::string Run(const boost::filesystem::path &path, std::string &errors)
		{
			this->mInput.clear();
			this->mOutput.clear();
			this->mErrors.clear();

			this->mInput.open(path.c_str());

			if (this->mInput)
			{
				fppTag tag;
				std::vector<fppTag> tags = this->mTags;
				const std::string name = path.string();

				tag.tag = FPPTAG_INPUT_NAME;
				tag.data = const_cast<void *>(static_cast<const void *>(name.c_str()));
				tags.push_back(tag);

				tag.tag = FPPTAG_END;
				tag.data = nullptr;
				tags.push_back(tag);

				if (fppPreProcess(tags.data()) != 0)
				{
					errors += this->mErrors;

					this->mOutput.clear();
				}

				this->mInput.close();
			}
			else
			{
				errors += "File not found!";
			}

			return this->mOutput;
		}

	private:
		static char *OnInput(char *buffer, int size, void *userdata)
		{
			EffectPreprocessor::Impl *pp = static_cast<EffectPreprocessor::Impl *>(userdata);

			pp->mInput.read(buffer, static_cast<std::streamsize>(size - 1));
			const std::streamsize gcount = pp->mInput.gcount();
			buffer[gcount] = '\0';

			return gcount > 0 ? buffer : nullptr;
		}
		static void OnOutput(int ch, void *userdata)
		{
			EffectPreprocessor::Impl *pp = static_cast<EffectPreprocessor::Impl *>(userdata);

			pp->mOutput += static_cast<char>(ch);
		}
		static void OnError(void *userdata, char *fmt, va_list args)
		{
			EffectPreprocessor::Impl *pp = static_cast<EffectPreprocessor::Impl *>(userdata);

			char buffer[1024];
			vsprintf_s(buffer, fmt, args);

			pp->mErrors += buffer;
		}

		std::vector<fppTag> mTags;
		std::fstream mInput;
		std::string mOutput;
		std::string mErrors;
		std::vector<char> mScratch;
		std::size_t mScratchCursor;
	};

	// -----------------------------------------------------------------------------------------------------

	EffectPreprocessor::EffectPreprocessor() : mImpl(new Impl())
	{
	}
	EffectPreprocessor::~EffectPreprocessor()
	{
	}

	void EffectPreprocessor::AddDefine(const std::string &name, const std::string &value)
	{
		this->mImpl->AddDefine(name, value);
	}
	void EffectPreprocessor::AddIncludePath(const boost::filesystem::path &path)
	{
		this->mImpl->AddIncludePath(path);
	}

	std::string EffectPreprocessor::Run(const boost::filesystem::path &path)
	{
		std::string errors;
		return this->mImpl->Run(path, errors);
	}
	std::string EffectPreprocessor::Run(const boost::filesystem::path &path, std::string &errors)
	{
		return this->mImpl->Run(path, errors);
	}
}