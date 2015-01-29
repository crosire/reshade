#include "Log.hpp"
#include "HookManager.hpp"
#include "Runtimes\RuntimeGL.hpp"

#include <unordered_map>

#pragma region Undefine Function Names
#undef glBindFramebuffer
#undef glBindTexture
#undef glBlendFunc
#undef glClear
#undef glClearColor
#undef glClearDepth
#undef glClearStencil
#undef glColorMask
#undef glCompileShader
#undef glCopyTexImage1D
#undef glCopyTexImage2D
#undef glCopyTexSubImage1D
#undef glCopyTexSubImage2D
#undef glCopyTexSubImage3D
#undef glCullFace
#undef glDeleteTextures
#undef glDepthFunc
#undef glDepthMask
#undef glDepthRange
#undef glDisable
#undef glDrawArrays
#undef glDrawArraysIndirect
#undef glDrawArraysInstanced
#undef glDrawArraysInstancedBaseInstance
#undef glDrawBuffer
#undef glDrawElements
#undef glDrawElementsBaseVertex
#undef glDrawElementsIndirect
#undef glDrawElementsInstanced
#undef glDrawElementsInstancedBaseVertex
#undef glDrawElementsInstancedBaseInstance
#undef glDrawElementsInstancedBaseVertexBaseInstance
#undef glDrawRangeElements
#undef glDrawRangeElementsBaseVertex
#undef glEnable
#undef glFinish
#undef glFlush
#undef glFramebufferRenderbuffer
#undef glFramebufferTexture
#undef glFramebufferTexture1D
#undef glFramebufferTexture2D
#undef glFramebufferTexture3D
#undef glFramebufferTextureLayer
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
#undef glMultiDrawArrays
#undef glMultiDrawArraysIndirect
#undef glMultiDrawElements
#undef glMultiDrawElementsBaseVertex
#undef glMultiDrawElementsIndirect
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
#undef glTexImage3D
#undef glTexParameterf
#undef glTexParameterfv
#undef glTexParameteri
#undef glTexParameteriv
#undef glTexSubImage1D
#undef glTexSubImage2D
#undef glTexSubImage3D
#undef glViewport
#pragma endregion

// -----------------------------------------------------------------------------------------------------

namespace
{
	class CriticalSection
	{
	public:
		struct Lock
		{
			Lock(CriticalSection &cs) : CS(cs)
			{
				EnterCriticalSection(&this->CS.mCS);
			}
			~Lock()
			{
				LeaveCriticalSection(&this->CS.mCS);
			}

			CriticalSection &CS;

		private:
			void operator =(const Lock &);
		};

	public:
		CriticalSection()
		{
			InitializeCriticalSection(&this->mCS);
		}
		~CriticalSection()
		{
			DeleteCriticalSection(&this->mCS);
		}

	private:
		CRITICAL_SECTION mCS;
	} sCS;
	std::unordered_map<HWND, RECT> sWindowRects;
	std::unordered_map<HGLRC, HGLRC> sSharedContexts;
	std::unordered_map<HDC, std::shared_ptr<ReShade::Runtimes::GLRuntime>> sRuntimes;
	__declspec(thread) GLuint sCurrentVertexCount = 0;
	__declspec(thread) ReShade::Runtimes::GLRuntime *sCurrentRuntime = nullptr;
}

// -----------------------------------------------------------------------------------------------------

// GL
EXPORT void WINAPI glAccum(GLenum op, GLfloat value)
{
	static const auto trampoline = ReShade::Hooks::Call(&glAccum);

	trampoline(op, value);
}
EXPORT void WINAPI glAlphaFunc(GLenum func, GLclampf ref)
{
	static const auto trampoline = ReShade::Hooks::Call(&glAlphaFunc);

	trampoline(func, ref);
}
EXPORT GLboolean WINAPI glAreTexturesResident(GLsizei n, const GLuint *textures, GLboolean *residences)
{
	static const auto trampoline = ReShade::Hooks::Call(&glAreTexturesResident);

	return trampoline(n, textures, residences);
}
EXPORT void WINAPI glArrayElement(GLint i)
{
	static const auto trampoline = ReShade::Hooks::Call(&glArrayElement);

	trampoline(i);
}
EXPORT void WINAPI glBegin(GLenum mode)
{
	static const auto trampoline = ReShade::Hooks::Call(&glBegin);

	sCurrentVertexCount = 0;

	trampoline(mode);
}
EXPORT void WINAPI glBindTexture(GLenum target, GLuint texture)
{
	static const auto trampoline = ReShade::Hooks::Call(&glBindTexture);

	trampoline(target, texture);
}
EXPORT void WINAPI glBitmap(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap)
{
	static const auto trampoline = ReShade::Hooks::Call(&glBitmap);

	trampoline(width, height, xorig, yorig, xmove, ymove, bitmap);
}
EXPORT void WINAPI glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	static const auto trampoline = ReShade::Hooks::Call(&glBlendFunc);

	trampoline(sfactor, dfactor);
}
EXPORT void WINAPI glCallList(GLuint list)
{
	static const auto trampoline = ReShade::Hooks::Call(&glCallList);

	trampoline(list);
}
EXPORT void WINAPI glCallLists(GLsizei n, GLenum type, const GLvoid *lists)
{
	static const auto trampoline = ReShade::Hooks::Call(&glCallLists);

	trampoline(n, type, lists);
}
EXPORT void WINAPI glClear(GLbitfield mask)
{
	static const auto trampoline = ReShade::Hooks::Call(&glClear);

	trampoline(mask);
}
EXPORT void WINAPI glClearAccum(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	static const auto trampoline = ReShade::Hooks::Call(&glClearAccum);

	trampoline(red, green, blue, alpha);
}
EXPORT void WINAPI glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	static const auto trampoline = ReShade::Hooks::Call(&glClearColor);

	trampoline(red, green, blue, alpha);
}
EXPORT void WINAPI glClearDepth(GLclampd depth)
{
	static const auto trampoline = ReShade::Hooks::Call(&glClearDepth);

	trampoline(depth);
}
EXPORT void WINAPI glClearIndex(GLfloat c)
{
	static const auto trampoline = ReShade::Hooks::Call(&glClearIndex);

	trampoline(c);
}
EXPORT void WINAPI glClearStencil(GLint s)
{
	static const auto trampoline = ReShade::Hooks::Call(&glClearStencil);

	trampoline(s);
}
EXPORT void WINAPI glClipPlane(GLenum plane, const GLdouble *equation)
{
	static const auto trampoline = ReShade::Hooks::Call(&glClipPlane);

	trampoline(plane, equation);
}
EXPORT void WINAPI glColor3b(GLbyte red, GLbyte green, GLbyte blue)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3b);

	trampoline(red, green, blue);
}
EXPORT void WINAPI glColor3bv(const GLbyte *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3bv);

	trampoline(v);
}
EXPORT void WINAPI glColor3d(GLdouble red, GLdouble green, GLdouble blue)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3d);

	trampoline(red, green, blue);
}
EXPORT void WINAPI glColor3dv(const GLdouble *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3dv);

	trampoline(v);
}
EXPORT void WINAPI glColor3f(GLfloat red, GLfloat green, GLfloat blue)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3f);

	trampoline(red, green, blue);
}
EXPORT void WINAPI glColor3fv(const GLfloat *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3fv);

	trampoline(v);
}
EXPORT void WINAPI glColor3i(GLint red, GLint green, GLint blue)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3i);

	trampoline(red, green, blue);
}
EXPORT void WINAPI glColor3iv(const GLint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3iv);

	trampoline(v);
}
EXPORT void WINAPI glColor3s(GLshort red, GLshort green, GLshort blue)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3s);

	trampoline(red, green, blue);
}
EXPORT void WINAPI glColor3sv(const GLshort *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3sv);

	trampoline(v);
}
EXPORT void WINAPI glColor3ub(GLubyte red, GLubyte green, GLubyte blue)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3ub);

	trampoline(red, green, blue);
}
EXPORT void WINAPI glColor3ubv(const GLubyte *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3ubv);

	trampoline(v);
}
EXPORT void WINAPI glColor3ui(GLuint red, GLuint green, GLuint blue)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3ui);

	trampoline(red, green, blue);
}
EXPORT void WINAPI glColor3uiv(const GLuint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3uiv);

	trampoline(v);
}
EXPORT void WINAPI glColor3us(GLushort red, GLushort green, GLushort blue)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3us);

	trampoline(red, green, blue);
}
EXPORT void WINAPI glColor3usv(const GLushort *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor3usv);

	trampoline(v);
}
EXPORT void WINAPI glColor4b(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4b);

	trampoline(red, green, blue, alpha);
}
EXPORT void WINAPI glColor4bv(const GLbyte *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4bv);

	trampoline(v);
}
EXPORT void WINAPI glColor4d(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4d);

	trampoline(red, green, blue, alpha);
}
EXPORT void WINAPI glColor4dv(const GLdouble *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4dv);

	trampoline(v);
}
EXPORT void WINAPI glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4f);

	trampoline(red, green, blue, alpha);
}
EXPORT void WINAPI glColor4fv(const GLfloat *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4fv);

	trampoline(v);
}
EXPORT void WINAPI glColor4i(GLint red, GLint green, GLint blue, GLint alpha)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4i);

	trampoline(red, green, blue, alpha);
}
EXPORT void WINAPI glColor4iv(const GLint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4iv);

	trampoline(v);
}
EXPORT void WINAPI glColor4s(GLshort red, GLshort green, GLshort blue, GLshort alpha)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4s);

	trampoline(red, green, blue, alpha);
}
EXPORT void WINAPI glColor4sv(const GLshort *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4sv);

	trampoline(v);
}
EXPORT void WINAPI glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4ub);

	trampoline(red, green, blue, alpha);
}
EXPORT void WINAPI glColor4ubv(const GLubyte *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4ubv);

	trampoline(v);
}
EXPORT void WINAPI glColor4ui(GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4ui);

	trampoline(red, green, blue, alpha);
}
EXPORT void WINAPI glColor4uiv(const GLuint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4uiv);

	trampoline(v);
}
EXPORT void WINAPI glColor4us(GLushort red, GLushort green, GLushort blue, GLushort alpha)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4us);

	trampoline(red, green, blue, alpha);
}
EXPORT void WINAPI glColor4usv(const GLushort *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColor4usv);

	trampoline(v);
}
EXPORT void WINAPI glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColorMask);

	trampoline(red, green, blue, alpha);
}
EXPORT void WINAPI glColorMaterial(GLenum face, GLenum mode)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColorMaterial);

	trampoline(face, mode);
}
EXPORT void WINAPI glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = ReShade::Hooks::Call(&glColorPointer);

	trampoline(size, type, stride, pointer);
}
void WINAPI glCompileShader(GLuint shader)
{
	LOG(TRACE) << "Redirecting '" << "glCompileShader" << "(" << shader << ")' ...";

	static const auto trampoline = ReShade::Hooks::Call(&glCompileShader);

	return trampoline(shader);
}
EXPORT void WINAPI glCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
	static const auto trampoline = ReShade::Hooks::Call(&glCopyPixels);

	trampoline(x, y, width, height, type);
}
EXPORT void WINAPI glCopyTexImage1D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border)
{
	static const auto trampoline = ReShade::Hooks::Call(&glCopyTexImage1D);

	trampoline(target, level, internalFormat, x, y, width, border);
}
EXPORT void WINAPI glCopyTexImage2D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
	static const auto trampoline = ReShade::Hooks::Call(&glCopyTexImage2D);

	trampoline(target, level, internalFormat, x, y, width, height, border);
}
EXPORT void WINAPI glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
	static const auto trampoline = ReShade::Hooks::Call(&glCopyTexSubImage1D);

	trampoline(target, level, xoffset, x, y, width);
}
EXPORT void WINAPI glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
	static const auto trampoline = ReShade::Hooks::Call(&glCopyTexSubImage2D);

	trampoline(target, level, xoffset, yoffset, x, y, width, height);
}
void WINAPI glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
	static const auto trampoline = ReShade::Hooks::Call(&glCopyTexSubImage3D);

	trampoline(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}
EXPORT void WINAPI glCullFace(GLenum mode)
{
	static const auto trampoline = ReShade::Hooks::Call(&glCullFace);

	trampoline(mode);
}
EXPORT void WINAPI glDeleteLists(GLuint list, GLsizei range)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDeleteLists);

	trampoline(list, range);
}
EXPORT void WINAPI glDeleteTextures(GLsizei n, const GLuint *textures)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDeleteTextures);

	trampoline(n, textures);
}
EXPORT void WINAPI glDepthFunc(GLenum func)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDepthFunc);

	trampoline(func);
}
EXPORT void WINAPI glDepthMask(GLboolean flag)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDepthMask);

	trampoline(flag);
}
EXPORT void WINAPI glDepthRange(GLclampd zNear, GLclampd zFar)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDepthRange);

	trampoline(zNear, zFar);
}
EXPORT void WINAPI glDisable(GLenum cap)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDisable);

	trampoline(cap);
}
EXPORT void WINAPI glDisableClientState(GLenum array)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDisableClientState);

	trampoline(array);
}
EXPORT void WINAPI glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawArrays);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(count);
	}

	trampoline(mode, first, count);
}
void WINAPI glDrawArraysIndirect(GLenum mode, const GLvoid *indirect)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawArraysIndirect);

	trampoline(mode, indirect);
}
void WINAPI glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawArraysInstanced);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(primcount * count);
	}

	trampoline(mode, first, count, primcount);
}
void WINAPI glDrawArraysInstancedARB(GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawArraysInstancedARB);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(primcount * count);
	}

	trampoline(mode, first, count, primcount);
}
void WINAPI glDrawArraysInstancedEXT(GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawArraysInstancedEXT);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(primcount * count);
	}

	trampoline(mode, first, count, primcount);
}
void WINAPI glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei primcount, GLuint baseinstance)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawArraysInstancedBaseInstance);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(primcount * count);
	}

	trampoline(mode, first, count, primcount, baseinstance);
}
EXPORT void WINAPI glDrawBuffer(GLenum mode)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawBuffer);

	trampoline(mode);
}
EXPORT void WINAPI glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawElements);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(count);
	}

	trampoline(mode, count, type, indices);
}
void WINAPI glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawElementsBaseVertex);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(count);
	}

	trampoline(mode, count, type, indices, basevertex);
}
void WINAPI glDrawElementsIndirect(GLenum mode, GLenum type, const GLvoid *indirect)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawElementsIndirect);

	trampoline(mode, type, indirect);
}
void WINAPI glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawElementsInstanced);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(primcount * count);
	}

	trampoline(mode, count, type, indices, primcount);
}
void WINAPI glDrawElementsInstancedARB(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawElementsInstancedARB);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(primcount * count);
	}

	trampoline(mode, count, type, indices, primcount);
}
void WINAPI glDrawElementsInstancedEXT(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawElementsInstancedEXT);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(primcount * count);
	}

	trampoline(mode, count, type, indices, primcount);
}
void WINAPI glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawElementsInstancedBaseVertex);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(primcount * count);
	}

	trampoline(mode, count, type, indices, primcount, basevertex);
}
void WINAPI glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLuint baseinstance)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawElementsInstancedBaseInstance);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(primcount * count);
	}

	trampoline(mode, count, type, indices, primcount, baseinstance);
}
void WINAPI glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex, GLuint baseinstance)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawElementsInstancedBaseVertexBaseInstance);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(primcount * count);
	}

	trampoline(mode, count, type, indices, primcount, basevertex, baseinstance);
}
EXPORT void WINAPI glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawPixels);

	trampoline(width, height, format, type, pixels);
}
void WINAPI glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawRangeElements);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(count);
	}

	trampoline(mode, start, end, count, type, indices);
}
void WINAPI glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
	static const auto trampoline = ReShade::Hooks::Call(&glDrawRangeElementsBaseVertex);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(count);
	}

	trampoline(mode, start, end, count, type, indices, basevertex);
}
EXPORT void WINAPI glEdgeFlag(GLboolean flag)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEdgeFlag);

	trampoline(flag);
}
EXPORT void WINAPI glEdgeFlagPointer(GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEdgeFlagPointer);

	trampoline(stride, pointer);
}
EXPORT void WINAPI glEdgeFlagv(const GLboolean *flag)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEdgeFlagv);

	trampoline(flag);
}
EXPORT void WINAPI glEnable(GLenum cap)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEnable);

	trampoline(cap);
}
EXPORT void WINAPI glEnableClientState(GLenum array)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEnableClientState);

	trampoline(array);
}
EXPORT void WINAPI glEnd()
{
	static const auto trampoline = ReShade::Hooks::Call(&glEnd);

	trampoline();

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnDrawInternal(sCurrentVertexCount);
	}
}
EXPORT void WINAPI glEndList()
{
	static const auto trampoline = ReShade::Hooks::Call(&glEndList);

	trampoline();
}
EXPORT void WINAPI glEvalCoord1d(GLdouble u)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEvalCoord1d);

	trampoline(u);
}
EXPORT void WINAPI glEvalCoord1dv(const GLdouble *u)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEvalCoord1dv);

	trampoline(u);
}
EXPORT void WINAPI glEvalCoord1f(GLfloat u)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEvalCoord1f);

	trampoline(u);
}
EXPORT void WINAPI glEvalCoord1fv(const GLfloat *u)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEvalCoord1fv);

	trampoline(u);
}
EXPORT void WINAPI glEvalCoord2d(GLdouble u, GLdouble v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEvalCoord2d);

	trampoline(u, v);
}
EXPORT void WINAPI glEvalCoord2dv(const GLdouble *u)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEvalCoord2dv);

	trampoline(u);
}
EXPORT void WINAPI glEvalCoord2f(GLfloat u, GLfloat v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEvalCoord2f);

	trampoline(u, v);
}
EXPORT void WINAPI glEvalCoord2fv(const GLfloat *u)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEvalCoord2fv);

	trampoline(u);
}
EXPORT void WINAPI glEvalMesh1(GLenum mode, GLint i1, GLint i2)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEvalMesh1);

	trampoline(mode, i1, i2);
}
EXPORT void WINAPI glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEvalMesh2);

	trampoline(mode, i1, i2, j1, j2);
}
EXPORT void WINAPI glEvalPoint1(GLint i)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEvalPoint1);

	trampoline(i);
}
EXPORT void WINAPI glEvalPoint2(GLint i, GLint j)
{
	static const auto trampoline = ReShade::Hooks::Call(&glEvalPoint2);

	trampoline(i, j);
}
EXPORT void WINAPI glFeedbackBuffer(GLsizei size, GLenum type, GLfloat *buffer)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFeedbackBuffer);

	trampoline(size, type, buffer);
}
EXPORT void WINAPI glFinish()
{
	static const auto trampoline = ReShade::Hooks::Call(&glFinish);

	trampoline();
}
EXPORT void WINAPI glFlush()
{
	static const auto trampoline = ReShade::Hooks::Call(&glFlush);

	trampoline();
}
EXPORT void WINAPI glFogf(GLenum pname, GLfloat param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFogf);

	trampoline(pname, param);
}
EXPORT void WINAPI glFogfv(GLenum pname, const GLfloat *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFogfv);

	trampoline(pname, params);
}
EXPORT void WINAPI glFogi(GLenum pname, GLint param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFogi);

	trampoline(pname, param);
}
EXPORT void WINAPI glFogiv(GLenum pname, const GLint *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFogiv);

	trampoline(pname, params);
}
void WINAPI glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFramebufferRenderbuffer);

	trampoline(target, attachment, renderbuffertarget, renderbuffer);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnFramebufferAttachment(target, attachment, renderbuffertarget, renderbuffer, 0);
	}
}
void WINAPI glFramebufferRenderbufferEXT(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFramebufferRenderbufferEXT);

	trampoline(target, attachment, renderbuffertarget, renderbuffer);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnFramebufferAttachment(target, attachment, renderbuffertarget, renderbuffer, 0);
	}
}
void WINAPI glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFramebufferTexture);

	trampoline(target, attachment, texture, level);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnFramebufferAttachment(target, attachment, GL_TEXTURE, texture, level);
	}
}
void WINAPI glFramebufferTextureARB(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFramebufferTextureARB);

	trampoline(target, attachment, texture, level);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnFramebufferAttachment(target, attachment, GL_TEXTURE, texture, level);
	}
}
void WINAPI glFramebufferTextureEXT(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFramebufferTextureEXT);

	trampoline(target, attachment, texture, level);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnFramebufferAttachment(target, attachment, GL_TEXTURE, texture, level);
	}
}
void WINAPI glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFramebufferTexture1D);

	trampoline(target, attachment, textarget, texture, level);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnFramebufferAttachment(target, attachment, textarget, texture, level);
	}
}
void WINAPI glFramebufferTexture1DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFramebufferTexture1DEXT);

	trampoline(target, attachment, textarget, texture, level);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnFramebufferAttachment(target, attachment, textarget, texture, level);
	}
}
void WINAPI glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFramebufferTexture2D);

	trampoline(target, attachment, textarget, texture, level);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnFramebufferAttachment(target, attachment, textarget, texture, level);
	}
}
void WINAPI glFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFramebufferTexture2DEXT);

	trampoline(target, attachment, textarget, texture, level);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnFramebufferAttachment(target, attachment, textarget, texture, level);
	}
}
void WINAPI glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFramebufferTexture3D);

	trampoline(target, attachment, textarget, texture, level, zoffset);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnFramebufferAttachment(target, attachment, textarget, texture, level);
	}
}
void WINAPI glFramebufferTexture3DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFramebufferTexture3DEXT);

	trampoline(target, attachment, textarget, texture, level, zoffset);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnFramebufferAttachment(target, attachment, textarget, texture, level);
	}
}
void WINAPI glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFramebufferTextureLayer);

	trampoline(target, attachment, texture, level, layer);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnFramebufferAttachment(target, attachment, GL_TEXTURE, texture, level);
	}
}
void WINAPI glFramebufferTextureLayerARB(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFramebufferTextureLayerARB);

	trampoline(target, attachment, texture, level, layer);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnFramebufferAttachment(target, attachment, GL_TEXTURE, texture, level);
	}
}
void WINAPI glFramebufferTextureLayerEXT(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFramebufferTextureLayerEXT);

	trampoline(target, attachment, texture, level, layer);

	if (sCurrentRuntime != nullptr)
	{
		sCurrentRuntime->OnFramebufferAttachment(target, attachment, GL_TEXTURE, texture, level);
	}
}
EXPORT void WINAPI glFrontFace(GLenum mode)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFrontFace);

	trampoline(mode);
}
EXPORT void WINAPI glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	static const auto trampoline = ReShade::Hooks::Call(&glFrustum);

	trampoline(left, right, bottom, top, zNear, zFar);
}
EXPORT GLuint WINAPI glGenLists(GLsizei range)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGenLists);

	return trampoline(range);
}
EXPORT void WINAPI glGenTextures(GLsizei n, GLuint *textures)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGenTextures);

	trampoline(n, textures);
}
EXPORT void WINAPI glGetBooleanv(GLenum pname, GLboolean *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetBooleanv);

	trampoline(pname, params);
}
EXPORT void WINAPI glGetDoublev(GLenum pname, GLdouble *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetDoublev);

	trampoline(pname, params);
}
EXPORT void WINAPI glGetFloatv(GLenum pname, GLfloat *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetFloatv);

	trampoline(pname, params);
}
EXPORT void WINAPI glGetIntegerv(GLenum pname, GLint *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetIntegerv);

	trampoline(pname, params);
}
EXPORT void WINAPI glGetClipPlane(GLenum plane, GLdouble *equation)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetClipPlane);

	trampoline(plane, equation);
}
EXPORT GLenum WINAPI glGetError()
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetError);

	return trampoline();
}
EXPORT void WINAPI glGetLightfv(GLenum light, GLenum pname, GLfloat *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetLightfv);

	trampoline(light, pname, params);
}
EXPORT void WINAPI glGetLightiv(GLenum light, GLenum pname, GLint *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetLightiv);

	trampoline(light, pname, params);
}
EXPORT void WINAPI glGetMapdv(GLenum target, GLenum query, GLdouble *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetMapdv);

	trampoline(target, query, v);
}
EXPORT void WINAPI glGetMapfv(GLenum target, GLenum query, GLfloat *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetMapfv);

	trampoline(target, query, v);
}
EXPORT void WINAPI glGetMapiv(GLenum target, GLenum query, GLint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetMapiv);

	trampoline(target, query, v);
}
EXPORT void WINAPI glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetMaterialfv);

	trampoline(face, pname, params);
}
EXPORT void WINAPI glGetMaterialiv(GLenum face, GLenum pname, GLint *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetMaterialiv);

	trampoline(face, pname, params);
}
EXPORT void WINAPI glGetPixelMapfv(GLenum map, GLfloat *values)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetPixelMapfv);

	trampoline(map, values);
}
EXPORT void WINAPI glGetPixelMapuiv(GLenum map, GLuint *values)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetPixelMapuiv);

	trampoline(map, values);
}
EXPORT void WINAPI glGetPixelMapusv(GLenum map, GLushort *values)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetPixelMapusv);

	trampoline(map, values);
}
EXPORT void WINAPI glGetPointerv(GLenum pname, GLvoid **params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetPointerv);

	trampoline(pname, params);
}
EXPORT void WINAPI glGetPolygonStipple(GLubyte *mask)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetPolygonStipple);

	trampoline(mask);
}
EXPORT const GLubyte *WINAPI glGetString(GLenum name)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetString);

	return trampoline(name);
}
EXPORT void WINAPI glGetTexEnvfv(GLenum target, GLenum pname, GLfloat *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetTexEnvfv);

	trampoline(target, pname, params);
}
EXPORT void WINAPI glGetTexEnviv(GLenum target, GLenum pname, GLint *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetTexEnviv);

	trampoline(target, pname, params);
}
EXPORT void WINAPI glGetTexGendv(GLenum coord, GLenum pname, GLdouble *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetTexGendv);

	trampoline(coord, pname, params);
}
EXPORT void WINAPI glGetTexGenfv(GLenum coord, GLenum pname, GLfloat *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetTexGenfv);

	trampoline(coord, pname, params);
}
EXPORT void WINAPI glGetTexGeniv(GLenum coord, GLenum pname, GLint *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetTexGeniv);

	trampoline(coord, pname, params);
}
EXPORT void WINAPI glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetTexImage);

	trampoline(target, level, format, type, pixels);
}
EXPORT void WINAPI glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetTexLevelParameterfv);

	trampoline(target, level, pname, params);
}
EXPORT void WINAPI glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetTexLevelParameteriv);

	trampoline(target, level, pname, params);
}
EXPORT void WINAPI glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetTexParameterfv);

	trampoline(target, pname, params);
}
EXPORT void WINAPI glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glGetTexParameteriv);

	trampoline(target, pname, params);
}
EXPORT void WINAPI glHint(GLenum target, GLenum mode)
{
	static const auto trampoline = ReShade::Hooks::Call(&glHint);

	trampoline(target, mode);
}
EXPORT void WINAPI glIndexMask(GLuint mask)
{
	static const auto trampoline = ReShade::Hooks::Call(&glIndexMask);

	trampoline(mask);
}
EXPORT void WINAPI glIndexPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = ReShade::Hooks::Call(&glIndexPointer);

	trampoline(type, stride, pointer);
}
EXPORT void WINAPI glIndexd(GLdouble c)
{
	static const auto trampoline = ReShade::Hooks::Call(&glIndexd);

	trampoline(c);
}
EXPORT void WINAPI glIndexdv(const GLdouble *c)
{
	static const auto trampoline = ReShade::Hooks::Call(&glIndexdv);

	trampoline(c);
}
EXPORT void WINAPI glIndexf(GLfloat c)
{
	static const auto trampoline = ReShade::Hooks::Call(&glIndexf);

	trampoline(c);
}
EXPORT void WINAPI glIndexfv(const GLfloat *c)
{
	static const auto trampoline = ReShade::Hooks::Call(&glIndexfv);

	trampoline(c);
}
EXPORT void WINAPI glIndexi(GLint c)
{
	static const auto trampoline = ReShade::Hooks::Call(&glIndexi);

	trampoline(c);
}
EXPORT void WINAPI glIndexiv(const GLint *c)
{
	static const auto trampoline = ReShade::Hooks::Call(&glIndexiv);

	trampoline(c);
}
EXPORT void WINAPI glIndexs(GLshort c)
{
	static const auto trampoline = ReShade::Hooks::Call(&glIndexs);

	trampoline(c);
}
EXPORT void WINAPI glIndexsv(const GLshort *c)
{
	static const auto trampoline = ReShade::Hooks::Call(&glIndexsv);

	trampoline(c);
}
EXPORT void WINAPI glIndexub(GLubyte c)
{
	static const auto trampoline = ReShade::Hooks::Call(&glIndexub);

	trampoline(c);
}
EXPORT void WINAPI glIndexubv(const GLubyte *c)
{
	static const auto trampoline = ReShade::Hooks::Call(&glIndexubv);

	trampoline(c);
}
EXPORT void WINAPI glInitNames()
{
	static const auto trampoline = ReShade::Hooks::Call(&glInitNames);

	trampoline();
}
EXPORT void WINAPI glInterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = ReShade::Hooks::Call(&glInterleavedArrays);

	trampoline(format, stride, pointer);
}
EXPORT GLboolean WINAPI glIsEnabled(GLenum cap)
{
	static const auto trampoline = ReShade::Hooks::Call(&glIsEnabled);

	return trampoline(cap);
}
EXPORT GLboolean WINAPI glIsList(GLuint list)
{
	static const auto trampoline = ReShade::Hooks::Call(&glIsList);

	return trampoline(list);
}
EXPORT GLboolean WINAPI glIsTexture(GLuint texture)
{
	static const auto trampoline = ReShade::Hooks::Call(&glIsTexture);

	return trampoline(texture);
}
EXPORT void WINAPI glLightModelf(GLenum pname, GLfloat param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glLightModelf);

	trampoline(pname, param);
}
EXPORT void WINAPI glLightModelfv(GLenum pname, const GLfloat *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glLightModelfv);

	trampoline(pname, params);
}
EXPORT void WINAPI glLightModeli(GLenum pname, GLint param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glLightModeli);

	trampoline(pname, param);
}
EXPORT void WINAPI glLightModeliv(GLenum pname, const GLint *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glLightModeliv);

	trampoline(pname, params);
}
EXPORT void WINAPI glLightf(GLenum light, GLenum pname, GLfloat param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glLightf);

	trampoline(light, pname, param);
}
EXPORT void WINAPI glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glLightfv);

	trampoline(light, pname, params);
}
EXPORT void WINAPI glLighti(GLenum light, GLenum pname, GLint param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glLighti);

	trampoline(light, pname, param);
}
EXPORT void WINAPI glLightiv(GLenum light, GLenum pname, const GLint *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glLightiv);

	trampoline(light, pname, params);
}
EXPORT void WINAPI glLineStipple(GLint factor, GLushort pattern)
{
	static const auto trampoline = ReShade::Hooks::Call(&glLineStipple);

	trampoline(factor, pattern);
}
EXPORT void WINAPI glLineWidth(GLfloat width)
{
	static const auto trampoline = ReShade::Hooks::Call(&glLineWidth);

	trampoline(width);
}
EXPORT void WINAPI glListBase(GLuint base)
{
	static const auto trampoline = ReShade::Hooks::Call(&glListBase);

	trampoline(base);
}
EXPORT void WINAPI glLoadIdentity()
{
	static const auto trampoline = ReShade::Hooks::Call(&glLoadIdentity);

	trampoline();
}
EXPORT void WINAPI glLoadMatrixd(const GLdouble *m)
{
	static const auto trampoline = ReShade::Hooks::Call(&glLoadMatrixd);

	trampoline(m);
}
EXPORT void WINAPI glLoadMatrixf(const GLfloat *m)
{
	static const auto trampoline = ReShade::Hooks::Call(&glLoadMatrixf);

	trampoline(m);
}
EXPORT void WINAPI glLoadName(GLuint name)
{
	static const auto trampoline = ReShade::Hooks::Call(&glLoadName);

	trampoline(name);
}
EXPORT void WINAPI glLogicOp(GLenum opcode)
{
	static const auto trampoline = ReShade::Hooks::Call(&glLogicOp);

	trampoline(opcode);
}
EXPORT void WINAPI glMap1d(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMap1d);

	trampoline(target, u1, u2, stride, order, points);
}
EXPORT void WINAPI glMap1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMap1f);

	trampoline(target, u1, u2, stride, order, points);
}
EXPORT void WINAPI glMap2d(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMap2d);

	trampoline(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}
EXPORT void WINAPI glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMap2f);

	trampoline(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}
EXPORT void WINAPI glMapGrid1d(GLint un, GLdouble u1, GLdouble u2)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMapGrid1d);

	trampoline(un, u1, u2);
}
EXPORT void WINAPI glMapGrid1f(GLint un, GLfloat u1, GLfloat u2)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMapGrid1f);

	trampoline(un, u1, u2);
}
EXPORT void WINAPI glMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMapGrid2d);

	trampoline(un, u1, u2, vn, v1, v2);
}
EXPORT void WINAPI glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMapGrid2f);

	trampoline(un, u1, u2, vn, v1, v2);
}
EXPORT void WINAPI glMaterialf(GLenum face, GLenum pname, GLfloat param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMaterialf);

	trampoline(face, pname, param);
}
EXPORT void WINAPI glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMaterialfv);

	trampoline(face, pname, params);
}
EXPORT void WINAPI glMateriali(GLenum face, GLenum pname, GLint param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMateriali);

	trampoline(face, pname, param);
}
EXPORT void WINAPI glMaterialiv(GLenum face, GLenum pname, const GLint *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMaterialiv);

	trampoline(face, pname, params);
}
EXPORT void WINAPI glMatrixMode(GLenum mode)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMatrixMode);

	trampoline(mode);
}
EXPORT void WINAPI glMultMatrixd(const GLdouble *m)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMultMatrixd);

	trampoline(m);
}
EXPORT void WINAPI glMultMatrixf(const GLfloat *m)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMultMatrixf);

	trampoline(m);
}
void WINAPI glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMultiDrawArrays);

	if (sCurrentRuntime != nullptr)
	{
		GLsizei totalcount = 0;

		for (GLint i = 0; i < drawcount; ++i)
		{
			totalcount += count[i];
		}

		sCurrentRuntime->OnDrawInternal(totalcount);
	}

	trampoline(mode, first, count, drawcount);
}
void WINAPI glMultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMultiDrawArraysIndirect);

	trampoline(mode, indirect, drawcount, stride);
}
void WINAPI glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei drawcount)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMultiDrawElements);

	if (sCurrentRuntime != nullptr)
	{
		GLsizei totalcount = 0;

		for (GLint i = 0; i < drawcount; ++i)
		{
			totalcount += count[i];
		}

		sCurrentRuntime->OnDrawInternal(totalcount);
	}

	trampoline(mode, count, type, indices, drawcount);
}
void WINAPI glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei drawcount, const GLint *basevertex)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMultiDrawElementsBaseVertex);

	if (sCurrentRuntime != nullptr)
	{
		GLsizei totalcount = 0;

		for (GLint i = 0; i < drawcount; ++i)
		{
			totalcount += count[i];
		}

		sCurrentRuntime->OnDrawInternal(totalcount);
	}

	trampoline(mode, count, type, indices, drawcount, basevertex);
}
void WINAPI glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride)
{
	static const auto trampoline = ReShade::Hooks::Call(&glMultiDrawElementsIndirect);

	trampoline(mode, type, indirect, drawcount, stride);
}
EXPORT void WINAPI glNewList(GLuint list, GLenum mode)
{
	static const auto trampoline = ReShade::Hooks::Call(&glNewList);

	trampoline(list, mode);
}
EXPORT void WINAPI glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz)
{
	static const auto trampoline = ReShade::Hooks::Call(&glNormal3b);

	trampoline(nx, ny, nz);
}
EXPORT void WINAPI glNormal3bv(const GLbyte *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glNormal3bv);

	trampoline(v);
}
EXPORT void WINAPI glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz)
{
	static const auto trampoline = ReShade::Hooks::Call(&glNormal3d);

	trampoline(nx, ny, nz);
}
EXPORT void WINAPI glNormal3dv(const GLdouble *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glNormal3dv);

	trampoline(v);
}
EXPORT void WINAPI glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
	static const auto trampoline = ReShade::Hooks::Call(&glNormal3f);

	trampoline(nx, ny, nz);
}
EXPORT void WINAPI glNormal3fv(const GLfloat *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glNormal3fv);

	trampoline(v);
}
EXPORT void WINAPI glNormal3i(GLint nx, GLint ny, GLint nz)
{
	static const auto trampoline = ReShade::Hooks::Call(&glNormal3i);

	trampoline(nx, ny, nz);
}
EXPORT void WINAPI glNormal3iv(const GLint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glNormal3iv);

	trampoline(v);
}
EXPORT void WINAPI glNormal3s(GLshort nx, GLshort ny, GLshort nz)
{
	static const auto trampoline = ReShade::Hooks::Call(&glNormal3s);

	trampoline(nx, ny, nz);
}
EXPORT void WINAPI glNormal3sv(const GLshort *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glNormal3sv);

	trampoline(v);
}
EXPORT void WINAPI glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = ReShade::Hooks::Call(&glNormalPointer);

	trampoline(type, stride, pointer);
}
EXPORT void WINAPI glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	static const auto trampoline = ReShade::Hooks::Call(&glOrtho);

	trampoline(left, right, bottom, top, zNear, zFar);
}
EXPORT void WINAPI glPassThrough(GLfloat token)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPassThrough);

	trampoline(token);
}
EXPORT void WINAPI glPixelMapfv(GLenum map, GLsizei mapsize, const GLfloat *values)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPixelMapfv);

	trampoline(map, mapsize, values);
}
EXPORT void WINAPI glPixelMapuiv(GLenum map, GLsizei mapsize, const GLuint *values)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPixelMapuiv);

	trampoline(map, mapsize, values);
}
EXPORT void WINAPI glPixelMapusv(GLenum map, GLsizei mapsize, const GLushort *values)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPixelMapusv);

	trampoline(map, mapsize, values);
}
EXPORT void WINAPI glPixelStoref(GLenum pname, GLfloat param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPixelStoref);

	trampoline(pname, param);
}
EXPORT void WINAPI glPixelStorei(GLenum pname, GLint param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPixelStorei);

	trampoline(pname, param);
}
EXPORT void WINAPI glPixelTransferf(GLenum pname, GLfloat param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPixelTransferf);

	trampoline(pname, param);
}
EXPORT void WINAPI glPixelTransferi(GLenum pname, GLint param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPixelTransferi);

	trampoline(pname, param);
}
EXPORT void WINAPI glPixelZoom(GLfloat xfactor, GLfloat yfactor)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPixelZoom);

	trampoline(xfactor, yfactor);
}
EXPORT void WINAPI glPointSize(GLfloat size)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPointSize);

	trampoline(size);
}
EXPORT void WINAPI glPolygonMode(GLenum face, GLenum mode)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPolygonMode);

	trampoline(face, mode);
}
EXPORT void WINAPI glPolygonOffset(GLfloat factor, GLfloat units)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPolygonOffset);

	trampoline(factor, units);
}
EXPORT void WINAPI glPolygonStipple(const GLubyte *mask)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPolygonStipple);

	trampoline(mask);
}
EXPORT void WINAPI glPopAttrib()
{
	static const auto trampoline = ReShade::Hooks::Call(&glPopAttrib);

	trampoline();
}
EXPORT void WINAPI glPopClientAttrib()
{
	static const auto trampoline = ReShade::Hooks::Call(&glPopClientAttrib);

	trampoline();
}
EXPORT void WINAPI glPopMatrix()
{
	static const auto trampoline = ReShade::Hooks::Call(&glPopMatrix);

	trampoline();
}
EXPORT void WINAPI glPopName()
{
	static const auto trampoline = ReShade::Hooks::Call(&glPopName);

	trampoline();
}
EXPORT void WINAPI glPrioritizeTextures(GLsizei n, const GLuint *textures, const GLclampf *priorities)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPrioritizeTextures);

	trampoline(n, textures, priorities);
}
EXPORT void WINAPI glPushAttrib(GLbitfield mask)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPushAttrib);

	trampoline(mask);
}
EXPORT void WINAPI glPushClientAttrib(GLbitfield mask)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPushClientAttrib);

	trampoline(mask);
}
EXPORT void WINAPI glPushMatrix()
{
	static const auto trampoline = ReShade::Hooks::Call(&glPushMatrix);

	trampoline();
}
EXPORT void WINAPI glPushName(GLuint name)
{
	static const auto trampoline = ReShade::Hooks::Call(&glPushName);

	trampoline(name);
}
EXPORT void WINAPI glRasterPos2d(GLdouble x, GLdouble y)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos2d);

	trampoline(x, y);
}
EXPORT void WINAPI glRasterPos2dv(const GLdouble *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos2dv);

	trampoline(v);
}
EXPORT void WINAPI glRasterPos2f(GLfloat x, GLfloat y)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos2f);

	trampoline(x, y);
}
EXPORT void WINAPI glRasterPos2fv(const GLfloat *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos2fv);

	trampoline(v);
}
EXPORT void WINAPI glRasterPos2i(GLint x, GLint y)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos2i);

	trampoline(x, y);
}
EXPORT void WINAPI glRasterPos2iv(const GLint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos2iv);

	trampoline(v);
}
EXPORT void WINAPI glRasterPos2s(GLshort x, GLshort y)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos2s);

	trampoline(x, y);
}
EXPORT void WINAPI glRasterPos2sv(const GLshort *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos2sv);

	trampoline(v);
}
EXPORT void WINAPI glRasterPos3d(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos3d);

	trampoline(x, y, z);
}
EXPORT void WINAPI glRasterPos3dv(const GLdouble *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos3dv);

	trampoline(v);
}
EXPORT void WINAPI glRasterPos3f(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos3f);

	trampoline(x, y, z);
}
EXPORT void WINAPI glRasterPos3fv(const GLfloat *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos3fv);

	trampoline(v);
}
EXPORT void WINAPI glRasterPos3i(GLint x, GLint y, GLint z)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos3i);

	trampoline(x, y, z);
}
EXPORT void WINAPI glRasterPos3iv(const GLint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos3iv);

	trampoline(v);
}
EXPORT void WINAPI glRasterPos3s(GLshort x, GLshort y, GLshort z)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos3s);

	trampoline(x, y, z);
}
EXPORT void WINAPI glRasterPos3sv(const GLshort *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos3sv);

	trampoline(v);
}
EXPORT void WINAPI glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos4d);

	trampoline(x, y, z, w);
}
EXPORT void WINAPI glRasterPos4dv(const GLdouble *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos4dv);

	trampoline(v);
}
EXPORT void WINAPI glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos4f);

	trampoline(x, y, z, w);
}
EXPORT void WINAPI glRasterPos4fv(const GLfloat *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos4fv);

	trampoline(v);
}
EXPORT void WINAPI glRasterPos4i(GLint x, GLint y, GLint z, GLint w)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos4i);

	trampoline(x, y, z, w);
}
EXPORT void WINAPI glRasterPos4iv(const GLint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos4iv);

	trampoline(v);
}
EXPORT void WINAPI glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos4s);

	trampoline(x, y, z, w);
}
EXPORT void WINAPI glRasterPos4sv(const GLshort *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRasterPos4sv);

	trampoline(v);
}
EXPORT void WINAPI glReadBuffer(GLenum mode)
{
	static const auto trampoline = ReShade::Hooks::Call(&glReadBuffer);

	trampoline(mode);
}
EXPORT void WINAPI glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
{
	static const auto trampoline = ReShade::Hooks::Call(&glReadPixels);

	trampoline(x, y, width, height, format, type, pixels);
}
EXPORT void WINAPI glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRectd);

	trampoline(x1, y1, x2, y2);
}
EXPORT void WINAPI glRectdv(const GLdouble *v1, const GLdouble *v2)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRectdv);

	trampoline(v1, v2);
}
EXPORT void WINAPI glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRectf);

	trampoline(x1, y1, x2, y2);
}
EXPORT void WINAPI glRectfv(const GLfloat *v1, const GLfloat *v2)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRectfv);

	trampoline(v1, v2);
}
EXPORT void WINAPI glRecti(GLint x1, GLint y1, GLint x2, GLint y2)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRecti);

	trampoline(x1, y1, x2, y2);
}
EXPORT void WINAPI glRectiv(const GLint *v1, const GLint *v2)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRectiv);

	trampoline(v1, v2);
}
EXPORT void WINAPI glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRects);

	trampoline(x1, y1, x2, y2);
}
EXPORT void WINAPI glRectsv(const GLshort *v1, const GLshort *v2)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRectsv);

	trampoline(v1, v2);
}
EXPORT GLint WINAPI glRenderMode(GLenum mode)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRenderMode);

	return trampoline(mode);
}
EXPORT void WINAPI glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRotated);

	trampoline(angle, x, y, z);
}
EXPORT void WINAPI glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = ReShade::Hooks::Call(&glRotatef);

	trampoline(angle, x, y, z);
}
EXPORT void WINAPI glScaled(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = ReShade::Hooks::Call(&glScaled);

	trampoline(x, y, z);
}
EXPORT void WINAPI glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = ReShade::Hooks::Call(&glScalef);

	trampoline(x, y, z);
}
EXPORT void WINAPI glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	static const auto trampoline = ReShade::Hooks::Call(&glScissor);

	trampoline(x, y, width, height);
}
EXPORT void WINAPI glSelectBuffer(GLsizei size, GLuint *buffer)
{
	static const auto trampoline = ReShade::Hooks::Call(&glSelectBuffer);

	trampoline(size, buffer);
}
EXPORT void WINAPI glShadeModel(GLenum mode)
{
	static const auto trampoline = ReShade::Hooks::Call(&glShadeModel);

	trampoline(mode);
}
EXPORT void WINAPI glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
	static const auto trampoline = ReShade::Hooks::Call(&glStencilFunc);

	trampoline(func, ref, mask);
}
EXPORT void WINAPI glStencilMask(GLuint mask)
{
	static const auto trampoline = ReShade::Hooks::Call(&glStencilMask);

	trampoline(mask);
}
EXPORT void WINAPI glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
	static const auto trampoline = ReShade::Hooks::Call(&glStencilOp);

	trampoline(fail, zfail, zpass);
}
EXPORT void WINAPI glTexCoord1d(GLdouble s)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord1d);

	trampoline(s);
}
EXPORT void WINAPI glTexCoord1dv(const GLdouble *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord1dv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoord1f(GLfloat s)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord1f);

	trampoline(s);
}
EXPORT void WINAPI glTexCoord1fv(const GLfloat *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord1fv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoord1i(GLint s)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord1i);

	trampoline(s);
}
EXPORT void WINAPI glTexCoord1iv(const GLint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord1iv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoord1s(GLshort s)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord1s);

	trampoline(s);
}
EXPORT void WINAPI glTexCoord1sv(const GLshort *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord1sv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoord2d(GLdouble s, GLdouble t)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord2d);

	trampoline(s, t);
}
EXPORT void WINAPI glTexCoord2dv(const GLdouble *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord2dv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoord2f(GLfloat s, GLfloat t)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord2f);

	trampoline(s, t);
}
EXPORT void WINAPI glTexCoord2fv(const GLfloat *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord2fv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoord2i(GLint s, GLint t)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord2i);

	trampoline(s, t);
}
EXPORT void WINAPI glTexCoord2iv(const GLint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord2iv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoord2s(GLshort s, GLshort t)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord2s);

	trampoline(s, t);
}
EXPORT void WINAPI glTexCoord2sv(const GLshort *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord2sv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoord3d(GLdouble s, GLdouble t, GLdouble r)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord3d);

	trampoline(s, t, r);
}
EXPORT void WINAPI glTexCoord3dv(const GLdouble *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord3dv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoord3f(GLfloat s, GLfloat t, GLfloat r)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord3f);

	trampoline(s, t, r);
}
EXPORT void WINAPI glTexCoord3fv(const GLfloat *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord3fv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoord3i(GLint s, GLint t, GLint r)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord3i);

	trampoline(s, t, r);
}
EXPORT void WINAPI glTexCoord3iv(const GLint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord3iv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoord3s(GLshort s, GLshort t, GLshort r)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord3s);

	trampoline(s, t, r);
}
EXPORT void WINAPI glTexCoord3sv(const GLshort *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord3sv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord4d);

	trampoline(s, t, r, q);
}
EXPORT void WINAPI glTexCoord4dv(const GLdouble *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord4dv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord4f);

	trampoline(s, t, r, q);
}
EXPORT void WINAPI glTexCoord4fv(const GLfloat *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord4fv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoord4i(GLint s, GLint t, GLint r, GLint q)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord4i);

	trampoline(s, t, r, q);
}
EXPORT void WINAPI glTexCoord4iv(const GLint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord4iv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord4s);

	trampoline(s, t, r, q);
}
EXPORT void WINAPI glTexCoord4sv(const GLshort *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoord4sv);

	trampoline(v);
}
EXPORT void WINAPI glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexCoordPointer);

	trampoline(size, type, stride, pointer);
}
EXPORT void WINAPI glTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexEnvf);

	trampoline(target, pname, param);
}
EXPORT void WINAPI glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexEnvfv);

	trampoline(target, pname, params);
}
EXPORT void WINAPI glTexEnvi(GLenum target, GLenum pname, GLint param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexEnvi);

	trampoline(target, pname, param);
}
EXPORT void WINAPI glTexEnviv(GLenum target, GLenum pname, const GLint *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexEnviv);

	trampoline(target, pname, params);
}
EXPORT void WINAPI glTexGend(GLenum coord, GLenum pname, GLdouble param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexGend);

	trampoline(coord, pname, param);
}
EXPORT void WINAPI glTexGendv(GLenum coord, GLenum pname, const GLdouble *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexGendv);

	trampoline(coord, pname, params);
}
EXPORT void WINAPI glTexGenf(GLenum coord, GLenum pname, GLfloat param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexGenf);

	trampoline(coord, pname, param);
}
EXPORT void WINAPI glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexGenfv);

	trampoline(coord, pname, params);
}
EXPORT void WINAPI glTexGeni(GLenum coord, GLenum pname, GLint param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexGeni);

	trampoline(coord, pname, param);
}
EXPORT void WINAPI glTexGeniv(GLenum coord, GLenum pname, const GLint *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexGeniv);

	trampoline(coord, pname, params);
}
EXPORT void WINAPI glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexImage1D);

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
EXPORT void WINAPI glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexImage2D);

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
void WINAPI glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexImage3D);

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
EXPORT void WINAPI glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexParameterf);

	trampoline(target, pname, param);
}
EXPORT void WINAPI glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexParameterfv);

	trampoline(target, pname, params);
}
EXPORT void WINAPI glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexParameteri);

	trampoline(target, pname, param);
}
EXPORT void WINAPI glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexParameteriv);

	trampoline(target, pname, params);
}
EXPORT void WINAPI glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexSubImage1D);

	trampoline(target, level, xoffset, width, format, type, pixels);
}
EXPORT void WINAPI glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexSubImage2D);

	trampoline(target, level, xoffset, yoffset, width, height, format, type, pixels);
}
void WINAPI glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTexSubImage3D);

	trampoline(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}
EXPORT void WINAPI glTranslated(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTranslated);

	trampoline(x, y, z);
}
EXPORT void WINAPI glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = ReShade::Hooks::Call(&glTranslatef);

	trampoline(x, y, z);
}
EXPORT void WINAPI glVertex2d(GLdouble x, GLdouble y)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex2d);

	sCurrentVertexCount += 2;

	trampoline(x, y);
}
EXPORT void WINAPI glVertex2dv(const GLdouble *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex2dv);

	sCurrentVertexCount += 2;

	trampoline(v);
}
EXPORT void WINAPI glVertex2f(GLfloat x, GLfloat y)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex2f);

	sCurrentVertexCount += 2;

	trampoline(x, y);
}
EXPORT void WINAPI glVertex2fv(const GLfloat *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex2fv);

	sCurrentVertexCount += 2;

	trampoline(v);
}
EXPORT void WINAPI glVertex2i(GLint x, GLint y)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex2i);

	sCurrentVertexCount += 2;

	trampoline(x, y);
}
EXPORT void WINAPI glVertex2iv(const GLint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex2iv);

	sCurrentVertexCount += 2;

	trampoline(v);
}
EXPORT void WINAPI glVertex2s(GLshort x, GLshort y)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex2s);

	sCurrentVertexCount += 2;

	trampoline(x, y);
}
EXPORT void WINAPI glVertex2sv(const GLshort *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex2sv);

	sCurrentVertexCount += 2;

	trampoline(v);
}
EXPORT void WINAPI glVertex3d(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex3d);

	sCurrentVertexCount += 3;

	trampoline(x, y, z);
}
EXPORT void WINAPI glVertex3dv(const GLdouble *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex3dv);

	sCurrentVertexCount += 3;

	trampoline(v);
}
EXPORT void WINAPI glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex3f);

	sCurrentVertexCount += 3;

	trampoline(x, y, z);
}
EXPORT void WINAPI glVertex3fv(const GLfloat *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex3fv);

	sCurrentVertexCount += 3;

	trampoline(v);
}
EXPORT void WINAPI glVertex3i(GLint x, GLint y, GLint z)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex3i);

	sCurrentVertexCount += 3;

	trampoline(x, y, z);
}
EXPORT void WINAPI glVertex3iv(const GLint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex3iv);

	sCurrentVertexCount += 3;

	trampoline(v);
}
EXPORT void WINAPI glVertex3s(GLshort x, GLshort y, GLshort z)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex3s);

	sCurrentVertexCount += 3;

	trampoline(x, y, z);
}
EXPORT void WINAPI glVertex3sv(const GLshort *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex3sv);

	sCurrentVertexCount += 3;

	trampoline(v);
}
EXPORT void WINAPI glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex4d);

	sCurrentVertexCount += 4;

	trampoline(x, y, z, w);
}
EXPORT void WINAPI glVertex4dv(const GLdouble *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex4dv);

	sCurrentVertexCount += 4;

	trampoline(v);
}
EXPORT void WINAPI glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex4f);

	sCurrentVertexCount += 4;

	trampoline(x, y, z, w);
}
EXPORT void WINAPI glVertex4fv(const GLfloat *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex4fv);

	sCurrentVertexCount += 4;

	trampoline(v);
}
EXPORT void WINAPI glVertex4i(GLint x, GLint y, GLint z, GLint w)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex4i);

	sCurrentVertexCount += 4;

	trampoline(x, y, z, w);
}
EXPORT void WINAPI glVertex4iv(const GLint *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex4iv);

	sCurrentVertexCount += 4;

	trampoline(v);
}
EXPORT void WINAPI glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex4s);

	sCurrentVertexCount += 4;

	trampoline(x, y, z, w);
}
EXPORT void WINAPI glVertex4sv(const GLshort *v)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertex4sv);

	sCurrentVertexCount += 4;

	trampoline(v);
}
EXPORT void WINAPI glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = ReShade::Hooks::Call(&glVertexPointer);

	trampoline(size, type, stride, pointer);
}
EXPORT void WINAPI glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	static const auto trampoline = ReShade::Hooks::Call(&glViewport);

	trampoline(x, y, width, height);
}

// WGL
EXPORT int WINAPI wglChoosePixelFormat(HDC hdc, CONST PIXELFORMATDESCRIPTOR *ppfd)
{
	LOG(INFO) << "Redirecting '" << "wglChoosePixelFormat" << "(" << hdc << ", " << ppfd << ")' ...";

	assert(ppfd != nullptr);

	LOG(TRACE) << "> Dumping pixel format descriptor:";
	LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(TRACE) << "  | Name                                    | Value                                   |";
	LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(TRACE) << "  | " << "Flags" << "                                  " << " | 0x" << std::left << std::setw(37) << std::hex << ppfd->dwFlags << std::dec << " |";
	LOG(TRACE) << "  | " << "ColorBits" << "                              " << " | " << std::setw(39) << static_cast<unsigned int>(ppfd->cColorBits) << " |";
	LOG(TRACE) << "  | " << "DepthBits" << "                              " << " | " << std::setw(39) << static_cast<unsigned int>(ppfd->cDepthBits) << " |";
	LOG(TRACE) << "  | " << "StencilBits" << "                            " << " | " << std::setw(39) << static_cast<unsigned int>(ppfd->cStencilBits) << std::internal << " |";
	LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";

	if ((ppfd->dwFlags & PFD_DOUBLEBUFFER) == 0)
	{
		LOG(WARNING) << "> Single buffered OpenGL contexts are not supported.";
	}

	const int format = ReShade::Hooks::Call(&wglChoosePixelFormat)(hdc, ppfd);

	LOG(TRACE) << "> Returned format: " << format;

	return format;
}
BOOL WINAPI wglChoosePixelFormatARB(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats)
{
	LOG(INFO) << "Redirecting '" << "wglChoosePixelFormatARB" << "(" << hdc << ", " << piAttribIList << ", " << pfAttribFList << ", " << nMaxFormats << ", " << piFormats << ", " << nNumFormats << ")' ...";

	struct Attrib
	{
		enum Names
		{
			WGL_NUMBER_PIXEL_FORMATS_ARB = 0x2000,
			WGL_DRAW_TO_WINDOW_ARB = 0x2001,
			WGL_DRAW_TO_BITMAP_ARB = 0x2002,
			WGL_ACCELERATION_ARB = 0x2003,
			WGL_NEED_PALETTE_ARB = 0x2004,
			WGL_NEED_SYSTEM_PALETTE_ARB = 0x2005,
			WGL_SWAP_LAYER_BUFFERS_ARB = 0x2006,
			WGL_SWAP_METHOD_ARB = 0x2007,
			WGL_NUMBER_OVERLAYS_ARB = 0x2008,
			WGL_NUMBER_UNDERLAYS_ARB = 0x2009,
			WGL_TRANSPARENT_ARB = 0x200A,
			WGL_TRANSPARENT_RED_VALUE_ARB = 0x2037,
			WGL_TRANSPARENT_GREEN_VALUE_ARB = 0x2038,
			WGL_TRANSPARENT_BLUE_VALUE_ARB = 0x2039,
			WGL_TRANSPARENT_ALPHA_VALUE_ARB = 0x203A,
			WGL_TRANSPARENT_INDEX_VALUE_ARB = 0x203B,
			WGL_SHARE_DEPTH_ARB = 0x200C,
			WGL_SHARE_STENCIL_ARB = 0x200D,
			WGL_SHARE_ACCUM_ARB = 0x200E,
			WGL_SUPPORT_GDI_ARB = 0x200F,
			WGL_SUPPORT_OPENGL_ARB = 0x2010,
			WGL_DOUBLE_BUFFER_ARB = 0x2011,
			WGL_STEREO_ARB = 0x2012,
			WGL_PIXEL_TYPE_ARB = 0x2013,
			WGL_COLOR_BITS_ARB = 0x2014,
			WGL_RED_BITS_ARB = 0x2015,
			WGL_RED_SHIFT_ARB = 0x2016,
			WGL_GREEN_BITS_ARB = 0x2017,
			WGL_GREEN_SHIFT_ARB = 0x2018,
			WGL_BLUE_BITS_ARB = 0x2019,
			WGL_BLUE_SHIFT_ARB = 0x201A,
			WGL_ALPHA_BITS_ARB = 0x201B,
			WGL_ALPHA_SHIFT_ARB = 0x201C,
			WGL_ACCUM_BITS_ARB = 0x201D,
			WGL_ACCUM_RED_BITS_ARB = 0x201E,
			WGL_ACCUM_GREEN_BITS_ARB = 0x201F,
			WGL_ACCUM_BLUE_BITS_ARB = 0x2020,
			WGL_ACCUM_ALPHA_BITS_ARB = 0x2021,
			WGL_DEPTH_BITS_ARB = 0x2022,
			WGL_STENCIL_BITS_ARB = 0x2023,
			WGL_AUX_BUFFERS_ARB = 0x2024,
			WGL_SAMPLE_BUFFERS_ARB = 0x2041,
			WGL_SAMPLES_ARB = 0x2042,
		};
		enum Values
		{
			WGL_NO_ACCELERATION_ARB = 0x2025,
			WGL_GENERIC_ACCELERATION_ARB = 0x2026,
			WGL_FULL_ACCELERATION_ARB = 0x2027,
			WGL_SWAP_EXCHANGE_ARB = 0x2028,
			WGL_SWAP_COPY_ARB = 0x2029,
			WGL_SWAP_UNDEFINED_ARB = 0x202A,
			WGL_TYPE_RGBA_ARB = 0x202B,
			WGL_TYPE_COLORINDEX_ARB = 0x202C,
		};
	};

	bool doublebuffered = false;

	LOG(TRACE) << "> Dumping Attributes:";
	LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(TRACE) << "  | Attribute                               | Value                                   |";
	LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";

	for (const int *attrib = piAttribIList; attrib != nullptr && *attrib != 0; attrib += 2)
	{
		switch (attrib[0])
		{
			case Attrib::WGL_DRAW_TO_WINDOW_ARB:
				LOG(TRACE) << "  | " << "WGL_DRAW_TO_WINDOW_ARB" << "                 " << " | " << (attrib[1] != FALSE ? "TRUE " : "FALSE") << "                                  " << " |";
				break;
			case Attrib::WGL_DRAW_TO_BITMAP_ARB:
				LOG(TRACE) << "  | " << "WGL_DRAW_TO_BITMAP_ARB" << "                 " << " | " << (attrib[1] != FALSE ? "TRUE " : "FALSE") << "                                  " << " |";
				break;
			case Attrib::WGL_ACCELERATION_ARB:
				LOG(TRACE) << "  | " << "WGL_ACCELERATION_ARB" << "                   " << " | 0x" << std::left << std::setw(37) << std::hex << attrib[1] << std::dec << std::internal << " |";
				break;
			case Attrib::WGL_SWAP_LAYER_BUFFERS_ARB:
				LOG(TRACE) << "  | " << "WGL_SWAP_LAYER_BUFFERS_ARB" << "             " << " | " << (attrib[1] != FALSE ? "TRUE " : "FALSE") << "                                  " << " |";
				break;
			case Attrib::WGL_SWAP_METHOD_ARB:
				LOG(TRACE) << "  | " << "WGL_SWAP_METHOD_ARB" << "                    " << " | 0x" << std::left << std::setw(37) << std::hex << attrib[1] << std::dec << std::internal << " |";
				break;
			case Attrib::WGL_NUMBER_OVERLAYS_ARB:
				LOG(TRACE) << "  | " << "WGL_NUMBER_OVERLAYS_ARB" << "                " << " | " << std::left << std::setw(39) << attrib[1] << std::internal << " |";
				break;
			case Attrib::WGL_NUMBER_UNDERLAYS_ARB:
				LOG(TRACE) << "  | " << "WGL_NUMBER_UNDERLAYS_ARB" << "               " << " | " << std::left << std::setw(39) << attrib[1] << std::internal << " |";
				break;
			case Attrib::WGL_SUPPORT_GDI_ARB:
				LOG(TRACE) << "  | " << "WGL_SUPPORT_GDI_ARB" << "                    " << " | " << (attrib[1] != FALSE ? "TRUE " : "FALSE") << "                                  " << " |";
				break;
			case Attrib::WGL_SUPPORT_OPENGL_ARB:
				LOG(TRACE) << "  | " << "WGL_SUPPORT_OPENGL_ARB" << "                 " << " | " << (attrib[1] != FALSE ? "TRUE " : "FALSE") << "                                  " << " |";
				break;
			case Attrib::WGL_DOUBLE_BUFFER_ARB:
				doublebuffered = attrib[1] != FALSE;
				LOG(TRACE) << "  | " << "WGL_DOUBLE_BUFFER_ARB" << "                  " << " | " << (doublebuffered ? "TRUE " : "FALSE") << "                                  " << " |";
				break;
			case Attrib::WGL_STEREO_ARB:
				LOG(TRACE) << "  | " << "WGL_STEREO_ARB" << "                         " << " | " << (attrib[1] != FALSE ? "TRUE " : "FALSE") << "                                  " << " |";
				break;
			case Attrib::WGL_RED_BITS_ARB:
				LOG(TRACE) << "  | " << "WGL_RED_BITS_ARB" << "                       " << " | " << std::left << std::setw(39) << attrib[1] << std::internal << " |";
				break;
			case Attrib::WGL_GREEN_BITS_ARB:
				LOG(TRACE) << "  | " << "WGL_GREEN_BITS_ARB" << "                     " << " | " << std::left << std::setw(39) << attrib[1] << std::internal << " |";
				break;
			case Attrib::WGL_BLUE_BITS_ARB:
				LOG(TRACE) << "  | " << "WGL_BLUE_BITS_ARB" << "                      " << " | " << std::left << std::setw(39) << attrib[1] << std::internal << " |";
				break;
			case Attrib::WGL_ALPHA_BITS_ARB:
				LOG(TRACE) << "  | " << "WGL_ALPHA_BITS_ARB" << "                     " << " | " << std::left << std::setw(39) << attrib[1] << std::internal << " |";
				break;
			case Attrib::WGL_COLOR_BITS_ARB:
				LOG(TRACE) << "  | " << "WGL_COLOR_BITS_ARB" << "                     " << " | " << std::left << std::setw(39) << attrib[1] << std::internal << " |";
				break;
			case Attrib::WGL_DEPTH_BITS_ARB:
				LOG(TRACE) << "  | " << "WGL_DEPTH_BITS_ARB" << "                     " << " | " << std::left << std::setw(39) << attrib[1] << std::internal << " |";
				break;
			case Attrib::WGL_STENCIL_BITS_ARB:
				LOG(TRACE) << "  | " << "WGL_STENCIL_BITS_ARB" << "                   " << " | " << std::left << std::setw(39) << attrib[1] << std::internal << " |";
				break;
			case Attrib::WGL_SAMPLE_BUFFERS_ARB:
				LOG(TRACE) << "  | " << "WGL_SAMPLE_BUFFERS_ARB" << "                 " << " | " << std::left << std::setw(39) << attrib[1] << std::internal << " |";
				break;
			case Attrib::WGL_SAMPLES_ARB:
				LOG(TRACE) << "  | " << "WGL_SAMPLES_ARB" << "                        " << " | " << std::left << std::setw(39) << attrib[1] << std::internal << " |";
				break;
			default:
				LOG(TRACE) << "  | 0x" << std::left << std::hex << std::setw(37) << attrib[0] << " | 0x" << std::setw(37) << attrib[1] << std::dec << std::internal << " |";
				break;
		}
	}
	for (const FLOAT *attrib = pfAttribFList; attrib != nullptr && *attrib != 0.0f; attrib += 2)
	{
		LOG(TRACE) << "  | 0x" << std::left << std::hex << std::setw(37) << static_cast<int>(attrib[0]) << " | " << std::setw(39) << attrib[1] << std::dec << std::internal << " |";
	}

	LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";

	if (!doublebuffered)
	{
		LOG(WARNING) << "> Single buffered OpenGL contexts are not supported.";
	}

	if (!ReShade::Hooks::Call(&wglChoosePixelFormatARB)(hdc, piAttribIList, pfAttribFList, nMaxFormats, piFormats, nNumFormats))
	{
		LOG(WARNING) << "> 'wglChoosePixelFormatARB' failed with '" << GetLastError() << "'!";

		return FALSE;
	}

	assert(nNumFormats != nullptr);

	std::string formats;

	for (UINT i = 0; i < std::min(*nNumFormats, nMaxFormats); ++i)
	{
		assert(piFormats[i] != 0);

		formats += " " + std::to_string(piFormats[i]);
	}

	LOG(TRACE) << "> Returned format(s):" << formats;

	return TRUE;
}
EXPORT BOOL WINAPI wglCopyContext(HGLRC hglrcSrc, HGLRC hglrcDst, UINT mask)
{
	return ReShade::Hooks::Call(&wglCopyContext)(hglrcSrc, hglrcDst, mask);
}
EXPORT HGLRC WINAPI wglCreateContext(HDC hdc)
{
	LOG(INFO) << "Redirecting '" << "wglCreateContext" << "(" << hdc << ")' ...";

	const HGLRC hglrc = ReShade::Hooks::Call(&wglCreateContext)(hdc);

	if (hglrc == nullptr)
	{
		LOG(WARNING) << "> 'wglCreateContext' failed with error code " << GetLastError() << "!";

		return nullptr;
	}

	CriticalSection::Lock lock(sCS);

	sSharedContexts.emplace(hglrc, nullptr);

	LOG(TRACE) << "> Returned OpenGL context: " << hglrc;

	return hglrc;
}
HGLRC WINAPI wglCreateContextAttribsARB(HDC hdc, HGLRC hShareContext, const int *attribList)
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
			case Attrib::WGL_CONTEXT_FLAGS_ARB:
				flags = attrib[1];
				break;
			case Attrib::WGL_CONTEXT_PROFILE_MASK_ARB:
				core = (attrib[1] & Attrib::WGL_CONTEXT_CORE_PROFILE_BIT_ARB) != 0;
				compatibility = (attrib[1] & Attrib::WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB) != 0;
				break;
		}
	}

	if (major < 3 || minor < 2)
	{
		core = compatibility = false;
	}

#ifdef _DEBUG
	attribs[i].Name = Attrib::WGL_CONTEXT_FLAGS_ARB;
	attribs[i++].Value = flags | Attrib::WGL_CONTEXT_DEBUG_BIT_ARB;
#endif

	attribs[i].Name = Attrib::WGL_CONTEXT_PROFILE_MASK_ARB;
	attribs[i++].Value = compatibility ? Attrib::WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB : Attrib::WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

	LOG(TRACE) << "> Requesting " << (core ? "core " : compatibility ? "compatibility " : "") << "OpenGL context for version " << major << '.' << minor << " ...";

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
				case Attrib::WGL_CONTEXT_PROFILE_MASK_ARB:
					attribs[k].Value = Attrib::WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
					break;
			}
		}
	}

	const HGLRC hglrc = ReShade::Hooks::Call(&wglCreateContextAttribsARB)(hdc, hShareContext, reinterpret_cast<const int *>(attribs));

	if (hglrc == nullptr)
	{
		LOG(WARNING) << "> 'wglCreateContextAttribsARB' failed with error code " << GetLastError() << "!";

		return nullptr;
	}

	CriticalSection::Lock lock(sCS);

	sSharedContexts.emplace(hglrc, hShareContext);

	if (hShareContext != nullptr)
	{
		auto it = sSharedContexts.find(hShareContext);

		while (it != sSharedContexts.end() && it->second != nullptr)
		{
			it = sSharedContexts.find(sSharedContexts.at(hglrc) = it->second);
		}
	}

	LOG(TRACE) << "> Returned OpenGL context: " << hglrc;

	return hglrc;
}
EXPORT HGLRC WINAPI wglCreateLayerContext(HDC hdc, int iLayerPlane)
{
	LOG(INFO) << "Redirecting '" << "wglCreateLayerContext" << "(" << hdc << ", " << iLayerPlane << ")' ...";

	return ReShade::Hooks::Call(&wglCreateLayerContext)(hdc, iLayerPlane);
}
EXPORT BOOL WINAPI wglDeleteContext(HGLRC hglrc)
{
	LOG(INFO) << "Redirecting '" << "wglDeleteContext" << "(" << hglrc << ")' ...";

	CriticalSection::Lock lock(sCS);

	for (auto it = sRuntimes.begin(); it != sRuntimes.end();)
	{
		assert(it->second != nullptr);

		if (it->second->mRenderContext == hglrc)
		{
			const HDC hdc = it->first, hdcPrevious = wglGetCurrentDC();
			const HGLRC hglrcPrevious = wglGetCurrentContext();

			LOG(INFO) << "> Cleaning up runtime resources ...";

			if (ReShade::Hooks::Call(&wglMakeCurrent)(hdc, hglrc) != FALSE)
			{
				it->second->OnDeleteInternal();
			}
			else
			{
				LOG(ERROR) << "> Could not switch to device context " << hdc << " to clean up (error code " << GetLastError() << "). Reverting ...";
			}

			it = sRuntimes.erase(it);

			ReShade::Hooks::Call(&wglMakeCurrent)(hdcPrevious, hglrcPrevious);
		}
		else
		{
			++it;
		}
	}

	for (auto it = sSharedContexts.begin(); it != sSharedContexts.end();)
	{
		if (it->first == hglrc)
		{
			it = sSharedContexts.erase(it);
			continue;
		}
		else if (it->second == hglrc)
		{
			it->second = nullptr;
		}

		++it;
	}

	if (!ReShade::Hooks::Call(&wglDeleteContext)(hglrc))
	{
		LOG(WARNING) << "> 'wglDeleteContext' failed with error code " << GetLastError() << "!";

		return FALSE;
	}

	return TRUE;
}
EXPORT BOOL WINAPI wglDescribeLayerPlane(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nBytes, LPLAYERPLANEDESCRIPTOR plpd)
{
	return ReShade::Hooks::Call(&wglDescribeLayerPlane)(hdc, iPixelFormat, iLayerPlane, nBytes, plpd);
}
EXPORT int WINAPI wglDescribePixelFormat(HDC hdc, int iPixelFormat, UINT nBytes, LPPIXELFORMATDESCRIPTOR ppfd)
{
	return ReShade::Hooks::Call(wglDescribePixelFormat)(hdc, iPixelFormat, nBytes, ppfd);
}
EXPORT HGLRC WINAPI wglGetCurrentContext()
{
	static const auto trampoline = ReShade::Hooks::Call(&wglGetCurrentContext);

	return trampoline();
}
EXPORT HDC WINAPI wglGetCurrentDC()
{
	static const auto trampoline = ReShade::Hooks::Call(&wglGetCurrentDC);

	return trampoline();
}
EXPORT int WINAPI wglGetLayerPaletteEntries(HDC hdc, int iLayerPlane, int iStart, int cEntries, COLORREF *pcr)
{
	return ReShade::Hooks::Call(&wglGetLayerPaletteEntries)(hdc, iLayerPlane, iStart, cEntries, pcr);
}
EXPORT int WINAPI wglGetPixelFormat(HDC hdc)
{
	return ReShade::Hooks::Call(&wglGetPixelFormat)(hdc);
}
BOOL WINAPI wglGetPixelFormatAttribivARB(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, int *piValues)
{
	return ReShade::Hooks::Call(&wglGetPixelFormatAttribivARB)(hdc, iPixelFormat, iLayerPlane, nAttributes, piAttributes, piValues);
}
BOOL WINAPI wglGetPixelFormatAttribfvARB(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, FLOAT *pfValues)
{
	return ReShade::Hooks::Call(&wglGetPixelFormatAttribfvARB)(hdc, iPixelFormat, iLayerPlane, nAttributes, piAttributes, pfValues);
}
const char *WINAPI wglGetExtensionsStringARB(HDC hdc)
{
	return ReShade::Hooks::Call(&wglGetExtensionsStringARB)(hdc);
}
int WINAPI wglGetSwapIntervalEXT()
{
	static const auto trampoline = ReShade::Hooks::Call(&wglGetSwapIntervalEXT);

	return trampoline();
}
EXPORT BOOL WINAPI wglMakeCurrent(HDC hdc, HGLRC hglrc)
{
	static const auto trampoline = ReShade::Hooks::Call(&wglMakeCurrent);

	LOG(INFO) << "Redirecting '" << "wglMakeCurrent" << "(" << hdc << ", " << hglrc << ")' ...";

	sCurrentRuntime = nullptr;

	if (!trampoline(hdc, hglrc))
	{
		LOG(WARNING) << "> 'wglMakeCurrent' failed with error code " << GetLastError() << "!";

		return FALSE;
	}

	if (hglrc == nullptr)
	{
		return TRUE;
	}

	CriticalSection::Lock lock(sCS);

	if (sSharedContexts.at(hglrc) != nullptr)
	{
		hglrc = sSharedContexts.at(hglrc);

		LOG(INFO) << "> Using shared OpenGL context " << hglrc << ".";
	}

	const auto it = sRuntimes.find(hdc);

	if (it != sRuntimes.end() && it->second->mRenderContext == hglrc)
	{
		sCurrentRuntime = it->second.get();
	}
	else
	{
		const HWND hwnd = WindowFromDC(hdc);

		if (hwnd == nullptr)
		{
			LOG(WARNING) << "> Aborted because there is no window associated with device context " << hdc << ".";

			return TRUE;
		}
		else if ((GetClassLongPtr(hwnd, GCL_STYLE) & CS_OWNDC) == 0)
		{
			LOG(WARNING) << "> Window class style of window " << hwnd << " is missing 'CS_OWNDC' flag.";
		}

		RECT rect;
		GetClientRect(hwnd, &rect);
		sWindowRects[hwnd] = rect;

		LOG(TRACE) << "> Initial size of window " << hwnd << " is " << rect.right << "x" << rect.bottom << ".";

		gl3wInit();

		if (gl3wIsSupported(4, 3))
		{
			const std::shared_ptr<ReShade::Runtimes::GLRuntime> runtime = std::make_shared<ReShade::Runtimes::GLRuntime>(hdc, hglrc);

			sRuntimes[hdc] = runtime;
			sCurrentRuntime = runtime.get();

			if (!runtime->OnCreateInternal(static_cast<unsigned int>(rect.right), static_cast<unsigned int>(rect.bottom)))
			{
				LOG(ERROR) << "Failed to initialize OpenGL renderer! Check tracelog for details.";
			}
		}
		else
		{
			LOG(ERROR) << "Your graphics card does not seem to support OpenGL 4.3. Initialization failed.";
		}
	}

	return TRUE;
}
EXPORT BOOL WINAPI wglRealizeLayerPalette(HDC hdc, int iLayerPlane, BOOL b)
{
	return ReShade::Hooks::Call(&wglRealizeLayerPalette)(hdc, iLayerPlane, b);
}
EXPORT int WINAPI wglSetLayerPaletteEntries(HDC hdc, int iLayerPlane, int iStart, int cEntries, CONST COLORREF *pcr)
{
	return ReShade::Hooks::Call(&wglSetLayerPaletteEntries)(hdc, iLayerPlane, iStart, cEntries, pcr);
}
EXPORT BOOL WINAPI wglSetPixelFormat(HDC hdc, int iPixelFormat, CONST PIXELFORMATDESCRIPTOR *ppfd)
{
	LOG(INFO) << "Redirecting '" << "wglSetPixelFormat" << "(" << hdc << ", " << iPixelFormat << ", " << ppfd << ")' ...";

	if (!ReShade::Hooks::Call(&wglSetPixelFormat)(hdc, iPixelFormat, ppfd))
	{
		LOG(WARNING) << "> 'wglSetPixelFormat' failed with error code " << GetLastError() << "!";

		return FALSE;
	}

	return TRUE;
}
EXPORT BOOL WINAPI wglShareLists(HGLRC hglrc1, HGLRC hglrc2)
{
	LOG(INFO) << "Redirecting '" << "wglShareLists" << "(" << hglrc1 << ", " << hglrc2 << ")' ...";

	if (!ReShade::Hooks::Call(&wglShareLists)(hglrc1, hglrc2))
	{
		LOG(WARNING) << "> 'wglShareLists' failed with error code " << GetLastError() << "!";

		return FALSE;
	}

	CriticalSection::Lock lock(sCS);

	sSharedContexts[hglrc2] = hglrc1;

	return TRUE;
}
EXPORT BOOL WINAPI wglSwapBuffers(HDC hdc)
{
	static const auto trampoline = ReShade::Hooks::Call(&wglSwapBuffers);

	const auto it = sRuntimes.find(hdc);
	const HWND hwnd = WindowFromDC(hdc);

	if (hwnd != nullptr && it != sRuntimes.end())
	{
		assert(it->second != nullptr);

		RECT rect, &rectPrevious = sWindowRects.at(hwnd);
		GetClientRect(hwnd, &rect);

		if (rect.right != rectPrevious.right || rect.bottom != rectPrevious.bottom)
		{
			LOG(INFO) << "Resizing runtime on device context " << hdc << " to " << rect.right << "x" << rect.bottom << " ...";

			it->second->OnDeleteInternal();

			if (!(rect.right == 0 && rect.bottom == 0) && !it->second->OnCreateInternal(static_cast<unsigned int>(rect.right), static_cast<unsigned int>(rect.bottom)))
			{
				LOG(ERROR) << "Failed to reinitialize OpenGL renderer! Check tracelog for details.";
			}

			rectPrevious = rect;
		}

		it->second->OnPresentInternal();
	}

	return trampoline(hdc);
}
EXPORT BOOL WINAPI wglSwapLayerBuffers(HDC hdc, UINT i)
{
	if (i != WGL_SWAP_MAIN_PLANE)
	{
		static const auto trampoline = ReShade::Hooks::Call(&wglSwapLayerBuffers);

		return trampoline(hdc, i);
	}
	else
	{
		return wglSwapBuffers(hdc);
	}
}
EXPORT DWORD WINAPI wglSwapMultipleBuffers(UINT cNumBuffers, CONST WGLSWAP *pBuffers)
{
	for (UINT i = 0; i < cNumBuffers; ++i)
	{
		assert(pBuffers != nullptr);

		wglSwapBuffers(pBuffers[i].hdc);
	}

	return 0;
}
BOOL WINAPI wglSwapIntervalEXT(int interval)
{
	static const auto trampoline = ReShade::Hooks::Call(&wglSwapIntervalEXT);

	return trampoline(interval);
}
EXPORT BOOL WINAPI wglUseFontBitmapsA(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3)
{
	static const auto trampoline = ReShade::Hooks::Call(&wglUseFontBitmapsA);

	return trampoline(hdc, dw1, dw2, dw3);
}
EXPORT BOOL WINAPI wglUseFontBitmapsW(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3)
{
	static const auto trampoline = ReShade::Hooks::Call(&wglUseFontBitmapsW);

	return trampoline(hdc, dw1, dw2, dw3);
}
EXPORT BOOL WINAPI wglUseFontOutlinesA(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3, FLOAT f1, FLOAT f2, int i, LPGLYPHMETRICSFLOAT pgmf)
{
	static const auto trampoline = ReShade::Hooks::Call(&wglUseFontOutlinesA);

	return trampoline(hdc, dw1, dw2, dw3, f1, f2, i, pgmf);
}
EXPORT BOOL WINAPI wglUseFontOutlinesW(HDC hdc, DWORD dw1, DWORD dw2, DWORD dw3, FLOAT f1, FLOAT f2, int i, LPGLYPHMETRICSFLOAT pgmf)
{
	static const auto trampoline = ReShade::Hooks::Call(&wglUseFontOutlinesW);

	return trampoline(hdc, dw1, dw2, dw3, f1, f2, i, pgmf);
}
EXPORT PROC WINAPI wglGetProcAddress(LPCSTR lpszProc)
{
	static const auto trampoline = ReShade::Hooks::Call(&wglGetProcAddress);

	const PROC address = trampoline(lpszProc);

	if (address == nullptr || lpszProc == nullptr)
	{
		return nullptr;
	}
	else if (strcmp(lpszProc, "glBindTexture") == 0)
	{
		return reinterpret_cast<PROC>(&glBindTexture);
	}
	else if (strcmp(lpszProc, "glBlendFunc") == 0)
	{
		return reinterpret_cast<PROC>(&glBlendFunc);
	}
	else if (strcmp(lpszProc, "glClear") == 0)
	{
		return reinterpret_cast<PROC>(&glClear);
	}
	else if (strcmp(lpszProc, "glClearColor") == 0)
	{
		return reinterpret_cast<PROC>(&glClearColor);
	}
	else if (strcmp(lpszProc, "glClearDepth") == 0)
	{
		return reinterpret_cast<PROC>(&glClearDepth);
	}
	else if (strcmp(lpszProc, "glClearStencil") == 0)
	{
		return reinterpret_cast<PROC>(&glClearStencil);
	}
	else if (strcmp(lpszProc, "glCompileShader") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glCompileShader));
	}
	else if (strcmp(lpszProc, "glCopyTexImage1D") == 0)
	{
		return reinterpret_cast<PROC>(&glCopyTexImage1D);
	}
	else if (strcmp(lpszProc, "glCopyTexImage2D") == 0)
	{
		return reinterpret_cast<PROC>(&glCopyTexImage2D);
	}
	else if (strcmp(lpszProc, "glCopyTexSubImage1D") == 0)
	{
		return reinterpret_cast<PROC>(&glCopyTexSubImage1D);
	}
	else if (strcmp(lpszProc, "glCopyTexSubImage2D") == 0)
	{
		return reinterpret_cast<PROC>(&glCopyTexSubImage2D);
	}
	else if (strcmp(lpszProc, "glCopyTexSubImage3D") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glCopyTexSubImage3D));
	}
	else if (strcmp(lpszProc, "glCullFace") == 0)
	{
		return reinterpret_cast<PROC>(&glCullFace);
	}
	else if (strcmp(lpszProc, "glDeleteTextures") == 0)
	{
		return reinterpret_cast<PROC>(&glDeleteTextures);
	}
	else if (strcmp(lpszProc, "glDepthFunc") == 0)
	{
		return reinterpret_cast<PROC>(&glDepthFunc);
	}
	else if (strcmp(lpszProc, "glDepthMask") == 0)
	{
		return reinterpret_cast<PROC>(&glDepthMask);
	}
	else if (strcmp(lpszProc, "glDepthRange") == 0)
	{
		return reinterpret_cast<PROC>(&glDepthRange);
	}
	else if (strcmp(lpszProc, "glDisable") == 0)
	{
		return reinterpret_cast<PROC>(&glDisable);
	}
	else if (strcmp(lpszProc, "glDrawArrays") == 0)
	{
		return reinterpret_cast<PROC>(&glDrawArrays);
	}
	else if (strcmp(lpszProc, "glDrawArraysIndirect") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glDrawArraysIndirect));
	}
	else if (strcmp(lpszProc, "glDrawArraysInstanced") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glDrawArraysInstanced));
	}
	else if (strcmp(lpszProc, "glDrawArraysInstancedARB") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glDrawArraysInstancedARB));
	}
	else if (strcmp(lpszProc, "glDrawArraysInstancedEXT") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glDrawArraysInstancedEXT));
	}
	else if (strcmp(lpszProc, "glDrawArraysInstancedBaseInstance") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glDrawArraysInstancedBaseInstance));
	}
	else if (strcmp(lpszProc, "glDrawBuffer") == 0)
	{
		return reinterpret_cast<PROC>(&glDrawBuffer);
	}
	else if (strcmp(lpszProc, "glDrawElements") == 0)
	{
		return reinterpret_cast<PROC>(&glDrawElements);
	}
	else if (strcmp(lpszProc, "glDrawElementsBaseVertex") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glDrawElementsBaseVertex));
	}
	else if (strcmp(lpszProc, "glDrawElementsIndirect") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glDrawElementsIndirect));
	}
	else if (strcmp(lpszProc, "glDrawElementsInstanced") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glDrawElementsInstanced));
	}
	else if (strcmp(lpszProc, "glDrawElementsInstancedARB") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glDrawElementsInstancedARB));
	}
	else if (strcmp(lpszProc, "glDrawElementsInstancedEXT") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glDrawElementsInstancedEXT));
	}
	else if (strcmp(lpszProc, "glDrawElementsInstancedBaseVertex") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glDrawElementsInstancedBaseVertex));
	}
	else if (strcmp(lpszProc, "glDrawElementsInstancedBaseInstance") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glDrawElementsInstancedBaseInstance));
	}
	else if (strcmp(lpszProc, "glDrawElementsInstancedBaseVertexBaseInstance") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glDrawElementsInstancedBaseVertexBaseInstance));
	}
	else if (strcmp(lpszProc, "glDrawRangeElements") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glDrawRangeElements));
	}
	else if (strcmp(lpszProc, "glDrawRangeElementsBaseVertex") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glDrawRangeElementsBaseVertex));
	}
	else if (strcmp(lpszProc, "glEnable") == 0)
	{
		return reinterpret_cast<PROC>(&glEnable);
	}
	else if (strcmp(lpszProc, "glFinish") == 0)
	{
		return reinterpret_cast<PROC>(&glFinish);
	}
	else if (strcmp(lpszProc, "glFlush") == 0)
	{
		return reinterpret_cast<PROC>(&glFlush);
	}
	else if (strcmp(lpszProc, "glFramebufferRenderbuffer") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glFramebufferRenderbuffer));
	}
	else if (strcmp(lpszProc, "glFramebufferRenderbufferEXT") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glFramebufferRenderbufferEXT));
	}
	else if (strcmp(lpszProc, "glFramebufferTexture") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glFramebufferTexture));
	}
	else if (strcmp(lpszProc, "glFramebufferTextureARB") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glFramebufferTextureARB));
	}
	else if (strcmp(lpszProc, "glFramebufferTextureEXT") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glFramebufferTextureEXT));
	}
	else if (strcmp(lpszProc, "glFramebufferTexture1D") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glFramebufferTexture1D));
	}
	else if (strcmp(lpszProc, "glFramebufferTexture1DEXT") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glFramebufferTexture1DEXT));
	}
	else if (strcmp(lpszProc, "glFramebufferTexture2D") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glFramebufferTexture2D));
	}
	else if (strcmp(lpszProc, "glFramebufferTexture2DEXT") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glFramebufferTexture2DEXT));
	}
	else if (strcmp(lpszProc, "glFramebufferTexture3D") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glFramebufferTexture3D));
	}
	else if (strcmp(lpszProc, "glFramebufferTexture3DEXT") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glFramebufferTexture3DEXT));
	}
	else if (strcmp(lpszProc, "glFramebufferTextureLayer") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glFramebufferTextureLayer));
	}
	else if (strcmp(lpszProc, "glFramebufferTextureLayerARB") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glFramebufferTextureLayerARB));
	}
	else if (strcmp(lpszProc, "glFramebufferTextureLayerEXT") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glFramebufferTextureLayerEXT));
	}
	else if (strcmp(lpszProc, "glFrontFace") == 0)
	{
		return reinterpret_cast<PROC>(&glFrontFace);
	}
	else if (strcmp(lpszProc, "glGenTextures") == 0)
	{
		return reinterpret_cast<PROC>(&glGenTextures);
	}
	else if (strcmp(lpszProc, "glGetBooleanv") == 0)
	{
		return reinterpret_cast<PROC>(&glGetBooleanv);
	}
	else if (strcmp(lpszProc, "glGetDoublev") == 0)
	{
		return reinterpret_cast<PROC>(&glGetDoublev);
	}
	else if (strcmp(lpszProc, "glGetFloatv") == 0)
	{
		return reinterpret_cast<PROC>(&glGetFloatv);
	}
	else if (strcmp(lpszProc, "glGetIntegerv") == 0)
	{
		return reinterpret_cast<PROC>(&glGetIntegerv);
	}
	else if (strcmp(lpszProc, "glGetError") == 0)
	{
		return reinterpret_cast<PROC>(&glGetError);
	}
	else if (strcmp(lpszProc, "glGetPointerv") == 0)
	{
		return reinterpret_cast<PROC>(&glGetPointerv);
	}
	else if (strcmp(lpszProc, "glGetString") == 0)
	{
		return reinterpret_cast<PROC>(&glGetString);
	}
	else if (strcmp(lpszProc, "glGetTexImage") == 0)
	{
		return reinterpret_cast<PROC>(&glGetTexImage);
	}
	else if (strcmp(lpszProc, "glGetTexLevelParameterfv") == 0)
	{
		return reinterpret_cast<PROC>(&glGetTexLevelParameterfv);
	}
	else if (strcmp(lpszProc, "glGetTexLevelParameteriv") == 0)
	{
		return reinterpret_cast<PROC>(&glGetTexLevelParameteriv);
	}
	else if (strcmp(lpszProc, "glGetTexParameterfv") == 0)
	{
		return reinterpret_cast<PROC>(&glGetTexParameterfv);
	}
	else if (strcmp(lpszProc, "glGetTexParameteriv") == 0)
	{
		return reinterpret_cast<PROC>(&glGetTexParameteriv);
	}
	else if (strcmp(lpszProc, "glHint") == 0)
	{
		return reinterpret_cast<PROC>(&glHint);
	}
	else if (strcmp(lpszProc, "glIsEnabled") == 0)
	{
		return reinterpret_cast<PROC>(&glIsEnabled);
	}
	else if (strcmp(lpszProc, "glIsTexture") == 0)
	{
		return reinterpret_cast<PROC>(&glIsTexture);
	}
	else if (strcmp(lpszProc, "glLineWidth") == 0)
	{
		return reinterpret_cast<PROC>(&glLineWidth);
	}
	else if (strcmp(lpszProc, "glLogicOp") == 0)
	{
		return reinterpret_cast<PROC>(&glLogicOp);
	}
	else if (strcmp(lpszProc, "glMultiDrawArrays") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glMultiDrawArrays));
	}
	else if (strcmp(lpszProc, "glMultiDrawArraysIndirect") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glMultiDrawArraysIndirect));
	}
	else if (strcmp(lpszProc, "glMultiDrawElements") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glMultiDrawElements));
	}
	else if (strcmp(lpszProc, "glMultiDrawElementsBaseVertex") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glMultiDrawElementsBaseVertex));
	}
	else if (strcmp(lpszProc, "glMultiDrawElementsIndirect") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glMultiDrawElementsIndirect));
	}
	else if (strcmp(lpszProc, "glPixelStoref") == 0)
	{
		return reinterpret_cast<PROC>(&glPixelStoref);
	}
	else if (strcmp(lpszProc, "glPixelStorei") == 0)
	{
		return reinterpret_cast<PROC>(&glPixelStorei);
	}
	else if (strcmp(lpszProc, "glPointSize") == 0)
	{
		return reinterpret_cast<PROC>(&glPointSize);
	}
	else if (strcmp(lpszProc, "glPolygonMode") == 0)
	{
		return reinterpret_cast<PROC>(&glPolygonMode);
	}
	else if (strcmp(lpszProc, "glPolygonOffset") == 0)
	{
		return reinterpret_cast<PROC>(&glPolygonOffset);
	}
	else if (strcmp(lpszProc, "glReadBuffer") == 0)
	{
		return reinterpret_cast<PROC>(&glReadBuffer);
	}
	else if (strcmp(lpszProc, "glReadPixels") == 0)
	{
		return reinterpret_cast<PROC>(&glReadPixels);
	}
	else if (strcmp(lpszProc, "glScissor") == 0)
	{
		return reinterpret_cast<PROC>(&glScissor);
	}
	else if (strcmp(lpszProc, "glStencilFunc") == 0)
	{
		return reinterpret_cast<PROC>(&glStencilFunc);
	}
	else if (strcmp(lpszProc, "glStencilMask") == 0)
	{
		return reinterpret_cast<PROC>(&glStencilMask);
	}
	else if (strcmp(lpszProc, "glStencilOp") == 0)
	{
		return reinterpret_cast<PROC>(&glStencilOp);
	}
	else if (strcmp(lpszProc, "glTexImage1D") == 0)
	{
		return reinterpret_cast<PROC>(&glTexImage1D);
	}
	else if (strcmp(lpszProc, "glTexImage2D") == 0)
	{
		return reinterpret_cast<PROC>(&glTexImage2D);
	}
	else if (strcmp(lpszProc, "glTexImage3D") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glTexImage3D));
	}
	else if (strcmp(lpszProc, "glTexParameterf") == 0)
	{
		return reinterpret_cast<PROC>(&glTexParameterf);
	}
	else if (strcmp(lpszProc, "glTexParameterfv") == 0)
	{
		return reinterpret_cast<PROC>(&glTexParameterfv);
	}
	else if (strcmp(lpszProc, "glTexParameteri") == 0)
	{
		return reinterpret_cast<PROC>(&glTexParameteri);
	}
	else if (strcmp(lpszProc, "glTexParameteriv") == 0)
	{
		return reinterpret_cast<PROC>(&glTexParameteriv);
	}
	else if (strcmp(lpszProc, "glTexSubImage1D") == 0)
	{
		return reinterpret_cast<PROC>(&glTexSubImage1D);
	}
	else if (strcmp(lpszProc, "glTexSubImage2D") == 0)
	{
		return reinterpret_cast<PROC>(&glTexSubImage2D);
	}
	else if (strcmp(lpszProc, "glTexSubImage3D") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&glTexSubImage3D));
	}
	else if (strcmp(lpszProc, "glViewport") == 0)
	{
		return reinterpret_cast<PROC>(&glViewport);
	}
	else if (strcmp(lpszProc, "wglChoosePixelFormatARB") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&wglChoosePixelFormatARB));
	}
	else if (strcmp(lpszProc, "wglCreateContextAttribsARB") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&wglCreateContextAttribsARB));
	}
	else if (strcmp(lpszProc, "wglGetPixelFormatAttribivARB") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&wglGetPixelFormatAttribivARB));
	}
	else if (strcmp(lpszProc, "wglGetPixelFormatAttribfvARB") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&wglGetPixelFormatAttribfvARB));
	}
	else if (strcmp(lpszProc, "wglGetExtensionsStringARB") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&wglGetExtensionsStringARB));
	}
	else if (strcmp(lpszProc, "wglGetSwapIntervalEXT") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&wglGetSwapIntervalEXT));
	}
	else if (strcmp(lpszProc, "wglSwapIntervalEXT") == 0)
	{
		ReShade::Hooks::Install(reinterpret_cast<ReShade::Hook::Function>(address), reinterpret_cast<ReShade::Hook::Function>(&wglSwapIntervalEXT));
	}

	return address;
}