/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include <GL/gl3w.h>

#undef glBindBuffer
extern "C" void WINAPI glBindBuffer(GLenum target, GLuint buffer);
#undef glBindBufferBase
extern "C" void WINAPI glBindBufferBase(GLenum target, GLuint index, GLuint buffer);
#undef glBindBufferRange
extern "C" void WINAPI glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
#undef glBindBuffersBase
extern "C" void WINAPI glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint *buffers);
#undef glBindBuffersRange
extern "C" void WINAPI glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLintptr *sizes);
#undef glBindFramebuffer
extern "C" void WINAPI glBindFramebuffer(GLenum target, GLuint framebuffer);
#undef glBindImageTexture
extern "C" void WINAPI glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);
#undef glBindImageTextures
extern "C" void WINAPI glBindImageTextures(GLuint first, GLsizei count, const GLuint *textures);
#undef glBindSampler
extern "C" void WINAPI glBindSampler(GLuint unit, GLuint sampler);
#undef glBindSamplers
extern "C" void WINAPI glBindSamplers(GLuint first, GLsizei count, const GLuint *samplers);
#undef glBindTexture
extern "C" void WINAPI glBindTexture(GLenum target, GLuint texture);
#undef glBindTextureUnit
extern "C" void WINAPI glBindTextureUnit(GLuint unit, GLuint texture);
#undef glBindTextures
extern "C" void WINAPI glBindTextures(GLuint first, GLsizei count, const GLuint *textures);
#undef glBindVertexBuffer
extern "C" void WINAPI glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
#undef glBindVertexBuffers
extern "C" void WINAPI glBindVertexBuffers(GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides);
#undef glBlendFunc
extern "C" void WINAPI glBlendFunc(GLenum sfactor, GLenum dfactor);
#undef glBlitFramebuffer
extern "C" void WINAPI glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
#undef glBlitNamedFramebuffer
extern "C" void WINAPI glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
#undef glBufferData
extern "C" void WINAPI glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
#undef glBufferStorage
extern "C" void WINAPI glBufferStorage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags);
#undef glClear
extern "C" void WINAPI glClear(GLbitfield mask);
#undef glClearBufferfv
extern "C" void WINAPI glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value);
#undef glClearBufferfi
extern "C" void WINAPI glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
#undef glClearNamedFramebufferfv
extern "C" void WINAPI glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value);
#undef glClearNamedFramebufferfi
extern "C" void WINAPI glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
#undef glClearColor
extern "C" void WINAPI glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
#undef glClearDepth
extern "C" void WINAPI glClearDepth(GLclampd depth);
#undef glClearStencil
extern "C" void WINAPI glClearStencil(GLint s);
#undef glColorMask
extern "C" void WINAPI glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
#undef glCopyBufferSubData
extern "C" void WINAPI glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
#undef glCopyImageSubData
extern "C" void WINAPI glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
#undef glCopyNamedBufferSubData
extern "C" void WINAPI glCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
#undef glCopyTexImage1D
extern "C" void WINAPI glCopyTexImage1D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border);
#undef glCopyTexImage2D
extern "C" void WINAPI glCopyTexImage2D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
#undef glCopyTexSubImage1D
extern "C" void WINAPI glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
#undef glCopyTexSubImage2D
extern "C" void WINAPI glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
#undef glCopyTexSubImage3D
extern "C" void WINAPI glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
#undef glCopyTextureSubImage1D
extern "C" void WINAPI glCopyTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
#undef glCopyTextureSubImage2D
extern "C" void WINAPI glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
#undef glCopyTextureSubImage3D
extern "C" void WINAPI glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
#undef glCullFace
extern "C" void WINAPI glCullFace(GLenum mode);
#undef glDeleteBuffers
extern "C" void WINAPI glDeleteBuffers(GLsizei n, const GLuint *buffers);
#undef glDeleteFramebuffers
extern "C" void WINAPI glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers);
#undef glDeleteSamplers
extern "C" void WINAPI glDeleteSamplers(GLsizei n, const GLuint *samplers);
#undef glDeleteShader
extern "C" void WINAPI glDeleteShader(GLuint shader);
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
#undef glDispatchCompute
extern "C" void WINAPI glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
#undef glDispatchComputeIndirect
extern "C" void WINAPI glDispatchComputeIndirect(GLintptr indirect);
#undef glDrawArrays
extern "C" void WINAPI glDrawArrays(GLenum mode, GLint first, GLsizei count);
#undef glDrawArraysIndirect
extern "C" void WINAPI glDrawArraysIndirect(GLenum mode, const GLvoid *indirect);
#undef glDrawArraysInstanced
extern "C" void WINAPI glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount);
#undef glDrawArraysInstancedBaseInstance
extern "C" void WINAPI glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei primcount, GLuint baseinstance);
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
#undef glGenerateMipmap
extern "C" void WINAPI glGenerateMipmap(GLenum target);
#undef glGenerateTextureMipmap
extern "C" void WINAPI glGenerateTextureMipmap(GLuint texture);
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
#undef glNamedBufferData
extern "C" void WINAPI glNamedBufferData(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage);
#undef glNamedBufferStorage
extern "C" void WINAPI glNamedBufferStorage(GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags);
#undef glNamedRenderbufferStorage
extern "C" void WINAPI glNamedRenderbufferStorage(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height);
#undef glNamedRenderbufferStorageMultisample
extern "C" void WINAPI glNamedRenderbufferStorageMultisample(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
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
#undef glRenderbufferStorage
extern "C" void WINAPI glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
#undef glRenderbufferStorageMultisample
extern "C" void WINAPI glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
#undef glScissor
extern "C" void WINAPI glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
#undef glScissorArrayv
extern "C" void WINAPI glScissorArrayv(GLuint first, GLsizei count, const GLint *v);
#undef glScissorIndexed
extern "C" void WINAPI glScissorIndexed(GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height);
#undef glScissorIndexedv
extern "C" void WINAPI glScissorIndexedv(GLuint index, const GLint *v);
#undef glShaderSource
extern "C" void WINAPI glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length);
#undef glStencilFunc
extern "C" void WINAPI glStencilFunc(GLenum func, GLint ref, GLuint mask);
#undef glStencilMask
extern "C" void WINAPI glStencilMask(GLuint mask);
#undef glStencilOp
extern "C" void WINAPI glStencilOp(GLenum fail, GLenum zfail, GLenum zpass);
#undef glTexBuffer
extern "C" void WINAPI glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer);
#undef glTextureBuffer
extern "C" void WINAPI glTextureBuffer(GLuint texture, GLenum internalformat, GLuint buffer);
#undef glTexBufferRange
extern "C" void WINAPI glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);
#undef glTextureBufferRange
extern "C" void WINAPI glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);
#undef glTexImage1D
extern "C" void WINAPI glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
#undef glTexImage2D
extern "C" void WINAPI glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
#undef glTexImage2DMultisample
extern "C" void WINAPI glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
#undef glTexImage3D
extern "C" void WINAPI glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
#undef glTexImage3DMultisample
extern "C" void WINAPI glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
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
#undef glTexStorage1D
extern "C" void WINAPI glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
#undef glTexStorage2D
extern "C" void WINAPI glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
#undef glTexStorage2DMultisample
extern "C" void WINAPI glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
#undef glTexStorage3D
extern "C" void WINAPI glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
#undef glTexStorage3DMultisample
extern "C" void WINAPI glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
#undef glTextureStorage1D
extern "C" void WINAPI glTextureStorage1D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width);
#undef glTextureStorage2D
extern "C" void WINAPI glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
#undef glTextureStorage2DMultisample
extern "C" void WINAPI glTextureStorage2DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
#undef glTextureStorage3D
extern "C" void WINAPI glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
#undef glTextureStorage3DMultisample
extern "C" void WINAPI glTextureStorage3DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
#undef glTextureView
extern "C" void WINAPI glTextureView(GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers);
#undef glUniform1f
extern "C" void WINAPI glUniform1f(GLint location, GLfloat v0);
#undef glUniform2f
extern "C" void WINAPI glUniform2f(GLint location, GLfloat v0, GLfloat v1);
#undef glUniform3f
extern "C" void WINAPI glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
#undef glUniform4f
extern "C" void WINAPI glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
#undef glUniform1i
extern "C" void WINAPI glUniform1i(GLint location, GLint v0);
#undef glUniform2i
extern "C" void WINAPI glUniform2i(GLint location, GLint v0, GLint v1);
#undef glUniform3i
extern "C" void WINAPI glUniform3i(GLint location, GLint v0, GLint v1, GLint v2);
#undef glUniform4i
extern "C" void WINAPI glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
#undef glUniform1ui
extern "C" void WINAPI glUniform1ui(GLint location, GLuint v0);
#undef glUniform2ui
extern "C" void WINAPI glUniform2ui(GLint location, GLuint v0, GLuint v1);
#undef glUniform3ui
extern "C" void WINAPI glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2);
#undef glUniform4ui
extern "C" void WINAPI glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
#undef glUniform1fv
extern "C" void WINAPI glUniform1fv(GLint location, GLsizei count, const GLfloat *v);
#undef glUniform2fv
extern "C" void WINAPI glUniform2fv(GLint location, GLsizei count, const GLfloat *v);
#undef glUniform3fv
extern "C" void WINAPI glUniform3fv(GLint location, GLsizei count, const GLfloat *v);
#undef glUniform4fv
extern "C" void WINAPI glUniform4fv(GLint location, GLsizei count, const GLfloat *v);
#undef glUniform1iv
extern "C" void WINAPI glUniform1iv(GLint location, GLsizei count, const GLint *v);
#undef glUniform2iv
extern "C" void WINAPI glUniform2iv(GLint location, GLsizei count, const GLint *v);
#undef glUniform3iv
extern "C" void WINAPI glUniform3iv(GLint location, GLsizei count, const GLint *v);
#undef glUniform4iv
extern "C" void WINAPI glUniform4iv(GLint location, GLsizei count, const GLint *v);
#undef glUniform1uiv
extern "C" void WINAPI glUniform1uiv(GLint location, GLsizei count, const GLuint *v);
#undef glUniform2uiv
extern "C" void WINAPI glUniform2uiv(GLint location, GLsizei count, const GLuint *v);
#undef glUniform3uiv
extern "C" void WINAPI glUniform3uiv(GLint location, GLsizei count, const GLuint *v);
#undef glUniform4uiv
extern "C" void WINAPI glUniform4uiv(GLint location, GLsizei count, const GLuint *v);
#undef glUseProgram
extern "C" void WINAPI glUseProgram(GLuint program);
#undef glViewport
extern "C" void WINAPI glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
#undef glViewportArrayv
extern "C" void WINAPI glViewportArrayv(GLuint first, GLsizei count, const GLfloat *v);
#undef glViewportIndexedf
extern "C" void WINAPI glViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h);
#undef glViewportIndexedfv
extern "C" void WINAPI glViewportIndexedfv(GLuint index, const GLfloat *v);
