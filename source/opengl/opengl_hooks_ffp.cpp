/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "opengl_impl_device.hpp"
#include "opengl_impl_device_context.hpp"
#include "opengl_impl_type_convert.hpp"
#include "opengl_hooks.hpp" // Fix name clashes with gl3w
#include "hook_manager.hpp"
#include "addon_manager.hpp"

extern thread_local reshade::opengl::device_context_impl *g_current_context;

// Fixed function pipeline hooks

extern "C" void APIENTRY glAccum(GLenum op, GLfloat value)
{
	static const auto trampoline = reshade::hooks::call(glAccum);
	trampoline(op, value);
}

extern "C" auto APIENTRY glAreTexturesResident(GLsizei n, const GLuint *textures, GLboolean *residences) -> GLboolean
{
	static const auto trampoline = reshade::hooks::call(glAreTexturesResident);
	return trampoline(n, textures, residences);
}

extern "C" void APIENTRY glArrayElement(GLint i)
{
	static const auto trampoline = reshade::hooks::call(glArrayElement);
	trampoline(i);
}

extern "C" void APIENTRY glBegin(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glBegin);
	trampoline(mode);

	if (g_current_context != nullptr)
	{
		g_current_context->_current_prim_mode = mode;

		assert(g_current_context->_current_vertex_count == 0);

#if RESHADE_ADDON >= 2
		const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
		uint32_t value = static_cast<uint32_t>(reshade::opengl::convert_primitive_topology(mode));

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, &state, &value);
#endif
	}
}

extern "C" void APIENTRY glBitmap(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap)
{
	static const auto trampoline = reshade::hooks::call(glBitmap);
	trampoline(width, height, xorig, yorig, xmove, ymove, bitmap);
}

extern "C" void APIENTRY glCallList(GLuint list)
{
	static const auto trampoline = reshade::hooks::call(glCallList);
	trampoline(list);
}
extern "C" void APIENTRY glCallLists(GLsizei n, GLenum type, const GLvoid *lists)
{
	static const auto trampoline = reshade::hooks::call(glCallLists);
	trampoline(n, type, lists);
}

extern "C" void APIENTRY glClearAccum(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	static const auto trampoline = reshade::hooks::call(glClearAccum);
	trampoline(red, green, blue, alpha);
}
extern "C" void APIENTRY glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	static const auto trampoline = reshade::hooks::call(glClearColor);
	trampoline(red, green, blue, alpha);
}
extern "C" void APIENTRY glClearDepth(GLclampd depth)
{
	static const auto trampoline = reshade::hooks::call(glClearDepth);
	trampoline(depth);
}
extern "C" void APIENTRY glClearIndex(GLfloat c)
{
	static const auto trampoline = reshade::hooks::call(glClearIndex);
	trampoline(c);
}
extern "C" void APIENTRY glClearStencil(GLint s)
{
	static const auto trampoline = reshade::hooks::call(glClearStencil);
	trampoline(s);
}

extern "C" void APIENTRY glClipPlane(GLenum plane, const GLdouble *equation)
{
	static const auto trampoline = reshade::hooks::call(glClipPlane);
	trampoline(plane, equation);
}

extern "C" void APIENTRY glColor3b(GLbyte red, GLbyte green, GLbyte blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3b);
	trampoline(red, green, blue);
}
extern "C" void APIENTRY glColor3bv(const GLbyte *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3bv);
	trampoline(v);
}
extern "C" void APIENTRY glColor3d(GLdouble red, GLdouble green, GLdouble blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3d);
	trampoline(red, green, blue);
}
extern "C" void APIENTRY glColor3dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3dv);
	trampoline(v);
}
extern "C" void APIENTRY glColor3f(GLfloat red, GLfloat green, GLfloat blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3f);
	trampoline(red, green, blue);
}
extern "C" void APIENTRY glColor3fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3fv);
	trampoline(v);
}
extern "C" void APIENTRY glColor3i(GLint red, GLint green, GLint blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3i);
	trampoline(red, green, blue);
}
extern "C" void APIENTRY glColor3iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3iv);
	trampoline(v);
}
extern "C" void APIENTRY glColor3s(GLshort red, GLshort green, GLshort blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3s);
	trampoline(red, green, blue);
}
extern "C" void APIENTRY glColor3sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3sv);
	trampoline(v);
}
extern "C" void APIENTRY glColor3ub(GLubyte red, GLubyte green, GLubyte blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3ub);
	trampoline(red, green, blue);
}
extern "C" void APIENTRY glColor3ubv(const GLubyte *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3ubv);
	trampoline(v);
}
extern "C" void APIENTRY glColor3ui(GLuint red, GLuint green, GLuint blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3ui);
	trampoline(red, green, blue);
}
extern "C" void APIENTRY glColor3uiv(const GLuint *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3uiv);
	trampoline(v);
}
extern "C" void APIENTRY glColor3us(GLushort red, GLushort green, GLushort blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3us);
	trampoline(red, green, blue);
}
extern "C" void APIENTRY glColor3usv(const GLushort *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3usv);
	trampoline(v);
}
extern "C" void APIENTRY glColor4b(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4b);
	trampoline(red, green, blue, alpha);
}
extern "C" void APIENTRY glColor4bv(const GLbyte *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4bv);
	trampoline(v);
}
extern "C" void APIENTRY glColor4d(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4d);
	trampoline(red, green, blue, alpha);
}
extern "C" void APIENTRY glColor4dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4dv);
	trampoline(v);
}
extern "C" void APIENTRY glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4f);
	trampoline(red, green, blue, alpha);
}
extern "C" void APIENTRY glColor4fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4fv);
	trampoline(v);
}
extern "C" void APIENTRY glColor4i(GLint red, GLint green, GLint blue, GLint alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4i);
	trampoline(red, green, blue, alpha);
}
extern "C" void APIENTRY glColor4iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4iv);
	trampoline(v);
}
extern "C" void APIENTRY glColor4s(GLshort red, GLshort green, GLshort blue, GLshort alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4s);
	trampoline(red, green, blue, alpha);
}
extern "C" void APIENTRY glColor4sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4sv);
	trampoline(v);
}
extern "C" void APIENTRY glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4ub);
	trampoline(red, green, blue, alpha);
}
extern "C" void APIENTRY glColor4ubv(const GLubyte *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4ubv);
	trampoline(v);
}
extern "C" void APIENTRY glColor4ui(GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4ui);
	trampoline(red, green, blue, alpha);
}
extern "C" void APIENTRY glColor4uiv(const GLuint *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4uiv);
	trampoline(v);
}
extern "C" void APIENTRY glColor4us(GLushort red, GLushort green, GLushort blue, GLushort alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4us);
	trampoline(red, green, blue, alpha);
}
extern "C" void APIENTRY glColor4usv(const GLushort *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4usv);
	trampoline(v);
}

extern "C" void APIENTRY glColorMaterial(GLenum face, GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glColorMaterial);
	trampoline(face, mode);
}

extern "C" void APIENTRY glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(glColorPointer);
	trampoline(size, type, stride, pointer);
}

extern "C" void APIENTRY glDeleteLists(GLuint list, GLsizei range)
{
	static const auto trampoline = reshade::hooks::call(glDeleteLists);
	trampoline(list, range);
}

extern "C" void APIENTRY glDisableClientState(GLenum array)
{
	static const auto trampoline = reshade::hooks::call(glDisableClientState);
	trampoline(array);
}

extern "C" void APIENTRY glDrawBuffer(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glDrawBuffer);
	trampoline(mode);
}

extern "C" void APIENTRY glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(glDrawPixels);
	trampoline(width, height, format, type, pixels);
}

extern "C" void APIENTRY glEdgeFlag(GLboolean flag)
{
	static const auto trampoline = reshade::hooks::call(glEdgeFlag);
	trampoline(flag);
}
extern "C" void APIENTRY glEdgeFlagPointer(GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(glEdgeFlagPointer);
	trampoline(stride, pointer);
}
extern "C" void APIENTRY glEdgeFlagv(const GLboolean *flag)
{
	static const auto trampoline = reshade::hooks::call(glEdgeFlagv);
	trampoline(flag);
}

extern "C" void APIENTRY glEnableClientState(GLenum array)
{
	static const auto trampoline = reshade::hooks::call(glEnableClientState);
	trampoline(array);
}

extern "C" void APIENTRY glEnd()
{
	static const auto trampoline = reshade::hooks::call(glEnd);
	trampoline();

	if (g_current_context)
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::draw>(g_current_context, g_current_context->_current_vertex_count, 1, 0, 0); // Cannot be skipped
#endif
		g_current_context->_current_vertex_count = 0;
	}
}

extern "C" void APIENTRY glEndList()
{
	static const auto trampoline = reshade::hooks::call(glEndList);
	trampoline();
}

extern "C" void APIENTRY glEvalCoord1d(GLdouble u)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord1d);
	trampoline(u);
}
extern "C" void APIENTRY glEvalCoord1dv(const GLdouble *u)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord1dv);
	trampoline(u);
}
extern "C" void APIENTRY glEvalCoord1f(GLfloat u)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord1f);
	trampoline(u);
}
extern "C" void APIENTRY glEvalCoord1fv(const GLfloat *u)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord1fv);
	trampoline(u);
}
extern "C" void APIENTRY glEvalCoord2d(GLdouble u, GLdouble v)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord2d);
	trampoline(u, v);
}
extern "C" void APIENTRY glEvalCoord2dv(const GLdouble *u)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord2dv);
	trampoline(u);
}
extern "C" void APIENTRY glEvalCoord2f(GLfloat u, GLfloat v)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord2f);
	trampoline(u, v);
}
extern "C" void APIENTRY glEvalCoord2fv(const GLfloat *u)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord2fv);
	trampoline(u);
}

extern "C" void APIENTRY glEvalMesh1(GLenum mode, GLint i1, GLint i2)
{
	static const auto trampoline = reshade::hooks::call(glEvalMesh1);
	trampoline(mode, i1, i2);
}
extern "C" void APIENTRY glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
	static const auto trampoline = reshade::hooks::call(glEvalMesh2);
	trampoline(mode, i1, i2, j1, j2);
}

extern "C" void APIENTRY glEvalPoint1(GLint i)
{
	static const auto trampoline = reshade::hooks::call(glEvalPoint1);
	trampoline(i);
}
extern "C" void APIENTRY glEvalPoint2(GLint i, GLint j)
{
	static const auto trampoline = reshade::hooks::call(glEvalPoint2);
	trampoline(i, j);
}

extern "C" void APIENTRY glFeedbackBuffer(GLsizei size, GLenum type, GLfloat *buffer)
{
	static const auto trampoline = reshade::hooks::call(glFeedbackBuffer);
	trampoline(size, type, buffer);
}

extern "C" void APIENTRY glFinish()
{
	static const auto trampoline = reshade::hooks::call(glFinish);
	trampoline();
}
extern "C" void APIENTRY glFlush()
{
#if RESHADE_ADDON
	if (g_current_context)
		reshade::invoke_addon_event<reshade::addon_event::execute_command_list>(g_current_context, g_current_context);
#endif

	static const auto trampoline = reshade::hooks::call(glFlush);
	trampoline();
}

extern "C" void APIENTRY glFogf(GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glFogf);
	trampoline(pname, param);
}
extern "C" void APIENTRY glFogfv(GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glFogfv);
	trampoline(pname, params);
}
extern "C" void APIENTRY glFogi(GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glFogi);
	trampoline(pname, param);
}
extern "C" void APIENTRY glFogiv(GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glFogiv);
	trampoline(pname, params);
}

extern "C" void APIENTRY glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	static const auto trampoline = reshade::hooks::call(glFrustum);
	trampoline(left, right, bottom, top, zNear, zFar);
}

extern "C" auto APIENTRY glGenLists(GLsizei range) -> GLuint
{
	static const auto trampoline = reshade::hooks::call(glGenLists);
	return trampoline(range);
}

extern "C" void APIENTRY glGenTextures(GLsizei n, GLuint *textures)
{
	static const auto trampoline = reshade::hooks::call(glGenTextures);
	trampoline(n, textures);
}

extern "C" void APIENTRY glGetBooleanv(GLenum pname, GLboolean *params)
{
	static const auto trampoline = reshade::hooks::call(glGetBooleanv);
	trampoline(pname, params);
}
extern "C" void APIENTRY glGetDoublev(GLenum pname, GLdouble *params)
{
	static const auto trampoline = reshade::hooks::call(glGetDoublev);
	trampoline(pname, params);
}
extern "C" void APIENTRY glGetFloatv(GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glGetFloatv);
	trampoline(pname, params);
}
extern "C" void APIENTRY glGetIntegerv(GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glGetIntegerv);
	trampoline(pname, params);
}

extern "C" void APIENTRY glGetClipPlane(GLenum plane, GLdouble *equation)
{
	static const auto trampoline = reshade::hooks::call(glGetClipPlane);
	trampoline(plane, equation);
}

extern "C" auto APIENTRY glGetError() -> GLenum
{
	static const auto trampoline = reshade::hooks::call(glGetError);
	return trampoline();
}

extern "C" void APIENTRY glGetLightfv(GLenum light, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glGetLightfv);
	trampoline(light, pname, params);
}
extern "C" void APIENTRY glGetLightiv(GLenum light, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glGetLightiv);
	trampoline(light, pname, params);
}

extern "C" void APIENTRY glGetMapdv(GLenum target, GLenum query, GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glGetMapdv);
	trampoline(target, query, v);
}
extern "C" void APIENTRY glGetMapfv(GLenum target, GLenum query, GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glGetMapfv);
	trampoline(target, query, v);
}
extern "C" void APIENTRY glGetMapiv(GLenum target, GLenum query, GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glGetMapiv);
	trampoline(target, query, v);
}

extern "C" void APIENTRY glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glGetMaterialfv);
	trampoline(face, pname, params);
}
extern "C" void APIENTRY glGetMaterialiv(GLenum face, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glGetMaterialiv);
	trampoline(face, pname, params);
}

extern "C" void APIENTRY glGetPixelMapfv(GLenum map, GLfloat *values)
{
	static const auto trampoline = reshade::hooks::call(glGetPixelMapfv);
	trampoline(map, values);
}
extern "C" void APIENTRY glGetPixelMapuiv(GLenum map, GLuint *values)
{
	static const auto trampoline = reshade::hooks::call(glGetPixelMapuiv);
	trampoline(map, values);
}
extern "C" void APIENTRY glGetPixelMapusv(GLenum map, GLushort *values)
{
	static const auto trampoline = reshade::hooks::call(glGetPixelMapusv);
	trampoline(map, values);
}

extern "C" void APIENTRY glGetPointerv(GLenum pname, GLvoid **params)
{
	static const auto trampoline = reshade::hooks::call(glGetPointerv);
	trampoline(pname, params);
}

extern "C" void APIENTRY glGetPolygonStipple(GLubyte *mask)
{
	static const auto trampoline = reshade::hooks::call(glGetPolygonStipple);
	trampoline(mask);
}

extern "C" auto APIENTRY glGetString(GLenum name) -> const GLubyte *
{
	static const auto trampoline = reshade::hooks::call(glGetString);
	return trampoline(name);
}

extern "C" void APIENTRY glGetTexEnvfv(GLenum target, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexEnvfv);
	trampoline(target, pname, params);
}
extern "C" void APIENTRY glGetTexEnviv(GLenum target, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexEnviv);
	trampoline(target, pname, params);
}

extern "C" void APIENTRY glGetTexGendv(GLenum coord, GLenum pname, GLdouble *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexGendv);
	trampoline(coord, pname, params);
}
extern "C" void APIENTRY glGetTexGenfv(GLenum coord, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexGenfv);
	trampoline(coord, pname, params);
}
extern "C" void APIENTRY glGetTexGeniv(GLenum coord, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexGeniv);
	trampoline(coord, pname, params);
}

extern "C" void APIENTRY glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(glGetTexImage);
	trampoline(target, level, format, type, pixels);
}

extern "C" void APIENTRY glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexLevelParameterfv);
	trampoline(target, level, pname, params);
}
extern "C" void APIENTRY glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexLevelParameteriv);
	trampoline(target, level, pname, params);
}
extern "C" void APIENTRY glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexParameterfv);
	trampoline(target, pname, params);
}
extern "C" void APIENTRY glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexParameteriv);
	trampoline(target, pname, params);
}

extern "C" void APIENTRY glIndexMask(GLuint mask)
{
	static const auto trampoline = reshade::hooks::call(glIndexMask);
	trampoline(mask);
}

extern "C" void APIENTRY glIndexPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(glIndexPointer);
	trampoline(type, stride, pointer);
}

extern "C" void APIENTRY glIndexd(GLdouble c)
{
	static const auto trampoline = reshade::hooks::call(glIndexd);
	trampoline(c);
}
extern "C" void APIENTRY glIndexdv(const GLdouble *c)
{
	static const auto trampoline = reshade::hooks::call(glIndexdv);
	trampoline(c);
}
extern "C" void APIENTRY glIndexf(GLfloat c)
{
	static const auto trampoline = reshade::hooks::call(glIndexf);
	trampoline(c);
}
extern "C" void APIENTRY glIndexfv(const GLfloat *c)
{
	static const auto trampoline = reshade::hooks::call(glIndexfv);
	trampoline(c);
}
extern "C" void APIENTRY glIndexi(GLint c)
{
	static const auto trampoline = reshade::hooks::call(glIndexi);
	trampoline(c);
}
extern "C" void APIENTRY glIndexiv(const GLint *c)
{
	static const auto trampoline = reshade::hooks::call(glIndexiv);
	trampoline(c);
}
extern "C" void APIENTRY glIndexs(GLshort c)
{
	static const auto trampoline = reshade::hooks::call(glIndexs);
	trampoline(c);
}
extern "C" void APIENTRY glIndexsv(const GLshort *c)
{
	static const auto trampoline = reshade::hooks::call(glIndexsv);
	trampoline(c);
}
extern "C" void APIENTRY glIndexub(GLubyte c)
{
	static const auto trampoline = reshade::hooks::call(glIndexub);
	trampoline(c);
}
extern "C" void APIENTRY glIndexubv(const GLubyte *c)
{
	static const auto trampoline = reshade::hooks::call(glIndexubv);
	trampoline(c);
}

extern "C" void APIENTRY glInitNames()
{
	static const auto trampoline = reshade::hooks::call(glInitNames);
	trampoline();
}

extern "C" void APIENTRY glInterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(glInterleavedArrays);
	trampoline(format, stride, pointer);
}

extern "C" auto APIENTRY glIsEnabled(GLenum cap) -> GLboolean
{
	static const auto trampoline = reshade::hooks::call(glIsEnabled);
	return trampoline(cap);
}

extern "C" auto APIENTRY glIsList(GLuint list) -> GLboolean
{
	static const auto trampoline = reshade::hooks::call(glIsList);
	return trampoline(list);
}

extern "C" auto APIENTRY glIsTexture(GLuint texture) -> GLboolean
{
	static const auto trampoline = reshade::hooks::call(glIsTexture);
	return trampoline(texture);
}

extern "C" void APIENTRY glLightModelf(GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glLightModelf);
	trampoline(pname, param);
}
extern "C" void APIENTRY glLightModelfv(GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glLightModelfv);
	trampoline(pname, params);
}
extern "C" void APIENTRY glLightModeli(GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glLightModeli);
	trampoline(pname, param);
}
extern "C" void APIENTRY glLightModeliv(GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glLightModeliv);
	trampoline(pname, params);
}

extern "C" void APIENTRY glLightf(GLenum light, GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glLightf);
	trampoline(light, pname, param);
}
extern "C" void APIENTRY glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glLightfv);
	trampoline(light, pname, params);
}
extern "C" void APIENTRY glLighti(GLenum light, GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glLighti);
	trampoline(light, pname, param);
}
extern "C" void APIENTRY glLightiv(GLenum light, GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glLightiv);
	trampoline(light, pname, params);
}

extern "C" void APIENTRY glLineStipple(GLint factor, GLushort pattern)
{
	static const auto trampoline = reshade::hooks::call(glLineStipple);
	trampoline(factor, pattern);
}

extern "C" void APIENTRY glListBase(GLuint base)
{
	static const auto trampoline = reshade::hooks::call(glListBase);
	trampoline(base);
}

extern "C" void APIENTRY glLoadIdentity()
{
	static const auto trampoline = reshade::hooks::call(glLoadIdentity);
	trampoline();
}
extern "C" void APIENTRY glLoadMatrixd(const GLdouble *m)
{
	static const auto trampoline = reshade::hooks::call(glLoadMatrixd);
	trampoline(m);
}
extern "C" void APIENTRY glLoadMatrixf(const GLfloat *m)
{
	static const auto trampoline = reshade::hooks::call(glLoadMatrixf);
	trampoline(m);
}

extern "C" void APIENTRY glLoadName(GLuint name)
{
	static const auto trampoline = reshade::hooks::call(glLoadName);
	trampoline(name);
}

extern "C" void APIENTRY glMap1d(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points)
{
	static const auto trampoline = reshade::hooks::call(glMap1d);
	trampoline(target, u1, u2, stride, order, points);
}
extern "C" void APIENTRY glMap1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points)
{
	static const auto trampoline = reshade::hooks::call(glMap1f);
	trampoline(target, u1, u2, stride, order, points);
}
extern "C" void APIENTRY glMap2d(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points)
{
	static const auto trampoline = reshade::hooks::call(glMap2d);
	trampoline(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}
extern "C" void APIENTRY glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points)
{
	static const auto trampoline = reshade::hooks::call(glMap2f);
	trampoline(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}

extern "C" void APIENTRY glMapGrid1d(GLint un, GLdouble u1, GLdouble u2)
{
	static const auto trampoline = reshade::hooks::call(glMapGrid1d);
	trampoline(un, u1, u2);
}
extern "C" void APIENTRY glMapGrid1f(GLint un, GLfloat u1, GLfloat u2)
{
	static const auto trampoline = reshade::hooks::call(glMapGrid1f);
	trampoline(un, u1, u2);
}
extern "C" void APIENTRY glMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2)
{
	static const auto trampoline = reshade::hooks::call(glMapGrid2d);
	trampoline(un, u1, u2, vn, v1, v2);
}
extern "C" void APIENTRY glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{
	static const auto trampoline = reshade::hooks::call(glMapGrid2f);
	trampoline(un, u1, u2, vn, v1, v2);
}

extern "C" void APIENTRY glMaterialf(GLenum face, GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glMaterialf);
	trampoline(face, pname, param);
}
extern "C" void APIENTRY glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glMaterialfv);
	trampoline(face, pname, params);
}
extern "C" void APIENTRY glMateriali(GLenum face, GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glMateriali);
	trampoline(face, pname, param);
}
extern "C" void APIENTRY glMaterialiv(GLenum face, GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glMaterialiv);
	trampoline(face, pname, params);
}

extern "C" void APIENTRY glMatrixMode(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glMatrixMode);
	trampoline(mode);
}

extern "C" void APIENTRY glMultMatrixd(const GLdouble *m)
{
	static const auto trampoline = reshade::hooks::call(glMultMatrixd);
	trampoline(m);
}
extern "C" void APIENTRY glMultMatrixf(const GLfloat *m)
{
	static const auto trampoline = reshade::hooks::call(glMultMatrixf);
	trampoline(m);
}

extern "C" void APIENTRY glNewList(GLuint list, GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glNewList);
	trampoline(list, mode);
}

extern "C" void APIENTRY glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz)
{
	static const auto trampoline = reshade::hooks::call(glNormal3b);
	trampoline(nx, ny, nz);
}
extern "C" void APIENTRY glNormal3bv(const GLbyte *v)
{
	static const auto trampoline = reshade::hooks::call(glNormal3bv);
	trampoline(v);
}
extern "C" void APIENTRY glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz)
{
	static const auto trampoline = reshade::hooks::call(glNormal3d);
	trampoline(nx, ny, nz);
}
extern "C" void APIENTRY glNormal3dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glNormal3dv);
	trampoline(v);
}
extern "C" void APIENTRY glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
	static const auto trampoline = reshade::hooks::call(glNormal3f);
	trampoline(nx, ny, nz);
}
extern "C" void APIENTRY glNormal3fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glNormal3fv);
	trampoline(v);
}
extern "C" void APIENTRY glNormal3i(GLint nx, GLint ny, GLint nz)
{
	static const auto trampoline = reshade::hooks::call(glNormal3i);
	trampoline(nx, ny, nz);
}
extern "C" void APIENTRY glNormal3iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glNormal3iv);
	trampoline(v);
}
extern "C" void APIENTRY glNormal3s(GLshort nx, GLshort ny, GLshort nz)
{
	static const auto trampoline = reshade::hooks::call(glNormal3s);
	trampoline(nx, ny, nz);
}
extern "C" void APIENTRY glNormal3sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glNormal3sv);
	trampoline(v);
}

extern "C" void APIENTRY glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(glNormalPointer);
	trampoline(type, stride, pointer);
}

extern "C" void APIENTRY glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	static const auto trampoline = reshade::hooks::call(glOrtho);
	trampoline(left, right, bottom, top, zNear, zFar);
}

extern "C" void APIENTRY glPassThrough(GLfloat token)
{
	static const auto trampoline = reshade::hooks::call(glPassThrough);
	trampoline(token);
}

extern "C" void APIENTRY glPixelMapfv(GLenum map, GLsizei mapsize, const GLfloat *values)
{
	static const auto trampoline = reshade::hooks::call(glPixelMapfv);
	trampoline(map, mapsize, values);
}
extern "C" void APIENTRY glPixelMapuiv(GLenum map, GLsizei mapsize, const GLuint *values)
{
	static const auto trampoline = reshade::hooks::call(glPixelMapuiv);
	trampoline(map, mapsize, values);
}
extern "C" void APIENTRY glPixelMapusv(GLenum map, GLsizei mapsize, const GLushort *values)
{
	static const auto trampoline = reshade::hooks::call(glPixelMapusv);
	trampoline(map, mapsize, values);
}

extern "C" void APIENTRY glPixelStoref(GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glPixelStoref);
	trampoline(pname, param);
}
extern "C" void APIENTRY glPixelStorei(GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glPixelStorei);
	trampoline(pname, param);
}

extern "C" void APIENTRY glPixelTransferf(GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glPixelTransferf);
	trampoline(pname, param);
}
extern "C" void APIENTRY glPixelTransferi(GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glPixelTransferi);
	trampoline(pname, param);
}

extern "C" void APIENTRY glPixelZoom(GLfloat xfactor, GLfloat yfactor)
{
	static const auto trampoline = reshade::hooks::call(glPixelZoom);
	trampoline(xfactor, yfactor);
}

extern "C" void APIENTRY glPolygonStipple(const GLubyte *mask)
{
	static const auto trampoline = reshade::hooks::call(glPolygonStipple);
	trampoline(mask);
}

extern "C" void APIENTRY glPopAttrib()
{
	static const auto trampoline = reshade::hooks::call(glPopAttrib);
	trampoline();
}
extern "C" void APIENTRY glPopClientAttrib()
{
	static const auto trampoline = reshade::hooks::call(glPopClientAttrib);
	trampoline();
}

extern "C" void APIENTRY glPopMatrix()
{
	static const auto trampoline = reshade::hooks::call(glPopMatrix);
	trampoline();
}

extern "C" void APIENTRY glPopName()
{
	static const auto trampoline = reshade::hooks::call(glPopName);
	trampoline();
}

extern "C" void APIENTRY glPrioritizeTextures(GLsizei n, const GLuint *textures, const GLclampf *priorities)
{
	static const auto trampoline = reshade::hooks::call(glPrioritizeTextures);
	trampoline(n, textures, priorities);
}

extern "C" void APIENTRY glPushAttrib(GLbitfield mask)
{
	static const auto trampoline = reshade::hooks::call(glPushAttrib);
	trampoline(mask);
}
extern "C" void APIENTRY glPushClientAttrib(GLbitfield mask)
{
	static const auto trampoline = reshade::hooks::call(glPushClientAttrib);
	trampoline(mask);
}

extern "C" void APIENTRY glPushMatrix()
{
	static const auto trampoline = reshade::hooks::call(glPushMatrix);
	trampoline();
}

extern "C" void APIENTRY glPushName(GLuint name)
{
	static const auto trampoline = reshade::hooks::call(glPushName);
	trampoline(name);
}

extern "C" void APIENTRY glRasterPos2d(GLdouble x, GLdouble y)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2d);
	trampoline(x, y);
}
extern "C" void APIENTRY glRasterPos2dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2dv);
	trampoline(v);
}
extern "C" void APIENTRY glRasterPos2f(GLfloat x, GLfloat y)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2f);
	trampoline(x, y);
}
extern "C" void APIENTRY glRasterPos2fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2fv);
	trampoline(v);
}
extern "C" void APIENTRY glRasterPos2i(GLint x, GLint y)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2i);
	trampoline(x, y);
}
extern "C" void APIENTRY glRasterPos2iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2iv);
	trampoline(v);
}
extern "C" void APIENTRY glRasterPos2s(GLshort x, GLshort y)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2s);
	trampoline(x, y);
}
extern "C" void APIENTRY glRasterPos2sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2sv);
	trampoline(v);
}
extern "C" void APIENTRY glRasterPos3d(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3d);
	trampoline(x, y, z);
}
extern "C" void APIENTRY glRasterPos3dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3dv);
	trampoline(v);
}
extern "C" void APIENTRY glRasterPos3f(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3f);
	trampoline(x, y, z);
}
extern "C" void APIENTRY glRasterPos3fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3fv);
	trampoline(v);
}
extern "C" void APIENTRY glRasterPos3i(GLint x, GLint y, GLint z)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3i);
	trampoline(x, y, z);
}
extern "C" void APIENTRY glRasterPos3iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3iv);
	trampoline(v);
}
extern "C" void APIENTRY glRasterPos3s(GLshort x, GLshort y, GLshort z)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3s);
	trampoline(x, y, z);
}
extern "C" void APIENTRY glRasterPos3sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3sv);
	trampoline(v);
}
extern "C" void APIENTRY glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4d);
	trampoline(x, y, z, w);
}
extern "C" void APIENTRY glRasterPos4dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4dv);
	trampoline(v);
}
extern "C" void APIENTRY glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4f);
	trampoline(x, y, z, w);
}
extern "C" void APIENTRY glRasterPos4fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4fv);
	trampoline(v);
}
extern "C" void APIENTRY glRasterPos4i(GLint x, GLint y, GLint z, GLint w)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4i);
	trampoline(x, y, z, w);
}
extern "C" void APIENTRY glRasterPos4iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4iv);
	trampoline(v);
}
extern "C" void APIENTRY glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4s);
	trampoline(x, y, z, w);
}
extern "C" void APIENTRY glRasterPos4sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4sv);
	trampoline(v);
}

extern "C" void APIENTRY glReadBuffer(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glReadBuffer);
	trampoline(mode);
}

extern "C" void APIENTRY glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(glReadPixels);
	trampoline(x, y, width, height, format, type, pixels);
}

extern "C" void APIENTRY glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
	static const auto trampoline = reshade::hooks::call(glRectd);
	trampoline(x1, y1, x2, y2);
}
extern "C" void APIENTRY glRectdv(const GLdouble *v1, const GLdouble *v2)
{
	static const auto trampoline = reshade::hooks::call(glRectdv);
	trampoline(v1, v2);
}
extern "C" void APIENTRY glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	static const auto trampoline = reshade::hooks::call(glRectf);
	trampoline(x1, y1, x2, y2);
}
extern "C" void APIENTRY glRectfv(const GLfloat *v1, const GLfloat *v2)
{
	static const auto trampoline = reshade::hooks::call(glRectfv);
	trampoline(v1, v2);
}
extern "C" void APIENTRY glRecti(GLint x1, GLint y1, GLint x2, GLint y2)
{
	static const auto trampoline = reshade::hooks::call(glRecti);
	trampoline(x1, y1, x2, y2);
}
extern "C" void APIENTRY glRectiv(const GLint *v1, const GLint *v2)
{
	static const auto trampoline = reshade::hooks::call(glRectiv);
	trampoline(v1, v2);
}

extern "C" void APIENTRY glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2)
{
	static const auto trampoline = reshade::hooks::call(glRects);
	trampoline(x1, y1, x2, y2);
}
extern "C" void APIENTRY glRectsv(const GLshort *v1, const GLshort *v2)
{
	static const auto trampoline = reshade::hooks::call(glRectsv);
	trampoline(v1, v2);
}

extern "C" auto APIENTRY glRenderMode(GLenum mode) -> GLint
{
	static const auto trampoline = reshade::hooks::call(glRenderMode);
	return trampoline(mode);
}

extern "C" void APIENTRY glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = reshade::hooks::call(glRotated);
	trampoline(angle, x, y, z);
}
extern "C" void APIENTRY glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = reshade::hooks::call(glRotatef);
	trampoline(angle, x, y, z);
}

extern "C" void APIENTRY glScaled(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = reshade::hooks::call(glScaled);
	trampoline(x, y, z);
}
extern "C" void APIENTRY glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = reshade::hooks::call(glScalef);
	trampoline(x, y, z);
}

extern "C" void APIENTRY glSelectBuffer(GLsizei size, GLuint *buffer)
{
	static const auto trampoline = reshade::hooks::call(glSelectBuffer);
	trampoline(size, buffer);
}

extern "C" void APIENTRY glShadeModel(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glShadeModel);
	trampoline(mode);
}

extern "C" void APIENTRY glTexCoord1d(GLdouble s)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1d);
	trampoline(s);
}
extern "C" void APIENTRY glTexCoord1dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1dv);
	trampoline(v);
}
extern "C" void APIENTRY glTexCoord1f(GLfloat s)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1f);
	trampoline(s);
}
extern "C" void APIENTRY glTexCoord1fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1fv);
	trampoline(v);
}
extern "C" void APIENTRY glTexCoord1i(GLint s)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1i);
	trampoline(s);
}
extern "C" void APIENTRY glTexCoord1iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1iv);
	trampoline(v);
}
extern "C" void APIENTRY glTexCoord1s(GLshort s)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1s);
	trampoline(s);
}
extern "C" void APIENTRY glTexCoord1sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1sv);
	trampoline(v);
}
extern "C" void APIENTRY glTexCoord2d(GLdouble s, GLdouble t)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2d);
	trampoline(s, t);
}
extern "C" void APIENTRY glTexCoord2dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2dv);
	trampoline(v);
}
extern "C" void APIENTRY glTexCoord2f(GLfloat s, GLfloat t)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2f);
	trampoline(s, t);
}
extern "C" void APIENTRY glTexCoord2fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2fv);
	trampoline(v);
}
extern "C" void APIENTRY glTexCoord2i(GLint s, GLint t)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2i);
	trampoline(s, t);
}
extern "C" void APIENTRY glTexCoord2iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2iv);
	trampoline(v);
}
extern "C" void APIENTRY glTexCoord2s(GLshort s, GLshort t)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2s);
	trampoline(s, t);
}
extern "C" void APIENTRY glTexCoord2sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2sv);
	trampoline(v);
}
extern "C" void APIENTRY glTexCoord3d(GLdouble s, GLdouble t, GLdouble r)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3d);
	trampoline(s, t, r);
}
extern "C" void APIENTRY glTexCoord3dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3dv);
	trampoline(v);
}
extern "C" void APIENTRY glTexCoord3f(GLfloat s, GLfloat t, GLfloat r)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3f);
	trampoline(s, t, r);
}
extern "C" void APIENTRY glTexCoord3fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3fv);
	trampoline(v);
}
extern "C" void APIENTRY glTexCoord3i(GLint s, GLint t, GLint r)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3i);
	trampoline(s, t, r);
}
extern "C" void APIENTRY glTexCoord3iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3iv);
	trampoline(v);
}
extern "C" void APIENTRY glTexCoord3s(GLshort s, GLshort t, GLshort r)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3s);
	trampoline(s, t, r);
}
extern "C" void APIENTRY glTexCoord3sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3sv);
	trampoline(v);
}
extern "C" void APIENTRY glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4d);
	trampoline(s, t, r, q);
}
extern "C" void APIENTRY glTexCoord4dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4dv);
	trampoline(v);
}
extern "C" void APIENTRY glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4f);
	trampoline(s, t, r, q);
}
extern "C" void APIENTRY glTexCoord4fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4fv);
	trampoline(v);
}
extern "C" void APIENTRY glTexCoord4i(GLint s, GLint t, GLint r, GLint q)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4i);
	trampoline(s, t, r, q);
}
extern "C" void APIENTRY glTexCoord4iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4iv);
	trampoline(v);
}
extern "C" void APIENTRY glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4s);
	trampoline(s, t, r, q);
}
extern "C" void APIENTRY glTexCoord4sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4sv);
	trampoline(v);
}

extern "C" void APIENTRY glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(glTexCoordPointer);
	trampoline(size, type, stride, pointer);
}

extern "C" void APIENTRY glTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glTexEnvf);
	trampoline(target, pname, param);
}
extern "C" void APIENTRY glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glTexEnvfv);
	trampoline(target, pname, params);
}
extern "C" void APIENTRY glTexEnvi(GLenum target, GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glTexEnvi);
	trampoline(target, pname, param);
}
extern "C" void APIENTRY glTexEnviv(GLenum target, GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glTexEnviv);
	trampoline(target, pname, params);
}

extern "C" void APIENTRY glTexGend(GLenum coord, GLenum pname, GLdouble param)
{
	static const auto trampoline = reshade::hooks::call(glTexGend);
	trampoline(coord, pname, param);
}
extern "C" void APIENTRY glTexGendv(GLenum coord, GLenum pname, const GLdouble *params)
{
	static const auto trampoline = reshade::hooks::call(glTexGendv);
	trampoline(coord, pname, params);
}
extern "C" void APIENTRY glTexGenf(GLenum coord, GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glTexGenf);
	trampoline(coord, pname, param);
}
extern "C" void APIENTRY glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glTexGenfv);
	trampoline(coord, pname, params);
}
extern "C" void APIENTRY glTexGeni(GLenum coord, GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glTexGeni);
	trampoline(coord, pname, param);
}
extern "C" void APIENTRY glTexGeniv(GLenum coord, GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glTexGeniv);
	trampoline(coord, pname, params);
}

extern "C" void APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glTexParameterf);
	trampoline(target, pname, param);
}
extern "C" void APIENTRY glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glTexParameterfv);
	trampoline(target, pname, params);
}
extern "C" void APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glTexParameteri);
	trampoline(target, pname, param);
}
extern "C" void APIENTRY glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glTexParameteriv);
	trampoline(target, pname, params);
}

extern "C" void APIENTRY glTranslated(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = reshade::hooks::call(glTranslated);
	trampoline(x, y, z);
}
extern "C" void APIENTRY glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = reshade::hooks::call(glTranslatef);
	trampoline(x, y, z);
}

extern "C" void APIENTRY glVertex2d(GLdouble x, GLdouble y)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2d);
	trampoline(x, y);
}
extern "C" void APIENTRY glVertex2dv(const GLdouble *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2dv);
	trampoline(v);
}
extern "C" void APIENTRY glVertex2f(GLfloat x, GLfloat y)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2f);
	trampoline(x, y);
}
extern "C" void APIENTRY glVertex2fv(const GLfloat *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2fv);
	trampoline(v);
}
extern "C" void APIENTRY glVertex2i(GLint x, GLint y)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2i);
	trampoline(x, y);
}
extern "C" void APIENTRY glVertex2iv(const GLint *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2iv);
	trampoline(v);
}
extern "C" void APIENTRY glVertex2s(GLshort x, GLshort y)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2s);
	trampoline(x, y);
}
extern "C" void APIENTRY glVertex2sv(const GLshort *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2sv);
	trampoline(v);
}
extern "C" void APIENTRY glVertex3d(GLdouble x, GLdouble y, GLdouble z)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3d);
	trampoline(x, y, z);
}
extern "C" void APIENTRY glVertex3dv(const GLdouble *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3dv);
	trampoline(v);
}
extern "C" void APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3f);
	trampoline(x, y, z);
}
extern "C" void APIENTRY glVertex3fv(const GLfloat *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3fv);
	trampoline(v);
}
extern "C" void APIENTRY glVertex3i(GLint x, GLint y, GLint z)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3i);
	trampoline(x, y, z);
}
extern "C" void APIENTRY glVertex3iv(const GLint *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3iv);
	trampoline(v);
}
extern "C" void APIENTRY glVertex3s(GLshort x, GLshort y, GLshort z)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3s);
	trampoline(x, y, z);
}
extern "C" void APIENTRY glVertex3sv(const GLshort *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3sv);
	trampoline(v);
}
extern "C" void APIENTRY glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4d);
	trampoline(x, y, z, w);
}
extern "C" void APIENTRY glVertex4dv(const GLdouble *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4dv);
	trampoline(v);
}
extern "C" void APIENTRY glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4f);
	trampoline(x, y, z, w);
}
extern "C" void APIENTRY glVertex4fv(const GLfloat *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4fv);
	trampoline(v);
}
extern "C" void APIENTRY glVertex4i(GLint x, GLint y, GLint z, GLint w)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4i);
	trampoline(x, y, z, w);
}
extern "C" void APIENTRY glVertex4iv(const GLint *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4iv);
	trampoline(v);
}
extern "C" void APIENTRY glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4s);
	trampoline(x, y, z, w);
}
extern "C" void APIENTRY glVertex4sv(const GLshort *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4sv);
	trampoline(v);
}

extern "C" void APIENTRY glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(glVertexPointer);
	trampoline(size, type, stride, pointer);
}
