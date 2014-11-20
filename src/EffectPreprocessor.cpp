#include "EffectPreprocessor.hpp"

#include <fpp.h>
#include <fstream>

namespace ReShade
{
	struct EffectPreprocessor::Impl
	{
		static void OnOutput(EffectPreprocessor *pp, char ch)
		{
			pp->mImpl->mOutput += ch;
		}
		static void OnError(EffectPreprocessor *pp, const char *format, va_list args)
		{
			char buffer[1024];
			vsprintf_s(buffer, format, args);

			pp->mImpl->mErrors += buffer;
		}

		std::vector<fppTag> mTags;
		std::string mOutput;
		std::string mErrors;
		std::vector<char> mScratch;
		std::size_t mScratchCursor;
	};

	// -----------------------------------------------------------------------------------------------------

	EffectPreprocessor::EffectPreprocessor() : mImpl(new Impl())
	{
		this->mImpl->mScratch.resize(16384);
		this->mImpl->mScratchCursor = 0;

		this->mImpl->mTags.resize(6);
		this->mImpl->mTags[0].tag = FPPTAG_USERDATA;
		this->mImpl->mTags[0].data = static_cast<void *>(this);
		this->mImpl->mTags[1].tag = FPPTAG_OUTPUT;
		this->mImpl->mTags[1].data = reinterpret_cast<void *>(&Impl::OnOutput);
		this->mImpl->mTags[2].tag = FPPTAG_ERROR;
		this->mImpl->mTags[2].data = reinterpret_cast<void *>(&Impl::OnError);
		this->mImpl->mTags[3].tag = FPPTAG_IGNOREVERSION;
		this->mImpl->mTags[3].data = reinterpret_cast<void *>(true);
		this->mImpl->mTags[4].tag = FPPTAG_OUTPUTLINE;
		this->mImpl->mTags[4].data = reinterpret_cast<void *>(true);
		this->mImpl->mTags[5].tag = FPPTAG_OUTPUTSPACE;
		this->mImpl->mTags[5].data = reinterpret_cast<void *>(true);
	}
	EffectPreprocessor::~EffectPreprocessor()
	{
	}

	void EffectPreprocessor::AddDefine(const std::string &name, const std::string &value)
	{
		const std::string define = name + (value.empty() ? "" : "=" + value);
		const std::size_t size = define.length() + 1;

		assert(this->mImpl->mScratchCursor + size < this->mImpl->mScratch.size());

		fppTag tag;
		tag.tag = FPPTAG_DEFINE;
		tag.data = std::memcpy(this->mImpl->mScratch.data() + this->mImpl->mScratchCursor, define.c_str(), size);
		this->mImpl->mTags.push_back(tag);
		this->mImpl->mScratchCursor += size;
	}
	void EffectPreprocessor::AddIncludePath(const boost::filesystem::path &path)
	{
		const std::string directory = path.string() + '\\';
		const std::size_t size = directory.length() + 1;

		assert(this->mImpl->mScratchCursor + size < this->mImpl->mScratch.size());

		fppTag tag;
		tag.tag = FPPTAG_INCLUDE_DIR;
		tag.data = std::memcpy(this->mImpl->mScratch.data() + this->mImpl->mScratchCursor, directory.c_str(), size);
		this->mImpl->mTags.push_back(tag);
		this->mImpl->mScratchCursor += size;
	}

	std::string EffectPreprocessor::Run(const boost::filesystem::path &path, std::string &errors)
	{
		this->mImpl->mOutput.clear();
		this->mImpl->mErrors.clear();

		fppTag tag;
		std::vector<fppTag> tags = this->mImpl->mTags;
		const std::string name = path.string();

		tag.tag = FPPTAG_INPUT_NAME;
		tag.data = const_cast<void *>(static_cast<const void *>(name.c_str()));
		tags.push_back(tag);

		tag.tag = FPPTAG_END;
		tag.data = nullptr;
		tags.push_back(tag);

		if (fppPreProcess(tags.data()) != 0)
		{
			errors += this->mImpl->mErrors;

			this->mImpl->mOutput.clear();
		}

		return this->mImpl->mOutput;
	}
}