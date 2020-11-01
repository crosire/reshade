/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#ifdef __INTELLISENSE__
#undef DEFINE_INTRINSIC
#undef IMPLEMENT_INTRINSIC_GLSL
#undef IMPLEMENT_INTRINSIC_HLSL
#undef IMPLEMENT_INTRINSIC_SPIRV
#endif

#ifndef DEFINE_INTRINSIC
#define DEFINE_INTRINSIC(name, i, ret_type, ...)
#endif
#ifndef IMPLEMENT_INTRINSIC_GLSL
#define IMPLEMENT_INTRINSIC_GLSL(name, i, code)
#endif
#ifndef IMPLEMENT_INTRINSIC_HLSL
#define IMPLEMENT_INTRINSIC_HLSL(name, i, code)
#endif
#ifndef IMPLEMENT_INTRINSIC_SPIRV
#define IMPLEMENT_INTRINSIC_SPIRV(name, i, code)
#endif

// ret abs(x)
DEFINE_INTRINSIC(abs, 0, int, int)
DEFINE_INTRINSIC(abs, 0, int2, int2)
DEFINE_INTRINSIC(abs, 0, int3, int3)
DEFINE_INTRINSIC(abs, 0, int4, int4)
DEFINE_INTRINSIC(abs, 1, float, float)
DEFINE_INTRINSIC(abs, 1, float2, float2)
DEFINE_INTRINSIC(abs, 1, float3, float3)
DEFINE_INTRINSIC(abs, 1, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(abs, 0, {
	code += "abs(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_GLSL(abs, 1, {
	code += "abs(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(abs, 0, {
	code += "abs(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(abs, 1, {
	code += "abs(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(abs, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450SAbs)
		.add(args[0].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(abs, 1, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450FAbs)
		.add(args[0].base)
		.result;
	})

// ret all(x)
DEFINE_INTRINSIC(all, 0, bool, bool)
DEFINE_INTRINSIC(all, 1, bool, bool2)
DEFINE_INTRINSIC(all, 1, bool, bool3)
DEFINE_INTRINSIC(all, 1, bool, bool4)
IMPLEMENT_INTRINSIC_GLSL(all, 0, {
	code += id_to_name(args[0].base);
	})
IMPLEMENT_INTRINSIC_GLSL(all, 1, {
	code += "all(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(all, 0, {
	code += id_to_name(args[0].base);
	})
IMPLEMENT_INTRINSIC_HLSL(all, 1, {
	code += "all(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(all, 0, {
	return args[0].base;
	})
IMPLEMENT_INTRINSIC_SPIRV(all, 1, {
	return add_instruction(spv::OpAll, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret any(x)
DEFINE_INTRINSIC(any, 0, bool, bool)
DEFINE_INTRINSIC(any, 1, bool, bool2)
DEFINE_INTRINSIC(any, 1, bool, bool3)
DEFINE_INTRINSIC(any, 1, bool, bool4)
IMPLEMENT_INTRINSIC_GLSL(any, 0, {
	code += id_to_name(args[0].base);
	})
IMPLEMENT_INTRINSIC_GLSL(any, 1, {
	code += "any(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(any, 0, {
	code += id_to_name(args[0].base);
	})
IMPLEMENT_INTRINSIC_HLSL(any, 1, {
	code += "any(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(any, 0, {
	return args[0].base;
	})
IMPLEMENT_INTRINSIC_SPIRV(any, 1, {
	return add_instruction(spv::OpAny, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret asin(x)
DEFINE_INTRINSIC(asin, 0, float, float)
DEFINE_INTRINSIC(asin, 0, float2, float2)
DEFINE_INTRINSIC(asin, 0, float3, float3)
DEFINE_INTRINSIC(asin, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(asin, 0, {
	code += "asin(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(asin, 0, {
	code += "asin(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(asin, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Asin)
		.add(args[0].base)
		.result;
	})

// ret acos(x)
DEFINE_INTRINSIC(acos, 0, float, float)
DEFINE_INTRINSIC(acos, 0, float2, float2)
DEFINE_INTRINSIC(acos, 0, float3, float3)
DEFINE_INTRINSIC(acos, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(acos, 0, {
	code += "acos(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(acos, 0, {
	code += "acos(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(acos, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Acos)
		.add(args[0].base)
		.result;
	})

// ret atan(x)
DEFINE_INTRINSIC(atan, 0, float, float)
DEFINE_INTRINSIC(atan, 0, float2, float2)
DEFINE_INTRINSIC(atan, 0, float3, float3)
DEFINE_INTRINSIC(atan, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(atan, 0, {
	code += "atan(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(atan, 0, {
	code += "atan(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(atan, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Atan)
		.add(args[0].base)
		.result;
	})

// ret atan2(x, y)
DEFINE_INTRINSIC(atan2, 0, float, float, float)
DEFINE_INTRINSIC(atan2, 0, float2, float2, float2)
DEFINE_INTRINSIC(atan2, 0, float3, float3, float3)
DEFINE_INTRINSIC(atan2, 0, float4, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(atan2, 0, {
	code += "atan(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(atan2, 0, {
	code += "atan2(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(atan2, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Atan2)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret sin(x)
DEFINE_INTRINSIC(sin, 0, float, float)
DEFINE_INTRINSIC(sin, 0, float2, float2)
DEFINE_INTRINSIC(sin, 0, float3, float3)
DEFINE_INTRINSIC(sin, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(sin, 0, {
	code += "sin(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(sin, 0, {
	code += "sin(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(sin, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Sin)
		.add(args[0].base)
		.result;
	})

// ret sinh(x)
DEFINE_INTRINSIC(sinh, 0, float, float)
DEFINE_INTRINSIC(sinh, 0, float2, float2)
DEFINE_INTRINSIC(sinh, 0, float3, float3)
DEFINE_INTRINSIC(sinh, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(sinh, 0, {
	code += "sinh(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(sinh, 0, {
	code += "sinh(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(sinh, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Sinh)
		.add(args[0].base)
		.result;
	})

// ret cos(x)
DEFINE_INTRINSIC(cos, 0, float, float)
DEFINE_INTRINSIC(cos, 0, float2, float2)
DEFINE_INTRINSIC(cos, 0, float3, float3)
DEFINE_INTRINSIC(cos, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(cos, 0, {
	code += "cos(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(cos, 0, {
	code += "cos(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(cos, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Cos)
		.add(args[0].base)
		.result;
	})

// ret cosh(x)
DEFINE_INTRINSIC(cosh, 0, float, float)
DEFINE_INTRINSIC(cosh, 0, float2, float2)
DEFINE_INTRINSIC(cosh, 0, float3, float3)
DEFINE_INTRINSIC(cosh, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(cosh, 0, {
	code += "cosh(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(cosh, 0, {
	code += "cosh(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(cosh, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Cosh)
		.add(args[0].base)
		.result;
	})

// ret tan(x)
DEFINE_INTRINSIC(tan, 0, float, float)
DEFINE_INTRINSIC(tan, 0, float2, float2)
DEFINE_INTRINSIC(tan, 0, float3, float3)
DEFINE_INTRINSIC(tan, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(tan, 0, {
	code += "tan(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(tan, 0, {
	code += "tan(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(tan, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Tan)
		.add(args[0].base)
		.result;
	})

// ret tanh(x)
DEFINE_INTRINSIC(tanh, 0, float, float)
DEFINE_INTRINSIC(tanh, 0, float2, float2)
DEFINE_INTRINSIC(tanh, 0, float3, float3)
DEFINE_INTRINSIC(tanh, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(tanh, 0, {
	code += "tanh(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(tanh, 0, {
	code += "tanh(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(tanh, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Tanh)
		.add(args[0].base)
		.result;
	})

// sincos(x, out s, out c)
DEFINE_INTRINSIC(sincos, 0, void, float, out_float, out_float)
DEFINE_INTRINSIC(sincos, 0, void, float2, out_float2, out_float2)
DEFINE_INTRINSIC(sincos, 0, void, float3, out_float3, out_float3)
DEFINE_INTRINSIC(sincos, 0, void, float4, out_float4, out_float4)
IMPLEMENT_INTRINSIC_GLSL(sincos, 0, {
	code += id_to_name(args[1].base) + " = sin(" + id_to_name(args[0].base) + "), " + id_to_name(args[2].base) + " = cos(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(sincos, 0, {
	code += id_to_name(args[1].base) + " = sin(" + id_to_name(args[0].base) + "), " + id_to_name(args[2].base) + " = cos(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(sincos, 0, {
	const spv::Id sin_result = add_instruction(spv::OpExtInst, convert_type(args[0].type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Sin)
		.add(args[0].base)
		.result;
	const spv::Id cos_result = add_instruction(spv::OpExtInst, convert_type(args[0].type))
		.add(_glsl_ext)
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
DEFINE_INTRINSIC(asint, 0, int, float)
DEFINE_INTRINSIC(asint, 0, int2, float2)
DEFINE_INTRINSIC(asint, 0, int3, float3)
DEFINE_INTRINSIC(asint, 0, int4, float4)
IMPLEMENT_INTRINSIC_GLSL(asint, 0, {
	code += "floatBitsToInt(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(asint, 0, {
	// Really only supported on SM4+
	code += "asint(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(asint, 0, {
	return add_instruction(spv::OpBitcast, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret asuint(x)
DEFINE_INTRINSIC(asuint, 0, uint, float)
DEFINE_INTRINSIC(asuint, 0, uint2, float2)
DEFINE_INTRINSIC(asuint, 0, uint3, float3)
DEFINE_INTRINSIC(asuint, 0, uint4, float4)
IMPLEMENT_INTRINSIC_GLSL(asuint, 0, {
	code += "floatBitsToUint(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(asuint, 0, {
	code += "asuint(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(asuint, 0, {
	return add_instruction(spv::OpBitcast, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret asfloat(x)
DEFINE_INTRINSIC(asfloat, 0, float, int)
DEFINE_INTRINSIC(asfloat, 0, float2, int2)
DEFINE_INTRINSIC(asfloat, 0, float3, int3)
DEFINE_INTRINSIC(asfloat, 0, float4, int4)
DEFINE_INTRINSIC(asfloat, 1, float, uint)
DEFINE_INTRINSIC(asfloat, 1, float2, uint2)
DEFINE_INTRINSIC(asfloat, 1, float3, uint3)
DEFINE_INTRINSIC(asfloat, 1, float4, uint4)
IMPLEMENT_INTRINSIC_GLSL(asfloat, 0, {
	code += "intBitsToFloat(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_GLSL(asfloat, 1, {
	code += "uintBitsToFloat(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(asfloat, 0, {
	code += "asfloat(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(asfloat, 1, {
	code += "asfloat(" + id_to_name(args[0].base) + ')';
	})
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
DEFINE_INTRINSIC(ceil, 0, float, float)
DEFINE_INTRINSIC(ceil, 0, float2, float2)
DEFINE_INTRINSIC(ceil, 0, float3, float3)
DEFINE_INTRINSIC(ceil, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(ceil, 0, {
	code += "ceil(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(ceil, 0, {
	code += "ceil(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(ceil, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Ceil)
		.add(args[0].base)
		.result;
	})

// ret floor(x)
DEFINE_INTRINSIC(floor, 0, float, float)
DEFINE_INTRINSIC(floor, 0, float2, float2)
DEFINE_INTRINSIC(floor, 0, float3, float3)
DEFINE_INTRINSIC(floor, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(floor, 0, {
	code += "floor(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(floor, 0, {
	if (_shader_model >= 40u)
		code += "floor(" + id_to_name(args[0].base) + ')';
	else // Using the floor intrinsic sometimes causes the SM3 D3DCompiler to generate wrong code, so replace it with a custom implementation
		code += id_to_name(args[0].base) + " - frac(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(floor, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Floor)
		.add(args[0].base)
		.result;
	})

// ret clamp(x, min, max)
DEFINE_INTRINSIC(clamp, 0, int, int, int, int)
DEFINE_INTRINSIC(clamp, 0, int2, int2, int2, int2)
DEFINE_INTRINSIC(clamp, 0, int3, int3, int3, int3)
DEFINE_INTRINSIC(clamp, 0, int4, int4, int4, int4)
DEFINE_INTRINSIC(clamp, 1, uint, uint, uint, uint)
DEFINE_INTRINSIC(clamp, 1, uint2, uint2, uint2, uint2)
DEFINE_INTRINSIC(clamp, 1, uint3, uint3, uint3, uint3)
DEFINE_INTRINSIC(clamp, 1, uint4, uint4, uint4, uint4)
DEFINE_INTRINSIC(clamp, 2, float, float, float, float)
DEFINE_INTRINSIC(clamp, 2, float2, float2, float2, float2)
DEFINE_INTRINSIC(clamp, 2, float3, float3, float3, float3)
DEFINE_INTRINSIC(clamp, 2, float4, float4, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(clamp, 0, {
	code += "clamp(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_GLSL(clamp, 1, {
	code += "clamp(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_GLSL(clamp, 2, {
	code += "clamp(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(clamp, 0, {
	code += "clamp(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(clamp, 1, {
	code += "clamp(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(clamp, 2, {
	code += "clamp(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(clamp, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450SClamp)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(clamp, 1, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450UClamp)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(clamp, 2, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450FClamp)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})

// ret saturate(x)
DEFINE_INTRINSIC(saturate, 0, float, float)
DEFINE_INTRINSIC(saturate, 0, float2, float2)
DEFINE_INTRINSIC(saturate, 0, float3, float3)
DEFINE_INTRINSIC(saturate, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(saturate, 0, {
	code += "clamp(" + id_to_name(args[0].base) + ", 0.0, 1.0)";
	})
IMPLEMENT_INTRINSIC_HLSL(saturate, 0, {
	code += "saturate(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(saturate, 0, {
	const spv::Id constant_one = emit_constant(args[0].type, 1u);
	const spv::Id constant_zero = emit_constant(args[0].type, 0u);

	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450FClamp)
		.add(args[0].base)
		.add(constant_zero)
		.add(constant_one)
		.result;
	})

// ret mad(mvalue, avalue, bvalue)
DEFINE_INTRINSIC(mad, 0, float, float, float, float)
DEFINE_INTRINSIC(mad, 0, float2, float2, float2, float2)
DEFINE_INTRINSIC(mad, 0, float3, float3, float3, float3)
DEFINE_INTRINSIC(mad, 0, float4, float4, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(mad, 0, {
	code += "fma(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(mad, 0, {
	if (_shader_model >= 50u)
		code += "mad(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	else
		code += id_to_name(args[0].base) + " * " + id_to_name(args[1].base) + " + " + id_to_name(args[2].base);
	})
IMPLEMENT_INTRINSIC_SPIRV(mad, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Fma)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})

// ret rcp(x)
DEFINE_INTRINSIC(rcp, 0, float, float)
DEFINE_INTRINSIC(rcp, 0, float2, float2)
DEFINE_INTRINSIC(rcp, 0, float3, float3)
DEFINE_INTRINSIC(rcp, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(rcp, 0, {
	code += "1.0 / " + id_to_name(args[0].base);
	})
IMPLEMENT_INTRINSIC_HLSL(rcp, 0, {
	if (_shader_model >= 50u)
		code += "rcp(" + id_to_name(args[0].base) + ')';
	else
		code += "1.0 / " + id_to_name(args[0].base);
	})
IMPLEMENT_INTRINSIC_SPIRV(rcp, 0, {
	const spv::Id constant_one = emit_constant(args[0].type, 1u);

	return add_instruction(spv::OpFDiv, convert_type(res_type))
		.add(constant_one)
		.add(args[0].base)
		.result;
	})

// ret pow(x, y)
DEFINE_INTRINSIC(pow, 0, float, float, float)
DEFINE_INTRINSIC(pow, 0, float2, float2, float2)
DEFINE_INTRINSIC(pow, 0, float3, float3, float3)
DEFINE_INTRINSIC(pow, 0, float4, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(pow, 0, {
	code += "pow(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(pow, 0, {
	code += "pow(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(pow, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Pow)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret exp(x)
DEFINE_INTRINSIC(exp, 0, float, float)
DEFINE_INTRINSIC(exp, 0, float2, float2)
DEFINE_INTRINSIC(exp, 0, float3, float3)
DEFINE_INTRINSIC(exp, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(exp, 0, {
	code += "exp(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(exp, 0, {
	code += "exp(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(exp, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Exp)
		.add(args[0].base)
		.result;
	})

// ret exp2(x)
DEFINE_INTRINSIC(exp2, 0, float, float)
DEFINE_INTRINSIC(exp2, 0, float2, float2)
DEFINE_INTRINSIC(exp2, 0, float3, float3)
DEFINE_INTRINSIC(exp2, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(exp2, 0, {
	code += "exp2(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(exp2, 0, {
	code += "exp2(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(exp2, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Exp2)
		.add(args[0].base)
		.result;
	})

// ret log(x)
DEFINE_INTRINSIC(log, 0, float, float)
DEFINE_INTRINSIC(log, 0, float2, float2)
DEFINE_INTRINSIC(log, 0, float3, float3)
DEFINE_INTRINSIC(log, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(log, 0, {
	code += "log(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(log, 0, {
	code += "log(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(log, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Log)
		.add(args[0].base)
		.result;
	})

// ret log2(x)
DEFINE_INTRINSIC(log2, 0, float, float)
DEFINE_INTRINSIC(log2, 0, float2, float2)
DEFINE_INTRINSIC(log2, 0, float3, float3)
DEFINE_INTRINSIC(log2, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(log2, 0, {
	code += "log2(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(log2, 0, {
	code += "log2(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(log2, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Log2)
		.add(args[0].base)
		.result;
	})

// ret log10(x)
DEFINE_INTRINSIC(log10, 0, float, float)
DEFINE_INTRINSIC(log10, 0, float2, float2)
DEFINE_INTRINSIC(log10, 0, float3, float3)
DEFINE_INTRINSIC(log10, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(log10, 0, {
	code += "(log2(" + id_to_name(args[0].base) + ") / log2(10.0))";
	})
IMPLEMENT_INTRINSIC_HLSL(log10, 0, {
	code += "(log2(" + id_to_name(args[0].base) + ") / log2(10.0))";
	})
IMPLEMENT_INTRINSIC_SPIRV(log10, 0, {
	const spv::Id log2 = add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Log2)
		.add(args[0].base)
		.result;

	const spv::Id log10 = emit_constant(args[0].type, /* log2(10) */
		{ { 3.321928f, 3.321928f, 3.321928f, 3.321928f } });

	return add_instruction(spv::OpFDiv, convert_type(res_type))
		.add(log2)
		.add(log10)
		.result; })

// ret sign(x)
DEFINE_INTRINSIC(sign, 0, int, int)
DEFINE_INTRINSIC(sign, 0, int2, int2)
DEFINE_INTRINSIC(sign, 0, int3, int3)
DEFINE_INTRINSIC(sign, 0, int4, int4)
DEFINE_INTRINSIC(sign, 1, float, float)
DEFINE_INTRINSIC(sign, 1, float2, float2)
DEFINE_INTRINSIC(sign, 1, float3, float3)
DEFINE_INTRINSIC(sign, 1, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(sign, 0, {
	code += "sign(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_GLSL(sign, 1, {
	code += "sign(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(sign, 0, {
	code += "sign(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(sign, 1, {
	code += "sign(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(sign, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450SSign)
		.add(args[0].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(sign, 1, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450FSign)
		.add(args[0].base)
		.result;
	})

// ret sqrt(x)
DEFINE_INTRINSIC(sqrt, 0, float, float)
DEFINE_INTRINSIC(sqrt, 0, float2, float2)
DEFINE_INTRINSIC(sqrt, 0, float3, float3)
DEFINE_INTRINSIC(sqrt, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(sqrt, 0, {
	code += "sqrt(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(sqrt, 0, {
	code += "sqrt(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(sqrt, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Sqrt)
		.add(args[0].base)
		.result;
	})

// ret rsqrt(x)
DEFINE_INTRINSIC(rsqrt, 0, float, float)
DEFINE_INTRINSIC(rsqrt, 0, float2, float2)
DEFINE_INTRINSIC(rsqrt, 0, float3, float3)
DEFINE_INTRINSIC(rsqrt, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(rsqrt, 0, {
	code += "inversesqrt(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(rsqrt, 0, {
	code += "rsqrt(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(rsqrt, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450InverseSqrt)
		.add(args[0].base)
		.result;
	})

// ret lerp(x, y, s)
DEFINE_INTRINSIC(lerp, 0, float, float, float, float)
DEFINE_INTRINSIC(lerp, 0, float2, float2, float2, float2)
DEFINE_INTRINSIC(lerp, 0, float3, float3, float3, float3)
DEFINE_INTRINSIC(lerp, 0, float4, float4, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(lerp, 0, {
	code += "mix(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(lerp, 0, {
	code += "lerp(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(lerp, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450FMix)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})

// ret step(y, x)
DEFINE_INTRINSIC(step, 0, float, float, float)
DEFINE_INTRINSIC(step, 0, float2, float2, float2)
DEFINE_INTRINSIC(step, 0, float3, float3, float3)
DEFINE_INTRINSIC(step, 0, float4, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(step, 0, {
	code += "step(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(step, 0, {
	code += "step(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(step, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Step)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret smoothstep(min, max, x)
DEFINE_INTRINSIC(smoothstep, 0, float, float, float, float)
DEFINE_INTRINSIC(smoothstep, 0, float2, float2, float2, float2)
DEFINE_INTRINSIC(smoothstep, 0, float3, float3, float3, float3)
DEFINE_INTRINSIC(smoothstep, 0, float4, float4, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(smoothstep, 0, {
	code += "smoothstep(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(smoothstep, 0, {
	code += "smoothstep(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(smoothstep, 0, {
	return add_instruction(spv::OpExtInst, convert_type(args[2].type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450SmoothStep)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})

// ret frac(x)
DEFINE_INTRINSIC(frac, 0, float, float)
DEFINE_INTRINSIC(frac, 0, float2, float2)
DEFINE_INTRINSIC(frac, 0, float3, float3)
DEFINE_INTRINSIC(frac, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(frac, 0, {
	code += "fract(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(frac, 0, {
	code += "frac(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(frac, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Fract)
		.add(args[0].base)
		.result;
	})

// ret ldexp(x, exp)
DEFINE_INTRINSIC(ldexp, 0, float, float, int)
DEFINE_INTRINSIC(ldexp, 0, float2, float2, int2)
DEFINE_INTRINSIC(ldexp, 0, float3, float3, int3)
DEFINE_INTRINSIC(ldexp, 0, float4, float4, int4)
IMPLEMENT_INTRINSIC_GLSL(ldexp, 0, {
	code += "ldexp(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(ldexp, 0, {
	code += "ldexp(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(ldexp, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Ldexp)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret modf(x, out ip)
DEFINE_INTRINSIC(modf, 0, float, float, out_float)
DEFINE_INTRINSIC(modf, 0, float2, float2, out_float2)
DEFINE_INTRINSIC(modf, 0, float3, float3, out_float3)
DEFINE_INTRINSIC(modf, 0, float4, float4, out_float4)
IMPLEMENT_INTRINSIC_GLSL(modf, 0, {
	code += "modf(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(modf, 0, {
	code += "modf(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(modf, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Modf)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret frexp(x, out exp)
DEFINE_INTRINSIC(frexp, 0, float, float, out_float)
DEFINE_INTRINSIC(frexp, 0, float2, float2, out_float2)
DEFINE_INTRINSIC(frexp, 0, float3, float3, out_float3)
DEFINE_INTRINSIC(frexp, 0, float4, float4, out_float4)
IMPLEMENT_INTRINSIC_GLSL(frexp, 0, {
	code += "frexp(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(frexp, 0, {
	code += "frexp(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(frexp, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Frexp)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret trunc(x)
DEFINE_INTRINSIC(trunc, 0, float, float)
DEFINE_INTRINSIC(trunc, 0, float2, float2)
DEFINE_INTRINSIC(trunc, 0, float3, float3)
DEFINE_INTRINSIC(trunc, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(trunc, 0, {
	code += "trunc(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(trunc, 0, {
	code += "trunc(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(trunc, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Trunc)
		.add(args[0].base)
		.result;
	})

// ret round(x)
DEFINE_INTRINSIC(round, 0, float, float)
DEFINE_INTRINSIC(round, 0, float2, float2)
DEFINE_INTRINSIC(round, 0, float3, float3)
DEFINE_INTRINSIC(round, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(round, 0, {
	code += "round(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(round, 0, {
	code += "round(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(round, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Round)
		.add(args[0].base)
		.result;
	})

// ret min(x, y)
DEFINE_INTRINSIC(min, 0, int, int, int)
DEFINE_INTRINSIC(min, 0, int2, int2, int2)
DEFINE_INTRINSIC(min, 0, int3, int3, int3)
DEFINE_INTRINSIC(min, 0, int4, int4, int4)
DEFINE_INTRINSIC(min, 1, float, float, float)
DEFINE_INTRINSIC(min, 1, float2, float2, float2)
DEFINE_INTRINSIC(min, 1, float3, float3, float3)
DEFINE_INTRINSIC(min, 1, float4, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(min, 0, {
	code += "min(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_GLSL(min, 1, {
	code += "min(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(min, 0, {
	code += "min(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(min, 1, {
	code += "min(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(min, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450SMin)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(min, 1, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450FMin)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret max(x, y)
DEFINE_INTRINSIC(max, 0, int, int, int)
DEFINE_INTRINSIC(max, 0, int2, int2, int2)
DEFINE_INTRINSIC(max, 0, int3, int3, int3)
DEFINE_INTRINSIC(max, 0, int4, int4, int4)
DEFINE_INTRINSIC(max, 1, float, float, float)
DEFINE_INTRINSIC(max, 1, float2, float2, float2)
DEFINE_INTRINSIC(max, 1, float3, float3, float3)
DEFINE_INTRINSIC(max, 1, float4, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(max, 0, {
	code += "max(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_GLSL(max, 1, {
	code += "max(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(max, 0, {
	code += "max(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(max, 1, {
	code += "max(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(max, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450SMax)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(max, 1, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450FMax)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret degree(x)
DEFINE_INTRINSIC(degrees, 0, float, float)
DEFINE_INTRINSIC(degrees, 0, float2, float2)
DEFINE_INTRINSIC(degrees, 0, float3, float3)
DEFINE_INTRINSIC(degrees, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(degrees, 0, {
	code += "degrees(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(degrees, 0, {
	code += "degrees(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(degrees, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Degrees)
		.add(args[0].base)
		.result;
	})

// ret radians(x)
DEFINE_INTRINSIC(radians, 0, float, float)
DEFINE_INTRINSIC(radians, 0, float2, float2)
DEFINE_INTRINSIC(radians, 0, float3, float3)
DEFINE_INTRINSIC(radians, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(radians, 0, {
	code += "radians(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(radians, 0, {
	code += "radians(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(radians, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Radians)
		.add(args[0].base)
		.result;
	})

// ret ddx(x)
DEFINE_INTRINSIC(ddx, 0, float, float)
DEFINE_INTRINSIC(ddx, 0, float2, float2)
DEFINE_INTRINSIC(ddx, 0, float3, float3)
DEFINE_INTRINSIC(ddx, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(ddx, 0, {
	code += "dFdx(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(ddx, 0, {
	code += "ddx(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(ddx, 0, {
	return add_instruction(spv::OpDPdx, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret ddy(x)
DEFINE_INTRINSIC(ddy, 0, float, float)
DEFINE_INTRINSIC(ddy, 0, float2, float2)
DEFINE_INTRINSIC(ddy, 0, float3, float3)
DEFINE_INTRINSIC(ddy, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(ddy, 0, {
	code += "dFdy(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(ddy, 0, {
	code += "ddy(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(ddy, 0, {
	return add_instruction(spv::OpDPdy, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret fwidth(x)
DEFINE_INTRINSIC(fwidth, 0, float, float)
DEFINE_INTRINSIC(fwidth, 0, float2, float2)
DEFINE_INTRINSIC(fwidth, 0, float3, float3)
DEFINE_INTRINSIC(fwidth, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(fwidth, 0, {
	code += "fwidth(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(fwidth, 0, {
	code += "fwidth(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(fwidth, 0, {
	return add_instruction(spv::OpFwidth, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret dot(x, y)
DEFINE_INTRINSIC(dot, 0, float, float2, float2)
DEFINE_INTRINSIC(dot, 0, float, float3, float3)
DEFINE_INTRINSIC(dot, 0, float, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(dot, 0, {
	code += "dot(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(dot, 0, {
	code += "dot(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(dot, 0, {
	return add_instruction(spv::OpDot, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret cross(x, y)
DEFINE_INTRINSIC(cross, 0, float3, float3, float3)
IMPLEMENT_INTRINSIC_GLSL(cross, 0, {
	code += "cross(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(cross, 0, {
	code += "cross(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(cross, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Cross)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret length(x)
DEFINE_INTRINSIC(length, 0, float, float)
DEFINE_INTRINSIC(length, 0, float, float2)
DEFINE_INTRINSIC(length, 0, float, float3)
DEFINE_INTRINSIC(length, 0, float, float4)
IMPLEMENT_INTRINSIC_GLSL(length, 0, {
	code += "length(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(length, 0, {
	code += "length(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(length, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Length)
		.add(args[0].base)
		.result;
	})

// ret distance(x, y)
DEFINE_INTRINSIC(distance, 0, float, float, float)
DEFINE_INTRINSIC(distance, 0, float, float2, float2)
DEFINE_INTRINSIC(distance, 0, float, float3, float3)
DEFINE_INTRINSIC(distance, 0, float, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(distance, 0, {
	code += "distance(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(distance, 0, {
	code += "distance(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(distance, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Distance)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret normalize(x)
DEFINE_INTRINSIC(normalize, 0, float2, float2)
DEFINE_INTRINSIC(normalize, 0, float3, float3)
DEFINE_INTRINSIC(normalize, 0, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(normalize, 0, {
	code += "normalize(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(normalize, 0, {
	code += "normalize(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(normalize, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Normalize)
		.add(args[0].base)
		.result;
	})

// ret transpose(x)
DEFINE_INTRINSIC(transpose, 0, float2x2, float2x2)
DEFINE_INTRINSIC(transpose, 0, float3x3, float3x3)
DEFINE_INTRINSIC(transpose, 0, float4x4, float4x4)
IMPLEMENT_INTRINSIC_GLSL(transpose, 0, {
	code += "transpose(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(transpose, 0, {
	code += "transpose(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(transpose, 0, {
	return add_instruction(spv::OpTranspose, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret determinant(m)
DEFINE_INTRINSIC(determinant, 0, float, float2x2)
DEFINE_INTRINSIC(determinant, 0, float, float3x3)
DEFINE_INTRINSIC(determinant, 0, float, float4x4)
IMPLEMENT_INTRINSIC_GLSL(determinant, 0, {
	code += "determinant(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(determinant, 0, {
	code += "determinant(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(determinant, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Determinant)
		.add(args[0].base)
		.result;
	})

// ret reflect(i, n)
DEFINE_INTRINSIC(reflect, 0, float2, float2, float2)
DEFINE_INTRINSIC(reflect, 0, float3, float3, float3)
DEFINE_INTRINSIC(reflect, 0, float4, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(reflect, 0, {
	code += "reflect(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(reflect, 0, {
	code += "reflect(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(reflect, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Reflect)
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

// ret refract(i, n, eta)
DEFINE_INTRINSIC(refract, 0, float2, float2, float2, float)
DEFINE_INTRINSIC(refract, 0, float3, float3, float3, float)
DEFINE_INTRINSIC(refract, 0, float4, float4, float4, float)
IMPLEMENT_INTRINSIC_GLSL(refract, 0, {
	code += "refract(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(refract, 0, {
	code += "refract(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(refract, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450Refract)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})

// ret faceforward(n, i, ng)
DEFINE_INTRINSIC(faceforward, 0, float, float, float, float)
DEFINE_INTRINSIC(faceforward, 0, float2, float2, float2, float2)
DEFINE_INTRINSIC(faceforward, 0, float3, float3, float3, float3)
DEFINE_INTRINSIC(faceforward, 0, float4, float4, float4, float4)
IMPLEMENT_INTRINSIC_GLSL(faceforward, 0, {
	code += "faceforward(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(faceforward, 0, {
	code += "faceforward(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(faceforward, 0, {
	return add_instruction(spv::OpExtInst, convert_type(res_type))
		.add(_glsl_ext)
		.add(spv::GLSLstd450FaceForward)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base)
		.result;
	})

// ret mul(x, y)
DEFINE_INTRINSIC(mul, 0, float2, float, float2)
DEFINE_INTRINSIC(mul, 0, float3, float, float3)
DEFINE_INTRINSIC(mul, 0, float4, float, float4)
IMPLEMENT_INTRINSIC_GLSL(mul, 0, {
	code += '(' + id_to_name(args[0].base) + " * " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(mul, 0, {
	code += '(' + id_to_name(args[0].base) + " * " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(mul, 0, {
	return add_instruction(spv::OpVectorTimesScalar, convert_type(res_type))
		.add(args[1].base)
		.add(args[0].base)
		.result;
	})
DEFINE_INTRINSIC(mul, 1, float2, float2, float)
DEFINE_INTRINSIC(mul, 1, float3, float3, float)
DEFINE_INTRINSIC(mul, 1, float4, float4, float)
IMPLEMENT_INTRINSIC_GLSL(mul, 1, {
	code += '(' + id_to_name(args[0].base) + " * " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(mul, 1, {
	code += '(' + id_to_name(args[0].base) + " * " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(mul, 1, {
	return add_instruction(spv::OpVectorTimesScalar, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

DEFINE_INTRINSIC(mul, 2, float2x2, float, float2x2)
DEFINE_INTRINSIC(mul, 2, float3x3, float, float3x3)
DEFINE_INTRINSIC(mul, 2, float4x4, float, float4x4)
IMPLEMENT_INTRINSIC_GLSL(mul, 2, {
	code += '(' + id_to_name(args[0].base) + " * " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(mul, 2, {
	code += '(' + id_to_name(args[0].base) + " * " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(mul, 2, {
	return add_instruction(spv::OpMatrixTimesScalar, convert_type(res_type))
		.add(args[1].base)
		.add(args[0].base)
		.result;
	})
DEFINE_INTRINSIC(mul, 3, float2x2, float2x2, float)
DEFINE_INTRINSIC(mul, 3, float3x3, float3x3, float)
DEFINE_INTRINSIC(mul, 3, float4x4, float4x4, float)
IMPLEMENT_INTRINSIC_GLSL(mul, 3, {
	code += '(' + id_to_name(args[0].base) + " * " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(mul, 3, {
	code += '(' + id_to_name(args[0].base) + " * " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(mul, 3, {
	return add_instruction(spv::OpMatrixTimesScalar, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.result;
	})

DEFINE_INTRINSIC(mul, 4, float2, float2, float2x2)
DEFINE_INTRINSIC(mul, 4, float3, float3, float3x3)
DEFINE_INTRINSIC(mul, 4, float4, float4, float4x4)
IMPLEMENT_INTRINSIC_GLSL(mul, 4, {
	// Flip inputs because matrices are column-wise
	code += '(' + id_to_name(args[1].base) + " * " + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(mul, 4, {
	code += "mul(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(mul, 4, {
	return add_instruction(spv::OpMatrixTimesVector, convert_type(res_type))
		.add(args[1].base) // Flip inputs because matrices are column-wise
		.add(args[0].base)
		.result;
	})
DEFINE_INTRINSIC(mul, 5, float2, float2x2, float2)
DEFINE_INTRINSIC(mul, 5, float3, float3x3, float3)
DEFINE_INTRINSIC(mul, 5, float4, float4x4, float4)
IMPLEMENT_INTRINSIC_GLSL(mul, 5, {
	code += '(' + id_to_name(args[1].base) + " * " + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(mul, 5, {
	code += "mul(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(mul, 5, {
	return add_instruction(spv::OpVectorTimesMatrix, convert_type(res_type))
		.add(args[1].base)
		.add(args[0].base)
		.result;
	})

DEFINE_INTRINSIC(mul, 6, float2x2, float2x2, float2x2)
DEFINE_INTRINSIC(mul, 6, float3x3, float3x3, float3x3)
DEFINE_INTRINSIC(mul, 6, float4x4, float4x4, float4x4)
IMPLEMENT_INTRINSIC_GLSL(mul, 6, {
	code += '(' + id_to_name(args[1].base) + " * " + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(mul, 6, {
	code += "mul(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(mul, 6, {
	return add_instruction(spv::OpMatrixTimesMatrix, convert_type(res_type))
		.add(args[1].base)
		.add(args[0].base)
		.result;
	})

// ret isinf(x)
DEFINE_INTRINSIC(isinf, 0, bool, float)
DEFINE_INTRINSIC(isinf, 0, bool2, float2)
DEFINE_INTRINSIC(isinf, 0, bool3, float3)
DEFINE_INTRINSIC(isinf, 0, bool4, float4)
IMPLEMENT_INTRINSIC_GLSL(isinf, 0, {
	code += "isinf(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(isinf, 0, {
	code += "isinf(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(isinf, 0, {
	return add_instruction(spv::OpIsInf, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret isnan(x)
DEFINE_INTRINSIC(isnan, 0, bool, float)
DEFINE_INTRINSIC(isnan, 0, bool2, float2)
DEFINE_INTRINSIC(isnan, 0, bool3, float3)
DEFINE_INTRINSIC(isnan, 0, bool4, float4)
IMPLEMENT_INTRINSIC_GLSL(isnan, 0, {
	code += "isnan(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(isnan, 0, {
	code += "isnan(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(isnan, 0, {
	return add_instruction(spv::OpIsNan, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// ret tex2D(s, coords)
// ret tex2D(s, coords, offset)
DEFINE_INTRINSIC(tex2D, 0, float4, sampler, float2)
DEFINE_INTRINSIC(tex2D, 1, float4, sampler, float2, int2)
IMPLEMENT_INTRINSIC_GLSL(tex2D, 0, {
	code += "texture(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_GLSL(tex2D, 1, {
	code += "textureOffset(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(tex2D, 0, {
	if (_shader_model >= 40u) // SM4 and higher use a more object-oriented programming model for textures
		code += id_to_name(args[0].base) + ".t.Sample(" + id_to_name(args[0].base) + ".s, " + id_to_name(args[1].base) + ')';
	else
		code += "tex2D(" + id_to_name(args[0].base) + ".s, " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(tex2D, 1, {
	if (_shader_model >= 40u)
		code += id_to_name(args[0].base) + ".t.Sample(" + id_to_name(args[0].base) + ".s, " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	else
		code += "tex2D(" + id_to_name(args[0].base) + ".s, " + id_to_name(args[1].base) + " + " + id_to_name(args[2].base) + " * " + id_to_name(args[0].base) + ".pixelsize)";
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2D, 0, {
	return add_instruction(spv::OpImageSampleImplicitLod, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.add(spv::ImageOperandsMaskNone)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2D, 1, {
	// Non-constant offset operand needs extended capability
	if (!args[2].is_constant)
		add_capability(spv::CapabilityImageGatherExtended);

	return add_instruction(spv::OpImageSampleImplicitLod, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].is_constant ? spv::ImageOperandsConstOffsetMask : spv::ImageOperandsOffsetMask)
		.add(args[2].base)
		.result;
	})

// ret tex2Dlod(s, coords)
// ret tex2Dlod(s, coords, offset)
DEFINE_INTRINSIC(tex2Dlod, 0, float4, sampler, float4)
DEFINE_INTRINSIC(tex2Dlod, 1, float4, sampler, float4, int2)
IMPLEMENT_INTRINSIC_GLSL(tex2Dlod, 0, {
	code += "textureLod(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ".xy, " + id_to_name(args[1].base) + ".w)";
	})
IMPLEMENT_INTRINSIC_GLSL(tex2Dlod, 1, {
	code += "textureLodOffset(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ".xy, " + id_to_name(args[1].base) + ".w, " + id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(tex2Dlod, 0, {
	if (_shader_model >= 40u)
		code += id_to_name(args[0].base) + ".t.SampleLevel(" + id_to_name(args[0].base) + ".s, " + id_to_name(args[1].base) + ".xy, " + id_to_name(args[1].base) + ".w)";
	else
		code += "tex2Dlod(" + id_to_name(args[0].base) + ".s, " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(tex2Dlod, 1, {
	if (_shader_model >= 40u)
		code += id_to_name(args[0].base) + ".t.SampleLevel(" + id_to_name(args[0].base) + ".s, " + id_to_name(args[1].base) + ".xy, " + id_to_name(args[1].base) + ".w, " + id_to_name(args[2].base) + ')';
	else
		code += "tex2Dlod(" + id_to_name(args[0].base) + ".s, " + id_to_name(args[1].base) + " + float4(" + id_to_name(args[2].base) + " * " + id_to_name(args[0].base) + ".pixelsize, 0, 0))";
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2Dlod, 0, {
	const spv::Id xy = add_instruction(spv::OpVectorShuffle, convert_type({ type::t_float, 2, 1 }))
		.add(args[1].base)
		.add(args[1].base)
		.add(0) // .x
		.add(1) // .y
		.result;
	const spv::Id lod = add_instruction(spv::OpCompositeExtract, convert_type({ type::t_float, 1, 1 }))
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
IMPLEMENT_INTRINSIC_SPIRV(tex2Dlod, 1, {
	if (!args[2].is_constant)
		add_capability(spv::CapabilityImageGatherExtended);

	const spv::Id xy = add_instruction(spv::OpVectorShuffle, convert_type({ type::t_float, 2, 1 }))
		.add(args[1].base)
		.add(args[1].base)
		.add(0) // .x
		.add(1) // .y
		.result;
	const spv::Id lod = add_instruction(spv::OpCompositeExtract, convert_type({ type::t_float, 1, 1 }))
		.add(args[1].base)
		.add(3) // .w
		.result;

	return add_instruction(spv::OpImageSampleExplicitLod, convert_type(res_type))
		.add(args[0].base)
		.add(xy)
		.add(spv::ImageOperandsLodMask | (args[2].is_constant ? spv::ImageOperandsConstOffsetMask : spv::ImageOperandsOffsetMask))
		.add(lod)
		.add(args[2].base)
		.result;
	})

// ret tex2Dfetch(s, coords)
// ret tex2Dfetch(s, coords, lod)
DEFINE_INTRINSIC(tex2Dfetch, 0, float4, sampler, int2)
DEFINE_INTRINSIC(tex2Dfetch, 1, float4, sampler, int2, int)
IMPLEMENT_INTRINSIC_GLSL(tex2Dfetch, 0, {
	code += "texelFetch(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", 0)";
	})
IMPLEMENT_INTRINSIC_GLSL(tex2Dfetch, 1, {
	code += "texelFetch(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(tex2Dfetch, 0, {
	if (_shader_model >= 40u)
		code += id_to_name(args[0].base) + ".t.Load(int3(" + id_to_name(args[1].base) + ", 0))";
	else
		// SM3 does not have a fetch intrinsic, so emulate it by transforming coordinates into texture space ones
		// Also add a half-pixel offset to align texels with pixels
		//   (coords + 0.5) / size
		code += "tex2Dlod(" + id_to_name(args[0].base) + ".s, float4((" +
			id_to_name(args[1].base) + ".xy + 0.5) * " + id_to_name(args[0].base) + ".pixelsize, 0, 0))";
	})
IMPLEMENT_INTRINSIC_HLSL(tex2Dfetch, 1, {
	if (_shader_model >= 40u)
		code += id_to_name(args[0].base) + ".t.Load(int3(" + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + "))";
	else
		code += "tex2Dlod(" + id_to_name(args[0].base) + ".s, float4((" +
			id_to_name(args[1].base) + " + 0.5) * " + id_to_name(args[0].base) + ".pixelsize * exp2(" + id_to_name(args[2].base) + "), 0, " +
			id_to_name(args[2].base) + "))";
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2Dfetch, 0, {
	const spv::Id image = add_instruction(spv::OpImage, convert_type({ type::t_texture }))
		.add(args[0].base).result;

	return add_instruction(spv::OpImageFetch, convert_type(res_type))
		.add(image)
		.add(args[1].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2Dfetch, 1, {
	const spv::Id image = add_instruction(spv::OpImage, convert_type({ type::t_texture }))
		.add(args[0].base).result;

	return add_instruction(spv::OpImageFetch, convert_type(res_type))
		.add(image)
		.add(args[1].base)
		.add(spv::ImageOperandsLodMask)
		.add(args[2].base)
		.result;
	})

// ret tex2DgatherR(s, coords)
// ret tex2DgatherR(s, coords, offset)
DEFINE_INTRINSIC(tex2DgatherR, 0, float4, sampler, float2)
DEFINE_INTRINSIC(tex2DgatherR, 1, float4, sampler, float2, int2)
IMPLEMENT_INTRINSIC_GLSL(tex2DgatherR, 0, {
	code += "textureGather(" + id_to_name(args[0].base) + ", " +
		id_to_name(args[1].base) + ", 0)";
	})
IMPLEMENT_INTRINSIC_GLSL(tex2DgatherR, 1, {
	code += "textureGatherOffset(" + id_to_name(args[0].base) + ", " +
		id_to_name(args[1].base) + ", " +
		id_to_name(args[2].base) + ", 0)";
	})
IMPLEMENT_INTRINSIC_HLSL(tex2DgatherR, 0, {
	const std::string s = id_to_name(args[0].base);
	if (_shader_model >= 50u) {
		code += s + ".t.GatherRed(" + s + ".s, " + id_to_name(args[1].base) + ')';
	} else if (_shader_model >= 40u) { // Emulate texture gather intrinsic by sampling each location separately (SM41 has 'Gather', but that only works on single component texture formats)
		code += "float4(" +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(0, 1))." + 'r' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(1, 1))." + 'r' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(1, 0))." + 'r' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(0, 0))." + 'r' + ')';
	} else {
		code += "float4("
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(0, 1) * " + s + ".pixelsize, 0, 0))." + 'r' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(1, 1) * " + s + ".pixelsize, 0, 0))." + 'r' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(1, 0) * " + s + ".pixelsize, 0, 0))." + 'r' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(0, 0) * " + s + ".pixelsize, 0, 0))." + 'r' + ')';
	}
	})
IMPLEMENT_INTRINSIC_HLSL(tex2DgatherR, 1, {
	const std::string s = id_to_name(args[0].base);
	if (_shader_model >= 50u) {
		code += s + ".t.GatherRed(" + s + ".s, " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	} else if (_shader_model >= 40u) {
		code += "float4(" +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(0, 1))." + 'r' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(1, 1))." + 'r' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(1, 0))." + 'r' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(0, 0))." + 'r' + ')';
	} else {
		code += "float4("
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(0, 1)) * " + s + ".pixelsize, 0, 0))." + 'r' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(1, 1)) * " + s + ".pixelsize, 0, 0))." + 'r' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(1, 0)) * " + s + ".pixelsize, 0, 0))." + 'r' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(0, 0)) * " + s + ".pixelsize, 0, 0))." + 'r' + ')';
	}
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2DgatherR, 0, {
	const spv::Id component = emit_constant(0u);
	return add_instruction(spv::OpImageGather, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.add(component)
		.add(spv::ImageOperandsMaskNone)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2DgatherR, 1, {
	if (!args[2].is_constant)
		add_capability(spv::CapabilityImageGatherExtended);

	const spv::Id component = emit_constant(0u);
	return add_instruction(spv::OpImageGather, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.add(component)
		.add(args[2].is_constant ? spv::ImageOperandsConstOffsetMask : spv::ImageOperandsOffsetMask)
		.add(args[2].base)
		.result;
	})
// ret tex2DgatherG(s, coords)
// ret tex2DgatherG(s, coords, offset)
DEFINE_INTRINSIC(tex2DgatherG, 0, float4, sampler, float2)
DEFINE_INTRINSIC(tex2DgatherG, 1, float4, sampler, float2, int2)
IMPLEMENT_INTRINSIC_GLSL(tex2DgatherG, 0, {
	code += "textureGather(" + id_to_name(args[0].base) + ", " +
		id_to_name(args[1].base) + ", 1)";
	})
IMPLEMENT_INTRINSIC_GLSL(tex2DgatherG, 1, {
	code += "textureGatherOffset(" + id_to_name(args[0].base) + ", " +
		id_to_name(args[1].base) + ", " +
		id_to_name(args[2].base) + ", 1)";
	})
IMPLEMENT_INTRINSIC_HLSL(tex2DgatherG, 0, {
	const std::string s = id_to_name(args[0].base);
	if (_shader_model >= 50u) {
		code += s + ".t.GatherGreen(" + id_to_name(args[0].base) + ".s, " + id_to_name(args[1].base) + ')';
	} else if (_shader_model >= 40u) {
		code += "float4(" +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(0, 1))." + 'g' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(1, 1))." + 'g' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(1, 0))." + 'g' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(0, 0))." + 'g' + ')';
	} else {
		code += "float4("
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(0, 1) * " + s + ".pixelsize, 0, 0))." + 'g' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(1, 1) * " + s + ".pixelsize, 0, 0))." + 'g' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(1, 0) * " + s + ".pixelsize, 0, 0))." + 'g' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(0, 0) * " + s + ".pixelsize, 0, 0))." + 'g' + ')';
	}
	})
IMPLEMENT_INTRINSIC_HLSL(tex2DgatherG, 1, {
	const std::string s = id_to_name(args[0].base);
	if (_shader_model >= 50u) {
		code += s + ".t.GatherGreen(" + s + ".s, " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	} else if (_shader_model >= 40u) {
		code += "float4(" +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(0, 1))." + 'g' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(1, 1))." + 'g' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(1, 0))." + 'g' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(0, 0))." + 'g' + ')';
	} else {
		code += "float4("
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(0, 1)) * " + s + ".pixelsize, 0, 0))." + 'g' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(1, 1)) * " + s + ".pixelsize, 0, 0))." + 'g' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(1, 0)) * " + s + ".pixelsize, 0, 0))." + 'g' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(0, 0)) * " + s + ".pixelsize, 0, 0))." + 'g' + ')';
	}
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2DgatherG, 0, {
	const spv::Id component = emit_constant(1u);
	return add_instruction(spv::OpImageGather, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.add(component)
		.add(spv::ImageOperandsMaskNone)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2DgatherG, 1, {
	if (!args[2].is_constant)
		add_capability(spv::CapabilityImageGatherExtended);

	const spv::Id component = emit_constant(1u);
	return add_instruction(spv::OpImageGather, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.add(component)
		.add(args[2].is_constant ? spv::ImageOperandsConstOffsetMask : spv::ImageOperandsOffsetMask)
		.add(args[2].base)
		.result;
	})
// ret tex2DgatherB(s, coords)
// ret tex2DgatherB(s, coords, offset)
DEFINE_INTRINSIC(tex2DgatherB, 0, float4, sampler, float2)
DEFINE_INTRINSIC(tex2DgatherB, 1, float4, sampler, float2, int2)
IMPLEMENT_INTRINSIC_GLSL(tex2DgatherB, 0, {
	code += "textureGather(" + id_to_name(args[0].base) + ", " +
		id_to_name(args[1].base) + ", 2)";
	})
IMPLEMENT_INTRINSIC_GLSL(tex2DgatherB, 1, {
	code += "textureGatherOffset(" + id_to_name(args[0].base) + ", " +
		id_to_name(args[1].base) + ", " +
		id_to_name(args[2].base) + ", 2)";
	})
IMPLEMENT_INTRINSIC_HLSL(tex2DgatherB, 0, {
	const std::string s = id_to_name(args[0].base);
	if (_shader_model >= 50u) {
		code += s + ".t.GatherBlue(" + s + ".s, " + id_to_name(args[1].base) + ')';
	} else if (_shader_model >= 40u) {
		code += "float4(" +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(0, 1))." + 'b' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(1, 1))." + 'b' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(1, 0))." + 'b' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(0, 0))." + 'b' + ')';
	} else {
		code += "float4("
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(0, 1) * " + s + ".pixelsize, 0, 0))." + 'b' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(1, 1) * " + s + ".pixelsize, 0, 0))." + 'b' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(1, 0) * " + s + ".pixelsize, 0, 0))." + 'b' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(0, 0) * " + s + ".pixelsize, 0, 0))." + 'b' + ')';
	}
	})
IMPLEMENT_INTRINSIC_HLSL(tex2DgatherB, 1, {
	const std::string s = id_to_name(args[0].base);
	if (_shader_model >= 50u) {
		code += s + ".t.GatherBlue(" + s + ".s, " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	} else if (_shader_model >= 40u) {
		code += "float4(" +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(0, 1))." + 'b' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(1, 1))." + 'b' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(1, 0))." + 'b' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(0, 0))." + 'b' + ')';
	} else {
		code += "float4("
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(0, 1)) * " + s + ".pixelsize, 0, 0))." + 'b' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(1, 1)) * " + s + ".pixelsize, 0, 0))." + 'b' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(1, 0)) * " + s + ".pixelsize, 0, 0))." + 'b' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(0, 0)) * " + s + ".pixelsize, 0, 0))." + 'b' + ')';
	}
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2DgatherB, 0, {
	const spv::Id component = emit_constant(2u);
	return add_instruction(spv::OpImageGather, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.add(component)
		.add(spv::ImageOperandsMaskNone)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2DgatherB, 1, {
	if (!args[2].is_constant)
		add_capability(spv::CapabilityImageGatherExtended);

	const spv::Id component = emit_constant(2u);
	return add_instruction(spv::OpImageGather, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.add(component)
		.add(args[2].is_constant ? spv::ImageOperandsConstOffsetMask : spv::ImageOperandsOffsetMask)
		.add(args[2].base)
		.result;
	})
// ret tex2DgatherA(s, coords)
// ret tex2DgatherA(s, coords, offset)
DEFINE_INTRINSIC(tex2DgatherA, 0, float4, sampler, float2)
DEFINE_INTRINSIC(tex2DgatherA, 1, float4, sampler, float2, int2)
IMPLEMENT_INTRINSIC_GLSL(tex2DgatherA, 0, {
	code += "textureGather(" + id_to_name(args[0].base) + ", " +
		id_to_name(args[1].base) + ", 3)";
	})
IMPLEMENT_INTRINSIC_GLSL(tex2DgatherA, 1, {
	code += "textureGatherOffset(" + id_to_name(args[0].base) + ", " +
		id_to_name(args[1].base) + ", " +
		id_to_name(args[2].base) + ", 3)";
	})
IMPLEMENT_INTRINSIC_HLSL(tex2DgatherA, 0, {
	const std::string s = id_to_name(args[0].base);
	if (_shader_model >= 50u) {
		code += s + ".t.GatherAlpha(" + s + ".s, " + id_to_name(args[1].base) + ')';
	} else if (_shader_model >= 40u) {
		code += "float4(" +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(0, 1))." + 'a' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(1, 1))." + 'a' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(1, 0))." + 'a' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, int2(0, 0))." + 'a' + ')';
	} else {
		code += "float4("
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(0, 1) * " + s + ".pixelsize, 0, 0))." + 'a' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(1, 1) * " + s + ".pixelsize, 0, 0))." + 'a' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(1, 0) * " + s + ".pixelsize, 0, 0))." + 'a' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + float2(0, 0) * " + s + ".pixelsize, 0, 0))." + 'a' + ')';
	}
	})
IMPLEMENT_INTRINSIC_HLSL(tex2DgatherA, 1, {
	const std::string s = id_to_name(args[0].base);
	if (_shader_model >= 50u) {
		code += s + ".t.GatherAlpha(" + s + ".s, " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	} else if (_shader_model >= 40u) {
		code += "float4(" +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(0, 1))." + 'a' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(1, 1))." + 'a' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(1, 0))." + 'a' + ", " +
			s + ".t.SampleLevel(" + s + ".s, " + id_to_name(args[1].base) + ", 0, " + id_to_name(args[2].base) + " + int2(0, 0))." + 'a' + ')';
	} else {
		code += "float4("
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(0, 1)) * " + s + ".pixelsize, 0, 0))." + 'a' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(1, 1)) * " + s + ".pixelsize, 0, 0))." + 'a' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(1, 0)) * " + s + ".pixelsize, 0, 0))." + 'a' + ", "
			"tex2Dlod(" + s + ".s, float4(" + id_to_name(args[1].base) + " + (" + id_to_name(args[2].base) + " + float2(0, 0)) * " + s + ".pixelsize, 0, 0))." + 'a' + ')';
	}
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2DgatherA, 0, {
	const spv::Id component = emit_constant(3u);
	return add_instruction(spv::OpImageGather, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.add(component)
		.add(spv::ImageOperandsMaskNone)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2DgatherA, 1, {
	if (!args[2].is_constant)
		add_capability(spv::CapabilityImageGatherExtended);

	const spv::Id component = emit_constant(3u);
	return add_instruction(spv::OpImageGather, convert_type(res_type))
		.add(args[0].base)
		.add(args[1].base)
		.add(component)
		.add(args[2].is_constant ? spv::ImageOperandsConstOffsetMask : spv::ImageOperandsOffsetMask)
		.add(args[2].base)
		.result;
	})

// tex2Dstore(s, coords, value)
DEFINE_INTRINSIC(tex2Dstore, 0, void, storage, int2, float4)
IMPLEMENT_INTRINSIC_GLSL(tex2Dstore, 0, {
	code += "imageStore(" + id_to_name(args[0].base) + ", " +
		id_to_name(args[1].base) + ", " +
		id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(tex2Dstore, 0, {
	if (_shader_model >= 50) {
		code += id_to_name(args[0].base) + '[' + id_to_name(args[1].base) + "] = " + id_to_name(args[2].base);
	}
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2Dstore, 0, {
	add_instruction_without_result(spv::OpImageWrite)
		.add(args[0].base)
		.add(args[1].base)
		.add(args[2].base);
	return 0;
	})

// ret tex2Dsize(s)
// ret tex2Dsize(s, lod)
DEFINE_INTRINSIC(tex2Dsize, 0, int2, sampler)
DEFINE_INTRINSIC(tex2Dsize, 1, int2, sampler, int)
DEFINE_INTRINSIC(tex2Dsize, 2, int2, storage)
IMPLEMENT_INTRINSIC_GLSL(tex2Dsize, 0, {
	code += "textureSize(" + id_to_name(args[0].base) + ", 0)";
	})
IMPLEMENT_INTRINSIC_GLSL(tex2Dsize, 1, {
	code += "textureSize(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_GLSL(tex2Dsize, 2, {
	code += "imageSize(" + id_to_name(args[0].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(tex2Dsize, 0, {
	if (_shader_model >= 40u)
		code += id_to_name(args[0].base) + ".t.GetDimensions(" + id_to_name(res) + ".x, " + id_to_name(res) + ".y)";
	else
		code += "int2(1.0 / " + id_to_name(args[0].base) + ".pixelsize)";
	})
IMPLEMENT_INTRINSIC_HLSL(tex2Dsize, 1, {
	if (_shader_model >= 40u)
		code += "uint temp" + std::to_string(res) + "; " + // Don't need the number of levels out value, so route that to a dummy variable
			id_to_name(args[0].base) + ".t.GetDimensions(" + id_to_name(args[1].base) + ", " + id_to_name(res) + ".x, " + id_to_name(res) + ".y, temp" + std::to_string(res) + ')';
	else
		code += "int2(1.0 / " + id_to_name(args[0].base) + ".pixelsize) / exp2(" + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(tex2Dsize, 2, {
	if (_shader_model >= 50u)
		code += id_to_name(args[0].base) + ".GetDimensions(" + id_to_name(res) + ".x, " + id_to_name(res) + ".y)";
	else
		code += "int2(0, 0)"; // Only supported on SM5+
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2Dsize, 0, {
	add_capability(spv::CapabilityImageQuery);

	const spv::Id image = add_instruction(spv::OpImage, convert_type({ type::t_texture }))
		.add(args[0].base).result;
	const spv::Id level = emit_constant(0u);

	return add_instruction(spv::OpImageQuerySizeLod, convert_type(res_type))
		.add(image)
		.add(level)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2Dsize, 1, {
	add_capability(spv::CapabilityImageQuery);

	const spv::Id image = add_instruction(spv::OpImage, convert_type({ type::t_texture }))
		.add(args[0].base).result;

	return add_instruction(spv::OpImageQuerySizeLod, convert_type(res_type))
		.add(image)
		.add(args[1].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(tex2Dsize, 2, {
	add_capability(spv::CapabilityImageQuery);

	return add_instruction(spv::OpImageQuerySize, convert_type(res_type))
		.add(args[0].base)
		.result;
	})

// barrier()
DEFINE_INTRINSIC(barrier, 0, void)
IMPLEMENT_INTRINSIC_GLSL(barrier, 0, {
	code += "barrier()";
	})
IMPLEMENT_INTRINSIC_HLSL(barrier, 0, {
	if (_shader_model >= 50) {
		code += "GroupMemoryBarrierWithGroupSync()";
	}
	})
IMPLEMENT_INTRINSIC_SPIRV(barrier, 0, {
	const spv::Id mem_scope = emit_constant(spv::ScopeWorkgroup);
	const spv::Id mem_semantics = emit_constant(spv::MemorySemanticsWorkgroupMemoryMask | spv::MemorySemanticsAcquireReleaseMask);

	add_instruction_without_result(spv::OpControlBarrier)
		.add(mem_scope) // Execution scope
		.add(mem_scope)
		.add(mem_semantics);
	return 0;
	})

// memoryBarrier()
DEFINE_INTRINSIC(memoryBarrier, 0, void)
IMPLEMENT_INTRINSIC_GLSL(memoryBarrier, 0, {
	code += "memoryBarrier()";
	})
IMPLEMENT_INTRINSIC_HLSL(memoryBarrier, 0, {
	if (_shader_model >= 50) {
		code += "AllMemoryBarrier()";
	}
	})
IMPLEMENT_INTRINSIC_SPIRV(memoryBarrier, 0, {
	const spv::Id mem_scope = emit_constant(spv::ScopeDevice);
	const spv::Id mem_semantics = emit_constant(spv::MemorySemanticsImageMemoryMask | spv::MemorySemanticsUniformMemoryMask | spv::MemorySemanticsWorkgroupMemoryMask | spv::MemorySemanticsAcquireReleaseMask);

	add_instruction_without_result(spv::OpMemoryBarrier)
		.add(mem_scope)
		.add(mem_semantics);
	return 0;
	})
// groupMemoryBarrier()
DEFINE_INTRINSIC(groupMemoryBarrier, 0, void)
IMPLEMENT_INTRINSIC_GLSL(groupMemoryBarrier, 0, {
	code += "groupMemoryBarrier()";
	})
IMPLEMENT_INTRINSIC_HLSL(groupMemoryBarrier, 0, {
	if (_shader_model >= 50) {
		code += "GroupMemoryBarrier()";
	}
	})
IMPLEMENT_INTRINSIC_SPIRV(groupMemoryBarrier, 0, {
	const spv::Id mem_scope = emit_constant(spv::ScopeWorkgroup);
	const spv::Id mem_semantics = emit_constant(spv::MemorySemanticsWorkgroupMemoryMask | spv::MemorySemanticsAcquireReleaseMask);

	add_instruction_without_result(spv::OpMemoryBarrier)
		.add(mem_scope)
		.add(mem_semantics);
	return 0;
	})

// ret atomicAdd(inout mem, data)
DEFINE_INTRINSIC(atomicAdd, 0, int, inout_int, int)
DEFINE_INTRINSIC(atomicAdd, 0, uint, inout_uint, uint)
IMPLEMENT_INTRINSIC_GLSL(atomicAdd, 0, {
	code += "atomicAdd(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(atomicAdd, 0, {
	code += "InterlockedAdd(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(res) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(atomicAdd, 0, {
	const spv::Id mem_scope = emit_constant(spv::ScopeDevice);
	const spv::Id mem_semantics = emit_constant(spv::MemorySemanticsMaskNone);

	return add_instruction(spv::OpAtomicIAdd, convert_type(res_type))
		.add(args[0].base)
		.add(mem_scope)
		.add(mem_semantics)
		.add(args[1].base)
		.result;
	})

// ret atomicAnd(inout mem, data)
DEFINE_INTRINSIC(atomicAnd, 0, int, inout_int, int)
DEFINE_INTRINSIC(atomicAnd, 0, uint, inout_uint, uint)
IMPLEMENT_INTRINSIC_GLSL(atomicAnd, 0, {
	code += "atomicAnd(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(atomicAnd, 0, {
	code += "InterlockedAnd(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(res) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(atomicAnd, 0, {
	const spv::Id mem_scope = emit_constant(spv::ScopeDevice);
	const spv::Id mem_semantics = emit_constant(spv::MemorySemanticsMaskNone);

	return add_instruction(spv::OpAtomicAnd, convert_type(res_type))
		.add(args[0].base)
		.add(mem_scope)
		.add(mem_semantics)
		.add(args[1].base)
		.result;
	})

// ret atomicOr(inout mem, data)
DEFINE_INTRINSIC(atomicOr, 0, int, inout_int, int)
DEFINE_INTRINSIC(atomicOr, 0, uint, inout_uint, uint)
IMPLEMENT_INTRINSIC_GLSL(atomicOr, 0, {
	code += "atomicOr(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(atomicOr, 0, {
	code += "InterlockedOr(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(res) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(atomicOr, 0, {
	const spv::Id mem_scope = emit_constant(spv::ScopeDevice);
	const spv::Id mem_semantics = emit_constant(spv::MemorySemanticsMaskNone);

	return add_instruction(spv::OpAtomicOr, convert_type(res_type))
		.add(args[0].base)
		.add(mem_scope)
		.add(mem_semantics)
		.add(args[1].base)
		.result;
	})

// ret atomicXor(inout mem, data)
DEFINE_INTRINSIC(atomicXor, 0, int, inout_int, int)
DEFINE_INTRINSIC(atomicXor, 0, uint, inout_uint, uint)
IMPLEMENT_INTRINSIC_GLSL(atomicXor, 0, {
	code += "atomicXor(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(atomicXor, 0, {
	code += "InterlockedXor(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(res) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(atomicXor, 0, {
	const spv::Id mem_scope = emit_constant(spv::ScopeDevice);
	const spv::Id mem_semantics = emit_constant(spv::MemorySemanticsMaskNone);

	return add_instruction(spv::OpAtomicXor, convert_type(res_type))
		.add(args[0].base)
		.add(mem_scope)
		.add(mem_semantics)
		.add(args[1].base)
		.result;
	})

// ret atomicMin(inout mem, data)
DEFINE_INTRINSIC(atomicMin, 0, int, inout_int, int)
DEFINE_INTRINSIC(atomicMin, 1, uint, inout_uint, uint)
IMPLEMENT_INTRINSIC_GLSL(atomicMin, 0, {
	code += "atomicMin(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_GLSL(atomicMin, 1, {
	code += "atomicMin(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(atomicMin, 0, {
	code += "InterlockedMin(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(res) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(atomicMin, 1, {
	code += "InterlockedMin(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(res) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(atomicMin, 0, {
	const spv::Id mem_scope = emit_constant(spv::ScopeDevice);
	const spv::Id mem_semantics = emit_constant(spv::MemorySemanticsMaskNone);

	return add_instruction(spv::OpAtomicSMin, convert_type(res_type))
		.add(args[0].base)
		.add(mem_scope)
		.add(mem_semantics)
		.add(args[1].base)
		.result;
	})
IMPLEMENT_INTRINSIC_SPIRV(atomicMin, 1, {
	const spv::Id mem_scope = emit_constant(spv::ScopeDevice);
	const spv::Id mem_semantics = emit_constant(spv::MemorySemanticsMaskNone);

	return add_instruction(spv::OpAtomicUMin, convert_type(res_type))
		.add(args[0].base)
		.add(mem_scope)
		.add(mem_semantics)
		.add(args[1].base)
		.result;
	})

// ret atomicMax(inout mem, data)
DEFINE_INTRINSIC(atomicMax, 0, int, inout_int, int)
DEFINE_INTRINSIC(atomicMax, 1, uint, inout_uint, uint)
IMPLEMENT_INTRINSIC_GLSL(atomicMax, 0, {
	code += "atomicMax(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(atomicMax, 0, {
	code += "InterlockedMax(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(res) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(atomicMax, 0, {
	const spv::Id mem_scope = emit_constant(spv::ScopeDevice);
	const spv::Id mem_semantics = emit_constant(spv::MemorySemanticsMaskNone);

	return add_instruction(spv::OpAtomicSMax, convert_type(res_type))
		.add(args[0].base)
		.add(mem_scope)
		.add(mem_semantics)
		.add(args[1].base)
		.result;
	})
IMPLEMENT_INTRINSIC_GLSL(atomicMax, 1, {
	code += "atomicMax(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(atomicMax, 1, {
	code += "InterlockedMax(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(res) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(atomicMax, 1, {
	const spv::Id mem_scope = emit_constant(spv::ScopeDevice);
	const spv::Id mem_semantics = emit_constant(spv::MemorySemanticsMaskNone);

	return add_instruction(spv::OpAtomicUMax, convert_type(res_type))
		.add(args[0].base)
		.add(mem_scope)
		.add(mem_semantics)
		.add(args[1].base)
		.result;
	})

// ret atomicExchange(inout mem, data)
DEFINE_INTRINSIC(atomicExchange, 0, int, inout_int, int)
DEFINE_INTRINSIC(atomicExchange, 0, uint, inout_uint, uint)
IMPLEMENT_INTRINSIC_GLSL(atomicExchange, 0, {
	code += "atomicExchange(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(atomicExchange, 0, {
	code += "InterlockedExchange(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(res) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(atomicExchange, 0, {
	const spv::Id mem_scope = emit_constant(spv::ScopeDevice);
	const spv::Id mem_semantics = emit_constant(spv::MemorySemanticsMaskNone);

	return add_instruction(spv::OpAtomicExchange, convert_type(res_type))
		.add(args[0].base)
		.add(mem_scope)
		.add(mem_semantics)
		.add(args[1].base)
		.result;
	})

// ret atomicCompareExchange(inout mem, compare, data)
DEFINE_INTRINSIC(atomicCompareExchange, 0, int, inout_int, int, int)
DEFINE_INTRINSIC(atomicCompareExchange, 0, uint, inout_uint, uint, uint)
IMPLEMENT_INTRINSIC_GLSL(atomicCompareExchange, 0, {
	code += "atomicCompSwap(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ')';
	})
IMPLEMENT_INTRINSIC_HLSL(atomicCompareExchange, 0, {
	code += "InterlockedCompareExchange(" + id_to_name(args[0].base) + ", " + id_to_name(args[1].base) + ", " + id_to_name(args[2].base) + ", " + id_to_name(res) + ')';
	})
IMPLEMENT_INTRINSIC_SPIRV(atomicCompareExchange, 0, {
	const spv::Id mem_scope = emit_constant(spv::ScopeDevice);
	const spv::Id mem_semantics = emit_constant(spv::MemorySemanticsMaskNone);

	return add_instruction(spv::OpAtomicCompareExchange, convert_type(res_type))
		.add(args[0].base)
		.add(mem_scope)
		.add(mem_semantics)
		.add(mem_semantics)
		.add(args[2].base)
		.add(args[1].base)
		.result;
	})

#undef DEFINE_INTRINSIC
#undef IMPLEMENT_INTRINSIC_GLSL
#undef IMPLEMENT_INTRINSIC_HLSL
#undef IMPLEMENT_INTRINSIC_SPIRV
