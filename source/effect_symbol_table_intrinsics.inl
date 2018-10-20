/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#define T_VOID  { reshadefx::type::t_void }
#define T_BOOL1 { reshadefx::type::t_bool, 1, 1 }
#define T_BOOL2 { reshadefx::type::t_bool, 2, 1 }
#define T_BOOL3 { reshadefx::type::t_bool, 3, 1 }
#define T_BOOL4 { reshadefx::type::t_bool, 4, 1 }
#define T_INT1 { reshadefx::type::t_int, 1, 1 }
#define T_INT2 { reshadefx::type::t_int, 2, 1 }
#define T_INT3 { reshadefx::type::t_int, 3, 1 }
#define T_INT4 { reshadefx::type::t_int, 4, 1 }
#define T_UINT1 { reshadefx::type::t_uint, 1, 1 }
#define T_UINT2 { reshadefx::type::t_uint, 2, 1 }
#define T_UINT3 { reshadefx::type::t_uint, 3, 1 }
#define T_UINT4 { reshadefx::type::t_uint, 4, 1 }
#define T_FLOAT1 { reshadefx::type::t_float, 1, 1 }
#define T_FLOAT2 { reshadefx::type::t_float, 2, 1 }
#define T_FLOAT3 { reshadefx::type::t_float, 3, 1 }
#define T_FLOAT4 { reshadefx::type::t_float, 4, 1 }
#define T_FLOAT1_OUT { reshadefx::type::t_float, 1, 1, reshadefx::type::q_out, true }
#define T_FLOAT2_OUT { reshadefx::type::t_float, 2, 1, reshadefx::type::q_out, true }
#define T_FLOAT3_OUT { reshadefx::type::t_float, 3, 1, reshadefx::type::q_out, true }
#define T_FLOAT4_OUT { reshadefx::type::t_float, 4, 1, reshadefx::type::q_out, true }
#define T_FLOAT2X2 { reshadefx::type::t_float, 2, 2 }
#define T_FLOAT3X3 { reshadefx::type::t_float, 3, 3 }
#define T_FLOAT4X4 { reshadefx::type::t_float, 4, 4 }
#define T_SAMPLER { reshadefx::type::t_sampler }

#ifndef DEFINE_INTRINSIC
#define DEFINE_INTRINSIC(name, i, ret_type, ...)
#endif
#ifndef IMPLEMENT_INTRINSIC_HLSL
#define IMPLEMENT_INTRINSIC_HLSL(name, i, code)
#endif
#ifndef IMPLEMENT_INTRINSIC_SPIRV
#define IMPLEMENT_INTRINSIC_SPIRV(name, i, code)
#endif

// ret abs(x)
DEFINE_INTRINSIC(abs, 0, T_INT1, T_INT1)
DEFINE_INTRINSIC(abs, 0, T_INT2, T_INT2)
DEFINE_INTRINSIC(abs, 0, T_INT3, T_INT3)
DEFINE_INTRINSIC(abs, 0, T_INT4, T_INT4)
DEFINE_INTRINSIC(abs, 1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(abs, 1, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(abs, 1, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(abs, 1, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(abs, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450SAbs)
		.add(args[0].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(abs, 1, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450FAbs)
		.add(args[0].base)
		.result;
	})

// ret all(x)
DEFINE_INTRINSIC(all, 0, T_BOOL1, T_BOOL1)
DEFINE_INTRINSIC(all, 1, T_BOOL1, T_BOOL2)
DEFINE_INTRINSIC(all, 1, T_BOOL1, T_BOOL3)
DEFINE_INTRINSIC(all, 1, T_BOOL1, T_BOOL4)
IMPLEMENT_INTRINSIC_SPIRV(all, 0, {
	return args[0].base;
	})
IMPLEMENT_INTRINSIC_SPIRV(all, 1, {
	return add_instruction(spv::OpAll, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret any(x)
DEFINE_INTRINSIC(any, 0, T_BOOL1, T_BOOL1)
DEFINE_INTRINSIC(any, 1, T_BOOL1, T_BOOL2)
DEFINE_INTRINSIC(any, 1, T_BOOL1, T_BOOL3)
DEFINE_INTRINSIC(any, 1, T_BOOL1, T_BOOL4)
IMPLEMENT_INTRINSIC_SPIRV(any, 0, {
	return args[0].base;
	})
IMPLEMENT_INTRINSIC_SPIRV(any, 1, {
	return add_instruction(spv::OpAny, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret asin(x)
DEFINE_INTRINSIC(asin, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(asin, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(asin, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(asin, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(asin, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Asin)
		.add(args[0].base)
		.result;
	})

// ret acos(x)
DEFINE_INTRINSIC(acos, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(acos, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(acos, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(acos, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(acos, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Acos)
		.add(args[0].base)
		.result;
	})

// ret atan(x)
DEFINE_INTRINSIC(atan, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(atan, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(atan, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(atan, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(atan, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Atan)
		.add(args[0].base)
		.result;
	})

// ret atan2(x, y)
DEFINE_INTRINSIC(atan2, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(atan2, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(atan2, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(atan2, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(atan2, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Atan2)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret sin(x)
DEFINE_INTRINSIC(sin, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(sin, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(sin, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(sin, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(sin, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Sin)
		.add(args[0].base)
		.result;
	})

// ret sinh(x)
DEFINE_INTRINSIC(sinh, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(sinh, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(sinh, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(sinh, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(sinh, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Sinh)
		.add(args[0].base)
		.result;
	})

// ret cos(x)
DEFINE_INTRINSIC(cos, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(cos, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(cos, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(cos, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(cos, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Cos)
		.add(args[0].base)
		.result;
	})

// ret cosh(x)
DEFINE_INTRINSIC(cosh, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(cosh, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(cosh, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(cosh, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(cosh, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Cosh)
		.add(args[0].base)
		.result;
	})

// ret tan(x)
DEFINE_INTRINSIC(tan, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(tan, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(tan, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(tan, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(tan, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Tan)
		.add(args[0].base)
		.result;
	})

// ret tanh(x)
DEFINE_INTRINSIC(tanh, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(tanh, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(tanh, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(tanh, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(tanh, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Tanh)
		.add(args[0].base)
		.result;
	})

// sincos(x, out s, out c)
DEFINE_INTRINSIC(sincos, 0, T_VOID, T_FLOAT1, T_FLOAT1_OUT, T_FLOAT1_OUT)
DEFINE_INTRINSIC(sincos, 0, T_VOID, T_FLOAT2, T_FLOAT2_OUT, T_FLOAT2_OUT)
DEFINE_INTRINSIC(sincos, 0, T_VOID, T_FLOAT3, T_FLOAT3_OUT, T_FLOAT3_OUT)
DEFINE_INTRINSIC(sincos, 0, T_VOID, T_FLOAT4, T_FLOAT4_OUT, T_FLOAT4_OUT)
IMPLEMENT_INTRINSIC_SPIRV(sincos, 0, {
	const spv::Id sin_result = add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Sin)
		.add(args[0].base)
		.result;
	const spv::Id cos_result = add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Cos)
		.add(args[0].base)
		.result;

	add_instruction_without_result(spv::OpStore)
		.add(args[1].base)
		.add(sin_result);
	add_instruction_without_result(spv::OpStore)
		.add(args[2].base)
		.add(cos_result);

	return 0;
	})

// ret asint(x)
DEFINE_INTRINSIC(asint, 0, T_INT1, T_FLOAT1)
DEFINE_INTRINSIC(asint, 0, T_INT2, T_FLOAT2)
DEFINE_INTRINSIC(asint, 0, T_INT3, T_FLOAT3)
DEFINE_INTRINSIC(asint, 0, T_INT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(asint, 0, {
	return add_instruction(spv::OpBitcast, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret asuint(x)
DEFINE_INTRINSIC(asuint, 0, T_UINT1, T_FLOAT1)
DEFINE_INTRINSIC(asuint, 0, T_UINT2, T_FLOAT2)
DEFINE_INTRINSIC(asuint, 0, T_UINT3, T_FLOAT3)
DEFINE_INTRINSIC(asuint, 0, T_UINT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(asuint, 0, {
	return add_instruction(spv::OpBitcast, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret asfloat(x)
DEFINE_INTRINSIC(asfloat, 0, T_FLOAT1, T_INT1)
DEFINE_INTRINSIC(asfloat, 0, T_FLOAT2, T_INT2)
DEFINE_INTRINSIC(asfloat, 0, T_FLOAT3, T_INT3)
DEFINE_INTRINSIC(asfloat, 0, T_FLOAT4, T_INT4)
DEFINE_INTRINSIC(asfloat, 1, T_FLOAT1, T_UINT1)
DEFINE_INTRINSIC(asfloat, 1, T_FLOAT2, T_UINT2)
DEFINE_INTRINSIC(asfloat, 1, T_FLOAT3, T_UINT3)
DEFINE_INTRINSIC(asfloat, 1, T_FLOAT4, T_UINT4)
IMPLEMENT_INTRINSIC_SPIRV(asfloat, 0, {
	return add_instruction(spv::OpBitcast, convert_type(res_type))
		.add(args[0].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(asfloat, 1, {
	return add_instruction(spv::OpBitcast, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret ceil(x)
DEFINE_INTRINSIC(ceil, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(ceil, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(ceil, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(ceil, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(ceil, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Ceil)
		.add(args[0].base)
		.result;
	})

// ret floor(x)
DEFINE_INTRINSIC(floor, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(floor, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(floor, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(floor, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(floor, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Floor)
		.add(args[0].base)
		.result;
	})

// ret clamp(x, min, max)
DEFINE_INTRINSIC(clamp, 0, T_INT1, T_INT1, T_INT1, T_INT1)
DEFINE_INTRINSIC(clamp, 0, T_INT2, T_INT2, T_INT2, T_INT2)
DEFINE_INTRINSIC(clamp, 0, T_INT3, T_INT3, T_INT3, T_INT3)
DEFINE_INTRINSIC(clamp, 0, T_INT4, T_INT4, T_INT4, T_INT4)
DEFINE_INTRINSIC(clamp, 1, T_UINT1, T_UINT1, T_UINT1, T_UINT1)
DEFINE_INTRINSIC(clamp, 1, T_UINT2, T_UINT2, T_UINT2, T_UINT2)
DEFINE_INTRINSIC(clamp, 1, T_UINT3, T_UINT3, T_UINT3, T_UINT3)
DEFINE_INTRINSIC(clamp, 1, T_UINT4, T_UINT4, T_UINT4, T_UINT4)
DEFINE_INTRINSIC(clamp, 2, T_FLOAT1, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(clamp, 2, T_FLOAT2, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(clamp, 2, T_FLOAT3, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(clamp, 2, T_FLOAT4, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(clamp, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450SClamp)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(clamp, 1, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450UClamp)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(clamp, 2, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450FClamp)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})

// ret saturate(x)
DEFINE_INTRINSIC(saturate, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(saturate, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(saturate, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(saturate, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(saturate, 0, {
	const spv::Id constant_one = emit_constant(args[0].type, { 1.0f, 1.0f, 1.0f, 1.0f });
	const spv::Id constant_zero = emit_constant(args[0].type, { 0.0f, 0.0f, 0.0f, 0.0f });

	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450FClamp)
		.add(args[0].base)
		.add(constant_zero)
		.add(constant_one)
		.result;
	})

// ret mad(mvalue, avalue, bvalue)
DEFINE_INTRINSIC(mad, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(mad, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(mad, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(mad, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(mad, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Fma)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})

// ret rcp(x)
DEFINE_INTRINSIC(rcp, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(rcp, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(rcp, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(rcp, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(rcp, 0, {
	const spv::Id constant_one = emit_constant(args[0].type, { 1.0f, 1.0f, 1.0f, 1.0f });

	return add_instruction(spv::OpFDiv, convert_type(res_type))
		.add(constant_one)
		.add(args[0].base)
		.result;
	})

// ret pow(x, y)
DEFINE_INTRINSIC(pow, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(pow, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(pow, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(pow, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(pow, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Pow)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret exp(x)
DEFINE_INTRINSIC(exp, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(exp, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(exp, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(exp, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(exp, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Exp)
		.add(args[0].base)
		.result;
	})

// ret exp2(x)
DEFINE_INTRINSIC(exp2, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(exp2, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(exp2, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(exp2, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(exp2, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Exp2)
		.add(args[0].base)
		.result;
	})

// ret log(x)
DEFINE_INTRINSIC(log, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(log, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(log, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(log, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(log, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Log)
		.add(args[0].base)
		.result;
	})

// ret log2(x)
DEFINE_INTRINSIC(log2, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(log2, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(log2, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(log2, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(log2, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Log2)
		.add(args[0].base)
		.result;
	})

// ret log10(x)
DEFINE_INTRINSIC(log10, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(log10, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(log10, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(log10, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(log10, 0, {
	const spv::Id log2 = add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Log2)
		.add(args[0].base)
		.result;

	const spv::Id log10 = emit_constant(args[0].type,
		{ 2.302585093f, 2.302585093f, 2.302585093f, 2.302585093f });

	return add_instruction(spv::OpFDiv, convert_type(res_type))
		.add(log2)
		.add(log10)
		.result; })

// ret sign(x)
DEFINE_INTRINSIC(sign, 0, T_INT1, T_INT1)
DEFINE_INTRINSIC(sign, 0, T_INT2, T_INT2)
DEFINE_INTRINSIC(sign, 0, T_INT3, T_INT3)
DEFINE_INTRINSIC(sign, 0, T_INT4, T_INT4)
DEFINE_INTRINSIC(sign, 1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(sign, 1, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(sign, 1, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(sign, 1, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(sign, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450SSign)
		.add(args[0].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(sign, 1, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450FSign)
		.add(args[0].base)
		.result;
	})

// ret sqrt(x)
DEFINE_INTRINSIC(sqrt, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(sqrt, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(sqrt, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(sqrt, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(sqrt, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Sqrt)
		.add(args[0].base)
		.result;
	})

// ret rsqrt(x)
DEFINE_INTRINSIC(rsqrt, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(rsqrt, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(rsqrt, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(rsqrt, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(rsqrt, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450InverseSqrt)
		.add(args[0].base)
		.result;
	})

// ret lerp(x, y, s)
DEFINE_INTRINSIC(lerp, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(lerp, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(lerp, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(lerp, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(lerp, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450FMix)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})

// ret step(y, x)
DEFINE_INTRINSIC(step, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(step, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(step, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(step, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(step, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Step)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret smoothstep(min, max, x)
DEFINE_INTRINSIC(smoothstep, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(smoothstep, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(smoothstep, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(smoothstep, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(smoothstep, 0, {
	return add_instruction(spv::OpExtInst, convert_type(args[2].type))
		.add(glsl_ext)
		.add(spv::GLSLstd450SmoothStep)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})

// ret frac(x)
DEFINE_INTRINSIC(frac, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(frac, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(frac, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(frac, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(frac, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Fract)
		.add(args[0].base)
		.result;
	})

// ret ldexp(x, exp)
DEFINE_INTRINSIC(ldexp, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(ldexp, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(ldexp, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(ldexp, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(ldexp, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Ldexp)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret modf(x, out ip)
DEFINE_INTRINSIC(modf, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1_OUT)
DEFINE_INTRINSIC(modf, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2_OUT)
DEFINE_INTRINSIC(modf, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3_OUT)
DEFINE_INTRINSIC(modf, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4_OUT)
IMPLEMENT_INTRINSIC_SPIRV(modf, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Modf)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret frexp(x, out exp)
DEFINE_INTRINSIC(frexp, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1_OUT)
DEFINE_INTRINSIC(frexp, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2_OUT)
DEFINE_INTRINSIC(frexp, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3_OUT)
DEFINE_INTRINSIC(frexp, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4_OUT)
IMPLEMENT_INTRINSIC_SPIRV(frexp, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Frexp)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret trunc(x)
DEFINE_INTRINSIC(trunc, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(trunc, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(trunc, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(trunc, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(trunc, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Trunc)
		.add(args[0].base)
		.result;
	})

// ret round(x)
DEFINE_INTRINSIC(round, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(round, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(round, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(round, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(round, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Round)
		.add(args[0].base)
		.result;
	})

// ret min(x, y)
DEFINE_INTRINSIC(min, 0, T_INT1, T_INT1, T_INT1)
DEFINE_INTRINSIC(min, 0, T_INT2, T_INT2, T_INT2)
DEFINE_INTRINSIC(min, 0, T_INT3, T_INT3, T_INT3)
DEFINE_INTRINSIC(min, 0, T_INT4, T_INT4, T_INT4)
DEFINE_INTRINSIC(min, 1, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(min, 1, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(min, 1, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(min, 1, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(min, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450SMin)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(min, 1, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450FMin)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret max(x, y)
DEFINE_INTRINSIC(max, 0, T_INT1, T_INT1, T_INT1)
DEFINE_INTRINSIC(max, 0, T_INT2, T_INT2, T_INT2)
DEFINE_INTRINSIC(max, 0, T_INT3, T_INT3, T_INT3)
DEFINE_INTRINSIC(max, 0, T_INT4, T_INT4, T_INT4)
DEFINE_INTRINSIC(max, 1, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(max, 1, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(max, 1, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(max, 1, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(max, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450SMax)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(max, 1, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450FMax)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret degree(x)
DEFINE_INTRINSIC(degrees, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(degrees, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(degrees, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(degrees, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(degrees, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Degrees)
		.add(args[0].base)
		.result;
	})

// ret radians(x)
DEFINE_INTRINSIC(radians, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(radians, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(radians, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(radians, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(radians, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Radians)
		.add(args[0].base)
		.result;
	})

// ret ddx(x)
DEFINE_INTRINSIC(ddx, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(ddx, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(ddx, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(ddx, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(ddx, 0, {
	return add_instruction(spv::OpDPdx, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret ddy(x)
DEFINE_INTRINSIC(ddy, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(ddy, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(ddy, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(ddy, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(ddy, 0, {
	return add_instruction(spv::OpDPdy, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret fwidth(x)
DEFINE_INTRINSIC(fwidth, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(fwidth, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(fwidth, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(fwidth, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(fwidth, 0, {
	return add_instruction(spv::OpFwidth, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret dot(x, y)
DEFINE_INTRINSIC(dot, 0, T_FLOAT1, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(dot, 0, T_FLOAT1, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(dot, 0, T_FLOAT1, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(dot, 0, {
	return add_instruction(spv::OpDot, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret cross(x, y)
DEFINE_INTRINSIC(cross, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3)
IMPLEMENT_INTRINSIC_SPIRV(cross, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Cross)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret length(x)
DEFINE_INTRINSIC(length, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(length, 0, T_FLOAT1, T_FLOAT2)
DEFINE_INTRINSIC(length, 0, T_FLOAT1, T_FLOAT3)
DEFINE_INTRINSIC(length, 0, T_FLOAT1, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(length, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Length)
		.add(args[0].base)
		.result;
	})

// ret distance(x, y)
DEFINE_INTRINSIC(distance, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(distance, 0, T_FLOAT1, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(distance, 0, T_FLOAT1, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(distance, 0, T_FLOAT1, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(distance, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Distance)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret normalize(x)
DEFINE_INTRINSIC(normalize, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(normalize, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(normalize, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(normalize, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Normalize)
		.add(args[0].base)
		.result;
	})

// ret transpose(x)
DEFINE_INTRINSIC(transpose, 0, T_FLOAT2X2, T_FLOAT2X2)
DEFINE_INTRINSIC(transpose, 0, T_FLOAT3X3, T_FLOAT3X3)
DEFINE_INTRINSIC(transpose, 0, T_FLOAT4X4, T_FLOAT4X4)
IMPLEMENT_INTRINSIC_SPIRV(transpose, 0, {
	return add_instruction(spv::OpTranspose, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret determinant(m)
DEFINE_INTRINSIC(determinant, 0, T_FLOAT1, T_FLOAT2X2)
DEFINE_INTRINSIC(determinant, 0, T_FLOAT1, T_FLOAT3X3)
DEFINE_INTRINSIC(determinant, 0, T_FLOAT1, T_FLOAT4X4)
IMPLEMENT_INTRINSIC_SPIRV(determinant, 0, {
	return add_instruction(spv::OpExtInst, convert_type(T_FLOAT1))
		.add(glsl_ext)
		.add(spv::GLSLstd450Determinant)
		.add(args[0].base)
		.result;
	})

// ret reflect(i, n)
DEFINE_INTRINSIC(reflect, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(reflect, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(reflect, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(reflect, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Reflect)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret refract(i, n, eta)
DEFINE_INTRINSIC(refract, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2, T_FLOAT1)
DEFINE_INTRINSIC(refract, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3, T_FLOAT1)
DEFINE_INTRINSIC(refract, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4, T_FLOAT1)
IMPLEMENT_INTRINSIC_SPIRV(refract, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450Refract)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})

// ret faceforward(n, i, ng)
DEFINE_INTRINSIC(faceforward, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(faceforward, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(faceforward, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(faceforward, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(faceforward, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(glsl_ext)
		.add(spv::GLSLstd450FaceForward)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})

// ret mul(x, y)
DEFINE_INTRINSIC(mul, 0, T_FLOAT2, T_FLOAT1, T_FLOAT2)
DEFINE_INTRINSIC(mul, 0, T_FLOAT3, T_FLOAT1, T_FLOAT3)
DEFINE_INTRINSIC(mul, 0, T_FLOAT4, T_FLOAT1, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(mul, 0, {
	return add_instruction(spv::OpVectorTimesScalar, convert_type(res_type))
		.add(args[1].base)
		.add(args[0].base)
		.result;
	})
DEFINE_INTRINSIC(mul, 1, T_FLOAT2, T_FLOAT2, T_FLOAT1)
DEFINE_INTRINSIC(mul, 1, T_FLOAT3, T_FLOAT3, T_FLOAT1)
DEFINE_INTRINSIC(mul, 1, T_FLOAT4, T_FLOAT4, T_FLOAT1)
IMPLEMENT_INTRINSIC_SPIRV(mul, 1, {
	return add_instruction(spv::OpVectorTimesScalar, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

DEFINE_INTRINSIC(mul, 2, T_FLOAT2X2, T_FLOAT1, T_FLOAT2X2)
DEFINE_INTRINSIC(mul, 2, T_FLOAT3X3, T_FLOAT1, T_FLOAT3X3)
DEFINE_INTRINSIC(mul, 2, T_FLOAT4X4, T_FLOAT1, T_FLOAT4X4)
IMPLEMENT_INTRINSIC_SPIRV(mul, 2, {
	return add_instruction(spv::OpMatrixTimesScalar, convert_type(res_type))
		.add(args[1].base)
		.add(args[0].base)
		.result;
	})
DEFINE_INTRINSIC(mul, 3, T_FLOAT2X2, T_FLOAT2X2, T_FLOAT1)
DEFINE_INTRINSIC(mul, 3, T_FLOAT3X3, T_FLOAT3X3, T_FLOAT1)
DEFINE_INTRINSIC(mul, 3, T_FLOAT4X4, T_FLOAT4X4, T_FLOAT1)
IMPLEMENT_INTRINSIC_SPIRV(mul, 3, {
	return add_instruction(spv::OpMatrixTimesScalar, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

DEFINE_INTRINSIC(mul, 4, T_FLOAT2, T_FLOAT2, T_FLOAT2X2)
DEFINE_INTRINSIC(mul, 4, T_FLOAT3, T_FLOAT3, T_FLOAT3X3)
DEFINE_INTRINSIC(mul, 4, T_FLOAT4, T_FLOAT4, T_FLOAT4X4)
IMPLEMENT_INTRINSIC_SPIRV(mul, 4, {
	return add_instruction(spv::OpVectorTimesMatrix, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})
DEFINE_INTRINSIC(mul, 5, T_FLOAT2, T_FLOAT2X2, T_FLOAT2)
DEFINE_INTRINSIC(mul, 5, T_FLOAT3, T_FLOAT3X3, T_FLOAT3)
DEFINE_INTRINSIC(mul, 5, T_FLOAT4, T_FLOAT4X4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(mul, 5, {
	return add_instruction(spv::OpMatrixTimesVector, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

DEFINE_INTRINSIC(mul, 6, T_FLOAT2X2, T_FLOAT2X2, T_FLOAT2X2)
DEFINE_INTRINSIC(mul, 6, T_FLOAT3X3, T_FLOAT3X3, T_FLOAT3X3)
DEFINE_INTRINSIC(mul, 6, T_FLOAT4X4, T_FLOAT4X4, T_FLOAT4X4)
IMPLEMENT_INTRINSIC_SPIRV(mul, 6, {
	return add_instruction(spv::OpMatrixTimesMatrix, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret isinf(x)
DEFINE_INTRINSIC(isinf, 0, T_BOOL1, T_FLOAT1)
DEFINE_INTRINSIC(isinf, 0, T_BOOL2, T_FLOAT2)
DEFINE_INTRINSIC(isinf, 0, T_BOOL3, T_FLOAT3)
DEFINE_INTRINSIC(isinf, 0, T_BOOL4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(isinf, 0, {
	return add_instruction(spv::OpIsInf, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret isnan(x)
DEFINE_INTRINSIC(isnan, 0, T_BOOL1, T_FLOAT1)
DEFINE_INTRINSIC(isnan, 0, T_BOOL2, T_FLOAT2)
DEFINE_INTRINSIC(isnan, 0, T_BOOL3, T_FLOAT3)
DEFINE_INTRINSIC(isnan, 0, T_BOOL4, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(isnan, 0, {
	return add_instruction(spv::OpIsNan, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret tex2D(s, coords)
DEFINE_INTRINSIC(tex2D, 0, T_FLOAT4, T_SAMPLER, T_FLOAT2)
IMPLEMENT_INTRINSIC_SPIRV(tex2D, 0, {
	return add_instruction(spv::OpImageSampleImplicitLod, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.add(spv::ImageOperandsMaskNone)
		.result;
	})
// ret tex2Doffset(s, coords, offset)
DEFINE_INTRINSIC(tex2Doffset, 0, T_FLOAT4, T_SAMPLER, T_FLOAT2, T_INT2)
IMPLEMENT_INTRINSIC_SPIRV(tex2Doffset, 0, {
	return add_instruction(spv::OpImageSampleImplicitLod, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.add(spv::ImageOperandsOffsetMask)
		.add(args[2].base)
		.result;
	})

// ret tex2Dlod(s, coords)
DEFINE_INTRINSIC(tex2Dlod, 0, T_FLOAT4, T_SAMPLER, T_FLOAT4)
IMPLEMENT_INTRINSIC_SPIRV(tex2Dlod, 0, {
	const spv::Id xy = add_instruction(spv::OpVectorShuffle, convert_type(T_FLOAT2))
		.add(args[1].base)
		.add(args[1].base)
		.add(0) // .x
		.add(1) // .y
		.result;
	const spv::Id lod = add_instruction(spv::OpCompositeExtract, convert_type(T_FLOAT1))
		.add(args[1].base)
		.add(3) // .w
		.result;

	return add_instruction(spv::OpImageSampleExplicitLod, convert_type(res_type))
		.add(args[0].base)
		.add(xy)
		.add(spv::ImageOperandsLodMask)
		.add(lod)
		.result;
	})
// ret tex2Dlodoffset(s, coords, offset)
DEFINE_INTRINSIC(tex2Dlodoffset, 0, T_FLOAT4, T_SAMPLER, T_FLOAT4, T_INT2)
IMPLEMENT_INTRINSIC_SPIRV(tex2Dlodoffset, 0, {
	const spv::Id xy = add_instruction(spv::OpVectorShuffle, convert_type(T_FLOAT2))
		.add(args[1].base)
		.add(args[1].base)
		.add(0) // .x
		.add(1) // .y
		.result;
	const spv::Id lod = add_instruction(spv::OpCompositeExtract, convert_type(T_FLOAT1))
		.add(args[1].base)
		.add(3) // .w
		.result;

	return add_instruction(spv::OpImageSampleExplicitLod, convert_type(res_type))
		.add(args[0].base)
		.add(xy)
		.add(spv::ImageOperandsLodMask)
		.add(lod)
		.add(spv::ImageOperandsOffsetMask)
		.add(args[2].base)
		.result;
	})

// ret tex2Dsize(s)
// ret tex2Dsize(s, lod)
DEFINE_INTRINSIC(tex2Dsize, 0, T_INT2, T_SAMPLER)
DEFINE_INTRINSIC(tex2Dsize, 1, T_INT2, T_SAMPLER, T_INT1)
IMPLEMENT_INTRINSIC_SPIRV(tex2Dsize, 0, {
	add_capability(spv::CapabilityImageQuery);

	const spv::Id image = add_instruction(spv::OpImage, convert_type({ reshadefx::type::t_texture }))
		.add(args[0].base)
		.result;

	return add_instruction(spv::OpImageQuerySize, convert_type(res_type))
		.add(image)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2Dsize, 1, {
	add_capability(spv::CapabilityImageQuery);

	const spv::Id image = add_instruction(spv::OpImage, convert_type({ reshadefx::type::t_texture }))
		.add(args[0].base)
		.result;

	return add_instruction(spv::OpImageQuerySizeLod, convert_type(res_type))
		.add(image)
		.add(args[1].base)
		.result;
	})

// ret tex2Dfetch(s, coords)
DEFINE_INTRINSIC(tex2Dfetch, 0, T_FLOAT4, T_SAMPLER, T_INT4)
IMPLEMENT_INTRINSIC_SPIRV(tex2Dfetch, 0, {
	const spv::Id xy = add_instruction(spv::OpVectorShuffle, convert_type(T_INT2))
		.add(args[1].base)
		.add(args[1].base)
		.add(0) // .x
		.add(1) // .y
		.result;
	const spv::Id lod = add_instruction(spv::OpCompositeExtract, convert_type(T_INT1))
		.add(args[1].base)
		.add(3) // .w
		.result;

	const spv::Id image = add_instruction(spv::OpImage, convert_type({ reshadefx::type::t_texture }))
		.add(args[0].base)
		.result;

	return add_instruction(spv::OpImageFetch, convert_type(res_type))
		.add(image)
		.add(xy)
		.add(spv::ImageOperandsLodMask)
		.add(lod)
		.result;
	})

// ret tex2Dgather(s, coords, component)
DEFINE_INTRINSIC(tex2Dgather, 0, T_FLOAT4, T_SAMPLER, T_FLOAT2, T_INT1)
IMPLEMENT_INTRINSIC_SPIRV(tex2Dgather, 0, {
	return add_instruction(spv::OpImageGather, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.add(spv::ImageOperandsMaskNone)
		.result;
	})
// ret tex2Dgatheroffset(s, coords, offset, component)
DEFINE_INTRINSIC(tex2Dgatheroffset, 0, T_FLOAT4, T_SAMPLER, T_FLOAT2, T_INT2, T_INT1)
IMPLEMENT_INTRINSIC_SPIRV(tex2Dgatheroffset, 0, {
	add_capability(spv::CapabilityImageGatherExtended);

	return add_instruction(spv::OpImageGather, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.add(args[3].base)
		.add(spv::ImageOperandsOffsetMask)
		.add(args[2].base)
		.result;
	})


#undef DEFINE_INTRINSIC
#undef IMPLEMENT_INTRINSIC_HLSL
#undef IMPLEMENT_INTRINSIC_SPIRV
