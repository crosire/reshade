/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include <GL/gl3w.h>

#undef glBindTexture
extern "C" void WINAPI glBindTexture(GLenum target, GLuint texture);
#undef glBindFramebuffer
extern "C" void WINAPI glBindFramebuffer(GLenum target, GLuint framebuffer);
#undef glBlendFunc
extern "C" void WINAPI glBlendFunc(GLenum sfactor, GLenum dfactor);
#undef glClear
extern "C" void WINAPI glClear(GLbitfield mask);
#undef glClearColor
extern "C" void WINAPI glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
#undef glClearDepth
extern "C" void WINAPI glClearDepth(GLclampd depth);
#undef glClearStencil
extern "C" void WINAPI glClearStencil(GLint s);
#undef glColorMask
extern "C" void WINAPI glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
#undef glCopyTexImage1D
extern "C" void WINAPI glCopyTexImage1D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border);
#undef glCopyTexImage2D
extern "C" void WINAPI glCopyTexImage2D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
#undef glCopyTexSubImage1D
extern "C" void WINAPI glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
#undef glCopyTexSubImage2D
extern "C" void WINAPI glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
#undef glCullFace
extern "C" void WINAPI glCullFace(GLenum mode);
#undef glDeleteTextures
extern "C" void WINAPI glDeleteTextures(GLsizei n, const GLuint *textures);
#undef glDepthFunc
extern "C" void WINAPI glDepthFunc(GLenum func);
#undef glDepthMask
extern "C" void WINAPI glDepthMask(GLboolean flag);
#undef glDepthRange
extern "C" void WINAPI glDepthRange(GLclampd zNear, GLclampd zFar);
#undef glDisable
extern "C" void WINAPI glDisable(GLenum cap);
#undef glDrawArrays
extern "C" void WINAPI glDrawArrays(GLenum mode, GLint first, GLsizei count);
#undef glDrawArraysIndirect
extern "C" void WINAPI glDrawArraysIndirect(GLenum mode, const GLvoid *indirect);
#undef glDrawArraysInstanced
extern "C" void WINAPI glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount);
#undef glDrawArraysInstancedBaseInstance
extern "C" void WINAPI glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei primcount, GLuint baseinstance);
#undef glDrawArraysInstancedARB
extern "C" void WINAPI glDrawArraysInstancedARB(GLenum mode, GLint first, GLsizei count, GLsizei primcount);
#undef glDrawArraysInstancedEXT
extern "C" void WINAPI glDrawArraysInstancedEXT(GLenum mode, GLint first, GLsizei count, GLsizei primcount);
#undef glDrawBuffer
extern "C" void WINAPI glDrawBuffer(GLenum mode);
#undef glDrawElements
extern "C" void WINAPI glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
#undef glDrawElementsBaseVertex
extern "C" void WINAPI glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex);
#undef glDrawElementsIndirect
extern "C" void WINAPI glDrawElementsIndirect(GLenum mode, GLenum type, const GLvoid *indirect);
#undef glDrawElementsInstanced
extern "C" void WINAPI glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount);
#undef glDrawElementsInstancedBaseVertex
extern "C" void WINAPI glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex);
#undef glDrawElementsInstancedBaseInstance
extern "C" void WINAPI glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLuint baseinstance);
#undef glDrawElementsInstancedBaseVertexBaseInstance
extern "C" void WINAPI glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex, GLuint baseinstance);
#undef glDrawElementsInstancedARB
extern "C" void WINAPI glDrawElementsInstancedARB(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount);
#undef glDrawElementsInstancedEXT
extern "C" void WINAPI glDrawElementsInstancedEXT(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount);
#undef glDrawRangeElements
extern "C" void WINAPI glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);
#undef glDrawRangeElementsBaseVertex
extern "C" void WINAPI glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex);
#undef glEnable
extern "C" void WINAPI glEnable(GLenum cap);
#undef glFinish
extern "C" void WINAPI glFinish();
#undef glFlush
extern "C" void WINAPI glFlush();
#undef glFrontFace
extern "C" void WINAPI glFrontFace(GLenum mode);
#undef glGenTextures
extern "C" void WINAPI glGenTextures(GLsizei n, GLuint *textures);
#undef glGetBooleanv
extern "C" void WINAPI glGetBooleanv(GLenum pname, GLboolean *params);
#undef glGetDoublev
extern "C" void WINAPI glGetDoublev(GLenum pname, GLdouble *params);
#undef glGetFloatv
extern "C" void WINAPI glGetFloatv(GLenum pname, GLfloat *params);
#undef glGetIntegerv
extern "C" void WINAPI glGetIntegerv(GLenum pname, GLint *params);
#undef glGetError
extern "C" GLenum WINAPI glGetError();
#undef glGetPointerv
extern "C" void WINAPI glGetPointerv(GLenum pname, GLvoid **params);
#undef glGetString
extern "C" const GLubyte *WINAPI glGetString(GLenum name);
#undef glGetTexImage
extern "C" void WINAPI glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels);
#undef glGetTexLevelParameterfv
extern "C" void WINAPI glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params);
#undef glGetTexLevelParameteriv
extern "C" void WINAPI glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params);
#undef glGetTexParameterfv
extern "C" void WINAPI glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params);
#undef glGetTexParameteriv
extern "C" void WINAPI glGetTexParameteriv(GLenum target, GLenum pname, GLint *params);
#undef glHint
extern "C" void WINAPI glHint(GLenum target, GLenum mode);
#undef glIsEnabled
extern "C" GLboolean WINAPI glIsEnabled(GLenum cap);
#undef glIsTexture
extern "C" GLboolean WINAPI glIsTexture(GLuint texture);
#undef glLineWidth
extern "C" void WINAPI glLineWidth(GLfloat width);
#undef glLogicOp
extern "C" void WINAPI glLogicOp(GLenum opcode);
#undef glMultiDrawArrays
extern "C" void WINAPI glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount);
#undef glMultiDrawArraysIndirect
extern "C" void WINAPI glMultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride);
#undef glMultiDrawElements
extern "C" void WINAPI glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei drawcount);
#undef glMultiDrawElementsBaseVertex
extern "C" void WINAPI glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei drawcount, const GLint *basevertex);
#undef glMultiDrawElementsIndirect
extern "C" void WINAPI glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride);
#undef glPixelStoref
extern "C" void WINAPI glPixelStoref(GLenum pname, GLfloat param);
#undef glPixelStorei
extern "C" void WINAPI glPixelStorei(GLenum pname, GLint param);
#undef glPointSize
extern "C" void WINAPI glPointSize(GLfloat size);
#undef glPolygonMode
extern "C" void WINAPI glPolygonMode(GLenum face, GLenum mode);
#undef glPolygonOffset
extern "C" void WINAPI glPolygonOffset(GLfloat factor, GLfloat units);
#undef glReadBuffer
extern "C" void WINAPI glReadBuffer(GLenum mode);
#undef glReadPixels
extern "C" void WINAPI glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
#undef glScissor
extern "C" void WINAPI glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
#undef glStencilFunc
extern "C" void WINAPI glStencilFunc(GLenum func, GLint ref, GLuint mask);
#undef glStencilMask
extern "C" void WINAPI glStencilMask(GLuint mask);
#undef glStencilOp
extern "C" void WINAPI glStencilOp(GLenum fail, GLenum zfail, GLenum zpass);
#undef glTexImage1D
extern "C" void WINAPI glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
#undef glTexImage2D
extern "C" void WINAPI glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
#undef glTexImage3D
extern "C" void WINAPI glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
#undef glTexParameterf
extern "C" void WINAPI glTexParameterf(GLenum target, GLenum pname, GLfloat param);
#undef glTexParameterfv
extern "C" void WINAPI glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params);
#undef glTexParameteri
extern "C" void WINAPI glTexParameteri(GLenum target, GLenum pname, GLint param);
#undef glTexParameteriv
extern "C" void WINAPI glTexParameteriv(GLenum target, GLenum pname, const GLint *params);
#undef glTexSubImage1D
extern "C" void WINAPI glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
#undef glTexSubImage2D
extern "C" void WINAPI glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
#undef glViewport
extern "C" void WINAPI glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
