/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "runtime_opengl.hpp"
#include "opengl_stubs_internal.hpp"
#include <memory>
#include <unordered_set>

extern std::unordered_map<HDC, reshade::opengl::runtime_opengl *> g_opengl_runtimes;

HOOK_EXPORT void WINAPI glAccum(GLenum op, GLfloat value)
{
	static const auto trampoline = reshade::hooks::call(&glAccum);

	trampoline(op, value);
}

HOOK_EXPORT void WINAPI glAlphaFunc(GLenum func, GLclampf ref)
{
	static const auto trampoline = reshade::hooks::call(&glAlphaFunc);

	trampoline(func, ref);
}

HOOK_EXPORT GLboolean WINAPI glAreTexturesResident(GLsizei n, const GLuint *textures, GLboolean *residences)
{
	static const auto trampoline = reshade::hooks::call(&glAreTexturesResident);

	return trampoline(n, textures, residences);
}

HOOK_EXPORT void WINAPI glArrayElement(GLint i)
{
	static const auto trampoline = reshade::hooks::call(&glArrayElement);

	trampoline(i);
}

HOOK_EXPORT void WINAPI glBegin(GLenum mode)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count = 0;
	}

	static const auto trampoline = reshade::hooks::call(&glBegin);

	trampoline(mode);
}

HOOK_EXPORT void WINAPI glBindTexture(GLenum target, GLuint texture)
{
	static const auto trampoline = reshade::hooks::call(&glBindTexture);

	trampoline(target, texture);
}

HOOK_EXPORT void WINAPI glBitmap(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap)
{
	static const auto trampoline = reshade::hooks::call(&glBitmap);

	trampoline(width, height, xorig, yorig, xmove, ymove, bitmap);
}

HOOK_EXPORT void WINAPI glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	static const auto trampoline = reshade::hooks::call(&glBlendFunc);

	trampoline(sfactor, dfactor);
}

HOOK_EXPORT void WINAPI glCallList(GLuint list)
{
	static const auto trampoline = reshade::hooks::call(&glCallList);

	trampoline(list);
}
HOOK_EXPORT void WINAPI glCallLists(GLsizei n, GLenum type, const GLvoid *lists)
{
	static const auto trampoline = reshade::hooks::call(&glCallLists);

	trampoline(n, type, lists);
}

HOOK_EXPORT void WINAPI glClear(GLbitfield mask)
{
	static const auto trampoline = reshade::hooks::call(&glClear);

	trampoline(mask);
}
HOOK_EXPORT void WINAPI glClearAccum(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	static const auto trampoline = reshade::hooks::call(&glClearAccum);

	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	static const auto trampoline = reshade::hooks::call(&glClearColor);

	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glClearDepth(GLclampd depth)
{
	static const auto trampoline = reshade::hooks::call(&glClearDepth);

	trampoline(depth);
}
HOOK_EXPORT void WINAPI glClearIndex(GLfloat c)
{
	static const auto trampoline = reshade::hooks::call(&glClearIndex);

	trampoline(c);
}
HOOK_EXPORT void WINAPI glClearStencil(GLint s)
{
	static const auto trampoline = reshade::hooks::call(&glClearStencil);

	trampoline(s);
}

HOOK_EXPORT void WINAPI glClipPlane(GLenum plane, const GLdouble *equation)
{
	static const auto trampoline = reshade::hooks::call(&glClipPlane);

	trampoline(plane, equation);
}

HOOK_EXPORT void WINAPI glColor3b(GLbyte red, GLbyte green, GLbyte blue)
{
	static const auto trampoline = reshade::hooks::call(&glColor3b);

	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3bv(const GLbyte *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor3bv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor3d(GLdouble red, GLdouble green, GLdouble blue)
{
	static const auto trampoline = reshade::hooks::call(&glColor3d);

	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor3dv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor3f(GLfloat red, GLfloat green, GLfloat blue)
{
	static const auto trampoline = reshade::hooks::call(&glColor3f);

	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor3fv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor3i(GLint red, GLint green, GLint blue)
{
	static const auto trampoline = reshade::hooks::call(&glColor3i);

	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor3iv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor3s(GLshort red, GLshort green, GLshort blue)
{
	static const auto trampoline = reshade::hooks::call(&glColor3s);

	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor3sv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor3ub(GLubyte red, GLubyte green, GLubyte blue)
{
	static const auto trampoline = reshade::hooks::call(&glColor3ub);

	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3ubv(const GLubyte *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor3ubv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor3ui(GLuint red, GLuint green, GLuint blue)
{
	static const auto trampoline = reshade::hooks::call(&glColor3ui);

	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3uiv(const GLuint *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor3uiv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor3us(GLushort red, GLushort green, GLushort blue)
{
	static const auto trampoline = reshade::hooks::call(&glColor3us);

	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3usv(const GLushort *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor3usv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4b(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
	static const auto trampoline = reshade::hooks::call(&glColor4b);

	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4bv(const GLbyte *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor4bv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4d(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
	static const auto trampoline = reshade::hooks::call(&glColor4d);

	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor4dv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	static const auto trampoline = reshade::hooks::call(&glColor4f);

	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor4fv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4i(GLint red, GLint green, GLint blue, GLint alpha)
{
	static const auto trampoline = reshade::hooks::call(&glColor4i);

	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor4iv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4s(GLshort red, GLshort green, GLshort blue, GLshort alpha)
{
	static const auto trampoline = reshade::hooks::call(&glColor4s);

	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor4sv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
	static const auto trampoline = reshade::hooks::call(&glColor4ub);

	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4ubv(const GLubyte *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor4ubv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4ui(GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
	static const auto trampoline = reshade::hooks::call(&glColor4ui);

	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4uiv(const GLuint *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor4uiv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4us(GLushort red, GLushort green, GLushort blue, GLushort alpha)
{
	static const auto trampoline = reshade::hooks::call(&glColor4us);

	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4usv(const GLushort *v)
{
	static const auto trampoline = reshade::hooks::call(&glColor4usv);

	trampoline(v);
}

HOOK_EXPORT void WINAPI glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	static const auto trampoline = reshade::hooks::call(&glColorMask);

	trampoline(red, green, blue, alpha);
}

HOOK_EXPORT void WINAPI glColorMaterial(GLenum face, GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(&glColorMaterial);

	trampoline(face, mode);
}

HOOK_EXPORT void WINAPI glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(&glColorPointer);

	trampoline(size, type, stride, pointer);
}

HOOK_EXPORT void WINAPI glCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
	static const auto trampoline = reshade::hooks::call(&glCopyPixels);

	trampoline(x, y, width, height, type);
}

HOOK_EXPORT void WINAPI glCopyTexImage1D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border)
{
	static const auto trampoline = reshade::hooks::call(&glCopyTexImage1D);

	trampoline(target, level, internalFormat, x, y, width, border);
}
HOOK_EXPORT void WINAPI glCopyTexImage2D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
	static const auto trampoline = reshade::hooks::call(&glCopyTexImage2D);

	trampoline(target, level, internalFormat, x, y, width, height, border);
}
HOOK_EXPORT void WINAPI glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
	static const auto trampoline = reshade::hooks::call(&glCopyTexSubImage1D);

	trampoline(target, level, xoffset, x, y, width);
}
HOOK_EXPORT void WINAPI glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(&glCopyTexSubImage2D);

	trampoline(target, level, xoffset, yoffset, x, y, width, height);
}

HOOK_EXPORT void WINAPI glCullFace(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(&glCullFace);

	trampoline(mode);
}

HOOK_EXPORT void WINAPI glDeleteLists(GLuint list, GLsizei range)
{
	static const auto trampoline = reshade::hooks::call(&glDeleteLists);

	trampoline(list, range);
}

HOOK_EXPORT void WINAPI glDeleteTextures(GLsizei n, const GLuint *textures)
{
	static const auto trampoline = reshade::hooks::call(&glDeleteTextures);

	trampoline(n, textures);
}

HOOK_EXPORT void WINAPI glDepthFunc(GLenum func)
{
	static const auto trampoline = reshade::hooks::call(&glDepthFunc);

	trampoline(func);
}
HOOK_EXPORT void WINAPI glDepthMask(GLboolean flag)
{
	static const auto trampoline = reshade::hooks::call(&glDepthMask);

	trampoline(flag);
}
HOOK_EXPORT void WINAPI glDepthRange(GLclampd zNear, GLclampd zFar)
{
	static const auto trampoline = reshade::hooks::call(&glDepthRange);

	trampoline(zNear, zFar);
}

HOOK_EXPORT void WINAPI glDisable(GLenum cap)
{
	static const auto trampoline = reshade::hooks::call(&glDisable);

	trampoline(cap);
}
HOOK_EXPORT void WINAPI glDisableClientState(GLenum array)
{
	static const auto trampoline = reshade::hooks::call(&glDisableClientState);

	trampoline(array);
}

HOOK_EXPORT void WINAPI glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(count);
	}

	static const auto trampoline = reshade::hooks::call(&glDrawArrays);

	trampoline(mode, first, count);
}
extern "C"  void WINAPI glDrawArraysIndirect(GLenum mode, const GLvoid *indirect)
{
	static const auto trampoline = reshade::hooks::call(&glDrawArraysIndirect);

	trampoline(mode, indirect);
}
extern "C"  void WINAPI glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(primcount * count);
	}

	static const auto trampoline = reshade::hooks::call(&glDrawArraysInstanced);

	trampoline(mode, first, count, primcount);
}
extern "C"  void WINAPI glDrawArraysInstancedARB(GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(primcount * count);
	}

	static const auto trampoline = reshade::hooks::call(&glDrawArraysInstancedARB);

	trampoline(mode, first, count, primcount);
}
extern "C"  void WINAPI glDrawArraysInstancedEXT(GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(primcount * count);
	}

	static const auto trampoline = reshade::hooks::call(&glDrawArraysInstancedEXT);

	trampoline(mode, first, count, primcount);
}
extern "C"  void WINAPI glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei primcount, GLuint baseinstance)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(primcount * count);
	}

	static const auto trampoline = reshade::hooks::call(&glDrawArraysInstancedBaseInstance);

	trampoline(mode, first, count, primcount, baseinstance);
}

HOOK_EXPORT void WINAPI glDrawBuffer(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(&glDrawBuffer);

	trampoline(mode);
}

HOOK_EXPORT void WINAPI glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(count);
	}

	static const auto trampoline = reshade::hooks::call(&glDrawElements);

	trampoline(mode, count, type, indices);
}
extern "C"  void WINAPI glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(count);
	}

	static const auto trampoline = reshade::hooks::call(&glDrawElementsBaseVertex);

	trampoline(mode, count, type, indices, basevertex);
}
extern "C"  void WINAPI glDrawElementsIndirect(GLenum mode, GLenum type, const GLvoid *indirect)
{
	static const auto trampoline = reshade::hooks::call(&glDrawElementsIndirect);

	trampoline(mode, type, indirect);
}
extern "C"  void WINAPI glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(primcount * count);
	}

	static const auto trampoline = reshade::hooks::call(&glDrawElementsInstanced);

	trampoline(mode, count, type, indices, primcount);
}
extern "C"  void WINAPI glDrawElementsInstancedARB(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(primcount * count);
	}

	static const auto trampoline = reshade::hooks::call(&glDrawElementsInstancedARB);

	trampoline(mode, count, type, indices, primcount);
}
extern "C"  void WINAPI glDrawElementsInstancedEXT(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(primcount * count);
	}

	static const auto trampoline = reshade::hooks::call(&glDrawElementsInstancedEXT);

	trampoline(mode, count, type, indices, primcount);
}
extern "C"  void WINAPI glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(primcount * count);
	}

	static const auto trampoline = reshade::hooks::call(&glDrawElementsInstancedBaseVertex);

	trampoline(mode, count, type, indices, primcount, basevertex);
}
extern "C"  void WINAPI glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLuint baseinstance)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(primcount * count);
	}

	static const auto trampoline = reshade::hooks::call(&glDrawElementsInstancedBaseInstance);

	trampoline(mode, count, type, indices, primcount, baseinstance);
}
extern "C"  void WINAPI glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex, GLuint baseinstance)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(primcount * count);
	}

	static const auto trampoline = reshade::hooks::call(&glDrawElementsInstancedBaseVertexBaseInstance);

	trampoline(mode, count, type, indices, primcount, basevertex, baseinstance);
}

HOOK_EXPORT void WINAPI glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(&glDrawPixels);

	trampoline(width, height, format, type, pixels);
}

extern "C"  void WINAPI glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(count);
	}

	static const auto trampoline = reshade::hooks::call(&glDrawRangeElements);

	trampoline(mode, start, end, count, type, indices);
}
extern "C"  void WINAPI glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(count);
	}

	static const auto trampoline = reshade::hooks::call(&glDrawRangeElementsBaseVertex);

	trampoline(mode, start, end, count, type, indices, basevertex);
}

HOOK_EXPORT void WINAPI glEdgeFlag(GLboolean flag)
{
	static const auto trampoline = reshade::hooks::call(&glEdgeFlag);

	trampoline(flag);
}
HOOK_EXPORT void WINAPI glEdgeFlagPointer(GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(&glEdgeFlagPointer);

	trampoline(stride, pointer);
}
HOOK_EXPORT void WINAPI glEdgeFlagv(const GLboolean *flag)
{
	static const auto trampoline = reshade::hooks::call(&glEdgeFlagv);

	trampoline(flag);
}

HOOK_EXPORT void WINAPI glEnable(GLenum cap)
{
	static const auto trampoline = reshade::hooks::call(&glEnable);

	trampoline(cap);
}
HOOK_EXPORT void WINAPI glEnableClientState(GLenum array)
{
	static const auto trampoline = reshade::hooks::call(&glEnableClientState);

	trampoline(array);
}

HOOK_EXPORT void WINAPI glEnd()
{
	static const auto trampoline = reshade::hooks::call(&glEnd);

	trampoline();

	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_draw_call(it->second->_current_vertex_count);
	}
}

HOOK_EXPORT void WINAPI glEndList()
{
	static const auto trampoline = reshade::hooks::call(&glEndList);

	trampoline();
}

HOOK_EXPORT void WINAPI glEvalCoord1d(GLdouble u)
{
	static const auto trampoline = reshade::hooks::call(&glEvalCoord1d);

	trampoline(u);
}
HOOK_EXPORT void WINAPI glEvalCoord1dv(const GLdouble *u)
{
	static const auto trampoline = reshade::hooks::call(&glEvalCoord1dv);

	trampoline(u);
}
HOOK_EXPORT void WINAPI glEvalCoord1f(GLfloat u)
{
	static const auto trampoline = reshade::hooks::call(&glEvalCoord1f);

	trampoline(u);
}
HOOK_EXPORT void WINAPI glEvalCoord1fv(const GLfloat *u)
{
	static const auto trampoline = reshade::hooks::call(&glEvalCoord1fv);

	trampoline(u);
}
HOOK_EXPORT void WINAPI glEvalCoord2d(GLdouble u, GLdouble v)
{
	static const auto trampoline = reshade::hooks::call(&glEvalCoord2d);

	trampoline(u, v);
}
HOOK_EXPORT void WINAPI glEvalCoord2dv(const GLdouble *u)
{
	static const auto trampoline = reshade::hooks::call(&glEvalCoord2dv);

	trampoline(u);
}
HOOK_EXPORT void WINAPI glEvalCoord2f(GLfloat u, GLfloat v)
{
	static const auto trampoline = reshade::hooks::call(&glEvalCoord2f);

	trampoline(u, v);
}
HOOK_EXPORT void WINAPI glEvalCoord2fv(const GLfloat *u)
{
	static const auto trampoline = reshade::hooks::call(&glEvalCoord2fv);

	trampoline(u);
}

HOOK_EXPORT void WINAPI glEvalMesh1(GLenum mode, GLint i1, GLint i2)
{
	static const auto trampoline = reshade::hooks::call(&glEvalMesh1);

	trampoline(mode, i1, i2);
}
HOOK_EXPORT void WINAPI glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
	static const auto trampoline = reshade::hooks::call(&glEvalMesh2);

	trampoline(mode, i1, i2, j1, j2);
}

HOOK_EXPORT void WINAPI glEvalPoint1(GLint i)
{
	static const auto trampoline = reshade::hooks::call(&glEvalPoint1);

	trampoline(i);
}
HOOK_EXPORT void WINAPI glEvalPoint2(GLint i, GLint j)
{
	static const auto trampoline = reshade::hooks::call(&glEvalPoint2);

	trampoline(i, j);
}

HOOK_EXPORT void WINAPI glFeedbackBuffer(GLsizei size, GLenum type, GLfloat *buffer)
{
	static const auto trampoline = reshade::hooks::call(&glFeedbackBuffer);

	trampoline(size, type, buffer);
}

HOOK_EXPORT void WINAPI glFinish()
{
	static const auto trampoline = reshade::hooks::call(&glFinish);

	trampoline();
}
HOOK_EXPORT void WINAPI glFlush()
{
	static const auto trampoline = reshade::hooks::call(&glFlush);

	trampoline();
}

HOOK_EXPORT void WINAPI glFogf(GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(&glFogf);

	trampoline(pname, param);
}
HOOK_EXPORT void WINAPI glFogfv(GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(&glFogfv);

	trampoline(pname, params);
}
HOOK_EXPORT void WINAPI glFogi(GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(&glFogi);

	trampoline(pname, param);
}
HOOK_EXPORT void WINAPI glFogiv(GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(&glFogiv);

	trampoline(pname, params);
}

extern "C"  void WINAPI glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	static const auto trampoline = reshade::hooks::call(&glFramebufferRenderbuffer);

	trampoline(target, attachment, renderbuffertarget, renderbuffer);

	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_fbo_attachment(target, attachment, renderbuffertarget, renderbuffer, 0);
	}
}
extern "C"  void WINAPI glFramebufferRenderbufferEXT(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	static const auto trampoline = reshade::hooks::call(&glFramebufferRenderbufferEXT);

	trampoline(target, attachment, renderbuffertarget, renderbuffer);

	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_fbo_attachment(target, attachment, renderbuffertarget, renderbuffer, 0);
	}
}
extern "C"  void WINAPI glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
	static const auto trampoline = reshade::hooks::call(&glFramebufferTexture);

	trampoline(target, attachment, texture, level);

	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_fbo_attachment(target, attachment, GL_TEXTURE, texture, level);
	}
}
extern "C"  void WINAPI glFramebufferTextureARB(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
	static const auto trampoline = reshade::hooks::call(&glFramebufferTextureARB);

	trampoline(target, attachment, texture, level);

	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_fbo_attachment(target, attachment, GL_TEXTURE, texture, level);
	}
}
extern "C"  void WINAPI glFramebufferTextureEXT(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
	static const auto trampoline = reshade::hooks::call(&glFramebufferTextureEXT);

	trampoline(target, attachment, texture, level);

	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_fbo_attachment(target, attachment, GL_TEXTURE, texture, level);
	}
}
extern "C"  void WINAPI glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	static const auto trampoline = reshade::hooks::call(&glFramebufferTexture1D);

	trampoline(target, attachment, textarget, texture, level);

	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_fbo_attachment(target, attachment, textarget, texture, level);
	}
}
extern "C"  void WINAPI glFramebufferTexture1DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	static const auto trampoline = reshade::hooks::call(&glFramebufferTexture1DEXT);

	trampoline(target, attachment, textarget, texture, level);

	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_fbo_attachment(target, attachment, textarget, texture, level);
	}
}
extern "C"  void WINAPI glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	static const auto trampoline = reshade::hooks::call(&glFramebufferTexture2D);

	trampoline(target, attachment, textarget, texture, level);

	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_fbo_attachment(target, attachment, textarget, texture, level);
	}
}
extern "C"  void WINAPI glFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	static const auto trampoline = reshade::hooks::call(&glFramebufferTexture2DEXT);

	trampoline(target, attachment, textarget, texture, level);

	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_fbo_attachment(target, attachment, textarget, texture, level);
	}
}
extern "C"  void WINAPI glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
	static const auto trampoline = reshade::hooks::call(&glFramebufferTexture3D);

	trampoline(target, attachment, textarget, texture, level, zoffset);

	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_fbo_attachment(target, attachment, textarget, texture, level);
	}
}
extern "C"  void WINAPI glFramebufferTexture3DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
	static const auto trampoline = reshade::hooks::call(&glFramebufferTexture3DEXT);

	trampoline(target, attachment, textarget, texture, level, zoffset);

	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_fbo_attachment(target, attachment, textarget, texture, level);
	}
}
extern "C"  void WINAPI glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	static const auto trampoline = reshade::hooks::call(&glFramebufferTextureLayer);

	trampoline(target, attachment, texture, level, layer);

	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_fbo_attachment(target, attachment, GL_TEXTURE, texture, level);
	}
}
extern "C"  void WINAPI glFramebufferTextureLayerARB(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	static const auto trampoline = reshade::hooks::call(&glFramebufferTextureLayerARB);

	trampoline(target, attachment, texture, level, layer);

	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_fbo_attachment(target, attachment, GL_TEXTURE, texture, level);
	}
}
extern "C"  void WINAPI glFramebufferTextureLayerEXT(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	static const auto trampoline = reshade::hooks::call(&glFramebufferTextureLayerEXT);

	trampoline(target, attachment, texture, level, layer);

	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->on_fbo_attachment(target, attachment, GL_TEXTURE, texture, level);
	}
}

HOOK_EXPORT void WINAPI glFrontFace(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(&glFrontFace);

	trampoline(mode);
}

HOOK_EXPORT void WINAPI glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	static const auto trampoline = reshade::hooks::call(&glFrustum);

	trampoline(left, right, bottom, top, zNear, zFar);
}

HOOK_EXPORT GLuint WINAPI glGenLists(GLsizei range)
{
	static const auto trampoline = reshade::hooks::call(&glGenLists);

	return trampoline(range);
}

HOOK_EXPORT void WINAPI glGenTextures(GLsizei n, GLuint *textures)
{
	static const auto trampoline = reshade::hooks::call(&glGenTextures);

	trampoline(n, textures);
}

HOOK_EXPORT void WINAPI glGetBooleanv(GLenum pname, GLboolean *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetBooleanv);

	trampoline(pname, params);
}
HOOK_EXPORT void WINAPI glGetDoublev(GLenum pname, GLdouble *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetDoublev);

	trampoline(pname, params);
}
HOOK_EXPORT void WINAPI glGetFloatv(GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetFloatv);

	trampoline(pname, params);
}
HOOK_EXPORT void WINAPI glGetIntegerv(GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetIntegerv);

	trampoline(pname, params);
}

HOOK_EXPORT void WINAPI glGetClipPlane(GLenum plane, GLdouble *equation)
{
	static const auto trampoline = reshade::hooks::call(&glGetClipPlane);

	trampoline(plane, equation);
}

HOOK_EXPORT GLenum WINAPI glGetError()
{
	static const auto trampoline = reshade::hooks::call(&glGetError);

	return trampoline();
}

HOOK_EXPORT void WINAPI glGetLightfv(GLenum light, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetLightfv);

	trampoline(light, pname, params);
}
HOOK_EXPORT void WINAPI glGetLightiv(GLenum light, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetLightiv);

	trampoline(light, pname, params);
}

HOOK_EXPORT void WINAPI glGetMapdv(GLenum target, GLenum query, GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(&glGetMapdv);

	trampoline(target, query, v);
}
HOOK_EXPORT void WINAPI glGetMapfv(GLenum target, GLenum query, GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(&glGetMapfv);

	trampoline(target, query, v);
}
HOOK_EXPORT void WINAPI glGetMapiv(GLenum target, GLenum query, GLint *v)
{
	static const auto trampoline = reshade::hooks::call(&glGetMapiv);

	trampoline(target, query, v);
}

HOOK_EXPORT void WINAPI glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetMaterialfv);

	trampoline(face, pname, params);
}
HOOK_EXPORT void WINAPI glGetMaterialiv(GLenum face, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetMaterialiv);

	trampoline(face, pname, params);
}

HOOK_EXPORT void WINAPI glGetPixelMapfv(GLenum map, GLfloat *values)
{
	static const auto trampoline = reshade::hooks::call(&glGetPixelMapfv);

	trampoline(map, values);
}
HOOK_EXPORT void WINAPI glGetPixelMapuiv(GLenum map, GLuint *values)
{
	static const auto trampoline = reshade::hooks::call(&glGetPixelMapuiv);

	trampoline(map, values);
}
HOOK_EXPORT void WINAPI glGetPixelMapusv(GLenum map, GLushort *values)
{
	static const auto trampoline = reshade::hooks::call(&glGetPixelMapusv);

	trampoline(map, values);
}

HOOK_EXPORT void WINAPI glGetPointerv(GLenum pname, GLvoid **params)
{
	static const auto trampoline = reshade::hooks::call(&glGetPointerv);

	trampoline(pname, params);
}

HOOK_EXPORT void WINAPI glGetPolygonStipple(GLubyte *mask)
{
	static const auto trampoline = reshade::hooks::call(&glGetPolygonStipple);

	trampoline(mask);
}

HOOK_EXPORT const GLubyte *WINAPI glGetString(GLenum name)
{
	static const auto trampoline = reshade::hooks::call(&glGetString);

	return trampoline(name);
}

HOOK_EXPORT void WINAPI glGetTexEnvfv(GLenum target, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetTexEnvfv);

	trampoline(target, pname, params);
}
HOOK_EXPORT void WINAPI glGetTexEnviv(GLenum target, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetTexEnviv);

	trampoline(target, pname, params);
}

HOOK_EXPORT void WINAPI glGetTexGendv(GLenum coord, GLenum pname, GLdouble *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetTexGendv);

	trampoline(coord, pname, params);
}
HOOK_EXPORT void WINAPI glGetTexGenfv(GLenum coord, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetTexGenfv);

	trampoline(coord, pname, params);
}
HOOK_EXPORT void WINAPI glGetTexGeniv(GLenum coord, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetTexGeniv);

	trampoline(coord, pname, params);
}

HOOK_EXPORT void WINAPI glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(&glGetTexImage);

	trampoline(target, level, format, type, pixels);
}

HOOK_EXPORT void WINAPI glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetTexLevelParameterfv);

	trampoline(target, level, pname, params);
}
HOOK_EXPORT void WINAPI glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetTexLevelParameteriv);

	trampoline(target, level, pname, params);
}
HOOK_EXPORT void WINAPI glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetTexParameterfv);

	trampoline(target, pname, params);
}
HOOK_EXPORT void WINAPI glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(&glGetTexParameteriv);

	trampoline(target, pname, params);
}

HOOK_EXPORT void WINAPI glHint(GLenum target, GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(&glHint);

	trampoline(target, mode);
}

HOOK_EXPORT void WINAPI glIndexMask(GLuint mask)
{
	static const auto trampoline = reshade::hooks::call(&glIndexMask);

	trampoline(mask);
}

HOOK_EXPORT void WINAPI glIndexPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(&glIndexPointer);

	trampoline(type, stride, pointer);
}

HOOK_EXPORT void WINAPI glIndexd(GLdouble c)
{
	static const auto trampoline = reshade::hooks::call(&glIndexd);

	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexdv(const GLdouble *c)
{
	static const auto trampoline = reshade::hooks::call(&glIndexdv);

	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexf(GLfloat c)
{
	static const auto trampoline = reshade::hooks::call(&glIndexf);

	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexfv(const GLfloat *c)
{
	static const auto trampoline = reshade::hooks::call(&glIndexfv);

	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexi(GLint c)
{
	static const auto trampoline = reshade::hooks::call(&glIndexi);

	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexiv(const GLint *c)
{
	static const auto trampoline = reshade::hooks::call(&glIndexiv);

	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexs(GLshort c)
{
	static const auto trampoline = reshade::hooks::call(&glIndexs);

	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexsv(const GLshort *c)
{
	static const auto trampoline = reshade::hooks::call(&glIndexsv);

	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexub(GLubyte c)
{
	static const auto trampoline = reshade::hooks::call(&glIndexub);

	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexubv(const GLubyte *c)
{
	static const auto trampoline = reshade::hooks::call(&glIndexubv);

	trampoline(c);
}

HOOK_EXPORT void WINAPI glInitNames()
{
	static const auto trampoline = reshade::hooks::call(&glInitNames);

	trampoline();
}

HOOK_EXPORT void WINAPI glInterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(&glInterleavedArrays);

	trampoline(format, stride, pointer);
}

HOOK_EXPORT GLboolean WINAPI glIsEnabled(GLenum cap)
{
	static const auto trampoline = reshade::hooks::call(&glIsEnabled);

	return trampoline(cap);
}

HOOK_EXPORT GLboolean WINAPI glIsList(GLuint list)
{
	static const auto trampoline = reshade::hooks::call(&glIsList);

	return trampoline(list);
}
HOOK_EXPORT GLboolean WINAPI glIsTexture(GLuint texture)
{
	static const auto trampoline = reshade::hooks::call(&glIsTexture);

	return trampoline(texture);
}

HOOK_EXPORT void WINAPI glLightModelf(GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(&glLightModelf);

	trampoline(pname, param);
}
HOOK_EXPORT void WINAPI glLightModelfv(GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(&glLightModelfv);

	trampoline(pname, params);
}
HOOK_EXPORT void WINAPI glLightModeli(GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(&glLightModeli);

	trampoline(pname, param);
}
HOOK_EXPORT void WINAPI glLightModeliv(GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(&glLightModeliv);

	trampoline(pname, params);
}

HOOK_EXPORT void WINAPI glLightf(GLenum light, GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(&glLightf);

	trampoline(light, pname, param);
}
HOOK_EXPORT void WINAPI glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(&glLightfv);

	trampoline(light, pname, params);
}
HOOK_EXPORT void WINAPI glLighti(GLenum light, GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(&glLighti);

	trampoline(light, pname, param);
}
HOOK_EXPORT void WINAPI glLightiv(GLenum light, GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(&glLightiv);

	trampoline(light, pname, params);
}

HOOK_EXPORT void WINAPI glLineStipple(GLint factor, GLushort pattern)
{
	static const auto trampoline = reshade::hooks::call(&glLineStipple);

	trampoline(factor, pattern);
}
HOOK_EXPORT void WINAPI glLineWidth(GLfloat width)
{
	static const auto trampoline = reshade::hooks::call(&glLineWidth);

	trampoline(width);
}

HOOK_EXPORT void WINAPI glListBase(GLuint base)
{
	static const auto trampoline = reshade::hooks::call(&glListBase);

	trampoline(base);
}

HOOK_EXPORT void WINAPI glLoadIdentity()
{
	static const auto trampoline = reshade::hooks::call(&glLoadIdentity);

	trampoline();
}
HOOK_EXPORT void WINAPI glLoadMatrixd(const GLdouble *m)
{
	static const auto trampoline = reshade::hooks::call(&glLoadMatrixd);

	trampoline(m);
}
HOOK_EXPORT void WINAPI glLoadMatrixf(const GLfloat *m)
{
	static const auto trampoline = reshade::hooks::call(&glLoadMatrixf);

	trampoline(m);
}

HOOK_EXPORT void WINAPI glLoadName(GLuint name)
{
	static const auto trampoline = reshade::hooks::call(&glLoadName);

	trampoline(name);
}

HOOK_EXPORT void WINAPI glLogicOp(GLenum opcode)
{
	static const auto trampoline = reshade::hooks::call(&glLogicOp);

	trampoline(opcode);
}

HOOK_EXPORT void WINAPI glMap1d(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points)
{
	static const auto trampoline = reshade::hooks::call(&glMap1d);

	trampoline(target, u1, u2, stride, order, points);
}
HOOK_EXPORT void WINAPI glMap1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points)
{
	static const auto trampoline = reshade::hooks::call(&glMap1f);

	trampoline(target, u1, u2, stride, order, points);
}
HOOK_EXPORT void WINAPI glMap2d(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points)
{
	static const auto trampoline = reshade::hooks::call(&glMap2d);

	trampoline(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}
HOOK_EXPORT void WINAPI glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points)
{
	static const auto trampoline = reshade::hooks::call(&glMap2f);

	trampoline(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}

HOOK_EXPORT void WINAPI glMapGrid1d(GLint un, GLdouble u1, GLdouble u2)
{
	static const auto trampoline = reshade::hooks::call(&glMapGrid1d);

	trampoline(un, u1, u2);
}
HOOK_EXPORT void WINAPI glMapGrid1f(GLint un, GLfloat u1, GLfloat u2)
{
	static const auto trampoline = reshade::hooks::call(&glMapGrid1f);

	trampoline(un, u1, u2);
}
HOOK_EXPORT void WINAPI glMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2)
{
	static const auto trampoline = reshade::hooks::call(&glMapGrid2d);

	trampoline(un, u1, u2, vn, v1, v2);
}
HOOK_EXPORT void WINAPI glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{
	static const auto trampoline = reshade::hooks::call(&glMapGrid2f);

	trampoline(un, u1, u2, vn, v1, v2);
}

HOOK_EXPORT void WINAPI glMaterialf(GLenum face, GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(&glMaterialf);

	trampoline(face, pname, param);
}
HOOK_EXPORT void WINAPI glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(&glMaterialfv);

	trampoline(face, pname, params);
}
HOOK_EXPORT void WINAPI glMateriali(GLenum face, GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(&glMateriali);

	trampoline(face, pname, param);
}
HOOK_EXPORT void WINAPI glMaterialiv(GLenum face, GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(&glMaterialiv);

	trampoline(face, pname, params);
}

HOOK_EXPORT void WINAPI glMatrixMode(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(&glMatrixMode);

	trampoline(mode);
}

HOOK_EXPORT void WINAPI glMultMatrixd(const GLdouble *m)
{
	static const auto trampoline = reshade::hooks::call(&glMultMatrixd);

	trampoline(m);
}
HOOK_EXPORT void WINAPI glMultMatrixf(const GLfloat *m)
{
	static const auto trampoline = reshade::hooks::call(&glMultMatrixf);

	trampoline(m);
}

extern "C"  void WINAPI glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		GLsizei totalcount = 0;

		for (GLint i = 0; i < drawcount; ++i)
		{
			totalcount += count[i];
		}

		it->second->on_draw_call(totalcount);
	}

	static const auto trampoline = reshade::hooks::call(&glMultiDrawArrays);

	trampoline(mode, first, count, drawcount);
}
extern "C"  void WINAPI glMultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride)
{
	static const auto trampoline = reshade::hooks::call(&glMultiDrawArraysIndirect);

	trampoline(mode, indirect, drawcount, stride);
}
extern "C"  void WINAPI glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei drawcount)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		GLsizei totalcount = 0;

		for (GLint i = 0; i < drawcount; ++i)
		{
			totalcount += count[i];
		}

		it->second->on_draw_call(totalcount);
	}

	static const auto trampoline = reshade::hooks::call(&glMultiDrawElements);

	trampoline(mode, count, type, indices, drawcount);
}
extern "C"  void WINAPI glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei drawcount, const GLint *basevertex)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		GLsizei totalcount = 0;

		for (GLint i = 0; i < drawcount; ++i)
		{
			totalcount += count[i];
		}

		it->second->on_draw_call(totalcount);
	}

	static const auto trampoline = reshade::hooks::call(&glMultiDrawElementsBaseVertex);

	trampoline(mode, count, type, indices, drawcount, basevertex);
}
extern "C"  void WINAPI glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride)
{
	static const auto trampoline = reshade::hooks::call(&glMultiDrawElementsIndirect);

	trampoline(mode, type, indirect, drawcount, stride);
}

HOOK_EXPORT void WINAPI glNewList(GLuint list, GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(&glNewList);

	trampoline(list, mode);
}

HOOK_EXPORT void WINAPI glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz)
{
	static const auto trampoline = reshade::hooks::call(&glNormal3b);

	trampoline(nx, ny, nz);
}
HOOK_EXPORT void WINAPI glNormal3bv(const GLbyte *v)
{
	static const auto trampoline = reshade::hooks::call(&glNormal3bv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz)
{
	static const auto trampoline = reshade::hooks::call(&glNormal3d);

	trampoline(nx, ny, nz);
}
HOOK_EXPORT void WINAPI glNormal3dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(&glNormal3dv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
	static const auto trampoline = reshade::hooks::call(&glNormal3f);

	trampoline(nx, ny, nz);
}
HOOK_EXPORT void WINAPI glNormal3fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(&glNormal3fv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glNormal3i(GLint nx, GLint ny, GLint nz)
{
	static const auto trampoline = reshade::hooks::call(&glNormal3i);

	trampoline(nx, ny, nz);
}
HOOK_EXPORT void WINAPI glNormal3iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(&glNormal3iv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glNormal3s(GLshort nx, GLshort ny, GLshort nz)
{
	static const auto trampoline = reshade::hooks::call(&glNormal3s);

	trampoline(nx, ny, nz);
}
HOOK_EXPORT void WINAPI glNormal3sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(&glNormal3sv);

	trampoline(v);
}

HOOK_EXPORT void WINAPI glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(&glNormalPointer);

	trampoline(type, stride, pointer);
}

HOOK_EXPORT void WINAPI glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	static const auto trampoline = reshade::hooks::call(&glOrtho);

	trampoline(left, right, bottom, top, zNear, zFar);
}

HOOK_EXPORT void WINAPI glPassThrough(GLfloat token)
{
	static const auto trampoline = reshade::hooks::call(&glPassThrough);

	trampoline(token);
}

HOOK_EXPORT void WINAPI glPixelMapfv(GLenum map, GLsizei mapsize, const GLfloat *values)
{
	static const auto trampoline = reshade::hooks::call(&glPixelMapfv);

	trampoline(map, mapsize, values);
}
HOOK_EXPORT void WINAPI glPixelMapuiv(GLenum map, GLsizei mapsize, const GLuint *values)
{
	static const auto trampoline = reshade::hooks::call(&glPixelMapuiv);

	trampoline(map, mapsize, values);
}
HOOK_EXPORT void WINAPI glPixelMapusv(GLenum map, GLsizei mapsize, const GLushort *values)
{
	static const auto trampoline = reshade::hooks::call(&glPixelMapusv);

	trampoline(map, mapsize, values);
}

HOOK_EXPORT void WINAPI glPixelStoref(GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(&glPixelStoref);

	trampoline(pname, param);
}
HOOK_EXPORT void WINAPI glPixelStorei(GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(&glPixelStorei);

	trampoline(pname, param);
}

HOOK_EXPORT void WINAPI glPixelTransferf(GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(&glPixelTransferf);

	trampoline(pname, param);
}
HOOK_EXPORT void WINAPI glPixelTransferi(GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(&glPixelTransferi);

	trampoline(pname, param);
}

HOOK_EXPORT void WINAPI glPixelZoom(GLfloat xfactor, GLfloat yfactor)
{
	static const auto trampoline = reshade::hooks::call(&glPixelZoom);

	trampoline(xfactor, yfactor);
}

HOOK_EXPORT void WINAPI glPointSize(GLfloat size)
{
	static const auto trampoline = reshade::hooks::call(&glPointSize);

	trampoline(size);
}

HOOK_EXPORT void WINAPI glPolygonMode(GLenum face, GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(&glPolygonMode);

	trampoline(face, mode);
}
HOOK_EXPORT void WINAPI glPolygonOffset(GLfloat factor, GLfloat units)
{
	static const auto trampoline = reshade::hooks::call(&glPolygonOffset);

	trampoline(factor, units);
}
HOOK_EXPORT void WINAPI glPolygonStipple(const GLubyte *mask)
{
	static const auto trampoline = reshade::hooks::call(&glPolygonStipple);

	trampoline(mask);
}

HOOK_EXPORT void WINAPI glPopAttrib()
{
	static const auto trampoline = reshade::hooks::call(&glPopAttrib);

	trampoline();
}
HOOK_EXPORT void WINAPI glPopClientAttrib()
{
	static const auto trampoline = reshade::hooks::call(&glPopClientAttrib);

	trampoline();
}
HOOK_EXPORT void WINAPI glPopMatrix()
{
	static const auto trampoline = reshade::hooks::call(&glPopMatrix);

	trampoline();
}
HOOK_EXPORT void WINAPI glPopName()
{
	static const auto trampoline = reshade::hooks::call(&glPopName);

	trampoline();
}

HOOK_EXPORT void WINAPI glPrioritizeTextures(GLsizei n, const GLuint *textures, const GLclampf *priorities)
{
	static const auto trampoline = reshade::hooks::call(&glPrioritizeTextures);

	trampoline(n, textures, priorities);
}

HOOK_EXPORT void WINAPI glPushAttrib(GLbitfield mask)
{
	static const auto trampoline = reshade::hooks::call(&glPushAttrib);

	trampoline(mask);
}
HOOK_EXPORT void WINAPI glPushClientAttrib(GLbitfield mask)
{
	static const auto trampoline = reshade::hooks::call(&glPushClientAttrib);

	trampoline(mask);
}
HOOK_EXPORT void WINAPI glPushMatrix()
{
	static const auto trampoline = reshade::hooks::call(&glPushMatrix);

	trampoline();
}
HOOK_EXPORT void WINAPI glPushName(GLuint name)
{
	static const auto trampoline = reshade::hooks::call(&glPushName);

	trampoline(name);
}

HOOK_EXPORT void WINAPI glRasterPos2d(GLdouble x, GLdouble y)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos2d);

	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glRasterPos2dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos2dv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos2f(GLfloat x, GLfloat y)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos2f);

	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glRasterPos2fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos2fv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos2i(GLint x, GLint y)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos2i);

	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glRasterPos2iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos2iv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos2s(GLshort x, GLshort y)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos2s);

	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glRasterPos2sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos2sv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos3d(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos3d);

	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glRasterPos3dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos3dv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos3f(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos3f);

	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glRasterPos3fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos3fv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos3i(GLint x, GLint y, GLint z)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos3i);

	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glRasterPos3iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos3iv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos3s(GLshort x, GLshort y, GLshort z)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos3s);

	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glRasterPos3sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos3sv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos4d);

	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glRasterPos4dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos4dv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos4f);

	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glRasterPos4fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos4fv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos4i(GLint x, GLint y, GLint z, GLint w)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos4i);

	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glRasterPos4iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos4iv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos4s);

	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glRasterPos4sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(&glRasterPos4sv);

	trampoline(v);
}

HOOK_EXPORT void WINAPI glReadBuffer(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(&glReadBuffer);

	trampoline(mode);
}

HOOK_EXPORT void WINAPI glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(&glReadPixels);

	trampoline(x, y, width, height, format, type, pixels);
}

HOOK_EXPORT void WINAPI glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
	static const auto trampoline = reshade::hooks::call(&glRectd);

	trampoline(x1, y1, x2, y2);
}
HOOK_EXPORT void WINAPI glRectdv(const GLdouble *v1, const GLdouble *v2)
{
	static const auto trampoline = reshade::hooks::call(&glRectdv);

	trampoline(v1, v2);
}
HOOK_EXPORT void WINAPI glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	static const auto trampoline = reshade::hooks::call(&glRectf);

	trampoline(x1, y1, x2, y2);
}
HOOK_EXPORT void WINAPI glRectfv(const GLfloat *v1, const GLfloat *v2)
{
	static const auto trampoline = reshade::hooks::call(&glRectfv);

	trampoline(v1, v2);
}
HOOK_EXPORT void WINAPI glRecti(GLint x1, GLint y1, GLint x2, GLint y2)
{
	static const auto trampoline = reshade::hooks::call(&glRecti);

	trampoline(x1, y1, x2, y2);
}
HOOK_EXPORT void WINAPI glRectiv(const GLint *v1, const GLint *v2)
{
	static const auto trampoline = reshade::hooks::call(&glRectiv);

	trampoline(v1, v2);
}

HOOK_EXPORT void WINAPI glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2)
{
	static const auto trampoline = reshade::hooks::call(&glRects);

	trampoline(x1, y1, x2, y2);
}
HOOK_EXPORT void WINAPI glRectsv(const GLshort *v1, const GLshort *v2)
{
	static const auto trampoline = reshade::hooks::call(&glRectsv);

	trampoline(v1, v2);
}

HOOK_EXPORT GLint WINAPI glRenderMode(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(&glRenderMode);

	return trampoline(mode);
}

HOOK_EXPORT void WINAPI glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = reshade::hooks::call(&glRotated);

	trampoline(angle, x, y, z);
}
HOOK_EXPORT void WINAPI glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = reshade::hooks::call(&glRotatef);

	trampoline(angle, x, y, z);
}

HOOK_EXPORT void WINAPI glScaled(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = reshade::hooks::call(&glScaled);

	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = reshade::hooks::call(&glScalef);

	trampoline(x, y, z);
}

HOOK_EXPORT void WINAPI glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(&glScissor);

	trampoline(x, y, width, height);
}

HOOK_EXPORT void WINAPI glSelectBuffer(GLsizei size, GLuint *buffer)
{
	static const auto trampoline = reshade::hooks::call(&glSelectBuffer);

	trampoline(size, buffer);
}

HOOK_EXPORT void WINAPI glShadeModel(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(&glShadeModel);

	trampoline(mode);
}

HOOK_EXPORT void WINAPI glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
	static const auto trampoline = reshade::hooks::call(&glStencilFunc);

	trampoline(func, ref, mask);
}
HOOK_EXPORT void WINAPI glStencilMask(GLuint mask)
{
	static const auto trampoline = reshade::hooks::call(&glStencilMask);

	trampoline(mask);
}
HOOK_EXPORT void WINAPI glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
	static const auto trampoline = reshade::hooks::call(&glStencilOp);

	trampoline(fail, zfail, zpass);
}

HOOK_EXPORT void WINAPI glTexCoord1d(GLdouble s)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord1d);

	trampoline(s);
}
HOOK_EXPORT void WINAPI glTexCoord1dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord1dv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord1f(GLfloat s)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord1f);

	trampoline(s);
}
HOOK_EXPORT void WINAPI glTexCoord1fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord1fv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord1i(GLint s)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord1i);

	trampoline(s);
}
HOOK_EXPORT void WINAPI glTexCoord1iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord1iv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord1s(GLshort s)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord1s);

	trampoline(s);
}
HOOK_EXPORT void WINAPI glTexCoord1sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord1sv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord2d(GLdouble s, GLdouble t)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord2d);

	trampoline(s, t);
}
HOOK_EXPORT void WINAPI glTexCoord2dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord2dv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord2f(GLfloat s, GLfloat t)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord2f);

	trampoline(s, t);
}
HOOK_EXPORT void WINAPI glTexCoord2fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord2fv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord2i(GLint s, GLint t)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord2i);

	trampoline(s, t);
}
HOOK_EXPORT void WINAPI glTexCoord2iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord2iv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord2s(GLshort s, GLshort t)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord2s);

	trampoline(s, t);
}
HOOK_EXPORT void WINAPI glTexCoord2sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord2sv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord3d(GLdouble s, GLdouble t, GLdouble r)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord3d);

	trampoline(s, t, r);
}
HOOK_EXPORT void WINAPI glTexCoord3dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord3dv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord3f(GLfloat s, GLfloat t, GLfloat r)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord3f);

	trampoline(s, t, r);
}
HOOK_EXPORT void WINAPI glTexCoord3fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord3fv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord3i(GLint s, GLint t, GLint r)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord3i);

	trampoline(s, t, r);
}
HOOK_EXPORT void WINAPI glTexCoord3iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord3iv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord3s(GLshort s, GLshort t, GLshort r)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord3s);

	trampoline(s, t, r);
}
HOOK_EXPORT void WINAPI glTexCoord3sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord3sv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord4d);

	trampoline(s, t, r, q);
}
HOOK_EXPORT void WINAPI glTexCoord4dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord4dv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord4f);

	trampoline(s, t, r, q);
}
HOOK_EXPORT void WINAPI glTexCoord4fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord4fv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord4i(GLint s, GLint t, GLint r, GLint q)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord4i);

	trampoline(s, t, r, q);
}
HOOK_EXPORT void WINAPI glTexCoord4iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord4iv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord4s);

	trampoline(s, t, r, q);
}
HOOK_EXPORT void WINAPI glTexCoord4sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoord4sv);

	trampoline(v);
}

HOOK_EXPORT void WINAPI glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(&glTexCoordPointer);

	trampoline(size, type, stride, pointer);
}

HOOK_EXPORT void WINAPI glTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(&glTexEnvf);

	trampoline(target, pname, param);
}
HOOK_EXPORT void WINAPI glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(&glTexEnvfv);

	trampoline(target, pname, params);
}
HOOK_EXPORT void WINAPI glTexEnvi(GLenum target, GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(&glTexEnvi);

	trampoline(target, pname, param);
}
HOOK_EXPORT void WINAPI glTexEnviv(GLenum target, GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(&glTexEnviv);

	trampoline(target, pname, params);
}

HOOK_EXPORT void WINAPI glTexGend(GLenum coord, GLenum pname, GLdouble param)
{
	static const auto trampoline = reshade::hooks::call(&glTexGend);

	trampoline(coord, pname, param);
}
HOOK_EXPORT void WINAPI glTexGendv(GLenum coord, GLenum pname, const GLdouble *params)
{
	static const auto trampoline = reshade::hooks::call(&glTexGendv);

	trampoline(coord, pname, params);
}
HOOK_EXPORT void WINAPI glTexGenf(GLenum coord, GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(&glTexGenf);

	trampoline(coord, pname, param);
}
HOOK_EXPORT void WINAPI glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(&glTexGenfv);

	trampoline(coord, pname, params);
}
HOOK_EXPORT void WINAPI glTexGeni(GLenum coord, GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(&glTexGeni);

	trampoline(coord, pname, param);
}
HOOK_EXPORT void WINAPI glTexGeniv(GLenum coord, GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(&glTexGeniv);

	trampoline(coord, pname, params);
}

HOOK_EXPORT void WINAPI glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(&glTexImage1D);

	switch (internalformat)
	{
		case GL_RED:
			internalformat = GL_R8;
			break;
		case GL_RG:
			internalformat = GL_RG8;
			break;
		case GL_RGB:
			internalformat = GL_RGB8;
			break;
		case GL_RGBA:
			internalformat = GL_RGBA8;
			break;
		case GL_DEPTH_COMPONENT:
			internalformat = GL_DEPTH_COMPONENT16;
			break;
		case GL_DEPTH_STENCIL:
			internalformat = GL_DEPTH24_STENCIL8;
			break;
	}

	trampoline(target, level, internalformat, width, border, format, type, pixels);
}
HOOK_EXPORT void WINAPI glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(&glTexImage2D);

	switch (internalformat)
	{
		case GL_RED:
			internalformat = GL_R8;
			break;
		case GL_RG:
			internalformat = GL_RG8;
			break;
		case GL_RGB:
			internalformat = GL_RGB8;
			break;
		case GL_RGBA:
			internalformat = GL_RGBA8;
			break;
		case GL_DEPTH_COMPONENT:
			internalformat = GL_DEPTH_COMPONENT16;
			break;
		case GL_DEPTH_STENCIL:
			internalformat = GL_DEPTH24_STENCIL8;
			break;
	}

	trampoline(target, level, internalformat, width, height, border, format, type, pixels);
}
extern "C"  void WINAPI glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(&glTexImage3D);

	switch (internalformat)
	{
		case GL_RED:
			internalformat = GL_R8;
			break;
		case GL_RG:
			internalformat = GL_RG8;
			break;
		case GL_RGB:
			internalformat = GL_RGB8;
			break;
		case GL_RGBA:
			internalformat = GL_RGBA8;
			break;
		case GL_DEPTH_COMPONENT:
			internalformat = GL_DEPTH_COMPONENT16;
			break;
		case GL_DEPTH_STENCIL:
			internalformat = GL_DEPTH24_STENCIL8;
			break;
	}

	trampoline(target, level, internalformat, width, height, depth, border, format, type, pixels);
}

HOOK_EXPORT void WINAPI glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(&glTexParameterf);

	trampoline(target, pname, param);
}
HOOK_EXPORT void WINAPI glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(&glTexParameterfv);

	trampoline(target, pname, params);
}
HOOK_EXPORT void WINAPI glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(&glTexParameteri);

	trampoline(target, pname, param);
}
HOOK_EXPORT void WINAPI glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(&glTexParameteriv);

	trampoline(target, pname, params);
}

HOOK_EXPORT void WINAPI glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(&glTexSubImage1D);

	trampoline(target, level, xoffset, width, format, type, pixels);
}
HOOK_EXPORT void WINAPI glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(&glTexSubImage2D);

	trampoline(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

HOOK_EXPORT void WINAPI glTranslated(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = reshade::hooks::call(&glTranslated);

	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = reshade::hooks::call(&glTranslatef);

	trampoline(x, y, z);
}

HOOK_EXPORT void WINAPI glVertex2d(GLdouble x, GLdouble y)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 2;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex2d);

	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glVertex2dv(const GLdouble *v)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 2;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex2dv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex2f(GLfloat x, GLfloat y)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 2;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex2f);

	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glVertex2fv(const GLfloat *v)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 2;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex2fv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex2i(GLint x, GLint y)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 2;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex2i);

	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glVertex2iv(const GLint *v)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 2;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex2iv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex2s(GLshort x, GLshort y)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 2;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex2s);

	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glVertex2sv(const GLshort *v)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 2;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex2sv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex3d(GLdouble x, GLdouble y, GLdouble z)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 3;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex3d);

	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glVertex3dv(const GLdouble *v)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 3;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex3dv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 3;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex3f);

	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glVertex3fv(const GLfloat *v)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 3;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex3fv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex3i(GLint x, GLint y, GLint z)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 3;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex3i);

	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glVertex3iv(const GLint *v)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 3;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex3iv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex3s(GLshort x, GLshort y, GLshort z)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 3;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex3s);

	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glVertex3sv(const GLshort *v)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 3;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex3sv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 4;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex4d);

	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glVertex4dv(const GLdouble *v)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 4;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex4dv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 4;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex4f);

	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glVertex4fv(const GLfloat *v)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 4;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex4fv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex4i(GLint x, GLint y, GLint z, GLint w)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 4;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex4i);

	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glVertex4iv(const GLint *v)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 4;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex4iv);

	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 4;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex4s);

	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glVertex4sv(const GLshort *v)
{
	if (const auto it = g_opengl_runtimes.find(wglGetCurrentDC()); it != g_opengl_runtimes.end())
	{
		it->second->_current_vertex_count += 4;
	}

	static const auto trampoline = reshade::hooks::call(&glVertex4sv);

	trampoline(v);
}

HOOK_EXPORT void WINAPI glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(&glVertexPointer);

	trampoline(size, type, stride, pointer);
}

HOOK_EXPORT void WINAPI glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(&glViewport);

	trampoline(x, y, width, height);
}
