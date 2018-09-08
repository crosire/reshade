/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#define T_VOID  { spirv_type::t_void }
#define T_BOOL1 { spirv_type::t_bool, 1, 1 }
#define T_BOOL2 { spirv_type::t_bool, 2, 1 }
#define T_BOOL3 { spirv_type::t_bool, 3, 1 }
#define T_BOOL4 { spirv_type::t_bool, 4, 1 }
#define T_INT1 { spirv_type::t_int, 1, 1 }
#define T_INT2 { spirv_type::t_int, 2, 1 }
#define T_INT3 { spirv_type::t_int, 3, 1 }
#define T_INT4 { spirv_type::t_int, 4, 1 }
#define T_UINT1 { spirv_type::t_uint, 1, 1 }
#define T_UINT2 { spirv_type::t_uint, 2, 1 }
#define T_UINT3 { spirv_type::t_uint, 3, 1 }
#define T_UINT4 { spirv_type::t_uint, 4, 1 }
#define T_FLOAT1 { spirv_type::t_float, 1, 1 }
#define T_FLOAT2 { spirv_type::t_float, 2, 1 }
#define T_FLOAT3 { spirv_type::t_float, 3, 1 }
#define T_FLOAT4 { spirv_type::t_float, 4, 1 }
#define T_FLOAT1_OUT { spirv_type::t_float, 1, 1, spirv_type::q_out, true }
#define T_FLOAT2_OUT { spirv_type::t_float, 2, 1, spirv_type::q_out, true }
#define T_FLOAT3_OUT { spirv_type::t_float, 3, 1, spirv_type::q_out, true }
#define T_FLOAT4_OUT { spirv_type::t_float, 4, 1, spirv_type::q_out, true }
#define T_FLOAT2X2 { spirv_type::t_float, 2, 2 }
#define T_FLOAT3X3 { spirv_type::t_float, 3, 3 }
#define T_FLOAT4X4 { spirv_type::t_float, 4, 4 }
#define T_SAMPLER { spirv_type::t_sampler }

#ifndef DEFINE_INTRINSIC
#define DEFINE_INTRINSIC(name, i, ret_type, ...)
#endif
#ifndef IMPLEMENT_INTRINSIC
#define IMPLEMENT_INTRINSIC(name, i, code)
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
IMPLEMENT_INTRINSIC(abs, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450SAbs)
		.add(args[0].base)
		.result;
	})
IMPLEMENT_INTRINSIC(abs, 1, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450FAbs)
		.add(args[0].base)
		.result;
	})

// ret all(x)
DEFINE_INTRINSIC(all, 0, T_BOOL1, T_BOOL1)
DEFINE_INTRINSIC(all, 1, T_BOOL1, T_BOOL2)
DEFINE_INTRINSIC(all, 1, T_BOOL1, T_BOOL3)
DEFINE_INTRINSIC(all, 1, T_BOOL1, T_BOOL4)
IMPLEMENT_INTRINSIC(all, 0, {
	return args[0].base;
	})
IMPLEMENT_INTRINSIC(all, 1, {
	return m.add_instruction(block, {}, spv::OpAll, m.convert_type(T_BOOL1))
		.add(args[0].base)
		.result;
	})

// ret any(x)
DEFINE_INTRINSIC(any, 0, T_BOOL1, T_BOOL1)
DEFINE_INTRINSIC(any, 1, T_BOOL1, T_BOOL2)
DEFINE_INTRINSIC(any, 1, T_BOOL1, T_BOOL3)
DEFINE_INTRINSIC(any, 1, T_BOOL1, T_BOOL4)
IMPLEMENT_INTRINSIC(any, 0, {
	return args[0].base;
	})
IMPLEMENT_INTRINSIC(any, 1, {
	return m.add_instruction(block, {}, spv::OpAny, m.convert_type(T_BOOL1))
		.add(args[0].base)
		.result;
	})

// ret asin(x)
DEFINE_INTRINSIC(asin, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(asin, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(asin, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(asin, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(asin, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Asin)
		.add(args[0].base)
		.result;
	})

// ret acos(x)
DEFINE_INTRINSIC(acos, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(acos, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(acos, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(acos, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(acos, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Acos)
		.add(args[0].base)
		.result;
	})

// ret atan(x)
DEFINE_INTRINSIC(atan, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(atan, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(atan, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(atan, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(atan, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Atan)
		.add(args[0].base)
		.result;
	})

// ret atan2(x, y)
DEFINE_INTRINSIC(atan2, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(atan2, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(atan2, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(atan2, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(atan2, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(sin, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Sin)
		.add(args[0].base)
		.result;
	})

// ret sinh(x)
DEFINE_INTRINSIC(sinh, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(sinh, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(sinh, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(sinh, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(sinh, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Sinh)
		.add(args[0].base)
		.result;
	})

// ret cos(x)
DEFINE_INTRINSIC(cos, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(cos, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(cos, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(cos, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(cos, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Cos)
		.add(args[0].base)
		.result;
	})

// ret cosh(x)
DEFINE_INTRINSIC(cosh, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(cosh, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(cosh, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(cosh, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(cosh, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Cosh)
		.add(args[0].base)
		.result;
	})

// ret tan(x)
DEFINE_INTRINSIC(tan, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(tan, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(tan, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(tan, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(tan, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Tan)
		.add(args[0].base)
		.result;
	})

// ret tanh(x)
DEFINE_INTRINSIC(tanh, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(tanh, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(tanh, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(tanh, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(tanh, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Tanh)
		.add(args[0].base)
		.result;
	})

// sincos(x, out s, out c)
DEFINE_INTRINSIC(sincos, 0, T_VOID, T_FLOAT1, T_FLOAT1_OUT, T_FLOAT1_OUT)
DEFINE_INTRINSIC(sincos, 0, T_VOID, T_FLOAT2, T_FLOAT2_OUT, T_FLOAT2_OUT)
DEFINE_INTRINSIC(sincos, 0, T_VOID, T_FLOAT3, T_FLOAT3_OUT, T_FLOAT3_OUT)
DEFINE_INTRINSIC(sincos, 0, T_VOID, T_FLOAT4, T_FLOAT4_OUT, T_FLOAT4_OUT)
IMPLEMENT_INTRINSIC(sincos, 0, {
	const spv::Id sin_result = m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Sin)
		.add(args[0].base)
		.result;
	const spv::Id cos_result = m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Cos)
		.add(args[0].base)
		.result;

	m.add_instruction_without_result(block, {}, spv::OpStore)
		.add(args[1].base)
		.add(sin_result);
	m.add_instruction_without_result(block, {}, spv::OpStore)
		.add(args[2].base)
		.add(cos_result);

	return 0;
	})

// ret asint(x)
DEFINE_INTRINSIC(asint, 0, T_INT1, T_FLOAT1)
DEFINE_INTRINSIC(asint, 0, T_INT2, T_FLOAT2)
DEFINE_INTRINSIC(asint, 0, T_INT3, T_FLOAT3)
DEFINE_INTRINSIC(asint, 0, T_INT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(asint, 0, {
	return m.add_instruction(block, {}, spv::OpBitcast, m.convert_type({ spirv_type::t_int, args[0].type.rows, 1 }))
		.add(args[0].base)
		.result;
	})

// ret asuint(x)
DEFINE_INTRINSIC(asuint, 0, T_UINT1, T_FLOAT1)
DEFINE_INTRINSIC(asuint, 0, T_UINT2, T_FLOAT2)
DEFINE_INTRINSIC(asuint, 0, T_UINT3, T_FLOAT3)
DEFINE_INTRINSIC(asuint, 0, T_UINT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(asuint, 0, {
	return m.add_instruction(block, {}, spv::OpBitcast, m.convert_type({ spirv_type::t_uint, args[0].type.rows, 1 }))
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
IMPLEMENT_INTRINSIC(asfloat, 0, {
	return m.add_instruction(block, {}, spv::OpBitcast, m.convert_type({ spirv_type::t_float, args[0].type.rows, 1 }))
		.add(args[0].base)
		.result;
	})
IMPLEMENT_INTRINSIC(asfloat, 1, {
	return m.add_instruction(block, {}, spv::OpBitcast, m.convert_type({ spirv_type::t_float, args[0].type.rows, 1 }))
		.add(args[0].base)
		.result;
	})

// ret ceil(x)
DEFINE_INTRINSIC(ceil, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(ceil, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(ceil, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(ceil, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(ceil, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Ceil)
		.add(args[0].base)
		.result;
	})

// ret floor(x)
DEFINE_INTRINSIC(floor, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(floor, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(floor, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(floor, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(floor, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(clamp, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450SClamp)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})
IMPLEMENT_INTRINSIC(clamp, 1, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450UClamp)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})
IMPLEMENT_INTRINSIC(clamp, 2, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(saturate, 0, {
	const spv::Id constant_one = m.convert_constant(args[0].type, { 1.0f, 1.0f, 1.0f, 1.0f });
	const spv::Id constant_zero = m.convert_constant(args[0].type, { 0.0f, 0.0f, 0.0f, 0.0f });

	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(mad, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(rcp, 0, {
	const spv::Id constant_one = m.convert_constant(args[0].type, { 1.0f, 1.0f, 1.0f, 1.0f });

	return m.add_instruction(block, {}, spv::OpFDiv, m.convert_type(args[0].type))
		.add(constant_one)
		.add(args[0].base)
		.result;
	})

// ret pow(x, y)
DEFINE_INTRINSIC(pow, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(pow, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(pow, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(pow, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(pow, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(exp, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Exp)
		.add(args[0].base)
		.result;
	})

// ret exp2(x)
DEFINE_INTRINSIC(exp2, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(exp2, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(exp2, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(exp2, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(exp2, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Exp2)
		.add(args[0].base)
		.result;
	})

// ret log(x)
DEFINE_INTRINSIC(log, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(log, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(log, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(log, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(log, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Log)
		.add(args[0].base)
		.result;
	})

// ret log2(x)
DEFINE_INTRINSIC(log2, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(log2, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(log2, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(log2, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(log2, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Log2)
		.add(args[0].base)
		.result;
	})

// ret log10(x)
DEFINE_INTRINSIC(log10, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(log10, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(log10, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(log10, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(log10, 0, {
	const spv::Id log2 = m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Log2)
		.add(args[0].base)
		.result;

	const spv::Id log10 = m.convert_constant(args[0].type,
		{ 2.302585093f, 2.302585093f, 2.302585093f, 2.302585093f });

	return m.add_instruction(block, {}, spv::OpFDiv, m.convert_type(args[0].type))
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
IMPLEMENT_INTRINSIC(sign, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450SSign)
		.add(args[0].base)
		.result;
	})
IMPLEMENT_INTRINSIC(sign, 1, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450FSign)
		.add(args[0].base)
		.result;
	})

// ret sqrt(x)
DEFINE_INTRINSIC(sqrt, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(sqrt, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(sqrt, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(sqrt, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(sqrt, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Sqrt)
		.add(args[0].base)
		.result;
	})

// ret rsqrt(x)
DEFINE_INTRINSIC(rsqrt, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(rsqrt, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(rsqrt, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(rsqrt, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(rsqrt, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450InverseSqrt)
		.add(args[0].base)
		.result;
	})

// ret lerp(x, y, s)
DEFINE_INTRINSIC(lerp, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(lerp, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(lerp, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(lerp, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(lerp, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(step, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(smoothstep, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[2].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(frac, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Fract)
		.add(args[0].base)
		.result;
	})

// ret ldexp(x, exp)
DEFINE_INTRINSIC(ldexp, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(ldexp, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(ldexp, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(ldexp, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(ldexp, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(modf, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(frexp, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(trunc, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Trunc)
		.add(args[0].base)
		.result;
	})

// ret round(x)
DEFINE_INTRINSIC(round, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(round, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(round, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(round, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(round, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(min, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450SMin)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})
IMPLEMENT_INTRINSIC(min, 1, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(max, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450SMax)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})
IMPLEMENT_INTRINSIC(max, 1, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(degrees, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Degrees)
		.add(args[0].base)
		.result;
	})

// ret radians(x)
DEFINE_INTRINSIC(radians, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(radians, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(radians, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(radians, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(radians, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Radians)
		.add(args[0].base)
		.result;
	})

// ret ddx(x)
DEFINE_INTRINSIC(ddx, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(ddx, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(ddx, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(ddx, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(ddx, 0, {
	return m.add_instruction(block, {}, spv::OpDPdx, m.convert_type(args[0].type))
		.add(args[0].base)
		.result;
	})

// ret ddy(x)
DEFINE_INTRINSIC(ddy, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(ddy, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(ddy, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(ddy, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(ddy, 0, {
	return m.add_instruction(block, {}, spv::OpDPdy, m.convert_type(args[0].type))
		.add(args[0].base)
		.result;
	})

// ret fwidth(x)
DEFINE_INTRINSIC(fwidth, 0, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(fwidth, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(fwidth, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(fwidth, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(fwidth, 0, {
	return m.add_instruction(block, {}, spv::OpFwidth, m.convert_type(args[0].type))
		.add(args[0].base)
		.result;
	})

// ret dot(x, y)
DEFINE_INTRINSIC(dot, 0, T_FLOAT1, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(dot, 0, T_FLOAT1, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(dot, 0, T_FLOAT1, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(dot, 0, {
	return m.add_instruction(block, {}, spv::OpDot, m.convert_type(T_FLOAT1))
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret cross(x, y)
DEFINE_INTRINSIC(cross, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3)
IMPLEMENT_INTRINSIC(cross, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(T_FLOAT3))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(length, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(T_FLOAT1))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Length)
		.add(args[0].base)
		.result;
	})

// ret distance(x, y)
DEFINE_INTRINSIC(distance, 0, T_FLOAT1, T_FLOAT1, T_FLOAT1)
DEFINE_INTRINSIC(distance, 0, T_FLOAT1, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(distance, 0, T_FLOAT1, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(distance, 0, T_FLOAT1, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(distance, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(T_FLOAT1))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Distance)
		.add(args[0].base)
		.result;
	})

// ret normalize(x)
DEFINE_INTRINSIC(normalize, 0, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(normalize, 0, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(normalize, 0, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(normalize, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Normalize)
		.add(args[0].base)
		.result;
	})

// ret transpose(x)
DEFINE_INTRINSIC(transpose, 0, T_FLOAT2X2, T_FLOAT2X2)
DEFINE_INTRINSIC(transpose, 0, T_FLOAT3X3, T_FLOAT3X3)
DEFINE_INTRINSIC(transpose, 0, T_FLOAT4X4, T_FLOAT4X4)
IMPLEMENT_INTRINSIC(transpose, 0, {
	return m.add_instruction(block, {}, spv::OpTranspose, m.convert_type(args[0].type))
		.add(args[0].base)
		.result;
	})

// ret determinant(m)
DEFINE_INTRINSIC(determinant, 0, T_FLOAT1, T_FLOAT2X2)
DEFINE_INTRINSIC(determinant, 0, T_FLOAT1, T_FLOAT3X3)
DEFINE_INTRINSIC(determinant, 0, T_FLOAT1, T_FLOAT4X4)
IMPLEMENT_INTRINSIC(determinant, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(T_FLOAT1))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Determinant)
		.add(args[0].base)
		.result;
	})

// ret reflect(i, n)
DEFINE_INTRINSIC(reflect, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2)
DEFINE_INTRINSIC(reflect, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3)
DEFINE_INTRINSIC(reflect, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4)
IMPLEMENT_INTRINSIC(reflect, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
		.add(spv::GLSLstd450Reflect)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret refract(i, n, eta)
DEFINE_INTRINSIC(refract, 0, T_FLOAT2, T_FLOAT2, T_FLOAT2, T_FLOAT1)
DEFINE_INTRINSIC(refract, 0, T_FLOAT3, T_FLOAT3, T_FLOAT3, T_FLOAT1)
DEFINE_INTRINSIC(refract, 0, T_FLOAT4, T_FLOAT4, T_FLOAT4, T_FLOAT1)
IMPLEMENT_INTRINSIC(refract, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(faceforward, 0, {
	return m.add_instruction(block, {}, spv::OpExtInst, m.convert_type(args[0].type))
		.add(m.glsl_ext)
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
IMPLEMENT_INTRINSIC(mul, 0, {
	return m.add_instruction(block, {}, spv::OpVectorTimesScalar, m.convert_type(args[1].type))
		.add(args[1].base)
		.add(args[0].base)
		.result;
	})
DEFINE_INTRINSIC(mul, 1, T_FLOAT2, T_FLOAT2, T_FLOAT1)
DEFINE_INTRINSIC(mul, 1, T_FLOAT3, T_FLOAT3, T_FLOAT1)
DEFINE_INTRINSIC(mul, 1, T_FLOAT4, T_FLOAT4, T_FLOAT1)
IMPLEMENT_INTRINSIC(mul, 1, {
	return m.add_instruction(block, {}, spv::OpVectorTimesScalar, m.convert_type(args[0].type))
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

DEFINE_INTRINSIC(mul, 2, T_FLOAT2X2, T_FLOAT1, T_FLOAT2X2)
DEFINE_INTRINSIC(mul, 2, T_FLOAT3X3, T_FLOAT1, T_FLOAT3X3)
DEFINE_INTRINSIC(mul, 2, T_FLOAT4X4, T_FLOAT1, T_FLOAT4X4)
IMPLEMENT_INTRINSIC(mul, 2, {
	return m.add_instruction(block, {}, spv::OpMatrixTimesScalar, m.convert_type(args[1].type))
		.add(args[1].base)
		.add(args[0].base)
		.result;
	})
DEFINE_INTRINSIC(mul, 3, T_FLOAT2X2, T_FLOAT2X2, T_FLOAT1)
DEFINE_INTRINSIC(mul, 3, T_FLOAT3X3, T_FLOAT3X3, T_FLOAT1)
DEFINE_INTRINSIC(mul, 3, T_FLOAT4X4, T_FLOAT4X4, T_FLOAT1)
IMPLEMENT_INTRINSIC(mul, 3, {
	return m.add_instruction(block, {}, spv::OpMatrixTimesScalar, m.convert_type(args[0].type))
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

DEFINE_INTRINSIC(mul, 4, T_FLOAT2, T_FLOAT2, T_FLOAT2X2)
DEFINE_INTRINSIC(mul, 4, T_FLOAT3, T_FLOAT3, T_FLOAT3X3)
DEFINE_INTRINSIC(mul, 4, T_FLOAT4, T_FLOAT4, T_FLOAT4X4)
IMPLEMENT_INTRINSIC(mul, 4, {
	return m.add_instruction(block, {}, spv::OpVectorTimesMatrix, m.convert_type(args[0].type))
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})
DEFINE_INTRINSIC(mul, 5, T_FLOAT2, T_FLOAT2X2, T_FLOAT2)
DEFINE_INTRINSIC(mul, 5, T_FLOAT3, T_FLOAT3X3, T_FLOAT3)
DEFINE_INTRINSIC(mul, 5, T_FLOAT4, T_FLOAT4X4, T_FLOAT4)
IMPLEMENT_INTRINSIC(mul, 5, {
	return m.add_instruction(block, {}, spv::OpMatrixTimesVector, m.convert_type(args[1].type))
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

DEFINE_INTRINSIC(mul, 6, T_FLOAT2X2, T_FLOAT2X2, T_FLOAT2X2)
DEFINE_INTRINSIC(mul, 6, T_FLOAT3X3, T_FLOAT3X3, T_FLOAT3X3)
DEFINE_INTRINSIC(mul, 6, T_FLOAT4X4, T_FLOAT4X4, T_FLOAT4X4)
IMPLEMENT_INTRINSIC(mul, 6, {
	return m.add_instruction(block, {}, spv::OpMatrixTimesMatrix, m.convert_type(args[0].type))
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret isinf(x)
DEFINE_INTRINSIC(isinf, 0, T_BOOL1, T_FLOAT1)
DEFINE_INTRINSIC(isinf, 0, T_BOOL2, T_FLOAT2)
DEFINE_INTRINSIC(isinf, 0, T_BOOL3, T_FLOAT3)
DEFINE_INTRINSIC(isinf, 0, T_BOOL4, T_FLOAT4)
IMPLEMENT_INTRINSIC(isinf, 0, {
	return m.add_instruction(block, {}, spv::OpIsInf, m.convert_type({ spirv_type::t_bool, args[0].type.rows, 1 }))
		.add(args[0].base)
		.result;
	})

// ret isnan(x)
DEFINE_INTRINSIC(isnan, 0, T_BOOL1, T_FLOAT1)
DEFINE_INTRINSIC(isnan, 0, T_BOOL2, T_FLOAT2)
DEFINE_INTRINSIC(isnan, 0, T_BOOL3, T_FLOAT3)
DEFINE_INTRINSIC(isnan, 0, T_BOOL4, T_FLOAT4)
IMPLEMENT_INTRINSIC(isnan, 0, {
	return m.add_instruction(block, {}, spv::OpIsNan, m.convert_type({ spirv_type::t_bool, args[0].type.rows, 1 }))
		.add(args[0].base)
		.result;
	})

// ret tex2D(s, coords)
DEFINE_INTRINSIC(tex2D, 0, T_FLOAT4, T_SAMPLER, T_FLOAT2)
IMPLEMENT_INTRINSIC(tex2D, 0, {
	return m.add_instruction(block, {}, spv::OpImageSampleImplicitLod, m.convert_type(T_FLOAT4))
		.add(args[0].base)
		.add(args[1].base)
		.add(spv::ImageOperandsMaskNone)
		.result;
	})

// ret tex2Dlod(s, coords)
DEFINE_INTRINSIC(tex2Dlod, 0, T_FLOAT4, T_SAMPLER, T_FLOAT4)
IMPLEMENT_INTRINSIC(tex2Dlod, 0, {
	const spv::Id lod = m.add_instruction(block, {}, spv::OpCompositeExtract, m.convert_type(T_FLOAT1))
		.add(args[1].base)
		.add(3)
		.result;

	return m.add_instruction(block, {}, spv::OpImageSampleExplicitLod, m.convert_type(T_FLOAT4))
		.add(args[0].base)
		.add(args[1].base)
		.add(spv::ImageOperandsLodMask)
		.add(lod)
		.result;
	})

// ret tex2Dsize(s)
// ret tex2Dsize(s, lod)
DEFINE_INTRINSIC(tex2Dsize, 0, T_INT2, T_SAMPLER)
DEFINE_INTRINSIC(tex2Dsize, 1, T_INT2, T_SAMPLER, T_INT1)
IMPLEMENT_INTRINSIC(tex2Dsize, 0, {
	m.add_capability(spv::CapabilityImageQuery);

	const spv::Id image = m.add_instruction(block, {}, spv::OpImage, m.convert_type({ spirv_type::t_texture }))
		.add(args[0].base)
		.result;

	return m.add_instruction(block, {}, spv::OpImageQuerySize, m.convert_type(T_INT2))
		.add(image)
		.result;
	})
IMPLEMENT_INTRINSIC(tex2Dsize, 1, {
	m.add_capability(spv::CapabilityImageQuery);

	const spv::Id image = m.add_instruction(block, {}, spv::OpImage, m.convert_type({ spirv_type::t_texture }))
		.add(args[0].base)
		.result;

	return m.add_instruction(block, {}, spv::OpImageQuerySizeLod, m.convert_type(T_INT2))
		.add(image)
		.add(args[1].base)
		.result;
	})

// ret tex2Dfetch(s, coords)
DEFINE_INTRINSIC(tex2Dfetch, 0, T_FLOAT4, T_SAMPLER, T_INT4)
IMPLEMENT_INTRINSIC(tex2Dfetch, 0, {
	const spv::Id lod = m.add_instruction(block, {}, spv::OpCompositeExtract, m.convert_type(T_INT1))
		.add(args[1].base)
		.add(3)
		.result;

	return m.add_instruction(block, {}, spv::OpImageSampleImplicitLod, m.convert_type(T_FLOAT4))
		.add(args[0].base)
		.add(args[1].base)
		.add(spv::ImageOperandsLodMask)
		.add(lod)
		.result;
	})

// ret tex2Dgather(s, coords, component)
DEFINE_INTRINSIC(tex2Dgather, 0, T_FLOAT4, T_SAMPLER, T_FLOAT2, T_INT1)
IMPLEMENT_INTRINSIC(tex2Dgather, 0, {
	return m.add_instruction(block, {}, spv::OpImageGather, m.convert_type(T_FLOAT4))
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.add(spv::ImageOperandsMaskNone)
		.result;
	})
