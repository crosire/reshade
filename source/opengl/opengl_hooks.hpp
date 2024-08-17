/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <GL/gl3w.h>

#undef glBindBuffer
extern "C" void APIENTRY glBindBuffer(GLenum target, GLuint buffer);
#undef glBindBufferBase
extern "C" void APIENTRY glBindBufferBase(GLenum target, GLuint index, GLuint buffer);
#undef glBindBufferRange
extern "C" void APIENTRY glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
#undef glBindBuffersBase
extern "C" void APIENTRY glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint *buffers);
#undef glBindBuffersRange
extern "C" void APIENTRY glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLintptr *sizes);
#undef glBindFramebuffer
extern "C" void APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer);
#undef glBindFramebufferEXT
extern "C" void APIENTRY glBindFramebufferEXT(GLenum target, GLuint framebuffer);
#undef glBindImageTexture
extern "C" void APIENTRY glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);
#undef glBindImageTextures
extern "C" void APIENTRY glBindImageTextures(GLuint first, GLsizei count, const GLuint *textures);
#undef glBindMultiTextureEXT
extern "C" void APIENTRY glBindMultiTextureEXT(GLenum texunit, GLenum target, GLuint texture);
#undef glBindProgramARB
extern "C" void APIENTRY glBindProgramARB(GLenum target, GLuint program);
#undef glBindTexture
extern "C" void APIENTRY glBindTexture(GLenum target, GLuint texture);
#undef glBindTextureUnit
extern "C" void APIENTRY glBindTextureUnit(GLuint unit, GLuint texture);
#undef glBindTextures
extern "C" void APIENTRY glBindTextures(GLuint first, GLsizei count, const GLuint *textures);
#undef glBindVertexArray
extern "C" void APIENTRY glBindVertexArray(GLuint array);
#undef glBindVertexBuffer
extern "C" void APIENTRY glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
#undef glBindVertexBuffers
extern "C" void APIENTRY glBindVertexBuffers(GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides);
#undef glBlendColor
extern "C" void APIENTRY glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
#undef glBlendEquation
extern "C" void APIENTRY glBlendEquation(GLenum mode);
#undef glBlendEquationSeparate
extern "C" void APIENTRY glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
#undef glBlendFunc
extern "C" void APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor);
#undef glBlendFuncSeparate
extern "C" void APIENTRY glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
#undef glBlitFramebuffer
extern "C" void APIENTRY glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
#undef glBlitNamedFramebuffer
extern "C" void APIENTRY glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
#undef glBufferData
extern "C" void APIENTRY glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
#undef glBufferStorage
extern "C" void APIENTRY glBufferStorage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags);
#undef glBufferSubData
extern "C" void APIENTRY glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
#undef glClear
extern "C" void APIENTRY glClear(GLbitfield mask);
#undef glClearBufferiv
extern "C" void APIENTRY glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value);
#undef glClearBufferuiv
extern "C" void APIENTRY glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value);
#undef glClearBufferfv
extern "C" void APIENTRY glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value);
#undef glClearBufferfi
extern "C" void APIENTRY glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
#undef glClearNamedFramebufferiv
extern "C" void APIENTRY glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value);
#undef glClearNamedFramebufferuiv
extern "C" void APIENTRY glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value);
#undef glClearNamedFramebufferfv
extern "C" void APIENTRY glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value);
#undef glClearNamedFramebufferfi
extern "C" void APIENTRY glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
#undef glClearColor
extern "C" void APIENTRY glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
#undef glClearDepth
extern "C" void APIENTRY glClearDepth(GLclampd depth);
#undef glClearStencil
extern "C" void APIENTRY glClearStencil(GLint s);
#undef glColorMask
extern "C" void APIENTRY glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
#undef glCompressedTexImage1D
extern "C" void APIENTRY glCompressedTexImage1D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *data);
#undef glCompressedTexImage2D
extern "C" void APIENTRY glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data);
#undef glCompressedTexImage3D
extern "C" void APIENTRY glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data);
#undef glCompressedTexSubImage1D
extern "C" void APIENTRY glCompressedTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data);
#undef glCompressedTexSubImage2D
extern "C" void APIENTRY glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
#undef glCompressedTexSubImage3D
extern "C" void APIENTRY glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data);
#undef glCompressedTextureSubImage1D
extern "C" void APIENTRY glCompressedTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data);
#undef glCompressedTextureSubImage2D
extern "C" void APIENTRY glCompressedTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
#undef glCompressedTextureSubImage3D
extern "C" void APIENTRY glCompressedTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data);
#undef glCopyBufferSubData
extern "C" void APIENTRY glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
#undef glCopyImageSubData
extern "C" void APIENTRY glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
#undef glCopyNamedBufferSubData
extern "C" void APIENTRY glCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
#undef glCopyTexImage1D
extern "C" void APIENTRY glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
#undef glCopyTexImage2D
extern "C" void APIENTRY glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
#undef glCopyTexSubImage1D
extern "C" void APIENTRY glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
#undef glCopyTexSubImage2D
extern "C" void APIENTRY glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
#undef glCopyTexSubImage3D
extern "C" void APIENTRY glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
#undef glCopyTextureSubImage1D
extern "C" void APIENTRY glCopyTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
#undef glCopyTextureSubImage2D
extern "C" void APIENTRY glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
#undef glCopyTextureSubImage3D
extern "C" void APIENTRY glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
#undef glCullFace
extern "C" void APIENTRY glCullFace(GLenum mode);
#undef glDeleteBuffers
extern "C" void APIENTRY glDeleteBuffers(GLsizei n, const GLuint *buffers);
#undef glDeleteProgram
extern "C" void APIENTRY glDeleteProgram(GLuint program);
#undef glDeleteProgramsARB
extern "C" void APIENTRY glDeleteProgramsARB(GLsizei n, const GLuint *programs);
#undef glDeleteRenderbuffers
extern "C" void APIENTRY glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers);
#undef glDeleteTextures
extern "C" void APIENTRY glDeleteTextures(GLsizei n, const GLuint *textures);
#undef glDeleteVertexArrays
extern "C" void APIENTRY glDeleteVertexArrays(GLsizei n, const GLuint *arrays);
#undef glDepthFunc
extern "C" void APIENTRY glDepthFunc(GLenum func);
#undef glDepthMask
extern "C" void APIENTRY glDepthMask(GLboolean flag);
#undef glDepthRange
extern "C" void APIENTRY glDepthRange(GLclampd zNear, GLclampd zFar);
#undef glDisable
extern "C" void APIENTRY glDisable(GLenum cap);
#undef glDispatchCompute
extern "C" void APIENTRY glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
#undef glDispatchComputeIndirect
extern "C" void APIENTRY glDispatchComputeIndirect(GLintptr indirect);
#undef glDrawArrays
extern "C" void APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count);
#undef glDrawArraysIndirect
extern "C" void APIENTRY glDrawArraysIndirect(GLenum mode, const GLvoid *indirect);
#undef glDrawArraysInstanced
extern "C" void APIENTRY glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount);
#undef glDrawArraysInstancedBaseInstance
extern "C" void APIENTRY glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei primcount, GLuint baseinstance);
#undef glDrawBuffer
extern "C" void APIENTRY glDrawBuffer(GLenum mode);
#undef glDrawElements
extern "C" void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
#undef glDrawElementsBaseVertex
extern "C" void APIENTRY glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex);
#undef glDrawElementsIndirect
extern "C" void APIENTRY glDrawElementsIndirect(GLenum mode, GLenum type, const GLvoid *indirect);
#undef glDrawElementsInstanced
extern "C" void APIENTRY glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount);
#undef glDrawElementsInstancedBaseVertex
extern "C" void APIENTRY glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex);
#undef glDrawElementsInstancedBaseInstance
extern "C" void APIENTRY glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLuint baseinstance);
#undef glDrawElementsInstancedBaseVertexBaseInstance
extern "C" void APIENTRY glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex, GLuint baseinstance);
#undef glDrawRangeElements
extern "C" void APIENTRY glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);
#undef glDrawRangeElementsBaseVertex
extern "C" void APIENTRY glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex);
#undef glEnable
extern "C" void APIENTRY glEnable(GLenum cap);
#undef glFinish
extern "C" void APIENTRY glFinish();
#undef glFlush
extern "C" void APIENTRY glFlush();
#undef glFramebufferTexture
extern "C" void APIENTRY glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level);
#undef glFramebufferTexture1D
extern "C" void APIENTRY glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
#undef glFramebufferTexture2D
extern "C" void APIENTRY glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
#undef glFramebufferTexture3D
extern "C" void APIENTRY glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint layer);
#undef glFramebufferTextureLayer
extern "C" void APIENTRY glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
#undef glFramebufferRenderbuffer
extern "C" void APIENTRY glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
#undef glFrontFace
extern "C" void APIENTRY glFrontFace(GLenum mode);
#undef glGenerateMipmap
extern "C" void APIENTRY glGenerateMipmap(GLenum target);
#undef glGenerateTextureMipmap
extern "C" void APIENTRY glGenerateTextureMipmap(GLuint texture);
#undef glGenTextures
extern "C" void APIENTRY glGenTextures(GLsizei n, GLuint *textures);
#undef glGetBooleanv
extern "C" void APIENTRY glGetBooleanv(GLenum pname, GLboolean *params);
#undef glGetDoublev
extern "C" void APIENTRY glGetDoublev(GLenum pname, GLdouble *params);
#undef glGetFloatv
extern "C" void APIENTRY glGetFloatv(GLenum pname, GLfloat *params);
#undef glGetIntegerv
extern "C" void APIENTRY glGetIntegerv(GLenum pname, GLint *params);
#undef glGetError
extern "C" auto APIENTRY glGetError() -> GLenum;
#undef glGetPointerv
extern "C" void APIENTRY glGetPointerv(GLenum pname, GLvoid **params);
#undef glGetString
extern "C" auto APIENTRY glGetString(GLenum name) -> const GLubyte *;
#undef glGetTexImage
extern "C" void APIENTRY glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels);
#undef glGetTexLevelParameterfv
extern "C" void APIENTRY glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params);
#undef glGetTexLevelParameteriv
extern "C" void APIENTRY glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params);
#undef glGetTexParameterfv
extern "C" void APIENTRY glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params);
#undef glGetTexParameteriv
extern "C" void APIENTRY glGetTexParameteriv(GLenum target, GLenum pname, GLint *params);
#undef glHint
extern "C" void APIENTRY glHint(GLenum target, GLenum mode);
#undef glIsEnabled
extern "C" auto APIENTRY glIsEnabled(GLenum cap) -> GLboolean;
#undef glIsTexture
extern "C" auto APIENTRY glIsTexture(GLuint texture) -> GLboolean;
#undef glLineWidth
extern "C" void APIENTRY glLineWidth(GLfloat width);
#undef glLinkProgram
extern "C" void APIENTRY glLinkProgram(GLuint program);
#undef glLogicOp
extern "C" void APIENTRY glLogicOp(GLenum opcode);
#undef glMapBuffer
extern "C" auto APIENTRY glMapBuffer(GLenum target, GLenum access) -> void *;
#undef glMapBufferRange
extern "C" auto APIENTRY glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLenum access) -> void *;
#undef glMapNamedBuffer
extern "C" auto APIENTRY glMapNamedBuffer(GLuint buffer, GLenum access) -> void *;
#undef glMapNamedBufferRange
extern "C" auto APIENTRY glMapNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length, GLenum access) -> void *;
#undef glMultiDrawArrays
extern "C" void APIENTRY glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount);
#undef glMultiDrawArraysIndirect
extern "C" void APIENTRY glMultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride);
#undef glMultiDrawElements
extern "C" void APIENTRY glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei drawcount);
#undef glMultiDrawElementsBaseVertex
extern "C" void APIENTRY glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei drawcount, const GLint *basevertex);
#undef glMultiDrawElementsIndirect
extern "C" void APIENTRY glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride);
#undef glNamedBufferData
extern "C" void APIENTRY glNamedBufferData(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage);
#undef glNamedBufferStorage
extern "C" void APIENTRY glNamedBufferStorage(GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags);
#undef glNamedBufferSubData
extern "C" void APIENTRY glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, const void* data);
#undef glNamedRenderbufferStorage
extern "C" void APIENTRY glNamedRenderbufferStorage(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height);
#undef glNamedRenderbufferStorageMultisample
extern "C" void APIENTRY glNamedRenderbufferStorageMultisample(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
#undef glPixelStoref
extern "C" void APIENTRY glPixelStoref(GLenum pname, GLfloat param);
#undef glPixelStorei
extern "C" void APIENTRY glPixelStorei(GLenum pname, GLint param);
#undef glPointSize
extern "C" void APIENTRY glPointSize(GLfloat size);
#undef glPolygonMode
extern "C" void APIENTRY glPolygonMode(GLenum face, GLenum mode);
#undef glPolygonOffset
extern "C" void APIENTRY glPolygonOffset(GLfloat factor, GLfloat units);
#undef glProgramStringARB
extern "C" void APIENTRY glProgramStringARB(GLenum target, GLenum format, GLsizei length, const GLvoid *string);
#undef glReadBuffer
extern "C" void APIENTRY glReadBuffer(GLenum mode);
#undef glReadPixels
extern "C" void APIENTRY glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
#undef glRenderbufferStorage
extern "C" void APIENTRY glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
#undef glRenderbufferStorageMultisample
extern "C" void APIENTRY glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
#undef glScissor
extern "C" void APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
#undef glScissorArrayv
extern "C" void APIENTRY glScissorArrayv(GLuint first, GLsizei count, const GLint *v);
#undef glScissorIndexed
extern "C" void APIENTRY glScissorIndexed(GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height);
#undef glScissorIndexedv
extern "C" void APIENTRY glScissorIndexedv(GLuint index, const GLint *v);
#undef glShaderSource
extern "C" void APIENTRY glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length);
#undef glStencilFunc
extern "C" void APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask);
#undef glStencilFuncSeparate
extern "C" void APIENTRY glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
#undef glStencilOp
extern "C" void APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass);
#undef glStencilOpSeparate
extern "C" void APIENTRY glStencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
#undef glStencilMask
extern "C" void APIENTRY glStencilMask(GLuint mask);
#undef glStencilMaskSeparate
extern "C" void APIENTRY glStencilMaskSeparate(GLenum face, GLuint mask);
#undef glTexBuffer
extern "C" void APIENTRY glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer);
#undef glTextureBuffer
extern "C" void APIENTRY glTextureBuffer(GLuint texture, GLenum internalformat, GLuint buffer);
#undef glTexBufferRange
extern "C" void APIENTRY glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);
#undef glTextureBufferRange
extern "C" void APIENTRY glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);
#undef glTexImage1D
extern "C" void APIENTRY glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
#undef glTexImage2D
extern "C" void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
#undef glTexImage2DMultisample
extern "C" void APIENTRY glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
#undef glTexImage3D
extern "C" void APIENTRY glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
#undef glTexImage3DMultisample
extern "C" void APIENTRY glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
#undef glTexParameterf
extern "C" void APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param);
#undef glTexParameterfv
extern "C" void APIENTRY glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params);
#undef glTexParameteri
extern "C" void APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param);
#undef glTexParameteriv
extern "C" void APIENTRY glTexParameteriv(GLenum target, GLenum pname, const GLint *params);
#undef glTexStorage1D
extern "C" void APIENTRY glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
#undef glTexStorage2D
extern "C" void APIENTRY glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
#undef glTexStorage2DMultisample
extern "C" void APIENTRY glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
#undef glTexStorage3D
extern "C" void APIENTRY glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
#undef glTexStorage3DMultisample
extern "C" void APIENTRY glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
#undef glTexSubImage1D
extern "C" void APIENTRY glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
#undef glTexSubImage2D
extern "C" void APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
#undef glTexSubImage3D
extern "C" void APIENTRY glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
#undef glTextureStorage1D
extern "C" void APIENTRY glTextureStorage1D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width);
#undef glTextureStorage2D
extern "C" void APIENTRY glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
#undef glTextureStorage2DMultisample
extern "C" void APIENTRY glTextureStorage2DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
#undef glTextureStorage3D
extern "C" void APIENTRY glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
#undef glTextureStorage3DMultisample
extern "C" void APIENTRY glTextureStorage3DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
#undef glTextureSubImage1D
extern "C" void APIENTRY glTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
#undef glTextureSubImage2D
extern "C" void APIENTRY glTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
#undef glTextureSubImage3D
extern "C" void APIENTRY glTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
#undef glTextureView
extern "C" void APIENTRY glTextureView(GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers);
#undef glUniform1f
extern "C" void APIENTRY glUniform1f(GLint location, GLfloat v0);
#undef glUniform2f
extern "C" void APIENTRY glUniform2f(GLint location, GLfloat v0, GLfloat v1);
#undef glUniform3f
extern "C" void APIENTRY glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
#undef glUniform4f
extern "C" void APIENTRY glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
#undef glUniform1i
extern "C" void APIENTRY glUniform1i(GLint location, GLint v0);
#undef glUniform2i
extern "C" void APIENTRY glUniform2i(GLint location, GLint v0, GLint v1);
#undef glUniform3i
extern "C" void APIENTRY glUniform3i(GLint location, GLint v0, GLint v1, GLint v2);
#undef glUniform4i
extern "C" void APIENTRY glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
#undef glUniform1ui
extern "C" void APIENTRY glUniform1ui(GLint location, GLuint v0);
#undef glUniform2ui
extern "C" void APIENTRY glUniform2ui(GLint location, GLuint v0, GLuint v1);
#undef glUniform3ui
extern "C" void APIENTRY glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2);
#undef glUniform4ui
extern "C" void APIENTRY glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
#undef glUniform1fv
extern "C" void APIENTRY glUniform1fv(GLint location, GLsizei count, const GLfloat *value);
#undef glUniform2fv
extern "C" void APIENTRY glUniform2fv(GLint location, GLsizei count, const GLfloat *value);
#undef glUniform3fv
extern "C" void APIENTRY glUniform3fv(GLint location, GLsizei count, const GLfloat *value);
#undef glUniform4fv
extern "C" void APIENTRY glUniform4fv(GLint location, GLsizei count, const GLfloat *value);
#undef glUniform1iv
extern "C" void APIENTRY glUniform1iv(GLint location, GLsizei count, const GLint *value);
#undef glUniform2iv
extern "C" void APIENTRY glUniform2iv(GLint location, GLsizei count, const GLint *value);
#undef glUniform3iv
extern "C" void APIENTRY glUniform3iv(GLint location, GLsizei count, const GLint *value);
#undef glUniform4iv
extern "C" void APIENTRY glUniform4iv(GLint location, GLsizei count, const GLint *value);
#undef glUniform1uiv
extern "C" void APIENTRY glUniform1uiv(GLint location, GLsizei count, const GLuint *value);
#undef glUniform2uiv
extern "C" void APIENTRY glUniform2uiv(GLint location, GLsizei count, const GLuint *value);
#undef glUniform3uiv
extern "C" void APIENTRY glUniform3uiv(GLint location, GLsizei count, const GLuint *value);
#undef glUniform4uiv
extern "C" void APIENTRY glUniform4uiv(GLint location, GLsizei count, const GLuint *value);
#undef glUniformMatrix2fv
extern "C" void APIENTRY glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
#undef glUniformMatrix3fv
extern "C" void APIENTRY glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
#undef glUniformMatrix4fv
extern "C" void APIENTRY glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
#undef glUniformMatrix2x3fv
extern "C" void APIENTRY glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
#undef glUniformMatrix3x2fv
extern "C" void APIENTRY glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
#undef glUniformMatrix2x4fv
extern "C" void APIENTRY glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
#undef glUniformMatrix4x2fv
extern "C" void APIENTRY glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
#undef glUniformMatrix3x4fv
extern "C" void APIENTRY glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
#undef glUniformMatrix4x3fv
extern "C" void APIENTRY glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
#undef glUnmapBuffer
extern "C" void APIENTRY glUnmapBuffer(GLenum target);
#undef glUnmapNamedBuffer
extern "C" void APIENTRY glUnmapNamedBuffer(GLuint buffer);
#undef glUseProgram
extern "C" void APIENTRY glUseProgram(GLuint program);
#undef glVertexAttribPointer
extern "C" void APIENTRY glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
#undef glVertexAttribIPointer
extern "C" void APIENTRY glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer);
#undef glVertexAttribLPointer
extern "C" void APIENTRY glVertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer);
#undef glViewport
extern "C" void APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
#undef glViewportArrayv
extern "C" void APIENTRY glViewportArrayv(GLuint first, GLsizei count, const GLfloat *v);
#undef glViewportIndexedf
extern "C" void APIENTRY glViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h);
#undef glViewportIndexedfv
extern "C" void APIENTRY glViewportIndexedfv(GLuint index, const GLfloat *v);
