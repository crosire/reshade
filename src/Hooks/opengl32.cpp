#include "Log.hpp"
#include "Runtime.hpp"
#include "HookManager.hpp"

#include <gl\gl.h>
#include <unordered_map>

#undef glBindTexture
#undef glBlendFunc
#undef glClear
#undef glClearColor
#undef glClearDepth
#undef glClearStencil
#undef glColorMask
#undef glCopyTexImage1D
#undef glCopyTexImage2D
#undef glCopyTexSubImage1D
#undef glCopyTexSubImage2D
#undef glCullFace
#undef glDeleteTextures
#undef glDepthFunc
#undef glDepthMask
#undef glDepthRange
#undef glDisable
#undef glDrawArrays
#undef glDrawBuffer
#undef glDrawElements
#undef glEnable
#undef glFinish
#undef glFlush
#undef glFrontFace
#undef glGenTextures
#undef glGetBooleanv
#undef glGetDoublev
#undef glGetFloatv
#undef glGetIntegerv
#undef glGetError
#undef glGetPointerv
#undef glGetString
#undef glGetTexImage
#undef glGetTexLevelParameterfv
#undef glGetTexLevelParameteriv
#undef glGetTexParameterfv
#undef glGetTexParameteriv
#undef glHint
#undef glIsEnabled
#undef glIsTexture
#undef glLineWidth
#undef glLogicOp
#undef glPixelStoref
#undef glPixelStorei
#undef glPointSize
#undef glPolygonMode
#undef glPolygonOffset
#undef glReadBuffer
#undef glReadPixels
#undef glScissor
#undef glStencilFunc
#undef glStencilMask
#undef glStencilOp
#undef glTexImage1D
#undef glTexImage2D
#undef glTexParameterf
#undef glTexParameterfv
#undef glTexParameteri
#undef glTexParameteriv
#undef glTexSubImage1D
#undef glTexSubImage2D
#undef glViewport

// -----------------------------------------------------------------------------------------------------

namespace
{
	class														CriticalSection
	{
	public:
		struct													Lock
		{
			inline Lock(CriticalSection &cs) : CS(cs)
			{
				this->CS.Enter();
			}
			inline ~Lock(void)
			{
				this->CS.Leave();
			}

			CriticalSection &									CS;

		private:
			void												operator =(const Lock &);
		};

	public:
		inline CriticalSection(void)
		{
			::InitializeCriticalSection(&this->mCS);
		}
		inline ~CriticalSection(void)
		{
			::DeleteCriticalSection(&this->mCS);
		}

		inline void												Enter(void)
		{
			::EnterCriticalSection(&this->mCS);
		}
		inline void												Leave(void)
		{
			::LeaveCriticalSection(&this->mCS);
		}

	private:
		CRITICAL_SECTION										mCS;
	};
	
	std::unordered_map<HGLRC, std::shared_ptr<ReShade::Runtime>> sManagers;
	std::unordered_map<HGLRC, HDC>								sDeviceContexts;
	std::unordered_map<HWND, RECT>								sWindowRects;
	CriticalSection												sCS;

	std::unordered_map<HDC, ReShade::Runtime *>					sCurrentManagers;
	__declspec(thread) HGLRC									sCurrentRenderContext = nullptr;
	__declspec(thread) HDC										sCurrentDeviceContext = nullptr;
}
namespace ReShade
{
	extern std::shared_ptr<ReShade::Runtime>					CreateEffectRuntime(HDC hdc, HGLRC hglrc);
}

// -----------------------------------------------------------------------------------------------------

// GL
EXPORT void APIENTRY											glAccum(GLenum op, GLfloat value)
{
	static const auto trampoline = ReHook::Call(&glAccum);

	trampoline(op, value);
}
EXPORT void APIENTRY											glAlphaFunc(GLenum func, GLclampf ref)
{
	static const auto trampoline = ReHook::Call(&glAlphaFunc);

	static GLenum lfunc = GL_ALWAYS;
	static GLclampf lref = 0;

	if (func == lfunc && ref == lref)
	{
		return;
	}
	else
	{
		lfunc = func;
		lref = ref;
	}

	trampoline(func, ref);
}
EXPORT GLboolean APIENTRY										glAreTexturesResident(GLsizei n, const GLuint *textures, GLboolean *residences)
{
	static const auto trampoline = ReHook::Call(&glAreTexturesResident);

	return trampoline(n, textures, residences);
}
EXPORT void APIENTRY											glArrayElement(GLint i)
{
	static const auto trampoline = ReHook::Call(&glArrayElement);

	trampoline(i);
}
EXPORT void APIENTRY											glBegin(GLenum mode)
{
	static const auto trampoline = ReHook::Call(&glBegin);

	trampoline(mode);
}
EXPORT void APIENTRY											glBindTexture(GLenum target, GLuint texture)
{
	static const auto trampoline = ReHook::Call(&glBindTexture);

	trampoline(target, texture);
}
EXPORT void APIENTRY											glBitmap(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap)
{
	static const auto trampoline = ReHook::Call(&glBitmap);

	trampoline(width, height, xorig, yorig, xmove, ymove, bitmap);
}
EXPORT void APIENTRY											glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	static const auto trampoline = ReHook::Call(&glBlendFunc);

	trampoline(sfactor, dfactor);
}
EXPORT void APIENTRY											glCallList(GLuint list)
{
	static const auto trampoline = ReHook::Call(&glCallList);

	trampoline(list);
}
EXPORT void APIENTRY											glCallLists(GLsizei n, GLenum type, const GLvoid *lists)
{
	static const auto trampoline = ReHook::Call(&glCallLists);

	trampoline(n, type, lists);
}
EXPORT void APIENTRY											glClear(GLbitfield mask)
{
	static const auto trampoline = ReHook::Call(&glClear);

	trampoline(mask);
}
EXPORT void APIENTRY											glClearAccum(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	static const auto trampoline = ReHook::Call(&glClearAccum);

	trampoline(red, green, blue, alpha);
}
EXPORT void APIENTRY											glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	static const auto trampoline = ReHook::Call(&glClearColor);

	trampoline(red, green, blue, alpha);
}
EXPORT void APIENTRY											glClearDepth(GLclampd depth)
{
	static const auto trampoline = ReHook::Call(&glClearDepth);

	trampoline(depth);
}
EXPORT void APIENTRY											glClearIndex(GLfloat c)
{
	static const auto trampoline = ReHook::Call(&glClearIndex);

	trampoline(c);
}
EXPORT void APIENTRY											glClearStencil(GLint s)
{
	static const auto trampoline = ReHook::Call(&glClearStencil);

	trampoline(s);
}
EXPORT void APIENTRY											glClipPlane(GLenum plane, const GLdouble *equation)
{
	static const auto trampoline = ReHook::Call(&glClipPlane);

	trampoline(plane, equation);
}
EXPORT void APIENTRY											glColor3b(GLbyte red, GLbyte green, GLbyte blue)
{
	static const auto trampoline = ReHook::Call(&glColor3b);

	trampoline(red, green, blue);
}
EXPORT void APIENTRY											glColor3bv(const GLbyte *v)
{
	static const auto trampoline = ReHook::Call(&glColor3bv);

	trampoline(v);
}
EXPORT void APIENTRY											glColor3d(GLdouble red, GLdouble green, GLdouble blue)
{
	static const auto trampoline = ReHook::Call(&glColor3d);

	trampoline(red, green, blue);
}
EXPORT void APIENTRY											glColor3dv(const GLdouble *v)
{
	static const auto trampoline = ReHook::Call(&glColor3dv);

	trampoline(v);
}
EXPORT void APIENTRY											glColor3f(GLfloat red, GLfloat green, GLfloat blue)
{
	static const auto trampoline = ReHook::Call(&glColor3f);

	trampoline(red, green, blue);
}
EXPORT void APIENTRY											glColor3fv(const GLfloat *v)
{
	static const auto trampoline = ReHook::Call(&glColor3fv);

	trampoline(v);
}
EXPORT void APIENTRY											glColor3i(GLint red, GLint green, GLint blue)
{
	static const auto trampoline = ReHook::Call(&glColor3i);

	trampoline(red, green, blue);
}
EXPORT void APIENTRY											glColor3iv(const GLint *v)
{
	static const auto trampoline = ReHook::Call(&glColor3iv);

	trampoline(v);
}
EXPORT void APIENTRY											glColor3s(GLshort red, GLshort green, GLshort blue)
{
	static const auto trampoline = ReHook::Call(&glColor3s);

	trampoline(red, green, blue);
}
EXPORT void APIENTRY											glColor3sv(const GLshort *v)
{
	static const auto trampoline = ReHook::Call(&glColor3sv);

	trampoline(v);
}
EXPORT void APIENTRY											glColor3ub(GLubyte red, GLubyte green, GLubyte blue)
{
	static const auto trampoline = ReHook::Call(&glColor3ub);

	trampoline(red, green, blue);
}
EXPORT void APIENTRY											glColor3ubv(const GLubyte *v)
{
	static const auto trampoline = ReHook::Call(&glColor3ubv);

	trampoline(v);
}
EXPORT void APIENTRY											glColor3ui(GLuint red, GLuint green, GLuint blue)
{
	static const auto trampoline = ReHook::Call(&glColor3ui);

	trampoline(red, green, blue);
}
EXPORT void APIENTRY											glColor3uiv(const GLuint *v)
{
	static const auto trampoline = ReHook::Call(&glColor3uiv);

	trampoline(v);
}
EXPORT void APIENTRY											glColor3us(GLushort red, GLushort green, GLushort blue)
{
	static const auto trampoline = ReHook::Call(&glColor3us);

	trampoline(red, green, blue);
}
EXPORT void APIENTRY											glColor3usv(const GLushort *v)
{
	static const auto trampoline = ReHook::Call(&glColor3usv);

	trampoline(v);
}
EXPORT void APIENTRY											glColor4b(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
	static const auto trampoline = ReHook::Call(&glColor4b);

	trampoline(red, green, blue, alpha);
}
EXPORT void APIENTRY											glColor4bv(const GLbyte *v)
{
	static const auto trampoline = ReHook::Call(&glColor4bv);

	trampoline(v);
}
EXPORT void APIENTRY											glColor4d(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
	static const auto trampoline = ReHook::Call(&glColor4d);

	trampoline(red, green, blue, alpha);
}
EXPORT void APIENTRY											glColor4dv(const GLdouble *v)
{
	static const auto trampoline = ReHook::Call(&glColor4dv);

	trampoline(v);
}
EXPORT void APIENTRY											glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	static const auto trampoline = ReHook::Call(&glColor4f);

	trampoline(red, green, blue, alpha);
}
EXPORT void APIENTRY											glColor4fv(const GLfloat *v)
{
	static const auto trampoline = ReHook::Call(&glColor4fv);

	trampoline(v);
}
EXPORT void APIENTRY											glColor4i(GLint red, GLint green, GLint blue, GLint alpha)
{
	static const auto trampoline = ReHook::Call(&glColor4i);

	trampoline(red, green, blue, alpha);
}
EXPORT void APIENTRY											glColor4iv(const GLint *v)
{
	static const auto trampoline = ReHook::Call(&glColor4iv);

	trampoline(v);
}
EXPORT void APIENTRY											glColor4s(GLshort red, GLshort green, GLshort blue, GLshort alpha)
{
	static const auto trampoline = ReHook::Call(&glColor4s);

	trampoline(red, green, blue, alpha);
}
EXPORT void APIENTRY											glColor4sv(const GLshort *v)
{
	static const auto trampoline = ReHook::Call(&glColor4sv);

	trampoline(v);
}
EXPORT void APIENTRY											glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
	static const auto trampoline = ReHook::Call(&glColor4ub);

	trampoline(red, green, blue, alpha);
}
EXPORT void APIENTRY											glColor4ubv(const GLubyte *v)
{
	static const auto trampoline = ReHook::Call(&glColor4ubv);

	trampoline(v);
}
EXPORT void APIENTRY											glColor4ui(GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
	static const auto trampoline = ReHook::Call(&glColor4ui);

	trampoline(red, green, blue, alpha);
}
EXPORT void APIENTRY											glColor4uiv(const GLuint *v)
{
	static const auto trampoline = ReHook::Call(&glColor4uiv);

	trampoline(v);
}
EXPORT void APIENTRY											glColor4us(GLushort red, GLushort green, GLushort blue, GLushort alpha)
{
	static const auto trampoline = ReHook::Call(&glColor4us);

	trampoline(red, green, blue, alpha);
}
EXPORT void APIENTRY											glColor4usv(const GLushort *v)
{
	static const auto trampoline = ReHook::Call(&glColor4usv);

	trampoline(v);
}
EXPORT void APIENTRY											glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	static const auto trampoline = ReHook::Call(&glColorMask);

	trampoline(red, green, blue, alpha);
}
EXPORT void APIENTRY											glColorMaterial(GLenum face, GLenum mode)
{
	static const auto trampoline = ReHook::Call(&glColorMaterial);

	trampoline(face, mode);
}
EXPORT void APIENTRY											glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = ReHook::Call(&glColorPointer);

	trampoline(size, type, stride, pointer);
}
EXPORT void APIENTRY											glCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
	static const auto trampoline = ReHook::Call(&glCopyPixels);

	trampoline(x, y, width, height, type);
}
EXPORT void APIENTRY											glCopyTexImage1D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border)
{
	static const auto trampoline = ReHook::Call(&glCopyTexImage1D);

	trampoline(target, level, internalFormat, x, y, width, border);
}
EXPORT void APIENTRY											glCopyTexImage2D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
	static const auto trampoline = ReHook::Call(&glCopyTexImage2D);

	trampoline(target, level, internalFormat, x, y, width, height, border);
}
EXPORT void APIENTRY											glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
	static const auto trampoline = ReHook::Call(&glCopyTexSubImage1D);

	trampoline(target, level, xoffset, x, y, width);
}
EXPORT void APIENTRY											glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
	static const auto trampoline = ReHook::Call(&glCopyTexSubImage2D);

	trampoline(target, level, xoffset, yoffset, x, y, width, height);
}
EXPORT void APIENTRY											glCullFace(GLenum mode)
{
	static const auto trampoline = ReHook::Call(&glCullFace);

	trampoline(mode);
}
EXPORT void APIENTRY											glDeleteLists(GLuint list, GLsizei range)
{
	static const auto trampoline = ReHook::Call(&glDeleteLists);

	trampoline(list, range);
}
EXPORT void APIENTRY											glDeleteTextures(GLsizei n, const GLuint *textures)
{
	static const auto trampoline = ReHook::Call(&glDeleteTextures);

	trampoline(n, textures);
}
EXPORT void APIENTRY											glDepthFunc(GLenum func)
{
	static const auto trampoline = ReHook::Call(&glDepthFunc);

	trampoline(func);
}
EXPORT void APIENTRY											glDepthMask(GLboolean flag)
{
	static const auto trampoline = ReHook::Call(&glDepthMask);

	trampoline(flag);
}
EXPORT void APIENTRY											glDepthRange(GLclampd zNear, GLclampd zFar)
{
	static const auto trampoline = ReHook::Call(&glDepthRange);

	trampoline(zNear, zFar);
}
EXPORT void APIENTRY											glDisable(GLenum cap)
{
	static const auto trampoline = ReHook::Call(&glDisable);

	trampoline(cap);
}
EXPORT void APIENTRY											glDisableClientState(GLenum array)
{
	static const auto trampoline = ReHook::Call(&glDisableClientState);

	trampoline(array);
}
EXPORT void APIENTRY											glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	static const auto trampoline = ReHook::Call(&glDrawArrays);

	trampoline(mode, first, count);
}
EXPORT void APIENTRY											glDrawBuffer(GLenum mode)
{
	static const auto trampoline = ReHook::Call(&glDrawBuffer);

	trampoline(mode);
}
EXPORT void APIENTRY											glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
	static const auto trampoline = ReHook::Call(&glDrawElements);

	trampoline(mode, count, type, indices);
}
EXPORT void APIENTRY											glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = ReHook::Call(&glDrawPixels);

	trampoline(width, height, format, type, pixels);
}
EXPORT void APIENTRY											glEdgeFlag(GLboolean flag)
{
	static const auto trampoline = ReHook::Call(&glEdgeFlag);

	trampoline(flag);
}
EXPORT void APIENTRY											glEdgeFlagPointer(GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = ReHook::Call(&glEdgeFlagPointer);

	trampoline(stride, pointer);
}
EXPORT void APIENTRY											glEdgeFlagv(const GLboolean *flag)
{
	static const auto trampoline = ReHook::Call(&glEdgeFlagv);

	trampoline(flag);
}
EXPORT void APIENTRY											glEnable(GLenum cap)
{
	static const auto trampoline = ReHook::Call(&glEnable);

	trampoline(cap);
}
EXPORT void APIENTRY											glEnableClientState(GLenum array)
{
	static const auto trampoline = ReHook::Call(&glEnableClientState);

	trampoline(array);
}
EXPORT void APIENTRY											glEnd(void)
{
	static const auto trampoline = ReHook::Call(&glEnd);

	trampoline();
}
EXPORT void APIENTRY											glEndList(void)
{
	static const auto trampoline = ReHook::Call(&glEndList);

	trampoline();
}
EXPORT void APIENTRY											glEvalCoord1d(GLdouble u)
{
	static const auto trampoline = ReHook::Call(&glEvalCoord1d);

	trampoline(u);
}
EXPORT void APIENTRY											glEvalCoord1dv(const GLdouble *u)
{
	static const auto trampoline = ReHook::Call(&glEvalCoord1dv);

	trampoline(u);
}
EXPORT void APIENTRY											glEvalCoord1f(GLfloat u)
{
	static const auto trampoline = ReHook::Call(&glEvalCoord1f);

	trampoline(u);
}
EXPORT void APIENTRY											glEvalCoord1fv(const GLfloat *u)
{
	static const auto trampoline = ReHook::Call(&glEvalCoord1fv);

	trampoline(u);
}
EXPORT void APIENTRY											glEvalCoord2d(GLdouble u, GLdouble v)
{
	static const auto trampoline = ReHook::Call(&glEvalCoord2d);

	trampoline(u, v);
}
EXPORT void APIENTRY											glEvalCoord2dv(const GLdouble *u)
{
	static const auto trampoline = ReHook::Call(&glEvalCoord2dv);

	trampoline(u);
}
EXPORT void APIENTRY											glEvalCoord2f(GLfloat u, GLfloat v)
{
	static const auto trampoline = ReHook::Call(&glEvalCoord2f);

	trampoline(u, v);
}
EXPORT void APIENTRY											glEvalCoord2fv(const GLfloat *u)
{
	static const auto trampoline = ReHook::Call(&glEvalCoord2fv);

	trampoline(u);
}
EXPORT void APIENTRY											glEvalMesh1(GLenum mode, GLint i1, GLint i2)
{
	static const auto trampoline = ReHook::Call(&glEvalMesh1);

	trampoline(mode, i1, i2);
}
EXPORT void APIENTRY											glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
	static const auto trampoline = ReHook::Call(&glEvalMesh2);

	trampoline(mode, i1, i2, j1, j2);
}
EXPORT void APIENTRY											glEvalPoint1(GLint i)
{
	static const auto trampoline = ReHook::Call(&glEvalPoint1);

	trampoline(i);
}
EXPORT void APIENTRY											glEvalPoint2(GLint i, GLint j)
{
	static const auto trampoline = ReHook::Call(&glEvalPoint2);

	trampoline(i, j);
}
EXPORT void APIENTRY											glFeedbackBuffer(GLsizei size, GLenum type, GLfloat *buffer)
{
	static const auto trampoline = ReHook::Call(&glFeedbackBuffer);

	trampoline(size, type, buffer);
}
EXPORT void APIENTRY											glFinish(void)
{
	static const auto trampoline = ReHook::Call(&glFinish);

	trampoline();
}
EXPORT void APIENTRY											glFlush(void)
{
	static const auto trampoline = ReHook::Call(&glFlush);

	trampoline();
}
EXPORT void APIENTRY											glFogf(GLenum pname, GLfloat param)
{
	static const auto trampoline = ReHook::Call(&glFogf);

	trampoline(pname, param);
}
EXPORT void APIENTRY											glFogfv(GLenum pname, const GLfloat *params)
{
	static const auto trampoline = ReHook::Call(&glFogfv);

	trampoline(pname, params);
}
EXPORT void APIENTRY											glFogi(GLenum pname, GLint param)
{
	static const auto trampoline = ReHook::Call(&glFogi);

	trampoline(pname, param);
}
EXPORT void APIENTRY											glFogiv(GLenum pname, const GLint *params)
{
	static const auto trampoline = ReHook::Call(&glFogiv);

	trampoline(pname, params);
}
EXPORT void APIENTRY											glFrontFace(GLenum mode)
{
	static const auto trampoline = ReHook::Call(&glFrontFace);

	trampoline(mode);
}
EXPORT void APIENTRY											glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	static const auto trampoline = ReHook::Call(&glFrustum);

	trampoline(left, right, bottom, top, zNear, zFar);
}
EXPORT GLuint APIENTRY											glGenLists(GLsizei range)
{
	static const auto trampoline = ReHook::Call(&glGenLists);

	return trampoline(range);
}
EXPORT void APIENTRY											glGenTextures(GLsizei n, GLuint *textures)
{
	static const auto trampoline = ReHook::Call(&glGenTextures);

	trampoline(n, textures);
}
EXPORT void APIENTRY											glGetBooleanv(GLenum pname, GLboolean *params)
{
	static const auto trampoline = ReHook::Call(&glGetBooleanv);

	trampoline(pname, params);
}
EXPORT void APIENTRY											glGetDoublev(GLenum pname, GLdouble *params)
{
	static const auto trampoline = ReHook::Call(&glGetDoublev);

	trampoline(pname, params);
}
EXPORT void APIENTRY											glGetFloatv(GLenum pname, GLfloat *params)
{
	static const auto trampoline = ReHook::Call(&glGetFloatv);

	trampoline(pname, params);
}
EXPORT void APIENTRY											glGetIntegerv(GLenum pname, GLint *params)
{
	static const auto trampoline = ReHook::Call(&glGetIntegerv);

	trampoline(pname, params);
}
EXPORT void APIENTRY											glGetClipPlane(GLenum plane, GLdouble *equation)
{
	static const auto trampoline = ReHook::Call(&glGetClipPlane);

	trampoline(plane, equation);
}
EXPORT GLenum APIENTRY											glGetError(void)
{
	static const auto trampoline = ReHook::Call(&glGetError);

	return trampoline();
}
EXPORT void APIENTRY											glGetLightfv(GLenum light, GLenum pname, GLfloat *params)
{
	static const auto trampoline = ReHook::Call(&glGetLightfv);

	trampoline(light, pname, params);
}
EXPORT void APIENTRY											glGetLightiv(GLenum light, GLenum pname, GLint *params)
{
	static const auto trampoline = ReHook::Call(&glGetLightiv);

	trampoline(light, pname, params);
}
EXPORT void APIENTRY											glGetMapdv(GLenum target, GLenum query, GLdouble *v)
{
	static const auto trampoline = ReHook::Call(&glGetMapdv);

	trampoline(target, query, v);
}
EXPORT void APIENTRY											glGetMapfv(GLenum target, GLenum query, GLfloat *v)
{
	static const auto trampoline = ReHook::Call(&glGetMapfv);

	trampoline(target, query, v);
}
EXPORT void APIENTRY											glGetMapiv(GLenum target, GLenum query, GLint *v)
{
	static const auto trampoline = ReHook::Call(&glGetMapiv);

	trampoline(target, query, v);
}
EXPORT void APIENTRY											glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params)
{
	static const auto trampoline = ReHook::Call(&glGetMaterialfv);

	trampoline(face, pname, params);
}
EXPORT void APIENTRY											glGetMaterialiv(GLenum face, GLenum pname, GLint *params)
{
	static const auto trampoline = ReHook::Call(&glGetMaterialiv);

	trampoline(face, pname, params);
}
EXPORT void APIENTRY											glGetPixelMapfv(GLenum map, GLfloat *values)
{
	static const auto trampoline = ReHook::Call(&glGetPixelMapfv);

	trampoline(map, values);
}
EXPORT void APIENTRY											glGetPixelMapuiv(GLenum map, GLuint *values)
{
	static const auto trampoline = ReHook::Call(&glGetPixelMapuiv);

	trampoline(map, values);
}
EXPORT void APIENTRY											glGetPixelMapusv(GLenum map, GLushort *values)
{
	static const auto trampoline = ReHook::Call(&glGetPixelMapusv);

	trampoline(map, values);
}
EXPORT void APIENTRY											glGetPointerv(GLenum pname, GLvoid **params)
{
	static const auto trampoline = ReHook::Call(&glGetPointerv);

	trampoline(pname, params);
}
EXPORT void APIENTRY											glGetPolygonStipple(GLubyte *mask)
{
	static const auto trampoline = ReHook::Call(&glGetPolygonStipple);

	trampoline(mask);
}
EXPORT const GLubyte *APIENTRY									glGetString(GLenum name)
{
	static const auto trampoline = ReHook::Call(&glGetString);

	const GLubyte *res = trampoline(name);

	if (res == nullptr)
	{
		return nullptr;
	}

	if (name == GL_RENDERER)
	{
		static const std::basic_string<GLubyte> renderer = res + std::basic_string<GLubyte>(reinterpret_cast<const GLubyte *>(" + ReShade"));
		res = renderer.c_str();
	}

	return res;
}
EXPORT void APIENTRY											glGetTexEnvfv(GLenum target, GLenum pname, GLfloat *params)
{
	static const auto trampoline = ReHook::Call(&glGetTexEnvfv);

	trampoline(target, pname, params);
}
EXPORT void APIENTRY											glGetTexEnviv(GLenum target, GLenum pname, GLint *params)
{
	static const auto trampoline = ReHook::Call(&glGetTexEnviv);

	trampoline(target, pname, params);
}
EXPORT void APIENTRY											glGetTexGendv(GLenum coord, GLenum pname, GLdouble *params)
{
	static const auto trampoline = ReHook::Call(&glGetTexGendv);

	trampoline(coord, pname, params);
}
EXPORT void APIENTRY											glGetTexGenfv(GLenum coord, GLenum pname, GLfloat *params)
{
	static const auto trampoline = ReHook::Call(&glGetTexGenfv);

	trampoline(coord, pname, params);
}
EXPORT void APIENTRY											glGetTexGeniv(GLenum coord, GLenum pname, GLint *params)
{
	static const auto trampoline = ReHook::Call(&glGetTexGeniv);

	trampoline(coord, pname, params);
}
EXPORT void APIENTRY											glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels)
{
	static const auto trampoline = ReHook::Call(&glGetTexImage);

	trampoline(target, level, format, type, pixels);
}
EXPORT void APIENTRY											glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
	static const auto trampoline = ReHook::Call(&glGetTexLevelParameterfv);

	trampoline(target, level, pname, params);
}
EXPORT void APIENTRY											glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
	static const auto trampoline = ReHook::Call(&glGetTexLevelParameteriv);

	trampoline(target, level, pname, params);
}
EXPORT void APIENTRY											glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
	static const auto trampoline = ReHook::Call(&glGetTexParameterfv);

	trampoline(target, pname, params);
}
EXPORT void APIENTRY											glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
	static const auto trampoline = ReHook::Call(&glGetTexParameteriv);

	trampoline(target, pname, params);
}
EXPORT void APIENTRY											glHint(GLenum target, GLenum mode)
{
	static const auto trampoline = ReHook::Call(&glHint);

	trampoline(target, mode);
}
EXPORT void APIENTRY											glIndexMask(GLuint mask)
{
	static const auto trampoline = ReHook::Call(&glIndexMask);

	trampoline(mask);
}
EXPORT void APIENTRY											glIndexPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = ReHook::Call(&glIndexPointer);

	trampoline(type, stride, pointer);
}
EXPORT void APIENTRY											glIndexd(GLdouble c)
{
	static const auto trampoline = ReHook::Call(&glIndexd);

	trampoline(c);
}
EXPORT void APIENTRY											glIndexdv(const GLdouble *c)
{
	static const auto trampoline = ReHook::Call(&glIndexdv);

	trampoline(c);
}
EXPORT void APIENTRY											glIndexf(GLfloat c)
{
	static const auto trampoline = ReHook::Call(&glIndexf);

	trampoline(c);
}
EXPORT void APIENTRY											glIndexfv(const GLfloat *c)
{
	static const auto trampoline = ReHook::Call(&glIndexfv);

	trampoline(c);
}
EXPORT void APIENTRY											glIndexi(GLint c)
{
	static const auto trampoline = ReHook::Call(&glIndexi);

	trampoline(c);
}
EXPORT void APIENTRY											glIndexiv(const GLint *c)
{
	static const auto trampoline = ReHook::Call(&glIndexiv);

	trampoline(c);
}
EXPORT void APIENTRY											glIndexs(GLshort c)
{
	static const auto trampoline = ReHook::Call(&glIndexs);

	trampoline(c);
}
EXPORT void APIENTRY											glIndexsv(const GLshort *c)
{
	static const auto trampoline = ReHook::Call(&glIndexsv);

	trampoline(c);
}
EXPORT void APIENTRY											glIndexub(GLubyte c)
{
	static const auto trampoline = ReHook::Call(&glIndexub);

	trampoline(c);
}
EXPORT void APIENTRY											glIndexubv(const GLubyte *c)
{
	static const auto trampoline = ReHook::Call(&glIndexubv);

	trampoline(c);
}
EXPORT void APIENTRY											glInitNames(void)
{
	static const auto trampoline = ReHook::Call(&glInitNames);

	trampoline();
}
EXPORT void APIENTRY											glInterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = ReHook::Call(&glInterleavedArrays);

	trampoline(format, stride, pointer);
}
EXPORT GLboolean APIENTRY										glIsEnabled(GLenum cap)
{
	static const auto trampoline = ReHook::Call(&glIsEnabled);

	return trampoline(cap);
}
EXPORT GLboolean APIENTRY										glIsList(GLuint list)
{
	static const auto trampoline = ReHook::Call(&glIsList);

	return trampoline(list);
}
EXPORT GLboolean APIENTRY										glIsTexture(GLuint texture)
{
	static const auto trampoline = ReHook::Call(&glIsTexture);

	return trampoline(texture);
}
EXPORT void APIENTRY											glLightModelf(GLenum pname, GLfloat param)
{
	static const auto trampoline = ReHook::Call(&glLightModelf);

	trampoline(pname, param);
}
EXPORT void APIENTRY											glLightModelfv(GLenum pname, const GLfloat *params)
{
	static const auto trampoline = ReHook::Call(&glLightModelfv);

	trampoline(pname, params);
}
EXPORT void APIENTRY											glLightModeli(GLenum pname, GLint param)
{
	static const auto trampoline = ReHook::Call(&glLightModeli);

	trampoline(pname, param);
}
EXPORT void APIENTRY											glLightModeliv(GLenum pname, const GLint *params)
{
	static const auto trampoline = ReHook::Call(&glLightModeliv);

	trampoline(pname, params);
}
EXPORT void APIENTRY											glLightf(GLenum light, GLenum pname, GLfloat param)
{
	static const auto trampoline = ReHook::Call(&glLightf);

	trampoline(light, pname, param);
}
EXPORT void APIENTRY											glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = ReHook::Call(&glLightfv);

	trampoline(light, pname, params);
}
EXPORT void APIENTRY											glLighti(GLenum light, GLenum pname, GLint param)
{
	static const auto trampoline = ReHook::Call(&glLighti);

	trampoline(light, pname, param);
}
EXPORT void APIENTRY											glLightiv(GLenum light, GLenum pname, const GLint *params)
{
	static const auto trampoline = ReHook::Call(&glLightiv);

	trampoline(light, pname, params);
}
EXPORT void APIENTRY											glLineStipple(GLint factor, GLushort pattern)
{
	static const auto trampoline = ReHook::Call(&glLineStipple);

	trampoline(factor, pattern);
}
EXPORT void APIENTRY											glLineWidth(GLfloat width)
{
	static const auto trampoline = ReHook::Call(&glLineWidth);

	trampoline(width);
}
EXPORT void APIENTRY											glListBase(GLuint base)
{
	static const auto trampoline = ReHook::Call(&glListBase);

	trampoline(base);
}
EXPORT void APIENTRY											glLoadIdentity(void)
{
	static const auto trampoline = ReHook::Call(&glLoadIdentity);

	trampoline();
}
EXPORT void APIENTRY											glLoadMatrixd(const GLdouble *m)
{
	static const auto trampoline = ReHook::Call(&glLoadMatrixd);

	trampoline(m);
}
EXPORT void APIENTRY											glLoadMatrixf(const GLfloat *m)
{
	static const auto trampoline = ReHook::Call(&glLoadMatrixf);

	trampoline(m);
}
EXPORT void APIENTRY											glLoadName(GLuint name)
{
	static const auto trampoline = ReHook::Call(&glLoadName);

	trampoline(name);
}
EXPORT void APIENTRY											glLogicOp(GLenum opcode)
{
	static const auto trampoline = ReHook::Call(&glLogicOp);

	trampoline(opcode);
}
EXPORT void APIENTRY											glMap1d(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points)
{
	static const auto trampoline = ReHook::Call(&glMap1d);

	trampoline(target, u1, u2, stride, order, points);
}
EXPORT void APIENTRY											glMap1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points)
{
	static const auto trampoline = ReHook::Call(&glMap1f);

	trampoline(target, u1, u2, stride, order, points);
}
EXPORT void APIENTRY											glMap2d(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points)
{
	static const auto trampoline = ReHook::Call(&glMap2d);

	trampoline(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}
EXPORT void APIENTRY											glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points)
{
	static const auto trampoline = ReHook::Call(&glMap2f);

	trampoline(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}
EXPORT void APIENTRY											glMapGrid1d(GLint un, GLdouble u1, GLdouble u2)
{
	static const auto trampoline = ReHook::Call(&glMapGrid1d);

	trampoline(un, u1, u2);
}
EXPORT void APIENTRY											glMapGrid1f(GLint un, GLfloat u1, GLfloat u2)
{
	static const auto trampoline = ReHook::Call(&glMapGrid1f);

	trampoline(un, u1, u2);
}
EXPORT void APIENTRY											glMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2)
{
	static const auto trampoline = ReHook::Call(&glMapGrid2d);

	trampoline(un, u1, u2, vn, v1, v2);
}
EXPORT void APIENTRY											glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{
	static const auto trampoline = ReHook::Call(&glMapGrid2f);

	trampoline(un, u1, u2, vn, v1, v2);
}
EXPORT void APIENTRY											glMaterialf(GLenum face, GLenum pname, GLfloat param)
{
	static const auto trampoline = ReHook::Call(&glMaterialf);

	trampoline(face, pname, param);
}
EXPORT void APIENTRY											glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = ReHook::Call(&glMaterialfv);

	trampoline(face, pname, params);
}
EXPORT void APIENTRY											glMateriali(GLenum face, GLenum pname, GLint param)
{
	static const auto trampoline = ReHook::Call(&glMateriali);

	trampoline(face, pname, param);
}
EXPORT void APIENTRY											glMaterialiv(GLenum face, GLenum pname, const GLint *params)
{
	static const auto trampoline = ReHook::Call(&glMaterialiv);

	trampoline(face, pname, params);
}
EXPORT void APIENTRY											glMatrixMode(GLenum mode)
{
	static const auto trampoline = ReHook::Call(&glMatrixMode);

	trampoline(mode);
}
EXPORT void APIENTRY											glMultMatrixd(const GLdouble *m)
{
	static const auto trampoline = ReHook::Call(&glMultMatrixd);

	trampoline(m);
}
EXPORT void APIENTRY											glMultMatrixf(const GLfloat *m)
{
	static const auto trampoline = ReHook::Call(&glMultMatrixf);

	trampoline(m);
}
EXPORT void APIENTRY											glNewList(GLuint list, GLenum mode)
{
	static const auto trampoline = ReHook::Call(&glNewList);

	trampoline(list, mode);
}
EXPORT void APIENTRY											glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz)
{
	static const auto trampoline = ReHook::Call(&glNormal3b);

	trampoline(nx, ny, nz);
}
EXPORT void APIENTRY											glNormal3bv(const GLbyte *v)
{
	static const auto trampoline = ReHook::Call(&glNormal3bv);

	trampoline(v);
}
EXPORT void APIENTRY											glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz)
{
	static const auto trampoline = ReHook::Call(&glNormal3d);

	trampoline(nx, ny, nz);
}
EXPORT void APIENTRY											glNormal3dv(const GLdouble *v)
{
	static const auto trampoline = ReHook::Call(&glNormal3dv);

	trampoline(v);
}
EXPORT void APIENTRY											glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
	static const auto trampoline = ReHook::Call(&glNormal3f);

	trampoline(nx, ny, nz);
}
EXPORT void APIENTRY											glNormal3fv(const GLfloat *v)
{
	static const auto trampoline = ReHook::Call(&glNormal3fv);

	trampoline(v);
}
EXPORT void APIENTRY											glNormal3i(GLint nx, GLint ny, GLint nz)
{
	static const auto trampoline = ReHook::Call(&glNormal3i);

	trampoline(nx, ny, nz);
}
EXPORT void APIENTRY											glNormal3iv(const GLint *v)
{
	static const auto trampoline = ReHook::Call(&glNormal3iv);

	trampoline(v);
}
EXPORT void APIENTRY											glNormal3s(GLshort nx, GLshort ny, GLshort nz)
{
	static const auto trampoline = ReHook::Call(&glNormal3s);

	trampoline(nx, ny, nz);
}
EXPORT void APIENTRY											glNormal3sv(const GLshort *v)
{
	static const auto trampoline = ReHook::Call(&glNormal3sv);

	trampoline(v);
}
EXPORT void APIENTRY											glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = ReHook::Call(&glNormalPointer);

	trampoline(type, stride, pointer);
}
EXPORT void APIENTRY											glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	static const auto trampoline = ReHook::Call(&glOrtho);

	trampoline(left, right, bottom, top, zNear, zFar);
}
EXPORT void APIENTRY											glPassThrough(GLfloat token)
{
	static const auto trampoline = ReHook::Call(&glPassThrough);

	trampoline(token);
}
EXPORT void APIENTRY											glPixelMapfv(GLenum map, GLsizei mapsize, const GLfloat *values)
{
	static const auto trampoline = ReHook::Call(&glPixelMapfv);

	trampoline(map, mapsize, values);
}
EXPORT void APIENTRY											glPixelMapuiv(GLenum map, GLsizei mapsize, const GLuint *values)
{
	static const auto trampoline = ReHook::Call(&glPixelMapuiv);

	trampoline(map, mapsize, values);
}
EXPORT void APIENTRY											glPixelMapusv(GLenum map, GLsizei mapsize, const GLushort *values)
{
	static const auto trampoline = ReHook::Call(&glPixelMapusv);

	trampoline(map, mapsize, values);
}
EXPORT void APIENTRY											glPixelStoref(GLenum pname, GLfloat param)
{
	static const auto trampoline = ReHook::Call(&glPixelStoref);

	trampoline(pname, param);
}
EXPORT void APIENTRY											glPixelStorei(GLenum pname, GLint param)
{
	static const auto trampoline = ReHook::Call(&glPixelStorei);

	trampoline(pname, param);
}
EXPORT void APIENTRY											glPixelTransferf(GLenum pname, GLfloat param)
{
	static const auto trampoline = ReHook::Call(&glPixelTransferf);

	trampoline(pname, param);
}
EXPORT void APIENTRY											glPixelTransferi(GLenum pname, GLint param)
{
	static const auto trampoline = ReHook::Call(&glPixelTransferi);

	trampoline(pname, param);
}
EXPORT void APIENTRY											glPixelZoom(GLfloat xfactor, GLfloat yfactor)
{
	static const auto trampoline = ReHook::Call(&glPixelZoom);

	trampoline(xfactor, yfactor);
}
EXPORT void APIENTRY											glPointSize(GLfloat size)
{
	static const auto trampoline = ReHook::Call(&glPointSize);

	trampoline(size);
}
EXPORT void APIENTRY											glPolygonMode(GLenum face, GLenum mode)
{
	static const auto trampoline = ReHook::Call(&glPolygonMode);

	trampoline(face, mode);
}
EXPORT void APIENTRY											glPolygonOffset(GLfloat factor, GLfloat units)
{
	static const auto trampoline = ReHook::Call(&glPolygonOffset);

	trampoline(factor, units);
}
EXPORT void APIENTRY											glPolygonStipple(const GLubyte *mask)
{
	static const auto trampoline = ReHook::Call(&glPolygonStipple);

	trampoline(mask);
}
EXPORT void APIENTRY											glPopAttrib(void)
{
	static const auto trampoline = ReHook::Call(&glPopAttrib);

	trampoline();
}
EXPORT void APIENTRY											glPopClientAttrib(void)
{
	static const auto trampoline = ReHook::Call(&glPopClientAttrib);

	trampoline();
}
EXPORT void APIENTRY											glPopMatrix(void)
{
	static const auto trampoline = ReHook::Call(&glPopMatrix);

	trampoline();
}
EXPORT void APIENTRY											glPopName(void)
{
	static const auto trampoline = ReHook::Call(&glPopName);

	trampoline();
}
EXPORT void APIENTRY											glPrioritizeTextures(GLsizei n, const GLuint *textures, const GLclampf *priorities)
{
	static const auto trampoline = ReHook::Call(&glPrioritizeTextures);

	trampoline(n, textures, priorities);
}
EXPORT void APIENTRY											glPushAttrib(GLbitfield mask)
{
	static const auto trampoline = ReHook::Call(&glPushAttrib);

	trampoline(mask);
}
EXPORT void APIENTRY											glPushClientAttrib(GLbitfield mask)
{
	static const auto trampoline = ReHook::Call(&glPushClientAttrib);

	trampoline(mask);
}
EXPORT void APIENTRY											glPushMatrix(void)
{
	static const auto trampoline = ReHook::Call(&glPushMatrix);

	trampoline();
}
EXPORT void APIENTRY											glPushName(GLuint name)
{
	static const auto trampoline = ReHook::Call(&glPushName);

	trampoline(name);
}
EXPORT void APIENTRY											glRasterPos2d(GLdouble x, GLdouble y)
{
	static const auto trampoline = ReHook::Call(&glRasterPos2d);

	trampoline(x, y);
}
EXPORT void APIENTRY											glRasterPos2dv(const GLdouble *v)
{
	static const auto trampoline = ReHook::Call(&glRasterPos2dv);

	trampoline(v);
}
EXPORT void APIENTRY											glRasterPos2f(GLfloat x, GLfloat y)
{
	static const auto trampoline = ReHook::Call(&glRasterPos2f);

	trampoline(x, y);
}
EXPORT void APIENTRY											glRasterPos2fv(const GLfloat *v)
{
	static const auto trampoline = ReHook::Call(&glRasterPos2fv);

	trampoline(v);
}
EXPORT void APIENTRY											glRasterPos2i(GLint x, GLint y)
{
	static const auto trampoline = ReHook::Call(&glRasterPos2i);

	trampoline(x, y);
}
EXPORT void APIENTRY											glRasterPos2iv(const GLint *v)
{
	static const auto trampoline = ReHook::Call(&glRasterPos2iv);

	trampoline(v);
}
EXPORT void APIENTRY											glRasterPos2s(GLshort x, GLshort y)
{
	static const auto trampoline = ReHook::Call(&glRasterPos2s);

	trampoline(x, y);
}
EXPORT void APIENTRY											glRasterPos2sv(const GLshort *v)
{
	static const auto trampoline = ReHook::Call(&glRasterPos2sv);

	trampoline(v);
}
EXPORT void APIENTRY											glRasterPos3d(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = ReHook::Call(&glRasterPos3d);

	trampoline(x, y, z);
}
EXPORT void APIENTRY											glRasterPos3dv(const GLdouble *v)
{
	static const auto trampoline = ReHook::Call(&glRasterPos3dv);

	trampoline(v);
}
EXPORT void APIENTRY											glRasterPos3f(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = ReHook::Call(&glRasterPos3f);

	trampoline(x, y, z);
}
EXPORT void APIENTRY											glRasterPos3fv(const GLfloat *v)
{
	static const auto trampoline = ReHook::Call(&glRasterPos3fv);

	trampoline(v);
}
EXPORT void APIENTRY											glRasterPos3i(GLint x, GLint y, GLint z)
{
	static const auto trampoline = ReHook::Call(&glRasterPos3i);

	trampoline(x, y, z);
}
EXPORT void APIENTRY											glRasterPos3iv(const GLint *v)
{
	static const auto trampoline = ReHook::Call(&glRasterPos3iv);

	trampoline(v);
}
EXPORT void APIENTRY											glRasterPos3s(GLshort x, GLshort y, GLshort z)
{
	static const auto trampoline = ReHook::Call(&glRasterPos3s);

	trampoline(x, y, z);
}
EXPORT void APIENTRY											glRasterPos3sv(const GLshort *v)
{
	static const auto trampoline = ReHook::Call(&glRasterPos3sv);

	trampoline(v);
}
EXPORT void APIENTRY											glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	static const auto trampoline = ReHook::Call(&glRasterPos4d);

	trampoline(x, y, z, w);
}
EXPORT void APIENTRY											glRasterPos4dv(const GLdouble *v)
{
	static const auto trampoline = ReHook::Call(&glRasterPos4dv);

	trampoline(v);
}
EXPORT void APIENTRY											glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	static const auto trampoline = ReHook::Call(&glRasterPos4f);

	trampoline(x, y, z, w);
}
EXPORT void APIENTRY											glRasterPos4fv(const GLfloat *v)
{
	static const auto trampoline = ReHook::Call(&glRasterPos4fv);

	trampoline(v);
}
EXPORT void APIENTRY											glRasterPos4i(GLint x, GLint y, GLint z, GLint w)
{
	static const auto trampoline = ReHook::Call(&glRasterPos4i);

	trampoline(x, y, z, w);
}
EXPORT void APIENTRY											glRasterPos4iv(const GLint *v)
{
	static const auto trampoline = ReHook::Call(&glRasterPos4iv);

	trampoline(v);
}
EXPORT void APIENTRY											glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w)
{
	static const auto trampoline = ReHook::Call(&glRasterPos4s);

	trampoline(x, y, z, w);
}
EXPORT void APIENTRY											glRasterPos4sv(const GLshort *v)
{
	static const auto trampoline = ReHook::Call(&glRasterPos4sv);

	trampoline(v);
}
EXPORT void APIENTRY											glReadBuffer(GLenum mode)
{
	static const auto trampoline = ReHook::Call(&glReadBuffer);

	trampoline(mode);
}
EXPORT void APIENTRY											glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
{
	static const auto trampoline = ReHook::Call(&glReadPixels);

	trampoline(x, y, width, height, format, type, pixels);
}
EXPORT void APIENTRY											glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
	static const auto trampoline = ReHook::Call(&glRectd);

	trampoline(x1, y1, x2, y2);
}
EXPORT void APIENTRY											glRectdv(const GLdouble *v1, const GLdouble *v2)
{
	static const auto trampoline = ReHook::Call(&glRectdv);

	trampoline(v1, v2);
}
EXPORT void APIENTRY											glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	static const auto trampoline = ReHook::Call(&glRectf);

	trampoline(x1, y1, x2, y2);
}
EXPORT void APIENTRY											glRectfv(const GLfloat *v1, const GLfloat *v2)
{
	static const auto trampoline = ReHook::Call(&glRectfv);

	trampoline(v1, v2);
}
EXPORT void APIENTRY											glRecti(GLint x1, GLint y1, GLint x2, GLint y2)
{
	static const auto trampoline = ReHook::Call(&glRecti);

	trampoline(x1, y1, x2, y2);
}
EXPORT void APIENTRY											glRectiv(const GLint *v1, const GLint *v2)
{
	static const auto trampoline = ReHook::Call(&glRectiv);

	trampoline(v1, v2);
}
EXPORT void APIENTRY											glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2)
{
	static const auto trampoline = ReHook::Call(&glRects);

	trampoline(x1, y1, x2, y2);
}
EXPORT void APIENTRY											glRectsv(const GLshort *v1, const GLshort *v2)
{
	static const auto trampoline = ReHook::Call(&glRectsv);

	trampoline(v1, v2);
}
EXPORT GLint APIENTRY											glRenderMode(GLenum mode)
{
	static const auto trampoline = ReHook::Call(&glRenderMode);

	return trampoline(mode);
}
EXPORT void APIENTRY											glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = ReHook::Call(&glRotated);

	trampoline(angle, x, y, z);
}
EXPORT void APIENTRY											glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = ReHook::Call(&glRotatef);

	trampoline(angle, x, y, z);
}
EXPORT void APIENTRY											glScaled(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = ReHook::Call(&glScaled);

	trampoline(x, y, z);
}
EXPORT void APIENTRY											glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = ReHook::Call(&glScalef);

	trampoline(x, y, z);
}
EXPORT void APIENTRY											glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	static const auto trampoline = ReHook::Call(&glScissor);

	trampoline(x, y, width, height);
}
EXPORT void APIENTRY											glSelectBuffer(GLsizei size, GLuint *buffer)
{
	static const auto trampoline = ReHook::Call(&glSelectBuffer);

	trampoline(size, buffer);
}
EXPORT void APIENTRY											glShadeModel(GLenum mode)
{
	static const auto trampoline = ReHook::Call(&glShadeModel);

	trampoline(mode);
}
EXPORT void APIENTRY											glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
	static const auto trampoline = ReHook::Call(&glStencilFunc);

	trampoline(func, ref, mask);
}
EXPORT void APIENTRY											glStencilMask(GLuint mask)
{
	static const auto trampoline = ReHook::Call(&glStencilMask);

	trampoline(mask);
}
EXPORT void APIENTRY											glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
	static const auto trampoline = ReHook::Call(&glStencilOp);

	trampoline(fail, zfail, zpass);
}
EXPORT void APIENTRY											glTexCoord1d(GLdouble s)
{
	static const auto trampoline = ReHook::Call(&glTexCoord1d);

	trampoline(s);
}
EXPORT void APIENTRY											glTexCoord1dv(const GLdouble *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord1dv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoord1f(GLfloat s)
{
	static const auto trampoline = ReHook::Call(&glTexCoord1f);

	trampoline(s);
}
EXPORT void APIENTRY											glTexCoord1fv(const GLfloat *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord1fv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoord1i(GLint s)
{
	static const auto trampoline = ReHook::Call(&glTexCoord1i);

	trampoline(s);
}
EXPORT void APIENTRY											glTexCoord1iv(const GLint *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord1iv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoord1s(GLshort s)
{
	static const auto trampoline = ReHook::Call(&glTexCoord1s);

	trampoline(s);
}
EXPORT void APIENTRY											glTexCoord1sv(const GLshort *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord1sv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoord2d(GLdouble s, GLdouble t)
{
	static const auto trampoline = ReHook::Call(&glTexCoord2d);

	trampoline(s, t);
}
EXPORT void APIENTRY											glTexCoord2dv(const GLdouble *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord2dv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoord2f(GLfloat s, GLfloat t)
{
	static const auto trampoline = ReHook::Call(&glTexCoord2f);

	trampoline(s, t);
}
EXPORT void APIENTRY											glTexCoord2fv(const GLfloat *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord2fv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoord2i(GLint s, GLint t)
{
	static const auto trampoline = ReHook::Call(&glTexCoord2i);

	trampoline(s, t);
}
EXPORT void APIENTRY											glTexCoord2iv(const GLint *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord2iv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoord2s(GLshort s, GLshort t)
{
	static const auto trampoline = ReHook::Call(&glTexCoord2s);

	trampoline(s, t);
}
EXPORT void APIENTRY											glTexCoord2sv(const GLshort *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord2sv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoord3d(GLdouble s, GLdouble t, GLdouble r)
{
	static const auto trampoline = ReHook::Call(&glTexCoord3d);

	trampoline(s, t, r);
}
EXPORT void APIENTRY											glTexCoord3dv(const GLdouble *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord3dv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoord3f(GLfloat s, GLfloat t, GLfloat r)
{
	static const auto trampoline = ReHook::Call(&glTexCoord3f);

	trampoline(s, t, r);
}
EXPORT void APIENTRY											glTexCoord3fv(const GLfloat *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord3fv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoord3i(GLint s, GLint t, GLint r)
{
	static const auto trampoline = ReHook::Call(&glTexCoord3i);

	trampoline(s, t, r);
}
EXPORT void APIENTRY											glTexCoord3iv(const GLint *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord3iv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoord3s(GLshort s, GLshort t, GLshort r)
{
	static const auto trampoline = ReHook::Call(&glTexCoord3s);

	trampoline(s, t, r);
}
EXPORT void APIENTRY											glTexCoord3sv(const GLshort *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord3sv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
	static const auto trampoline = ReHook::Call(&glTexCoord4d);

	trampoline(s, t, r, q);
}
EXPORT void APIENTRY											glTexCoord4dv(const GLdouble *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord4dv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
	static const auto trampoline = ReHook::Call(&glTexCoord4f);

	trampoline(s, t, r, q);
}
EXPORT void APIENTRY											glTexCoord4fv(const GLfloat *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord4fv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoord4i(GLint s, GLint t, GLint r, GLint q)
{
	static const auto trampoline = ReHook::Call(&glTexCoord4i);

	trampoline(s, t, r, q);
}
EXPORT void APIENTRY											glTexCoord4iv(const GLint *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord4iv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q)
{
	static const auto trampoline = ReHook::Call(&glTexCoord4s);

	trampoline(s, t, r, q);
}
EXPORT void APIENTRY											glTexCoord4sv(const GLshort *v)
{
	static const auto trampoline = ReHook::Call(&glTexCoord4sv);

	trampoline(v);
}
EXPORT void APIENTRY											glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = ReHook::Call(&glTexCoordPointer);

	trampoline(size, type, stride, pointer);
}
EXPORT void APIENTRY											glTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
	static const auto trampoline = ReHook::Call(&glTexEnvf);

	trampoline(target, pname, param);
}
EXPORT void APIENTRY											glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = ReHook::Call(&glTexEnvfv);

	trampoline(target, pname, params);
}
EXPORT void APIENTRY											glTexEnvi(GLenum target, GLenum pname, GLint param)
{
	static const auto trampoline = ReHook::Call(&glTexEnvi);

	trampoline(target, pname, param);
}
EXPORT void APIENTRY											glTexEnviv(GLenum target, GLenum pname, const GLint *params)
{
	static const auto trampoline = ReHook::Call(&glTexEnviv);

	trampoline(target, pname, params);
}
EXPORT void APIENTRY											glTexGend(GLenum coord, GLenum pname, GLdouble param)
{
	static const auto trampoline = ReHook::Call(&glTexGend);

	trampoline(coord, pname, param);
}
EXPORT void APIENTRY											glTexGendv(GLenum coord, GLenum pname, const GLdouble *params)
{
	static const auto trampoline = ReHook::Call(&glTexGendv);

	trampoline(coord, pname, params);
}
EXPORT void APIENTRY											glTexGenf(GLenum coord, GLenum pname, GLfloat param)
{
	static const auto trampoline = ReHook::Call(&glTexGenf);

	trampoline(coord, pname, param);
}
EXPORT void APIENTRY											glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = ReHook::Call(&glTexGenfv);

	trampoline(coord, pname, params);
}
EXPORT void APIENTRY											glTexGeni(GLenum coord, GLenum pname, GLint param)
{
	static const auto trampoline = ReHook::Call(&glTexGeni);

	trampoline(coord, pname, param);
}
EXPORT void APIENTRY											glTexGeniv(GLenum coord, GLenum pname, const GLint *params)
{
	static const auto trampoline = ReHook::Call(&glTexGeniv);

	trampoline(coord, pname, params);
}
EXPORT void APIENTRY											glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = ReHook::Call(&glTexImage1D);

	trampoline(target, level, internalformat, width, border, format, type, pixels);
}
EXPORT void APIENTRY											glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = ReHook::Call(&glTexImage2D);

	trampoline(target, level, internalformat, width, height, border, format, type, pixels);
}
EXPORT void APIENTRY											glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	static const auto trampoline = ReHook::Call(&glTexParameterf);

	trampoline(target, pname, param);
}
EXPORT void APIENTRY											glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = ReHook::Call(&glTexParameterfv);

	trampoline(target, pname, params);
}
EXPORT void APIENTRY											glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	static const auto trampoline = ReHook::Call(&glTexParameteri);

	trampoline(target, pname, param);
}
EXPORT void APIENTRY											glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
	static const auto trampoline = ReHook::Call(&glTexParameteriv);

	trampoline(target, pname, params);
}
EXPORT void APIENTRY											glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = ReHook::Call(&glTexSubImage1D);

	trampoline(target, level, xoffset, width, format, type, pixels);
}
EXPORT void APIENTRY											glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = ReHook::Call(&glTexSubImage2D);

	trampoline(target, level, xoffset, yoffset, width, height, format, type, pixels);
}
EXPORT void APIENTRY											glTranslated(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = ReHook::Call(&glTranslated);

	trampoline(x, y, z);
}
EXPORT void APIENTRY											glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = ReHook::Call(&glTranslatef);

	trampoline(x, y, z);
}
EXPORT void APIENTRY											glVertex2d(GLdouble x, GLdouble y)
{
	static const auto trampoline = ReHook::Call(&glVertex2d);

	trampoline(x, y);
}
EXPORT void APIENTRY											glVertex2dv(const GLdouble *v)
{
	static const auto trampoline = ReHook::Call(&glVertex2dv);

	trampoline(v);
}
EXPORT void APIENTRY											glVertex2f(GLfloat x, GLfloat y)
{
	static const auto trampoline = ReHook::Call(&glVertex2f);

	trampoline(x, y);
}
EXPORT void APIENTRY											glVertex2fv(const GLfloat *v)
{
	static const auto trampoline = ReHook::Call(&glVertex2fv);

	trampoline(v);
}
EXPORT void APIENTRY											glVertex2i(GLint x, GLint y)
{
	static const auto trampoline = ReHook::Call(&glVertex2i);

	trampoline(x, y);
}
EXPORT void APIENTRY											glVertex2iv(const GLint *v)
{
	static const auto trampoline = ReHook::Call(&glVertex2iv);

	trampoline(v);
}
EXPORT void APIENTRY											glVertex2s(GLshort x, GLshort y)
{
	static const auto trampoline = ReHook::Call(&glVertex2s);

	trampoline(x, y);
}
EXPORT void APIENTRY											glVertex2sv(const GLshort *v)
{
	static const auto trampoline = ReHook::Call(&glVertex2sv);

	trampoline(v);
}
EXPORT void APIENTRY											glVertex3d(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = ReHook::Call(&glVertex3d);

	trampoline(x, y, z);
}
EXPORT void APIENTRY											glVertex3dv(const GLdouble *v)
{
	static const auto trampoline = ReHook::Call(&glVertex3dv);

	trampoline(v);
}
EXPORT void APIENTRY											glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = ReHook::Call(&glVertex3f);

	trampoline(x, y, z);
}
EXPORT void APIENTRY											glVertex3fv(const GLfloat *v)
{
	static const auto trampoline = ReHook::Call(&glVertex3fv);

	trampoline(v);
}
EXPORT void APIENTRY											glVertex3i(GLint x, GLint y, GLint z)
{
	static const auto trampoline = ReHook::Call(&glVertex3i);

	trampoline(x, y, z);
}
EXPORT void APIENTRY											glVertex3iv(const GLint *v)
{
	static const auto trampoline = ReHook::Call(&glVertex3iv);

	trampoline(v);
}
EXPORT void APIENTRY											glVertex3s(GLshort x, GLshort y, GLshort z)
{
	static const auto trampoline = ReHook::Call(&glVertex3s);

	trampoline(x, y, z);
}
EXPORT void APIENTRY											glVertex3sv(const GLshort *v)
{
	static const auto trampoline = ReHook::Call(&glVertex3sv);

	trampoline(v);
}
EXPORT void APIENTRY											glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	static const auto trampoline = ReHook::Call(&glVertex4d);

	trampoline(x, y, z, w);
}
EXPORT void APIENTRY											glVertex4dv(const GLdouble *v)
{
	static const auto trampoline = ReHook::Call(&glVertex4dv);

	trampoline(v);
}
EXPORT void APIENTRY											glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	static const auto trampoline = ReHook::Call(&glVertex4f);

	trampoline(x, y, z, w);
}
EXPORT void APIENTRY											glVertex4fv(const GLfloat *v)
{
	static const auto trampoline = ReHook::Call(&glVertex4fv);

	trampoline(v);
}
EXPORT void APIENTRY											glVertex4i(GLint x, GLint y, GLint z, GLint w)
{
	static const auto trampoline = ReHook::Call(&glVertex4i);

	trampoline(x, y, z, w);
}
EXPORT void APIENTRY											glVertex4iv(const GLint *v)
{
	static const auto trampoline = ReHook::Call(&glVertex4iv);

	trampoline(v);
}
EXPORT void APIENTRY											glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w)
{
	static const auto trampoline = ReHook::Call(&glVertex4s);

	trampoline(x, y, z, w);
}
EXPORT void APIENTRY											glVertex4sv(const GLshort *v)
{
	static const auto trampoline = ReHook::Call(&glVertex4sv);

	trampoline(v);
}
EXPORT void APIENTRY											glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = ReHook::Call(&glVertexPointer);

	trampoline(size, type, stride, pointer);
}
EXPORT void APIENTRY											glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	static const auto trampoline = ReHook::Call(&glViewport);

	trampoline(x, y, width, height);
}

// WGL
EXPORT int WINAPI												wglChoosePixelFormat(HDC hdc, CONST PIXELFORMATDESCRIPTOR *ppfd)
{
	LOG(INFO) << "Redirecting '" << "wglChoosePixelFormat" << "(" << hdc << ", " << ppfd << ")' ...";

	const int format = ReHook::Call(&wglChoosePixelFormat)(hdc, ppfd);

	LOG(TRACE) << "> Returned format: " << format;

	return format;
}
EXPORT BOOL WINAPI												wglChoosePixelFormatARB(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats)
{
	LOG(INFO) << "Redirecting '" << "wglChoosePixelFormatARB" << "(" << hdc << ", " << piAttribIList << ", " << pfAttribFList << ", " << nMaxFormats << ", " << piFormats << ", " << nNumFormats << ")' ...";
	
	struct														Attrib
	{
		enum													Names
		{
			WGL_NUMBER_PIXEL_FORMATS_ARB						= 0x2000,
			WGL_DRAW_TO_WINDOW_ARB								= 0x2001,
			WGL_DRAW_TO_BITMAP_ARB								= 0x2002,
			WGL_ACCELERATION_ARB								= 0x2003,
			WGL_NEED_PALETTE_ARB								= 0x2004,
			WGL_NEED_SYSTEM_PALETTE_ARB							= 0x2005,
			WGL_SWAP_LAYER_BUFFERS_ARB							= 0x2006,
			WGL_SWAP_METHOD_ARB									= 0x2007,
			WGL_NUMBER_OVERLAYS_ARB								= 0x2008,
			WGL_NUMBER_UNDERLAYS_ARB							= 0x2009,
			WGL_TRANSPARENT_ARB									= 0x200A,
			WGL_TRANSPARENT_RED_VALUE_ARB						= 0x2037,
			WGL_TRANSPARENT_GREEN_VALUE_ARB						= 0x2038,
			WGL_TRANSPARENT_BLUE_VALUE_ARB						= 0x2039,
			WGL_TRANSPARENT_ALPHA_VALUE_ARB						= 0x203A,
			WGL_TRANSPARENT_INDEX_VALUE_ARB						= 0x203B,
			WGL_SHARE_DEPTH_ARB									= 0x200C,
			WGL_SHARE_STENCIL_ARB								= 0x200D,
			WGL_SHARE_ACCUM_ARB									= 0x200E,
			WGL_SUPPORT_GDI_ARB									= 0x200F,
			WGL_SUPPORT_OPENGL_ARB								= 0x2010,
			WGL_DOUBLE_BUFFER_ARB								= 0x2011,
			WGL_STEREO_ARB										= 0x2012,
			WGL_PIXEL_TYPE_ARB									= 0x2013,
			WGL_COLOR_BITS_ARB									= 0x2014,
			WGL_RED_BITS_ARB									= 0x2015,
			WGL_RED_SHIFT_ARB									= 0x2016,
			WGL_GREEN_BITS_ARB									= 0x2017,
			WGL_GREEN_SHIFT_ARB									= 0x2018,
			WGL_BLUE_BITS_ARB									= 0x2019,
			WGL_BLUE_SHIFT_ARB									= 0x201A,
			WGL_ALPHA_BITS_ARB									= 0x201B,
			WGL_ALPHA_SHIFT_ARB									= 0x201C,
			WGL_ACCUM_BITS_ARB									= 0x201D,
			WGL_ACCUM_RED_BITS_ARB								= 0x201E,
			WGL_ACCUM_GREEN_BITS_ARB							= 0x201F,
			WGL_ACCUM_BLUE_BITS_ARB								= 0x2020,
			WGL_ACCUM_ALPHA_BITS_ARB							= 0x2021,
			WGL_DEPTH_BITS_ARB									= 0x2022,
			WGL_STENCIL_BITS_ARB								= 0x2023,
			WGL_AUX_BUFFERS_ARB									= 0x2024,
		};
		enum													Values
		{
			WGL_NO_ACCELERATION_ARB								= 0x2025,
			WGL_GENERIC_ACCELERATION_ARB						= 0x2026,
			WGL_FULL_ACCELERATION_ARB							= 0x2027,
			WGL_SWAP_EXCHANGE_ARB								= 0x2028,
			WGL_SWAP_COPY_ARB									= 0x2029,
			WGL_SWAP_UNDEFINED_ARB								= 0x202A,
			WGL_TYPE_RGBA_ARB									= 0x202B,
			WGL_TYPE_COLORINDEX_ARB								= 0x202C,
		};
	};

	LOG(TRACE) << "> Dumping Attributes:";
	LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(TRACE) << "  | Attribute                               | Value                                   |";
	LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";

	for (const int *attrib = piAttribIList; attrib != nullptr && *attrib != 0; attrib += 2)
	{
		char line[88];
		sprintf_s(line, "  | 0x%037X | 0x%037X |", attrib[0], attrib[1]);

		LOG(TRACE) << line;
	}
	for (const FLOAT *attrib = pfAttribFList; attrib != nullptr && *attrib != 0.0f; attrib += 2)
	{
		char line[88];
		sprintf_s(line, "  | 0x%037X | %39.2f |", static_cast<int>(attrib[0]), attrib[1]);

		LOG(TRACE) << line;
	}

	LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";

	if (!ReHook::Call(&wglChoosePixelFormatARB)(hdc, piAttribIList, pfAttribFList, nMaxFormats, piFormats, nNumFormats))
	{
		LOG(WARNING) << "> 'wglChoosePixelFormatARB' failed with '" << ::GetLastError() << "'!";

		return FALSE;
	}

	assert(nNumFormats != nullptr);

	std::string formats;

	for (UINT i = 0; i < *nNumFormats && piFormats[i] != 0; ++i)
	{
		formats += " " + std::to_string(piFormats[i]);
	}

	LOG(TRACE) << "> Returned format(s):" << formats;

	return TRUE;
}
EXPORT BOOL WINAPI												wglCopyContext(HGLRC hglrcSrc, HGLRC hglrcDst, UINT mask)
{
	return ReHook::Call(&wglCopyContext)(hglrcSrc, hglrcDst, mask);
}
EXPORT HGLRC WINAPI												wglCreateContext(HDC hdc)
{
	LOG(INFO) << "Redirecting '" << "wglCreateContext" << "(" << hdc << ")' ...";

	const HGLRC hglrc = ReHook::Call(&wglCreateContext)(hdc);

	if (hglrc != nullptr)
	{
		CriticalSection::Lock lock(sCS);

		sDeviceContexts.insert(std::make_pair(hglrc, hdc));
	}
	else
	{
		LOG(WARNING) << "> 'wglCreateContext' failed with '" << ::GetLastError() << "'!";
	}

	return hglrc;
}
EXPORT HGLRC WINAPI												wglCreateContextAttribsARB(HDC hdc, HGLRC hShareContext, const int *attribList)
{
	LOG(INFO) << "Redirecting '" << "wglCreateContextAttribsARB" << "(" << hdc << ", " << hShareContext << ", " << attribList << ")' ...";

	struct Attrib
	{
		enum Names
		{
			WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091,
			WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092,
			WGL_CONTEXT_LAYER_PLANE_ARB = 0x2093,
			WGL_CONTEXT_FLAGS_ARB = 0x2094,
			WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126,
		};
		enum Values
		{
			WGL_CONTEXT_DEBUG_BIT_ARB = 0x0001,
			WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB = 0x0002,
			WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x00000001,
			WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB = 0x00000002,
		};

		int Name, Value;
	};

	int i = 0, major = 1, minor = 0, flags = 0;
	bool core = true, compatibility = false;
	Attrib attribs[7] = { 0, 0 };

	for (const int *attrib = attribList; attrib != nullptr && *attrib != 0 && i < 5; attrib += 2, ++i)
	{
		attribs[i].Name = attrib[0];
		attribs[i].Value = attrib[1];

		switch (attrib[0])
		{
			case Attrib::WGL_CONTEXT_MAJOR_VERSION_ARB:
				major = attrib[1];
				break;
			case Attrib::WGL_CONTEXT_MINOR_VERSION_ARB:
				minor = attrib[1];
				break;
			case Attrib::WGL_CONTEXT_PROFILE_MASK_ARB:
				core = (attrib[1] & Attrib::WGL_CONTEXT_CORE_PROFILE_BIT_ARB) != 0;
				compatibility = (attrib[1] & Attrib::WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB) != 0;
				break;
			case Attrib::WGL_CONTEXT_FLAGS_ARB:
				flags = attrib[1];
				break;
		}
	}

	LOG(TRACE) << "> Requesting " << (core ? "core " : compatibility ? "compatibility " : "") << "OpenGL context for version " << major << '.' << minor << " ...";

#ifdef _DEBUG
	flags |= Attrib::WGL_CONTEXT_DEBUG_BIT_ARB;
#endif

	attribs[5].Name = Attrib::WGL_CONTEXT_FLAGS_ARB;
	attribs[5].Value = flags;

	if (major < 4 || minor < 3)
	{
		LOG(WARNING) << "> Replacing requested version with 4.3 ...";

		for (int k = 0; k < i; ++k)
		{
			switch (attribs[k].Name)
			{
				case Attrib::WGL_CONTEXT_MAJOR_VERSION_ARB:
					attribs[k].Value = 4;
					break;
				case Attrib::WGL_CONTEXT_MINOR_VERSION_ARB:
					attribs[k].Value = 3;
					break;
			}
		}
	}

	const HGLRC hglrc = ReHook::Call(&wglCreateContextAttribsARB)(hdc, hShareContext, reinterpret_cast<const int *>(attribs));

	if (hglrc != nullptr)
	{
		CriticalSection::Lock lock(sCS);

		sDeviceContexts.insert(std::make_pair(hglrc, hdc));
	}
	else
	{
		LOG(WARNING) << "> 'wglCreateContextAttribsARB' failed with '" << ::GetLastError() << "'!";
	}

	return hglrc;
}
EXPORT HGLRC WINAPI												wglCreateLayerContext(HDC hdc, int iLayerPlane)
{
	LOG(INFO) << "Redirecting '" << "wglCreateLayerContext" << "(" << hdc << ", " << iLayerPlane << ")' ...";

	return ReHook::Call(&wglCreateLayerContext)(hdc, iLayerPlane);
}
EXPORT BOOL WINAPI												wglDeleteContext(HGLRC hglrc)
{
	LOG(INFO) << "Redirecting '" << "wglDeleteContext" << "(" << hglrc << ")' ...";

	CriticalSection::Lock lock(sCS);

	const auto it = sManagers.find(hglrc);

	if (it != sManagers.end())
	{
		ReHook::Call(&wglMakeCurrent)(sDeviceContexts.at(hglrc), hglrc);

		sManagers.erase(it);

		ReHook::Call(&wglMakeCurrent)(sCurrentDeviceContext, sCurrentRenderContext);
	}

	sDeviceContexts.erase(hglrc);

	if (!ReHook::Call(&wglDeleteContext)(hglrc))
	{
		LOG(WARNING) << "> 'wglDeleteContext' failed with '" << ::GetLastError() << "'!";

		return FALSE;
	}

	return TRUE;
}
EXPORT BOOL WINAPI												wglDescribeLayerPlane(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nBytes, LPLAYERPLANEDESCRIPTOR plpd)
{
	return ReHook::Call(&wglDescribeLayerPlane)(hdc, iPixelFormat, iLayerPlane, nBytes, plpd);
}
EXPORT int WINAPI												wglDescribePixelFormat(HDC hdc, int iPixelFormat, UINT nBytes, LPPIXELFORMATDESCRIPTOR ppfd)
{
	return ReHook::Call(wglDescribePixelFormat)(hdc, iPixelFormat, nBytes, ppfd);
}
EXPORT HGLRC WINAPI												wglGetCurrentContext(void)
{
	return sCurrentRenderContext;
}
EXPORT HDC WINAPI												wglGetCurrentDC(void)
{
	return sCurrentDeviceContext;
}
EXPORT int WINAPI												wglGetLayerPaletteEntries(HDC hdc, int iLayerPlane, int iStart, int cEntries, COLORREF *pcr)
{
	return ReHook::Call(&wglGetLayerPaletteEntries)(hdc, iLayerPlane, iStart, cEntries, pcr);
}
EXPORT int WINAPI												wglGetPixelFormat(HDC hdc)
{
	return ReHook::Call(&wglGetPixelFormat)(hdc);
}
EXPORT BOOL WINAPI												wglGetPixelFormatAttribivARB(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, int *piValues)
{
	return ReHook::Call(&wglGetPixelFormatAttribivARB)(hdc, iPixelFormat, iLayerPlane, nAttributes, piAttributes, piValues);
}
EXPORT BOOL WINAPI												wglGetPixelFormatAttribfvARB(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, FLOAT *pfValues)
{
	return ReHook::Call(&wglGetPixelFormatAttribfvARB)(hdc, iPixelFormat, iLayerPlane, nAttributes, piAttributes, pfValues);
}
EXPORT const char *WINAPI										wglGetExtensionsStringARB(HDC hdc)
{
	return ReHook::Call(&wglGetExtensionsStringARB)(hdc);
}
EXPORT int WINAPI												wglGetSwapIntervalEXT(void)
{
	static const auto trampoline = ReHook::Call(&wglGetSwapIntervalEXT);

	return trampoline();
}
EXPORT BOOL WINAPI												wglMakeCurrent(HDC hdc, HGLRC hglrc)
{
	static const auto trampoline = ReHook::Call(&wglMakeCurrent);

	LOG(INFO) << "Redirecting '" << "wglMakeCurrent" << "(" << hdc << ", " << hglrc << ")' ...";

	if (hdc == sCurrentDeviceContext && hglrc == sCurrentRenderContext)
	{
		return TRUE;
	}

	if (!trampoline(hdc, hglrc))
	{
		LOG(WARNING) << "> 'wglMakeCurrent' failed with '" << ::GetLastError() << "'!";

		sCurrentManagers.erase(hdc);
		sCurrentDeviceContext = nullptr;
		sCurrentRenderContext = nullptr;

		return FALSE;
	}

	if (hglrc == nullptr)
	{
		sCurrentManagers.erase(hdc);
		sCurrentDeviceContext = nullptr;
		sCurrentRenderContext = nullptr;

		return TRUE;
	}

	sCurrentDeviceContext = hdc;
	sCurrentRenderContext = hglrc;

	CriticalSection::Lock lock(sCS);

	const auto it = sManagers.find(hglrc);

	if (it != sManagers.end() && sDeviceContexts.at(hglrc) == hdc)
	{
		sCurrentManagers[hdc] = it->second.get();
	}
	else
	{
		const HWND hwnd = ::WindowFromDC(hdc);

		assert(hwnd != nullptr);

		RECT rect;
		::GetClientRect(hwnd, &rect);
		const LONG width = rect.right - rect.left, height = rect.bottom - rect.top;

		LOG(INFO) << "> Initial size is " << width << "x" << height << ".";

		const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(hdc, hglrc);

		if (runtime != nullptr)
		{
			runtime->OnCreate(static_cast<unsigned int>(width), static_cast<unsigned int>(height));
		}
		else
		{
			LOG(ERROR) << "Failed to initialize OpenGL renderer on OpenGL context " << hglrc << "! Make sure your graphics card supports at least OpenGL 4.3.";
		}

		sManagers[hglrc] = runtime;
		sDeviceContexts[hglrc] = hdc;
		sWindowRects[hwnd] = rect;
		sCurrentManagers[hdc] = runtime.get();
	}

	return TRUE;
}
EXPORT BOOL WINAPI												wglRealizeLayerPalette(HDC hdc, int iLayerPlane, BOOL b)
{
	return ReHook::Call(&wglRealizeLayerPalette)(hdc, iLayerPlane, b);
}
EXPORT int WINAPI												wglSetLayerPaletteEntries(HDC hdc, int iLayerPlane, int iStart, int cEntries, CONST COLORREF *pcr)
{
	return ReHook::Call(&wglSetLayerPaletteEntries)(hdc, iLayerPlane, iStart, cEntries, pcr);
}
EXPORT BOOL WINAPI												wglSetPixelFormat(HDC hdc, int iPixelFormat, CONST PIXELFORMATDESCRIPTOR *ppfd)
{
	LOG(INFO) << "Redirecting '" << "wglSetPixelFormat" << "(" << hdc << ", " << iPixelFormat << ", " << ppfd << ")' ...";

	if (!ReHook::Call(&wglSetPixelFormat)(hdc, iPixelFormat, ppfd))
	{
		LOG(WARNING) << "> 'wglSetPixelFormat' failed with '" << ::GetLastError() << "'!";

		return FALSE;
	}

	return TRUE;
}
EXPORT BOOL WINAPI												wglShareLists(HGLRC hglrc1, HGLRC hglrc2)
{
	return ReHook::Call(&wglShareLists)(hglrc1, hglrc2);
}
EXPORT BOOL WINAPI												wglSwapBuffers(HDC hdc)
{
	static const auto trampoline = ReHook::Call(&wglSwapBuffers);

	const auto it = sCurrentManagers.find(hdc);

	if (it != sCurrentManagers.end())
	{
		ReShade::Runtime *runtime = it->second;

		const HWND hwnd = ::WindowFromDC(hdc);
		RECT rect, &rectPrevious = sWindowRects.at(hwnd);

		assert(hwnd != nullptr);

		::GetClientRect(hwnd, &rect);
		const ULONG width = rect.right - rect.left, height = rect.bottom - rect.top;
		const ULONG widthPrevious = rectPrevious.right - rectPrevious.left, heightPrevious = rectPrevious.bottom - rectPrevious.top;

		if (width != widthPrevious || height != heightPrevious)
		{
			LOG(INFO) << "Resizing OpenGL context " << sCurrentRenderContext << " to " << width << "x" << height << " ...";

			runtime->ReCreate(width, height);

			rectPrevious = rect;
		}

		runtime->OnPostProcess();
		runtime->OnPresent();
	}

	return trampoline(hdc);
}
EXPORT BOOL WINAPI												wglSwapLayerBuffers(HDC hdc, UINT i)
{
	static const auto trampoline = ReHook::Call(&wglSwapLayerBuffers);

	return trampoline(hdc, i);
}
EXPORT DWORD WINAPI												wglSwapMultipleBuffers(UINT cNumBuffers, CONST WGLSWAP *pBuffers)
{
	for (UINT i = 0; i < cNumBuffers; ++i)
	{
		wglSwapBuffers(pBuffers[i].hdc);
	}

	return 0;
}
EXPORT BOOL WINAPI												wglSwapIntervalEXT(int interval)
{
	static const auto trampoline = ReHook::Call(&wglSwapIntervalEXT);

	return trampoline(interval);
}
EXPORT BOOL WINAPI												wglUseFontBitmapsA(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3)
{
	static const auto trampoline = ReHook::Call(&wglUseFontBitmapsA);

	return trampoline(hdc, dw1, dw2, dw3);
}
EXPORT BOOL WINAPI												wglUseFontBitmapsW(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3)
{
	static const auto trampoline = ReHook::Call(&wglUseFontBitmapsW);

	return trampoline(hdc, dw1, dw2, dw3);
}
EXPORT BOOL WINAPI												wglUseFontOutlinesA(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3, FLOAT f1, FLOAT f2, int i, LPGLYPHMETRICSFLOAT pgmf)
{
	static const auto trampoline = ReHook::Call(&wglUseFontOutlinesA);

	return trampoline(hdc, dw1, dw2, dw3, f1, f2, i, pgmf);
}
EXPORT BOOL WINAPI												wglUseFontOutlinesW(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3, FLOAT f1, FLOAT f2, int i, LPGLYPHMETRICSFLOAT pgmf)
{
	static const auto trampoline = ReHook::Call(&wglUseFontOutlinesW);

	return trampoline(hdc, dw1, dw2, dw3, f1, f2, i, pgmf);
}
EXPORT PROC WINAPI												wglGetProcAddress(LPCSTR lpszProc)
{
	const PROC address = ReHook::Call(&wglGetProcAddress)(lpszProc);

	if (address == nullptr || lpszProc == nullptr)
	{
		return nullptr;
	}
	else if (::strcmp(lpszProc, "wglChoosePixelFormatARB") == 0)
	{
		ReHook::Register(reinterpret_cast<ReHook::Hook::Function>(address), reinterpret_cast<ReHook::Hook::Function>(&wglChoosePixelFormatARB));
	}
	else if (::strcmp(lpszProc, "wglCreateContextAttribsARB") == 0)
	{
		ReHook::Register(reinterpret_cast<ReHook::Hook::Function>(address), reinterpret_cast<ReHook::Hook::Function>(&wglCreateContextAttribsARB));
	}
	else if (::strcmp(lpszProc, "wglGetPixelFormatAttribivARB") == 0)
	{
		ReHook::Register(reinterpret_cast<ReHook::Hook::Function>(address), reinterpret_cast<ReHook::Hook::Function>(&wglGetPixelFormatAttribivARB));
	}
	else if (::strcmp(lpszProc, "wglGetPixelFormatAttribfvARB") == 0)
	{
		ReHook::Register(reinterpret_cast<ReHook::Hook::Function>(address), reinterpret_cast<ReHook::Hook::Function>(&wglGetPixelFormatAttribfvARB));
	}
	else if (::strcmp(lpszProc, "wglGetExtensionsStringARB") == 0)
	{
		ReHook::Register(reinterpret_cast<ReHook::Hook::Function>(address), reinterpret_cast<ReHook::Hook::Function>(&wglGetExtensionsStringARB));
	}
	else if (::strcmp(lpszProc, "wglGetSwapIntervalEXT") == 0)
	{
		ReHook::Register(reinterpret_cast<ReHook::Hook::Function>(address), reinterpret_cast<ReHook::Hook::Function>(&wglGetSwapIntervalEXT));
	}
	else if (::strcmp(lpszProc, "wglSwapIntervalEXT") == 0)
	{
		ReHook::Register(reinterpret_cast<ReHook::Hook::Function>(address), reinterpret_cast<ReHook::Hook::Function>(&wglSwapIntervalEXT));
	}

	return address;
}