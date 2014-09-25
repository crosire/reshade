#include "Log.hpp"
#include "Effect.hpp"
#include "EffectParser.hpp"
#include "EffectContext.hpp"

#include <fpp.h>
#include <array>
#include <fstream>

namespace ReShade
{
	class														EffectPreprocessor
	{
	public:
		EffectPreprocessor(void) : mTags(7), mScratch(16384), mScratchCursor(0)
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

		void													AddDefine(const std::string &name, const std::string &value)
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
		void													AddIncludePath(const boost::filesystem::path &path)
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

		std::string												Run(const boost::filesystem::path &path, std::string &errors)
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
		static char *											OnInput(char *buffer, int size, void *userdata)
		{
			EffectPreprocessor *pp = static_cast<EffectPreprocessor *>(userdata);

			pp->mInput.read(buffer, static_cast<std::streamsize>(size - 1));
			const std::streamsize gcount = pp->mInput.gcount();
			buffer[gcount] = '\0';

			return gcount > 0 ? buffer : nullptr;
		}
		static void												OnOutput(int ch, void *userdata)
		{
			EffectPreprocessor *pp = static_cast<EffectPreprocessor *>(userdata);

			pp->mOutput += static_cast<char>(ch);
		}
		static void												OnError(void *userdata, char *fmt, va_list args)
		{
			EffectPreprocessor *pp = static_cast<EffectPreprocessor *>(userdata);

			char buffer[1024];
			vsprintf_s(buffer, fmt, args);

			pp->mErrors += buffer;
		}

	private:
		std::vector<fppTag>										mTags;
		std::fstream											mInput;
		std::string												mOutput;
		std::string												mErrors;
		std::vector<char>										mScratch;
		std::size_t												mScratchCursor;
	};

	std::unique_ptr<Effect>										EffectContext::Compile(const boost::filesystem::path &path, const std::vector<std::pair<std::string, std::string>> &defines, std::string &errors)
	{
		EffectTree ast;
		EffectParser parser;
		EffectPreprocessor preprocessor;
		
		for (auto it = defines.cbegin(), end = defines.cend(); it != end; ++it)
		{
			preprocessor.AddDefine(it->first, it->second);
		}

		preprocessor.AddIncludePath(path.parent_path());

		LOG(TRACE) << "> Running preprocessor ...";

		const std::string source = preprocessor.Run(path, errors);
		bool status = !source.empty();

		if (status)
		{
			#pragma region Intrinsics
			static const char *intrinsics =
				"float    radians(float);"
				"float2   radians(float2);"
				"float2x2 radians(float2x2);"
				"float2x3 radians(float2x3);"
				"float2x4 radians(float2x4);"
				"float3   radians(float3);"
				"float3x2 radians(float3x2);"
				"float3x3 radians(float3x3);"
				"float3x4 radians(float3x4);"
				"float4   radians(float4);"
				"float4x2 radians(float4x2);"
				"float4x3 radians(float4x3);"
				"float4x4 radians(float4x4);"
				""
				"float    degrees(float);"
				"float2   degrees(float2);"
				"float2x2 degrees(float2x2);"
				"float2x3 degrees(float2x3);"
				"float2x4 degrees(float2x4);"
				"float3   degrees(float3);"
				"float3x2 degrees(float3x2);"
				"float3x3 degrees(float3x3);"
				"float3x4 degrees(float3x4);"
				"float4   degrees(float4);"
				"float4x2 degrees(float4x2);"
				"float4x3 degrees(float4x3);"
				"float4x4 degrees(float4x4);"
				""
				"float    sin(float);"
				"float2   sin(float2);"
				"float2x2 sin(float2x2);"
				"float2x3 sin(float2x3);"
				"float2x4 sin(float2x4);"
				"float3   sin(float3);"
				"float3x2 sin(float3x2);"
				"float3x3 sin(float3x3);"
				"float3x4 sin(float3x4);"
				"float4   sin(float4);"
				"float4x2 sin(float4x2);"
				"float4x3 sin(float4x3);"
				"float4x4 sin(float4x4);"
				""
				"float    sinh(float);"
				"float2   sinh(float2);"
				"float2x2 sinh(float2x2);"
				"float2x3 sinh(float2x3);"
				"float2x4 sinh(float2x4);"
				"float3   sinh(float3);"
				"float3x2 sinh(float3x2);"
				"float3x3 sinh(float3x3);"
				"float3x4 sinh(float3x4);"
				"float4   sinh(float4);"
				"float4x2 sinh(float4x2);"
				"float4x3 sinh(float4x3);"
				"float4x4 sinh(float4x4);"
				""
				"float    asin(float);"
				"float2   asin(float2);"
				"float2x2 asin(float2x2);"
				"float2x3 asin(float2x3);"
				"float2x4 asin(float2x4);"
				"float3   asin(float3);"
				"float3x2 asin(float3x2);"
				"float3x3 asin(float3x3);"
				"float3x4 asin(float3x4);"
				"float4   asin(float4);"
				"float4x2 asin(float4x2);"
				"float4x3 asin(float4x3);"
				"float4x4 asin(float4x4);"
				""
				"float    cos(float);"
				"float2   cos(float2);"
				"float2x2 cos(float2x2);"
				"float2x3 cos(float2x3);"
				"float2x4 cos(float2x4);"
				"float3   cos(float3);"
				"float3x2 cos(float3x2);"
				"float3x3 cos(float3x3);"
				"float3x4 cos(float3x4);"
				"float4   cos(float4);"
				"float4x2 cos(float4x2);"
				"float4x3 cos(float4x3);"
				"float4x4 cos(float4x4);"
				""
				"float    cosh(float);"
				"float2   cosh(float2);"
				"float2x2 cosh(float2x2);"
				"float2x3 cosh(float2x3);"
				"float2x4 cosh(float2x4);"
				"float3   cosh(float3);"
				"float3x2 cosh(float3x2);"
				"float3x3 cosh(float3x3);"
				"float3x4 cosh(float3x4);"
				"float4   cosh(float4);"
				"float4x2 cosh(float4x2);"
				"float4x3 cosh(float4x3);"
				"float4x4 cosh(float4x4);"
				""
				"float    acos(float);"
				"float2   acos(float2);"
				"float2x2 acos(float2x2);"
				"float2x3 acos(float2x3);"
				"float2x4 acos(float2x4);"
				"float3   acos(float3);"
				"float3x2 acos(float3x2);"
				"float3x3 acos(float3x3);"
				"float3x4 acos(float3x4);"
				"float4   acos(float4);"
				"float4x2 acos(float4x2);"
				"float4x3 acos(float4x3);"
				"float4x4 acos(float4x4);"
				""
				"float    tan(float);"
				"float2   tan(float2);"
				"float2x2 tan(float2x2);"
				"float2x3 tan(float2x3);"
				"float2x4 tan(float2x4);"
				"float3   tan(float3);"
				"float3x2 tan(float3x2);"
				"float3x3 tan(float3x3);"
				"float3x4 tan(float3x4);"
				"float4   tan(float4);"
				"float4x2 tan(float4x2);"
				"float4x3 tan(float4x3);"
				"float4x4 tan(float4x4);"
				""
				"float    tanh(float);"
				"float2   tanh(float2);"
				"float2x2 tanh(float2x2);"
				"float2x3 tanh(float2x3);"
				"float2x4 tanh(float2x4);"
				"float3   tanh(float3);"
				"float3x2 tanh(float3x2);"
				"float3x3 tanh(float3x3);"
				"float3x4 tanh(float3x4);"
				"float4   tanh(float4);"
				"float4x2 tanh(float4x2);"
				"float4x3 tanh(float4x3);"
				"float4x4 tanh(float4x4);"
				""
				"float    atan(float);"
				"float2   atan(float2);"
				"float2x2 atan(float2x2);"
				"float2x3 atan(float2x3);"
				"float2x4 atan(float2x4);"
				"float3   atan(float3);"
				"float3x2 atan(float3x2);"
				"float3x3 atan(float3x3);"
				"float3x4 atan(float3x4);"
				"float4   atan(float4);"
				"float4x2 atan(float4x2);"
				"float4x3 atan(float4x3);"
				"float4x4 atan(float4x4);"
				""
				"float    atan2(float, float);"
				"float2   atan2(float2, float2);"
				"float2x2 atan2(float2x2, float2x2);"
				"float2x3 atan2(float2x3, float2x3);"
				"float2x4 atan2(float2x4, float2x4);"
				"float3   atan2(float3, float3);"
				"float3x2 atan2(float3x2, float3x2);"
				"float3x3 atan2(float3x3, float3x3);"
				"float3x4 atan2(float3x4, float3x4);"
				"float4   atan2(float4, float4);"
				"float4x2 atan2(float4x2, float4x2);"
				"float4x3 atan2(float4x3, float4x3);"
				"float4x4 atan2(float4x4, float4x4);"
				""
				"void     sincos(float, out float, out float);"
				"void     sincos(float2, out float2, out float2);"
				"void     sincos(float2x2, out float2x2, out float2x2);"
				"void     sincos(float2x3, out float2x3, out float2x3);"
				"void     sincos(float2x4, out float2x4, out float2x4);"
				"void     sincos(float3, out float3, out float3);"
				"void     sincos(float3x2, out float3x2, out float3x2);"
				"void     sincos(float3x3, out float3x3, out float3x3);"
				"void     sincos(float3x4, out float3x4, out float3x4);"
				"void     sincos(float4, out float4, out float4);"
				"void     sincos(float4x2, out float4x2, out float4x2);"
				"void     sincos(float4x3, out float4x3, out float4x3);"
				"void     sincos(float4x4, out float4x4, out float4x4);"
				""
				"float    pow(float, float);"
				"float2   pow(float2, float2);"
				"float2x2 pow(float2x2, float2x2);"
				"float2x3 pow(float2x3, float2x3);"
				"float2x4 pow(float2x4, float2x4);"
				"float3   pow(float3, float3);"
				"float3x2 pow(float3x2, float3x2);"
				"float3x3 pow(float3x3, float3x3);"
				"float3x4 pow(float3x4, float3x4);"
				"float4   pow(float4, float4);"
				"float4x2 pow(float4x2, float4x2);"
				"float4x3 pow(float4x3, float4x3);"
				"float4x4 pow(float4x4, float4x4);"
				""
				"float    exp(float);"
				"float2   exp(float2);"
				"float2x2 exp(float2x2);"
				"float2x3 exp(float2x3);"
				"float2x4 exp(float2x4);"
				"float3   exp(float3);"
				"float3x2 exp(float3x2);"
				"float3x3 exp(float3x3);"
				"float3x4 exp(float3x4);"
				"float4   exp(float4);"
				"float4x2 exp(float4x2);"
				"float4x3 exp(float4x3);"
				"float4x4 exp(float4x4);"
				""
				"float    exp2(float);"
				"float2   exp2(float2);"
				"float2x2 exp2(float2x2);"
				"float2x3 exp2(float2x3);"
				"float2x4 exp2(float2x4);"
				"float3   exp2(float3);"
				"float3x2 exp2(float3x2);"
				"float3x3 exp2(float3x3);"
				"float3x4 exp2(float3x4);"
				"float4   exp2(float4);"
				"float4x2 exp2(float4x2);"
				"float4x3 exp2(float4x3);"
				"float4x4 exp2(float4x4);"
				""
				"float    log(float);"
				"float2   log(float2);"
				"float2x2 log(float2x2);"
				"float2x3 log(float2x3);"
				"float2x4 log(float2x4);"
				"float3   log(float3);"
				"float3x2 log(float3x2);"
				"float3x3 log(float3x3);"
				"float3x4 log(float3x4);"
				"float4   log(float4);"
				"float4x2 log(float4x2);"
				"float4x3 log(float4x3);"
				"float4x4 log(float4x4);"
				""
				"float    log2(float);"
				"float2   log2(float2);"
				"float2x2 log2(float2x2);"
				"float2x3 log2(float2x3);"
				"float2x4 log2(float2x4);"
				"float3   log2(float3);"
				"float3x2 log2(float3x2);"
				"float3x3 log2(float3x3);"
				"float3x4 log2(float3x4);"
				"float4   log2(float4);"
				"float4x2 log2(float4x2);"
				"float4x3 log2(float4x3);"
				"float4x4 log2(float4x4);"
				""
				"float    log10(float);"
				"float2   log10(float2);"
				"float2x2 log10(float2x2);"
				"float2x3 log10(float2x3);"
				"float2x4 log10(float2x4);"
				"float3   log10(float3);"
				"float3x2 log10(float3x2);"
				"float3x3 log10(float3x3);"
				"float3x4 log10(float3x4);"
				"float4   log10(float4);"
				"float4x2 log10(float4x2);"
				"float4x3 log10(float4x3);"
				"float4x4 log10(float4x4);"
				""
				"float    sqrt(float);"
				"float2   sqrt(float2);"
				"float2x2 sqrt(float2x2);"
				"float2x3 sqrt(float2x3);"
				"float2x4 sqrt(float2x4);"
				"float3   sqrt(float3);"
				"float3x2 sqrt(float3x2);"
				"float3x3 sqrt(float3x3);"
				"float3x4 sqrt(float3x4);"
				"float4   sqrt(float4);"
				"float4x2 sqrt(float4x2);"
				"float4x3 sqrt(float4x3);"
				"float4x4 sqrt(float4x4);"
				""
				"float    rsqrt(float);"
				"float2   rsqrt(float2);"
				"float2x2 rsqrt(float2x2);"
				"float2x3 rsqrt(float2x3);"
				"float2x4 rsqrt(float2x4);"
				"float3   rsqrt(float3);"
				"float3x2 rsqrt(float3x2);"
				"float3x3 rsqrt(float3x3);"
				"float3x4 rsqrt(float3x4);"
				"float4   rsqrt(float4);"
				"float4x2 rsqrt(float4x2);"
				"float4x3 rsqrt(float4x3);"
				"float4x4 rsqrt(float4x4);"
				""
				"float    abs(float);"
				"float2   abs(float2);"
				"float2x2 abs(float2x2);"
				"float2x3 abs(float2x3);"
				"float2x4 abs(float2x4);"
				"float3   abs(float3);"
				"float3x2 abs(float3x2);"
				"float3x3 abs(float3x3);"
				"float3x4 abs(float3x4);"
				"float4   abs(float4);"
				"float4x2 abs(float4x2);"
				"float4x3 abs(float4x3);"
				"float4x4 abs(float4x4);"
				""
				"float    sign(float);"
				"float2   sign(float2);"
				"float2x2 sign(float2x2);"
				"float2x3 sign(float2x3);"
				"float2x4 sign(float2x4);"
				"float3   sign(float3);"
				"float3x2 sign(float3x2);"
				"float3x3 sign(float3x3);"
				"float3x4 sign(float3x4);"
				"float4   sign(float4);"
				"float4x2 sign(float4x2);"
				"float4x3 sign(float4x3);"
				"float4x4 sign(float4x4);"
				""
				"float    floor(float);"
				"float2   floor(float2);"
				"float2x2 floor(float2x2);"
				"float2x3 floor(float2x3);"
				"float2x4 floor(float2x4);"
				"float3   floor(float3);"
				"float3x2 floor(float3x2);"
				"float3x3 floor(float3x3);"
				"float3x4 floor(float3x4);"
				"float4   floor(float4);"
				"float4x2 floor(float4x2);"
				"float4x3 floor(float4x3);"
				"float4x4 floor(float4x4);"
				""
				"float    ceil(float);"
				"float2   ceil(float2);"
				"float2x2 ceil(float2x2);"
				"float2x3 ceil(float2x3);"
				"float2x4 ceil(float2x4);"
				"float3   ceil(float3);"
				"float3x2 ceil(float3x2);"
				"float3x3 ceil(float3x3);"
				"float3x4 ceil(float3x4);"
				"float4   ceil(float4);"
				"float4x2 ceil(float4x2);"
				"float4x3 ceil(float4x3);"
				"float4x4 ceil(float4x4);"
				""
				"float    frac(float);"
				"float2   frac(float2);"
				"float2x2 frac(float2x2);"
				"float2x3 frac(float2x3);"
				"float2x4 frac(float2x4);"
				"float3   frac(float3);"
				"float3x2 frac(float3x2);"
				"float3x3 frac(float3x3);"
				"float3x4 frac(float3x4);"
				"float4   frac(float4);"
				"float4x2 frac(float4x2);"
				"float4x3 frac(float4x3);"
				"float4x4 frac(float4x4);"
				""
				"float    frexp(float, out float);"
				"float2   frexp(float2, out float2);"
				"float2x2 frexp(float2x2, out float2x2);"
				"float2x3 frexp(float2x3, out float2x3);"
				"float2x4 frexp(float2x4, out float2x4);"
				"float3   frexp(float3, out float3);"
				"float3x2 frexp(float3x2, out float3x2);"
				"float3x3 frexp(float3x3, out float3x3);"
				"float3x4 frexp(float3x4, out float3x4);"
				"float4   frexp(float4, out float4);"
				"float4x2 frexp(float4x2, out float4x2);"
				"float4x3 frexp(float4x3, out float4x3);"
				"float4x4 frexp(float4x4, out float4x4);"
				""
				"float    fmod(float, float);"
				"float2   fmod(float2, float2);"
				"float2x2 fmod(float2x2, float2x2);"
				"float2x3 fmod(float2x3, float2x3);"
				"float2x4 fmod(float2x4, float2x4);"
				"float3   fmod(float3, float3);"
				"float3x2 fmod(float3x2, float3x2);"
				"float3x3 fmod(float3x3, float3x3);"
				"float3x4 fmod(float3x4, float3x4);"
				"float4   fmod(float4, float4);"
				"float4x2 fmod(float4x2, float4x2);"
				"float4x3 fmod(float4x3, float4x3);"
				"float4x4 fmod(float4x4, float4x4);"
				""
				"float    min(float, float);"
				"float2   min(float2, float2);"
				"float2x2 min(float2x2, float2x2);"
				"float2x3 min(float2x3, float2x3);"
				"float2x4 min(float2x4, float2x4);"
				"float3   min(float3, float3);"
				"float3x2 min(float3x2, float3x2);"
				"float3x3 min(float3x3, float3x3);"
				"float3x4 min(float3x4, float3x4);"
				"float4   min(float4, float4);"
				"float4x2 min(float4x2, float4x2);"
				"float4x3 min(float4x3, float4x3);"
				"float4x4 min(float4x4, float4x4);"
				""
				"float    max(float, float);"
				"float2   max(float2, float2);"
				"float2x2 max(float2x2, float2x2);"
				"float2x3 max(float2x3, float2x3);"
				"float2x4 max(float2x4, float2x4);"
				"float3   max(float3, float3);"
				"float3x2 max(float3x2, float3x2);"
				"float3x3 max(float3x3, float3x3);"
				"float3x4 max(float3x4, float3x4);"
				"float4   max(float4, float4);"
				"float4x2 max(float4x2, float4x2);"
				"float4x3 max(float4x3, float4x3);"
				"float4x4 max(float4x4, float4x4);"
				""
				"float    clamp(float, float, float);"
				"float2   clamp(float2, float2, float2);"
				"float2x2 clamp(float2x2, float2x2, float2x2);"
				"float2x3 clamp(float2x3, float2x3, float2x3);"
				"float2x4 clamp(float2x4, float2x4, float2x4);"
				"float3   clamp(float3, float3, float3);"
				"float3x2 clamp(float3x2, float3x2, float3x2);"
				"float3x3 clamp(float3x3, float3x3, float3x3);"
				"float3x4 clamp(float3x4, float3x4, float3x4);"
				"float4   clamp(float4, float4, float4);"
				"float4x2 clamp(float4x2, float4x2, float4x2);"
				"float4x3 clamp(float4x3, float4x3, float4x3);"
				"float4x4 clamp(float4x4, float4x4, float4x4);"
				""
				"float    saturate(float);"
				"float2   saturate(float2);"
				"float2x2 saturate(float2x2);"
				"float2x3 saturate(float2x3);"
				"float2x4 saturate(float2x4);"
				"float3   saturate(float3);"
				"float3x2 saturate(float3x2);"
				"float3x3 saturate(float3x3);"
				"float3x4 saturate(float3x4);"
				"float4   saturate(float4);"
				"float4x2 saturate(float4x2);"
				"float4x3 saturate(float4x3);"
				"float4x4 saturate(float4x4);"
				""
				"float    modf(float, out float);"
				"float2   modf(float2, out float2);"
				"float2x2 modf(float2x2, out float2x2);"
				"float2x3 modf(float2x3, out float2x3);"
				"float2x4 modf(float2x4, out float2x4);"
				"float3   modf(float3, out float3);"
				"float3x2 modf(float3x2, out float3x2);"
				"float3x3 modf(float3x3, out float3x3);"
				"float3x4 modf(float3x4, out float3x4);"
				"float4   modf(float4, out float4);"
				"float4x2 modf(float4x2, out float4x2);"
				"float4x3 modf(float4x3, out float4x3);"
				"float4x4 modf(float4x4, out float4x4);"
				""
				"float    round(float);"
				"float2   round(float2);"
				"float2x2 round(float2x2);"
				"float2x3 round(float2x3);"
				"float2x4 round(float2x4);"
				"float3   round(float3);"
				"float3x2 round(float3x2);"
				"float3x3 round(float3x3);"
				"float3x4 round(float3x4);"
				"float4   round(float4);"
				"float4x2 round(float4x2);"
				"float4x3 round(float4x3);"
				"float4x4 round(float4x4);"
				""
				"float    trunc(float);"
				"float2   trunc(float2);"
				"float2x2 trunc(float2x2);"
				"float2x3 trunc(float2x3);"
				"float2x4 trunc(float2x4);"
				"float3   trunc(float3);"
				"float3x2 trunc(float3x2);"
				"float3x3 trunc(float3x3);"
				"float3x4 trunc(float3x4);"
				"float4   trunc(float4);"
				"float4x2 trunc(float4x2);"
				"float4x3 trunc(float4x3);"
				"float4x4 trunc(float4x4);"
				""
				"float    ldexp(float, float);"
				"float2   ldexp(float2, float2);"
				"float2x2 ldexp(float2x2, float2x2);"
				"float2x3 ldexp(float2x3, float2x3);"
				"float2x4 ldexp(float2x4, float2x4);"
				"float3   ldexp(float3, float3);"
				"float3x2 ldexp(float3x2, float3x2);"
				"float3x3 ldexp(float3x3, float3x3);"
				"float3x4 ldexp(float3x4, float3x4);"
				"float4   ldexp(float4, float4);"
				"float4x2 ldexp(float4x2, float4x2);"
				"float4x3 ldexp(float4x3, float4x3);"
				"float4x4 ldexp(float4x4, float4x4);"
				""
				"float    lerp(float, float, float);"
				"float2   lerp(float2, float2, float2);"
				"float2x2 lerp(float2x2, float2x2, float2x2);"
				"float2x3 lerp(float2x3, float2x3, float2x3);"
				"float2x4 lerp(float2x4, float2x4, float2x4);"
				"float3   lerp(float3, float3, float3);"
				"float3x2 lerp(float3x2, float3x2, float3x2);"
				"float3x3 lerp(float3x3, float3x3, float3x3);"
				"float3x4 lerp(float3x4, float3x4, float3x4);"
				"float4   lerp(float4, float4, float4);"
				"float4x2 lerp(float4x2, float4x2, float4x2);"
				"float4x3 lerp(float4x3, float4x3, float4x3);"
				"float4x4 lerp(float4x4, float4x4, float4x4);"
				""
				"float    step(float, float);"
				"float2   step(float2, float2);"
				"float2x2 step(float2x2, float2x2);"
				"float2x3 step(float2x3, float2x3);"
				"float2x4 step(float2x4, float2x4);"
				"float3   step(float3, float3);"
				"float3x2 step(float3x2, float3x2);"
				"float3x3 step(float3x3, float3x3);"
				"float3x4 step(float3x4, float3x4);"
				"float4   step(float4, float4);"
				"float4x2 step(float4x2, float4x2);"
				"float4x3 step(float4x3, float4x3);"
				"float4x4 step(float4x4, float4x4);"
				""
				"float    smoothstep(float, float, float);"
				"float2   smoothstep(float2, float2, float2);"
				"float2x2 smoothstep(float2x2, float2x2, float2x2);"
				"float2x3 smoothstep(float2x3, float2x3, float2x3);"
				"float2x4 smoothstep(float2x4, float2x4, float2x4);"
				"float3   smoothstep(float3, float3, float3);"
				"float3x2 smoothstep(float3x2, float3x2, float3x2);"
				"float3x3 smoothstep(float3x3, float3x3, float3x3);"
				"float3x4 smoothstep(float3x4, float3x4, float3x4);"
				"float4   smoothstep(float4, float4, float4);"
				"float4x2 smoothstep(float4x2, float4x2, float4x2);"
				"float4x3 smoothstep(float4x3, float4x3, float4x3);"
				"float4x4 smoothstep(float4x4, float4x4, float4x4);"
				""
				"float    length(float);"
				"float    length(float2);"
				"float    length(float3);"
				"float    length(float4);"
				""
				"float    distance(float, float);"
				"float    distance(float2, float2);"
				"float    distance(float3, float3);"
				"float    distance(float4, float4);"
				""
				"float    dot(float, float);"
				"float    dot(float2, float2);"
				"float    dot(float3, float3);"
				"float    dot(float4, float4);"
				""
				"float3   cross(float3, float3);"
				""
				"float    normalize(float);"
				"float2   normalize(float2);"
				"float3   normalize(float3);"
				"float4   normalize(float4);"
				""
				"float    faceforward(float, float, float);"
				"float2   faceforward(float2, float2, float2);"
				"float3   faceforward(float3, float3, float3);"
				"float4   faceforward(float4, float4, float4);"
				""
				"float    reflect(float, float);"
				"float2   reflect(float2, float2);"
				"float3   reflect(float3, float3);"
				"float4   reflect(float4, float4);"
				""
				"float    refract(float, float, float);"
				"float2   refract(float2, float2, float);"
				"float3   refract(float3, float3, float);"
				"float4   refract(float4, float4, float);"
				""
				"float1x1 transpose(float1x1);"
				"float2x1 transpose(float1x2);"
				"float3x1 transpose(float1x3);"
				"float4x1 transpose(float1x4);"
				"float1x2 transpose(float2x1);"
				"float2x2 transpose(float2x2);"
				"float3x2 transpose(float2x3);"
				"float4x2 transpose(float2x4);"
				"float1x3 transpose(float3x1);"
				"float2x3 transpose(float3x2);"
				"float3x3 transpose(float3x3);"
				"float4x3 transpose(float3x4);"
				"float1x4 transpose(float4x1);"
				"float2x4 transpose(float4x2);"
				"float3x4 transpose(float4x3);"
				"float4x4 transpose(float4x4);"
				""
				"float    determinant(float1x1);"
				"float    determinant(float2x2);"
				"float    determinant(float3x3);"
				"float    determinant(float4x4);"
				""
				"bool     any(bool);"
				"bool     any(bool2);"
				"bool     any(bool3);"
				"bool     any(bool4);"
				""
				"bool     all(bool);"
				"bool     all(bool2);"
				"bool     all(bool3);"
				"bool     all(bool4);"
				""
				"float    mad(float, float, float);"
				"float    mad(float1x2, float1x2, float1x2);"
				"float    mad(float1x3, float1x3, float1x3);"
				"float    mad(float1x4, float1x4, float1x4);"
				"float    mad(float2, float2, float2);"
				"float    mad(float2x2, float2x2, float2x2);"
				"float    mad(float2x3, float2x3, float2x3);"
				"float    mad(float2x4, float2x4, float2x4);"
				"float    mad(float3, float3, float3);"
				"float    mad(float3x2, float3x2, float3x2);"
				"float    mad(float3x3, float3x3, float3x3);"
				"float    mad(float3x4, float3x4, float3x4);"
				"float    mad(float4, float4, float4);"
				"float    mad(float4x2, float4x2, float4x2);"
				"float    mad(float4x3, float4x3, float4x3);"
				"float    mad(float4x4, float4x4, float4x4);"
				""
				"float    mul(float1x1, float);"
				"float    mul(float1x2, float2);"
				"float    mul(float1x3, float3);"
				"float    mul(float1x4, float4);"
				"float2   mul(float2x1, float);"
				"float2   mul(float2x2, float2);"
				"float2   mul(float2x3, float3);"
				"float2   mul(float2x4, float4);"
				"float3   mul(float3x1, float);"
				"float3   mul(float3x2, float2);"
				"float3   mul(float3x3, float3);"
				"float3   mul(float3x4, float4);"
				"float4   mul(float4x1, float);"
				"float4   mul(float4x2, float2);"
				"float4   mul(float4x3, float3);"
				"float4   mul(float4x4, float4);"
				"float    mul(float2, float2x1);"
				"float    mul(float3, float3x1);"
				"float    mul(float4, float4x1);"
				"float2   mul(float, float1x2);"
				"float2   mul(float2, float2x2);"
				"float2   mul(float3, float3x2);"
				"float2   mul(float4, float4x2);"
				"float3   mul(float, float1x3);"
				"float3   mul(float2, float2x3);"
				"float3   mul(float3, float3x3);"
				"float3   mul(float4, float4x3);"
				"float4   mul(float, float1x4);"
				"float4   mul(float2, float2x4);"
				"float4   mul(float3, float3x4);"
				"float4   mul(float4, float4x4);"
				"float1x1 mul(float1x1, float1x1);"
				"float1x2 mul(float1x1, float1x2);"
				"float1x3 mul(float1x1, float1x3);"
				"float1x4 mul(float1x1, float1x4);"
				"float1x1 mul(float1x2, float2x1);"
				"float1x2 mul(float1x2, float2x2);"
				"float1x3 mul(float1x2, float2x3);"
				"float1x4 mul(float1x2, float2x4);"
				"float1x1 mul(float1x3, float3x1);"
				"float1x2 mul(float1x3, float3x2);"
				"float1x3 mul(float1x3, float3x3);"
				"float1x4 mul(float1x3, float3x4);"
				"float1x1 mul(float1x4, float4x1);"
				"float1x2 mul(float1x4, float4x2);"
				"float1x3 mul(float1x4, float4x3);"
				"float1x4 mul(float1x4, float4x4);"
				"float2x1 mul(float2x1, float1x1);"
				"float2x2 mul(float2x1, float1x2);"
				"float2x3 mul(float2x1, float1x3);"
				"float2x4 mul(float2x1, float1x4);"
				"float2x1 mul(float2x2, float2x1);"
				"float2x2 mul(float2x2, float2x2);"
				"float2x3 mul(float2x2, float2x3);"
				"float2x4 mul(float2x2, float2x4);"
				"float2x1 mul(float2x3, float3x1);"
				"float2x2 mul(float2x3, float3x2);"
				"float2x3 mul(float2x3, float3x3);"
				"float2x4 mul(float2x3, float3x4);"
				"float2x1 mul(float2x4, float4x1);"
				"float2x2 mul(float2x4, float4x2);"
				"float2x3 mul(float2x4, float4x3);"
				"float2x4 mul(float2x4, float4x4);"
				"float3x1 mul(float3x1, float1x1);"
				"float3x2 mul(float3x1, float1x2);"
				"float3x3 mul(float3x1, float1x3);"
				"float3x4 mul(float3x1, float1x4);"
				"float3x1 mul(float3x2, float2x1);"
				"float3x2 mul(float3x2, float2x2);"
				"float3x3 mul(float3x2, float2x3);"
				"float3x4 mul(float3x2, float2x4);"
				"float3x1 mul(float3x3, float3x1);"
				"float3x2 mul(float3x3, float3x2);"
				"float3x3 mul(float3x3, float3x3);"
				"float3x4 mul(float3x3, float3x4);"
				"float3x1 mul(float3x4, float4x1);"
				"float3x2 mul(float3x4, float4x2);"
				"float3x3 mul(float3x4, float4x3);"
				"float3x4 mul(float3x4, float4x4);"
				"float4x1 mul(float4x1, float1x1);"
				"float4x2 mul(float4x1, float1x2);"
				"float4x3 mul(float4x1, float1x3);"
				"float4x4 mul(float4x1, float1x4);"
				"float4x1 mul(float4x2, float2x1);"
				"float4x2 mul(float4x2, float2x2);"
				"float4x3 mul(float4x2, float2x3);"
				"float4x4 mul(float4x2, float2x4);"
				"float4x1 mul(float4x3, float3x1);"
				"float4x2 mul(float4x3, float3x2);"
				"float4x3 mul(float4x3, float3x3);"
				"float4x4 mul(float4x3, float3x4);"
				"float4x1 mul(float4x4, float4x1);"
				"float4x2 mul(float4x4, float4x2);"
				"float4x3 mul(float4x4, float4x3);"
				"float4x4 mul(float4x4, float4x4);"
				""
				"float    fwidth(float);"
				"float2   fwidth(float2);"
				"float3   fwidth(float3);"
				"float4   fwidth(float4);"
				""
				"float    ddx(float);"
				"float2   ddx(float2);"
				"float3   ddx(float3);"
				"float4   ddx(float4);"
				"float    ddy(float);"
				"float2   ddy(float2);"
				"float3   ddy(float3);"
				"float4   ddy(float4);"
				""
				"float4   tex1D(sampler1D, float);"
				"float4   tex1Doffset(sampler1D, float, int);"
				"float4   tex1Dlod(sampler1D, float4);"
				"float4   tex1Dfetch(sampler1D, int4);"
				"float4   tex1Dbias(sampler1D, float4);"
				"int3     tex1Dsize(sampler1D, int);"
				"float4   tex2D(sampler2D, float2);"
				"float4   tex2Doffset(sampler2D, float2, int2);"
				"float4   tex2Dlod(sampler2D, float4);"
				"float4   tex2Dfetch(sampler2D, int4);"
				"float4   tex2Dbias(sampler2D, float4);"
				"int3     tex2Dsize(sampler2D, int);"
				"float4   tex3D(sampler3D, float3);"
				"float4   tex3Doffset(sampler3D, float3, int3);"
				"float4   tex3Dlod(sampler3D, float4);"
				"float4   tex3Dfetch(sampler3D, int4);"
				"float4   tex3Dbias(sampler3D, float4);"
				"int3     tex3Dsize(sampler3D, int);"
				""
				"int      asint(uint);"
				"int2     asint(uint2);"
				"int2x2   asint(uint2x2);"
				"int2x3   asint(uint2x3);"
				"int2x4   asint(uint2x4);"
				"int3     asint(uint3);"
				"int3x2   asint(uint3x2);"
				"int3x3   asint(uint3x3);"
				"int3x4   asint(uint3x4);"
				"int4     asint(uint4);"
				"int4x2   asint(uint4x2);"
				"int4x3   asint(uint4x3);"
				"int4x4   asint(uint4x4);"
				"int      asint(float);"
				"int2     asint(float2);"
				"int2x2   asint(float2x2);"
				"int2x3   asint(float2x3);"
				"int2x4   asint(float2x4);"
				"int3     asint(float3);"
				"int3x2   asint(float3x2);"
				"int3x3   asint(float3x3);"
				"int3x4   asint(float3x4);"
				"int4     asint(float4);"
				"int4x2   asint(float4x2);"
				"int4x3   asint(float4x3);"
				"int4x4   asint(float4x4);"
				""
				"uint     asuint(int);"
				"uint2    asuint(int2);"
				"uint2x2  asuint(int2x2);"
				"uint2x3  asuint(int2x3);"
				"uint2x4  asuint(int2x4);"
				"uint3    asuint(int3);"
				"uint3x2  asuint(int3x2);"
				"uint3x3  asuint(int3x3);"
				"uint3x4  asuint(int3x4);"
				"uint4    asuint(int4);"
				"uint4x2  asuint(int4x2);"
				"uint4x3  asuint(int4x3);"
				"uint4x4  asuint(int4x4);"
				"uint     asuint(float);"
				"uint2    asuint(float2);"
				"uint2x2  asuint(float2x2);"
				"uint2x3  asuint(float2x3);"
				"uint2x4  asuint(float2x4);"
				"uint3    asuint(float3);"
				"uint3x2  asuint(float3x2);"
				"uint3x3  asuint(float3x3);"
				"uint3x4  asuint(float3x4);"
				"uint4    asuint(float4);"
				"uint4x2  asuint(float4x2);"
				"uint4x3  asuint(float4x3);"
				"uint4x4  asuint(float4x4);"
				""
				"float    asfloat(float);"
				"float2   asfloat(float2);"
				"float2x2 asfloat(float2x2);"
				"float2x3 asfloat(float2x3);"
				"float2x4 asfloat(float2x4);"
				"float3   asfloat(float3);"
				"float3x2 asfloat(float3x2);"
				"float3x3 asfloat(float3x3);"
				"float3x4 asfloat(float3x4);"
				"float4   asfloat(float4);"
				"float4x2 asfloat(float4x2);"
				"float4x3 asfloat(float4x3);"
				"float4x4 asfloat(float4x4);"
				"float    asfloat(int);"
				"float2   asfloat(int2);"
				"float2x2 asfloat(int2x2);"
				"float2x3 asfloat(int2x3);"
				"float2x4 asfloat(int2x4);"
				"float3   asfloat(int3);"
				"float3x2 asfloat(int3x2);"
				"float3x3 asfloat(int3x3);"
				"float3x4 asfloat(int3x4);"
				"float4   asfloat(int4);"
				"float4x2 asfloat(int4x2);"
				"float4x3 asfloat(int4x3);"
				"float4x4 asfloat(int4x4);"
				"float    asfloat(uint);"
				"float2   asfloat(uint2);"
				"float2x2 asfloat(uint2x2);"
				"float2x3 asfloat(uint2x3);"
				"float2x4 asfloat(uint2x4);"
				"float3   asfloat(uint3);"
				"float3x2 asfloat(uint3x2);"
				"float3x3 asfloat(uint3x3);"
				"float3x4 asfloat(uint3x4);"
				"float4   asfloat(uint4);"
				"float4x2 asfloat(uint4x2);"
				"float4x3 asfloat(uint4x3);"
				"float4x4 asfloat(uint4x4);";
			#pragma endregion

			LOG(TRACE) << "> Running parser ...";

			parser.Parse(intrinsics, ast, false);
			status = parser.Parse(source, ast, errors, true);
		}

		if (status)
		{
			LOG(TRACE) << "> Running compiler ...";

			return Compile(ast, errors);
		}
		else
		{
			return nullptr;
		}
	}

	// -----------------------------------------------------------------------------------------------------

	template <> void											Effect::Constant::GetValue<bool>(bool *values, std::size_t count) const
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

		switch (desc.Type)
		{
			case Type::Bool:
			case Type::Int:
			case Type::Uint:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = reinterpret_cast<const int *>(data)[i] != 0;
				}
				break;
			}
			case Type::Float:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = reinterpret_cast<const float *>(data)[i] != 0.0f;
				}
				break;
			}
			case Type::Double:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = reinterpret_cast<const double *>(data)[i] != 0.0;
				}
				break;
			}
		}
	}
	template <> void											Effect::Constant::GetValue<int>(int *values, std::size_t count) const
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		if (desc.Type == Type::Bool || desc.Type == Type::Int || desc.Type == Type::Uint)
		{
			GetValue(reinterpret_cast<unsigned char *>(values), count * sizeof(int));
			return;
		}

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

		switch (desc.Type)
		{
			case Type::Float:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<int>(reinterpret_cast<const float *>(data)[i]);
				}
				break;
			}
			case Type::Double:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<int>(reinterpret_cast<const double *>(data)[i]);
				}
				break;
			}
		}
	}
	template <> void											Effect::Constant::GetValue<unsigned int>(unsigned int *values, std::size_t count) const
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		if (desc.Type == Type::Bool || desc.Type == Type::Int || desc.Type == Type::Uint)
		{
			GetValue(reinterpret_cast<unsigned char *>(values), count * sizeof(int));
			return;
		}

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

		switch (desc.Type)
		{
			case Type::Float:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<unsigned int>(reinterpret_cast<const float *>(data)[i]);
				}
				break;
			}
			case Type::Double:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<unsigned int>(reinterpret_cast<const double *>(data)[i]);
				}
				break;
			}
		}
	}
	template <> void											Effect::Constant::GetValue<long>(long *values, std::size_t count) const
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

		switch (desc.Type)
		{
			case Type::Bool:
			case Type::Int:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<long>(reinterpret_cast<const int *>(data)[i]);
				}
				break;
			}
			case Type::Uint:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<long>(reinterpret_cast<const unsigned int *>(data)[i]);
				}
				break;
			}
			case Type::Float:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<long>(reinterpret_cast<const float *>(data)[i]);
				}
				break;
			}
			case Type::Double:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<long>(reinterpret_cast<const double *>(data)[i]);
				}
				break;
			}
		}
	}
	template <> void											Effect::Constant::GetValue<unsigned long>(unsigned long *values, std::size_t count) const
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

		switch (desc.Type)
		{
			case Type::Bool:
			case Type::Int:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<unsigned long>(reinterpret_cast<const int *>(data)[i]);
				}
				break;
			}
			case Type::Uint:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<unsigned long>(reinterpret_cast<const unsigned int *>(data)[i]);
				}
				break;
			}
			case Type::Float:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<unsigned long>(reinterpret_cast<const float *>(data)[i]);
				}
				break;
			}
			case Type::Double:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<unsigned long>(reinterpret_cast<const double *>(data)[i]);
				}
				break;
			}
		}
	}
	template <> void											Effect::Constant::GetValue<long long>(long long *values, std::size_t count) const
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

		switch (desc.Type)
		{
			case Type::Bool:
			case Type::Int:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<long long>(reinterpret_cast<const int *>(data)[i]);
				}
				break;
			}
			case Type::Uint:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<long long>(reinterpret_cast<const unsigned int *>(data)[i]);
				}
				break;
			}
			case Type::Float:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<long long>(reinterpret_cast<const float *>(data)[i]);
				}
				break;
			}
			case Type::Double:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<long long>(reinterpret_cast<const double *>(data)[i]);
				}
				break;
			}
		}
	}
	template <> void											Effect::Constant::GetValue<unsigned long long>(unsigned long long *values, std::size_t count) const
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

		switch (desc.Type)
		{
			case Type::Bool:
			case Type::Int:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<unsigned long long>(reinterpret_cast<const int *>(data)[i]);
				}
				break;
			}
			case Type::Uint:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<unsigned long long>(reinterpret_cast<const unsigned int *>(data)[i]);
				}
				break;
			}
			case Type::Float:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<unsigned long long>(reinterpret_cast<const float *>(data)[i]);
				}
				break;
			}
			case Type::Double:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<unsigned long long>(reinterpret_cast<const double *>(data)[i]);
				}
				break;
			}
		}
	}
	template <> void											Effect::Constant::GetValue<float>(float *values, std::size_t count) const
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		if (desc.Type == Type::Float)
		{
			GetValue(reinterpret_cast<unsigned char *>(values), count * sizeof(float));
			return;
		}

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

		switch (desc.Type)
		{
			case Type::Bool:
			case Type::Int:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<float>(reinterpret_cast<const int *>(data)[i]);
				}
				break;
			}
			case Type::Uint:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<float>(reinterpret_cast<const unsigned int *>(data)[i]);
				}
				break;
			}
			case Type::Double:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<float>(reinterpret_cast<const double *>(data)[i]);
				}
				break;
			}
		}
	}
	template <> void											Effect::Constant::GetValue<double>(double *values, std::size_t count) const
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		if (desc.Type == Type::Double)
		{
			GetValue(reinterpret_cast<unsigned char *>(values), count * sizeof(double));
			return;
		}

		unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
		GetValue(data, desc.Size);

		switch (desc.Type)
		{
			case Type::Bool:
			case Type::Int:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<double>(reinterpret_cast<const int *>(data)[i]);
				}
				break;
			}
			case Type::Uint:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<double>(reinterpret_cast<const unsigned int *>(data)[i]);
				}
				break;
			}
			case Type::Float:
			{
				for (std::size_t i = 0; i < count; ++i)
				{
					values[i] = static_cast<double>(reinterpret_cast<const float *>(data)[i]);
				}
				break;
			}
		}
	}
	template <> void											Effect::Constant::SetValue<bool>(const bool *values, std::size_t count)
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		const unsigned char *data = nullptr;
		std::size_t dataSize = 0;

		switch (desc.Type)
		{
			case Type::Bool:
			{
				dataSize = count * sizeof(int);
				int *dataTyped = static_cast<int *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = values[i] ? -1 : 0;
				}
				break;
			}
			case Type::Int:
			case Type::Uint:
			{
				dataSize = count * sizeof(int);
				int *dataTyped = static_cast<int *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = values[i] ? 1 : 0;
				}
				break;
			}
			case Type::Float:
			{
				dataSize = count * sizeof(float);
				float *dataTyped = static_cast<float *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = values[i] ? 1.0f : 0.0f;
				}
				break;
			}
			case Type::Double:
			{
				dataSize = count * sizeof(double);
				double *dataTyped = static_cast<double *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = values[i] ? 1.0 : 0.0;
				}
				break;
			}
		}

		SetValue(data, dataSize);
	}
	template <> void											Effect::Constant::SetValue<int>(const int *values, std::size_t count)
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		const unsigned char *data = nullptr;
		std::size_t dataSize = 0;

		switch (desc.Type)
		{
			case Type::Bool:
			case Type::Int:
			case Type::Uint:
			{
				data = reinterpret_cast<const unsigned char *>(values);
				dataSize = count * sizeof(int);
				break;
			}
			case Type::Float:
			{
				dataSize = count * sizeof(float);
				float *dataTyped = static_cast<float *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<float>(values[i]);
				}
				break;
			}
			case Type::Double:
			{
				dataSize = count * sizeof(double);
				double *dataTyped = static_cast<double *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<double>(values[i]);
				}
				break;
			}
		}

		SetValue(data, dataSize);
	}
	template <> void											Effect::Constant::SetValue<unsigned int>(const unsigned int *values, std::size_t count)
	{
		const Description desc = GetDescription();
		
		assert(count == 0 || values != nullptr);

		const unsigned char *data = nullptr;
		std::size_t dataSize = 0;

		switch (desc.Type)
		{
			case Type::Bool:
			case Type::Int:
			case Type::Uint:
			{
				data = reinterpret_cast<const unsigned char *>(values);
				dataSize = count * sizeof(int);
				break;
			}
			case Type::Float:
			{
				dataSize = count * sizeof(float);
				float *dataTyped = static_cast<float *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);
				
				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<float>(values[i]);
				}
				break;
			}
			case Type::Double:
			{
				dataSize = count * sizeof(double);
				double *dataTyped = static_cast<double *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);
				
				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<double>(values[i]);
				}
				break;
			}
		}

		SetValue(data, dataSize);
	}
	template <> void											Effect::Constant::SetValue<long>(const long *values, std::size_t count)
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		const unsigned char *data = nullptr;
		std::size_t dataSize = 0;

		switch (desc.Type)
		{
			case Type::Bool:
			case Type::Int:
			case Type::Uint:
			{
				dataSize = count * sizeof(int);
				int *dataTyped = static_cast<int *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<int>(values[i]);
				}
				break;
			}
			case Type::Float:
			{
				dataSize = count * sizeof(float);
				float *dataTyped = static_cast<float *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<float>(values[i]);
				}
				break;
			}
			case Type::Double:
			{
				dataSize = count * sizeof(double);
				double *dataTyped = static_cast<double *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<double>(values[i]);
				}
				break;
			}
		}

		SetValue(data, dataSize);
	}
	template <> void											Effect::Constant::SetValue<unsigned long>(const unsigned long *values, std::size_t count)
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		const unsigned char *data = nullptr;
		std::size_t dataSize = 0;

		switch (desc.Type)
		{
			case Type::Bool:
			case Type::Int:
			case Type::Uint:
			{
				dataSize = count * sizeof(int);
				int *dataTyped = static_cast<int *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<int>(values[i]);
				}
				break;
			}
			case Type::Float:
			{
				dataSize = count * sizeof(float);
				float *dataTyped = static_cast<float *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<float>(values[i]);
				}
				break;
			}
			case Type::Double:
			{
				dataSize = count * sizeof(double);
				double *dataTyped = static_cast<double *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<double>(values[i]);
				}
				break;
			}
		}

		SetValue(data, dataSize);
	}
	template <> void											Effect::Constant::SetValue<long long>(const long long *values, std::size_t count)
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		const unsigned char *data = nullptr;
		std::size_t dataSize = 0;

		switch (desc.Type)
		{
			case Type::Bool:
			case Type::Int:
			case Type::Uint:
			{
				dataSize = count * sizeof(int);
				int *dataTyped = static_cast<int *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<int>(values[i]);
				}
				break;
			}
			case Type::Float:
			{
				dataSize = count * sizeof(float);
				float *dataTyped = static_cast<float *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<float>(values[i]);
				}
				break;
			}
			case Type::Double:
			{
				dataSize = count * sizeof(double);
				double *dataTyped = static_cast<double *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<double>(values[i]);
				}
				break;
			}
		}

		SetValue(data, dataSize);
	}
	template <> void											Effect::Constant::SetValue<unsigned long long>(const unsigned long long *values, std::size_t count)
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		const unsigned char *data = nullptr;
		std::size_t dataSize = 0;

		switch (desc.Type)
		{
			case Type::Bool:
			case Type::Int:
			case Type::Uint:
			{
				dataSize = count * sizeof(int);
				int *dataTyped = static_cast<int *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<int>(values[i]);
				}
				break;
			}
			case Type::Float:
			{
				dataSize = count * sizeof(float);
				float *dataTyped = static_cast<float *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<float>(values[i]);
				}
				break;
			}
			case Type::Double:
			{
				dataSize = count * sizeof(double);
				double *dataTyped = static_cast<double *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<double>(values[i]);
				}
				break;
			}
		}

		SetValue(data, dataSize);
	}
	template <> void											Effect::Constant::SetValue<float>(const float *values, std::size_t count)
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		const unsigned char *data = nullptr;
		std::size_t dataSize = 0;

		switch (desc.Type)
		{
			case Type::Bool:
			case Type::Int:
			case Type::Uint:
			{
				dataSize = count * sizeof(int);
				int *dataTyped = static_cast<int *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<int>(values[i]);
				}
				break;
			}
			case Type::Float:
			{
				data = reinterpret_cast<const unsigned char *>(values);
				dataSize = count * sizeof(float);
				break;
			}
			case Type::Double:
			{
				dataSize = count * sizeof(double);
				double *dataTyped = static_cast<double *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<double>(values[i]);
				}
				break;
			}
		}

		SetValue(data, dataSize);
	}
	template <> void											Effect::Constant::SetValue<double>(const double *values, std::size_t count)
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		const unsigned char *data = nullptr;
		std::size_t dataSize = 0;

		switch (desc.Type)
		{
			case Type::Bool:
			case Type::Int:
			case Type::Uint:
			{
				dataSize = count * sizeof(int);
				int *dataTyped = static_cast<int *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<int>(values[i]);
				}
				break;
			}
			case Type::Float:
			{
				dataSize = count * sizeof(float);
				float *dataTyped = static_cast<float *>(::alloca(dataSize));
				data = reinterpret_cast<const unsigned char *>(dataTyped);

				for (std::size_t i = 0; i < count; ++i)
				{
					dataTyped[i] = static_cast<float>(values[i]);
				}
				break;
			}
			case Type::Double:
			{
				data = reinterpret_cast<const unsigned char *>(values);
				dataSize = count * sizeof(double);
				break;
			}
		}

		SetValue(data, dataSize);
	}

	void														Effect::Technique::RenderPass(const std::string &name) const
	{
		if (name.empty())
		{
			return;
		}

		const Description desc = GetDescription();

		for (auto begin = desc.Passes.cbegin(), end = desc.Passes.cend(), it = begin; it != end; ++it)
		{
			if (*it == name)
			{
				RenderPass(static_cast<unsigned int>(it - begin));
				break;
			}
		}
	}
}