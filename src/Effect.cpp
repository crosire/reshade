#include "Log.hpp"
#include "Effect.hpp"
#include "EffectParser.hpp"
#include "EffectPreprocessor.hpp"

#include <boost\assign\list_of.hpp>

namespace ReShade
{
	static inline bool											GetCallRanks(const EffectTree &ast, const EffectNodes::Call &call, const EffectTree &ast1, const EffectNodes::Function *function, unsigned int ranks[], unsigned int argumentCount)
	{
		const EffectNodes::List &arguments = ast[call.Arguments].As<EffectNodes::List>();
		const EffectNodes::List &parameters = ast1[function->Parameters].As<EffectNodes::List>();

		for (unsigned int i = 0; i < argumentCount; ++i)
		{
			ranks[i] = EffectNodes::Type::Compatible(ast[arguments[i]].As<EffectNodes::RValue>().Type, ast1[parameters[i]].As<EffectNodes::Variable>().Type);
			
			if (!ranks[i])
			{
				return false;
			}
		}

		return true;
	}
	static int													CompareFunctions(const EffectTree &ast, const EffectNodes::Call &call, const EffectTree &ast1, const EffectNodes::Function *function1, const EffectTree &ast2, const EffectNodes::Function *function2, unsigned int argumentCount)
	{
		if (function2 == nullptr)
		{
			return -1;
		}

		// Adapted from: https://github.com/unknownworlds/hlslparser
		unsigned int *function1Ranks = static_cast<unsigned int *>(alloca(argumentCount * sizeof(unsigned int)));
		unsigned int *function2Ranks = static_cast<unsigned int *>(alloca(argumentCount * sizeof(unsigned int)));
		const bool function1Viable = GetCallRanks(ast, call, ast1, function1, function1Ranks, argumentCount);
		const bool function2Viable = GetCallRanks(ast, call, ast2, function2, function2Ranks, argumentCount);

		if (!(function1Viable && function2Viable))
		{
			if (function1Viable)
			{
				return -1;
			}
			else if (function2Viable)
			{
				return +1;
			}
			else
			{
				return 0;
			}
		}
		
		std::sort(function1Ranks, function1Ranks + argumentCount, std::greater<unsigned int>());
		std::sort(function2Ranks, function2Ranks + argumentCount, std::greater<unsigned int>());
		
		for (unsigned int i = 0; i < argumentCount; ++i)
		{
			if (function1Ranks[i] < function2Ranks[i])
			{
				return -1;
			}
			else if (function2Ranks[i] < function1Ranks[i])
			{
				return +1;
			}
		}

		return 0;
	}

	bool 														EffectParser::ResolveCall(EffectNodes::Call &call, bool &intrinsic, bool &ambiguous) const
	{
		intrinsic = false;
		ambiguous = false;

		#pragma region Intrinsics
		static EffectTree sIntrinsics;
		static bool sIntrinsicsInitialized = false;
		static const char *sIntrinsicOverloads =
			"float abs(float x); float2 abs(float2 x); float3 abs(float3 x); float4 abs(float4 x);"
			"int sign(float x); int2 sign(float2 x); int3 sign(float3 x); int4 sign(float4 x);"
			"float rcp(float x); float2 rcp(float2 x); float3 rcp(float3 x); float4 rcp(float4 x);"
			"bool all(bool x); bool all(bool2 x); bool all(bool3 x); bool all(bool4 x);"
			"bool any(bool x); bool any(bool2 x); bool any(bool3 x); bool any(bool4 x);"
			"float sin(float x); float2 sin(float2 x); float3 sin(float3 x); float4 sin(float4 x);"
			"float sinh(float x); float2 sinh(float2 x); float3 sinh(float3 x); float4 sinh(float4 x);"
			"float cos(float x); float2 cos(float2 x); float3 cos(float3 x); float4 cos(float4 x);"
			"float cosh(float x); float2 cosh(float2 x); float3 cosh(float3 x); float4 cosh(float4 x);"
			"float tan(float x); float2 tan(float2 x); float3 tan(float3 x); float4 tan(float4 x);"
			"float tanh(float x); float2 tanh(float2 x); float3 tanh(float3 x); float4 tanh(float4 x);"
			"float asin(float x); float2 asin(float2 x); float3 asin(float3 x); float4 asin(float4 x);"
			"float acos(float x); float2 acos(float2 x); float3 acos(float3 x); float4 acos(float4 x);"
			"float atan(float x); float2 atan(float2 x); float3 atan(float3 x); float4 atan(float4 x);"
			"float atan2(float x, float y); float2 atan2(float2 x, float2 y); float3 atan2(float3 x, float3 y); float4 atan2(float4 x, float4 y);"
			"void sincos(float x, out float s, out float c); void sincos(float2 x, out float2 s, out float2 c); void sincos(float3 x, out float3 s, out float3 c); void sincos(float4 x, out float4 s, out float4 c);"
			"float exp(float x); float2 exp(float2 x); float3 exp(float3 x); float4 exp(float4 x);"
			"float exp2(float x); float2 exp2(float2 x); float3 exp2(float3 x); float4 exp2(float4 x);"
			"float log(float x); float2 log(float2 x); float3 log(float3 x); float4 log(float4 x);"
			"float log2(float x); float2 log2(float2 x); float3 log2(float3 x); float4 log2(float4 x);"
			"float log10(float x); float2 log10(float2 x); float3 log10(float3 x); float4 log10(float4 x);"
			"float sqrt(float x); float2 sqrt(float2 x); float3 sqrt(float3 x); float4 sqrt(float4 x);"
			"float rsqrt(float x); float2 rsqrt(float2 x); float3 rsqrt(float3 x); float4 rsqrt(float4 x);"
			"float ceil(float x); float2 ceil(float2 x); float3 ceil(float3 x); float4 ceil(float4 x);"
			"float floor(float x); float2 floor(float2 x); float3 floor(float3 x); float4 floor(float4 x);"
			"float frac(float x); float2 frac(float2 x); float3 frac(float3 x); float4 frac(float4 x);"
			"float trunc(float x); float2 trunc(float2 x); float3 trunc(float3 x); float4 trunc(float4 x);"
			"float round(float x); float2 round(float2 x); float3 round(float3 x); float4 round(float4 x);"
			"float radians(float x); float2 radians(float2 x); float3 radians(float3 x); float4 radians(float4 x);"
			"float degrees(float x); float2 degrees(float2 x); float3 degrees(float3 x); float4 degrees(float4 x);"
			"float noise(float x); float noise(float2 x); float noise(float3 x); float noise(float4 x);"
			"float length(float x); float length(float2 x); float length(float3 x); float length(float4 x);"
			"float normalize(float x); float2 normalize(float2 x); float3 normalize(float3 x); float4 normalize(float4 x);"
			"float2x2 transpose(float2x2 m); float3x3 transpose(float3x3 m); float4x4 transpose(float4x4 m);"
			"float determinant(float2x2 m); float determinant(float3x3 m); float determinant(float4x4 m);"
			"int asint(float x); int2 asint(float2 x); int3 asint(float3 x); int4 asint(float4 x);"
			"uint asuint(float x); uint2 asuint(float2 x); uint3 asuint(float3 x); uint4 asuint(float4 x);"
			"float asfloat(int x); float2 asfloat(int2 x); float3 asfloat(int3 x); float4 asfloat(int4 x);"
			"float asfloat(uint x); float2 asfloat(uint2 x); float3 asfloat(uint3 x); float4 asfloat(uint4 x);"
			"float mul(float a, float b);"
			"float2 mul(float a, float2 b); float3 mul(float a, float3 b); float4 mul(float a, float4 b);"
			"float2 mul(float2 a, float b); float3 mul(float3 a, float b); float4 mul(float4 a, float b);"
			"float2x2 mul(float a, float2x2 b); float3x3 mul(float a, float3x3 b); float4x4 mul(float a, float4x4 b);"
			"float2x2 mul(float2x2 a, float b); float3x3 mul(float3x3 a, float b); float4x4 mul(float4x4 a, float b);"
			"float2 mul(float2 a, float2x2 b); float3 mul(float3 a, float3x3 b); float4 mul(float4 a, float4x4 b);"
			"float2 mul(float2x2 a, float2 b); float3 mul(float3x3 a, float3 b); float4 mul(float4x4 a, float4 b);"
			"float mad(float m, float a, float b); float2 mad(float2 m, float2 a, float2 b); float3 mad(float3 m, float3 a, float3 b); float4 mad(float4 m, float4 a, float4 b);"
			"float dot(float x, float y); float dot(float2 x, float2 y); float dot(float3 x, float3 y); float dot(float4 x, float4 y);"
			"float3 cross(float3 x, float3 y);"
			"float distance(float x, float y); float distance(float2 x, float2 y); float distance(float3 x, float3 y); float distance(float4 x, float4 y);"
			"float pow(float x, float y); float2 pow(float2 x, float2 y); float3 pow(float3 x, float3 y); float4 pow(float4 x, float4 y);"
			"float modf(float x, out float ip); float2 modf(float2 x, out float2 ip); float3 modf(float3 x, out float3 ip); float4 modf(float4 x, out float4 ip);"
			"float frexp(float x, out float exp); float2 frexp(float2 x, out float2 exp); float3 frexp(float3 x, out float3 exp); float4 frexp(float4 x, out float4 exp);"
			"float ldexp(float x, float exp); float2 ldexp(float2 x, float2 exp); float3 ldexp(float3 x, float3 exp); float4 ldexp(float4 x, float4 exp);"
			"float min(float x, float y); float2 min(float2 x, float2 y); float3 min(float3 x, float3 y); float4 min(float4 x, float4 y);"
			"float max(float x, float y); float2 max(float2 x, float2 y); float3 max(float3 x, float3 y); float4 max(float4 x, float4 y);"
			"float clamp(float x, float min, float max); float2 clamp(float2 x, float2 min, float2 max); float3 clamp(float3 x, float3 min, float3 max); float4 clamp(float4 x, float4 min, float4 max);"
			"float saturate(float x); float2 saturate(float2 x); float3 saturate(float3 x); float4 saturate(float4 x);"
			"float step(float y, float x); float2 step(float2 y, float2 x); float3 step(float3 y, float3 x); float4 step(float4 y, float4 x);"
			"float smoothstep(float min, float max, float x); float2 smoothstep(float2 min, float2 max, float2 x); float3 smoothstep(float3 min, float3 max, float3 x); float4 smoothstep(float4 min, float4 max, float4 x);"
			"float lerp(float x, float y, float s); float2 lerp(float2 x, float2 y, float2 s); float3 lerp(float3 x, float3 y, float3 s); float4 lerp(float4 x, float4 y, float4 s);"
			"float reflect(float i, float n); float2 reflect(float2 i, float2 n); float3 reflect(float3 i, float3 n); float4 reflect(float4 i, float4 n);"
			"float refract(float i, float n, float r); float2 refract(float2 i, float2 n, float r); float3 refract(float3 i, float3 n, float r); float4 refract(float4 i, float4 n, float r);"
			"float faceforward(float n, float i, float ng); float2 faceforward(float2 n, float2 i, float2 ng); float3 faceforward(float3 n, float3 i, float3 ng); float4 faceforward(float4 n, float4 i, float4 ng);"
			"float4 tex2D(sampler2D s, float2 t);"
			"float4 tex2Doffset(sampler2D s, float2 t, int2 o);"
			"float4 tex2Dlod(sampler2D s, float4 t);"
			"float4 tex2Dlodoffset(sampler2D s, float4 t, int2 o);"
			"float4 tex2Dgather(sampler2D s, float2 t);"
			"float4 tex2Dgatheroffset(sampler2D s, float2 t, int2 o);"
			"float4 tex2Dfetch(sampler2D s, int2 t);"
			"float4 tex2Dbias(sampler2D s, float4 t);"
			"int2 tex2Dsize(sampler2D s, int lod);";
		static const std::unordered_map<std::string, unsigned int> sIntrinsicOperators =
			boost::assign::map_list_of
			("abs", EffectNodes::Expression::Abs)
			("sign", EffectNodes::Expression::Sign)
			("rcp", EffectNodes::Expression::Rcp)
			("all", EffectNodes::Expression::All)
			("any", EffectNodes::Expression::Any)
			("sin", EffectNodes::Expression::Sin)
			("sinh", EffectNodes::Expression::Sinh)
			("cos", EffectNodes::Expression::Cos)
			("cosh", EffectNodes::Expression::Cosh)
			("tan", EffectNodes::Expression::Tan)
			("tanh", EffectNodes::Expression::Tanh)
			("asin", EffectNodes::Expression::Asin)
			("acos", EffectNodes::Expression::Acos)
			("atan", EffectNodes::Expression::Atan)
			("atan2", EffectNodes::Expression::Atan2)
			("sincos", EffectNodes::Expression::SinCos)
			("exp", EffectNodes::Expression::Exp)
			("exp2", EffectNodes::Expression::Exp2)
			("log", EffectNodes::Expression::Log)
			("log2", EffectNodes::Expression::Log2)
			("log10", EffectNodes::Expression::Log10)
			("sqrt", EffectNodes::Expression::Sqrt)
			("rsqrt", EffectNodes::Expression::Rsqrt)
			("ceil", EffectNodes::Expression::Ceil)
			("floor", EffectNodes::Expression::Floor)
			("frac", EffectNodes::Expression::Frac)
			("trunc", EffectNodes::Expression::Trunc)
			("round", EffectNodes::Expression::Round)
			("radians", EffectNodes::Expression::Radians)
			("degrees", EffectNodes::Expression::Degrees)
			("noise", EffectNodes::Expression::Noise)
			("length", EffectNodes::Expression::Length)
			("normalize", EffectNodes::Expression::Normalize)
			("transpose", EffectNodes::Expression::Transpose)
			("determinant", EffectNodes::Expression::Determinant)
			("asint", EffectNodes::Expression::BitCastFloat2Int)
			("asuint", EffectNodes::Expression::BitCastFloat2Uint)
			("asfloat", EffectNodes::Expression::BitCastInt2Float)
			("asfloat", EffectNodes::Expression::BitCastUint2Float)
			("mul", EffectNodes::Expression::Mul)
			("mad", EffectNodes::Expression::Mad)
			("dot", EffectNodes::Expression::Dot)
			("cross", EffectNodes::Expression::Cross)
			("distance", EffectNodes::Expression::Distance)
			("pow", EffectNodes::Expression::Pow)
			("modf", EffectNodes::Expression::Modf)
			("frexp", EffectNodes::Expression::Frexp)
			("ldexp", EffectNodes::Expression::Ldexp)
			("min", EffectNodes::Expression::Min)
			("max", EffectNodes::Expression::Max)
			("clamp", EffectNodes::Expression::Clamp)
			("saturate", EffectNodes::Expression::Saturate)
			("step", EffectNodes::Expression::Step)
			("smoothstep", EffectNodes::Expression::SmoothStep)
			("lerp", EffectNodes::Expression::Lerp)
			("reflect", EffectNodes::Expression::Reflect)
			("refract", EffectNodes::Expression::Refract)
			("faceforward", EffectNodes::Expression::FaceForward)
			("tex2D", EffectNodes::Expression::Tex)
			("tex2Doffset", EffectNodes::Expression::TexOffset)
			("tex2Dlod", EffectNodes::Expression::TexLevel)
			("tex2Dlodoffset",	EffectNodes::Expression::TexLevelOffset)
			("tex2Dgather", EffectNodes::Expression::TexGather)
			("tex2Dgatheroffset", EffectNodes::Expression::TexGatherOffset)
			("tex2Dbias", EffectNodes::Expression::TexBias)
			("tex2Dfetch", EffectNodes::Expression::TexFetch)
			("tex2Dsize", EffectNodes::Expression::TexSize);

		if (!sIntrinsicsInitialized)
		{
			sIntrinsicsInitialized = EffectParser(sIntrinsics).Parse(sIntrinsicOverloads);
		}
		#pragma endregion

		const unsigned int argumentCount = call.HasArguments() ? this->mAST[call.Arguments].As<EffectNodes::List>().Length : 0;
		EffectNodes::Function const *overload = nullptr;
		unsigned int overloadCount = 0;
		unsigned int intrinsicOperator = EffectNodes::Expression::None;

		const auto it = this->mSymbolStack.find(call.CalleeName);

		if (it != this->mSymbolStack.end() && !it->second.empty())
		{
			const auto &scopes = it->second;

			for (auto it = scopes.rbegin(), end = scopes.rend(); it != end; ++it)
			{
				if (it->first > this->mCurrentScope)
				{
					continue;
				}
		
				const EffectTree::Node &symbol = this->mAST[it->second];
		
				if (!symbol.Is<EffectNodes::Function>())
				{
					continue;
				}

				const EffectNodes::Function &function = symbol.As<EffectNodes::Function>();

				if (!function.HasParameters())
				{
					if (argumentCount == 0)
					{
						overload = &function;
						overloadCount = 1;
						break;
					}
					else
					{
						continue;
					}
				}
				else if (argumentCount != this->mAST[function.Parameters].As<EffectNodes::List>().Length)
				{
					continue;
				}

				const int result = CompareFunctions(this->mAST, call, this->mAST, &function, this->mAST, overload, argumentCount);

				if (result < 0)
				{
					overload = &function;
					overloadCount = 1;
				}
				else if (result == 0)
				{
					++overloadCount;
				}
			}
		}

		const auto &intrinsics = sIntrinsics.Get().As<EffectNodes::List>();

		for (unsigned int i = 0; i < intrinsics.Length; ++i)
		{
			const EffectNodes::Function &function = sIntrinsics[intrinsics[i]].As<EffectNodes::Function>();

			if (::strcmp(function.Name, call.CalleeName) != 0)
			{
				continue;
			}
			if (argumentCount != sIntrinsics[function.Parameters].As<EffectNodes::List>().Length)
			{
				break;
			}

			const int result = CompareFunctions(this->mAST, call, sIntrinsics, &function, intrinsic ? sIntrinsics : this->mAST, overload, argumentCount);
				
			if (result < 0)
			{
				overload = &function;
				overloadCount = 1;

				intrinsic = true;
				intrinsicOperator = sIntrinsicOperators.at(call.CalleeName);
			}
			else if (result == 0)
			{
				++overloadCount;
			}
		}

		if (overloadCount == 1)
		{
			call.Type = overload->ReturnType;
			call.Callee = intrinsic ? intrinsicOperator : overload->Index;

			return true;
		}
		else
		{
			ambiguous = overloadCount > 1;

			return false;
		}
	}

	// -----------------------------------------------------------------------------------------------------

	EffectPreprocessor::EffectPreprocessor() : mTags(7), mScratch(16384), mScratchCursor(0)
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

	void														EffectPreprocessor::AddDefine(const std::string &name, const std::string &value)
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
	void														EffectPreprocessor::AddIncludePath(const boost::filesystem::path &path)
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

	std::string													EffectPreprocessor::Run(const boost::filesystem::path &path, std::string &errors)
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

	char *														EffectPreprocessor::OnInput(char *buffer, int size, void *userdata)
	{
		EffectPreprocessor *pp = static_cast<EffectPreprocessor *>(userdata);

		pp->mInput.read(buffer, static_cast<std::streamsize>(size - 1));
		const std::streamsize gcount = pp->mInput.gcount();
		buffer[gcount] = '\0';

		return gcount > 0 ? buffer : nullptr;
	}
	void														EffectPreprocessor::OnOutput(int ch, void *userdata)
	{
		EffectPreprocessor *pp = static_cast<EffectPreprocessor *>(userdata);

		pp->mOutput += static_cast<char>(ch);
	}
	void														EffectPreprocessor::OnError(void *userdata, char *fmt, va_list args)
	{
		EffectPreprocessor *pp = static_cast<EffectPreprocessor *>(userdata);

		char buffer[1024];
		vsprintf_s(buffer, fmt, args);

		pp->mErrors += buffer;
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
		}
	}
	template <> void											Effect::Constant::GetValue<int>(int *values, std::size_t count) const
	{
		const Description desc = GetDescription();

		assert(count == 0 || values != nullptr);

		if (desc.Type == Type::Bool || desc.Type == Type::Int || desc.Type == Type::Uint)
		{
			GetValue(reinterpret_cast<unsigned char *>(values), count * sizeof(int));
		}
		else
		{
			unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
			GetValue(data, desc.Size);

			for (std::size_t i = 0; i < count; ++i)
			{
				values[i] = static_cast<int>(reinterpret_cast<const float *>(data)[i]);
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
		}
		else
		{
			unsigned char *data = static_cast<unsigned char *>(::alloca(desc.Size));
			GetValue(data, desc.Size);

			for (std::size_t i = 0; i < count; ++i)
			{
				values[i] = static_cast<unsigned int>(reinterpret_cast<const float *>(data)[i]);
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
		}
	}
	template <> void											Effect::Constant::GetValue<double>(double *values, std::size_t count) const
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
		}

		SetValue(data, dataSize);
	}
}