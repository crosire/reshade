/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include <GL/gl3w.h>

// OpenGL tokens for compatibility profile
#define GL_ALPHA_TEST 0x0BC0

#ifndef NDEBUG

#include <cstdio>

#ifndef GLCHECK
#define GLCHECK(call) \
	{ \
		glGetError(); \
		call; \
		GLenum __e = glGetError(); \
		if (__e != GL_NO_ERROR) { \
			char __m[1024]; \
			sprintf_s(__m, "OpenGL error %x in %s at line %d: %s", __e, __FILE__, __LINE__, #call); \
			MessageBoxA(nullptr, __m, 0, MB_ICONERROR); \
		} \
	}
#endif

#undef glActiveShaderProgram
#define glActiveShaderProgram(...)                         GLCHECK(gl3wProcs.gl.ActiveShaderProgram(__VA_ARGS__))
#undef glActiveTexture
#define glActiveTexture(...)                               GLCHECK(gl3wProcs.gl.ActiveTexture(__VA_ARGS__))
#undef glAttachShader
#define glAttachShader(...)                                GLCHECK(gl3wProcs.gl.AttachShader(__VA_ARGS__))
#undef glBeginConditionalRender
#define glBeginConditionalRender(...)                      GLCHECK(gl3wProcs.gl.BeginConditionalRender(__VA_ARGS__))
#undef glBeginQuery
#define glBeginQuery(...)                                  GLCHECK(gl3wProcs.gl.BeginQuery(__VA_ARGS__))
#undef glBeginQueryIndexed
#define glBeginQueryIndexed(...)                           GLCHECK(gl3wProcs.gl.BeginQueryIndexed(__VA_ARGS__))
#undef glBeginTransformFeedback
#define glBeginTransformFeedback(...)                      GLCHECK(gl3wProcs.gl.BeginTransformFeedback(__VA_ARGS__))
#undef glBindAttribLocation
#define glBindAttribLocation(...)                          GLCHECK(gl3wProcs.gl.BindAttribLocation(__VA_ARGS__))
#undef glBindBuffer
#define glBindBuffer(...)                                  GLCHECK(gl3wProcs.gl.BindBuffer(__VA_ARGS__))
#undef glBindBufferBase
#define glBindBufferBase(...)                              GLCHECK(gl3wProcs.gl.BindBufferBase(__VA_ARGS__))
#undef glBindBufferRange
#define glBindBufferRange(...)                             GLCHECK(gl3wProcs.gl.BindBufferRange(__VA_ARGS__))
#undef glBindBuffersBase
#define glBindBuffersBase(...)                             GLCHECK(gl3wProcs.gl.BindBuffersBase(__VA_ARGS__))
#undef glBindBuffersRange
#define glBindBuffersRange(...)                            GLCHECK(gl3wProcs.gl.BindBuffersRange(__VA_ARGS__))
#undef glBindFragDataLocation
#define glBindFragDataLocation(...)                        GLCHECK(gl3wProcs.gl.BindFragDataLocation(__VA_ARGS__))
#undef glBindFragDataLocationIndexed
#define glBindFragDataLocationIndexed(...)                 GLCHECK(gl3wProcs.gl.BindFragDataLocationIndexed(__VA_ARGS__))
#undef glBindFramebuffer
#define glBindFramebuffer(...)                             GLCHECK(gl3wProcs.gl.BindFramebuffer(__VA_ARGS__))
#undef glBindImageTexture
#define glBindImageTexture(...)                            GLCHECK(gl3wProcs.gl.BindImageTexture(__VA_ARGS__))
#undef glBindImageTextures
#define glBindImageTextures(...)                           GLCHECK(gl3wProcs.gl.BindImageTextures(__VA_ARGS__))
#undef glBindProgramPipeline
#define glBindProgramPipeline(...)                         GLCHECK(gl3wProcs.gl.BindProgramPipeline(__VA_ARGS__))
#undef glBindRenderbuffer
#define glBindRenderbuffer(...)                            GLCHECK(gl3wProcs.gl.BindRenderbuffer(__VA_ARGS__))
#undef glBindSampler
#define glBindSampler(...)                                 GLCHECK(gl3wProcs.gl.BindSampler(__VA_ARGS__))
#undef glBindSamplers
#define glBindSamplers(...)                                GLCHECK(gl3wProcs.gl.BindSamplers(__VA_ARGS__))
#undef glBindTexture
#define glBindTexture(...)                                 GLCHECK(gl3wProcs.gl.BindTexture(__VA_ARGS__))
#undef glBindTextureUnit
#define glBindTextureUnit(...)                             GLCHECK(gl3wProcs.gl.BindTextureUnit(__VA_ARGS__))
#undef glBindTextures
#define glBindTextures(...)                                GLCHECK(gl3wProcs.gl.BindTextures(__VA_ARGS__))
#undef glBindTransformFeedback
#define glBindTransformFeedback(...)                       GLCHECK(gl3wProcs.gl.BindTransformFeedback(__VA_ARGS__))
#undef glBindVertexArray
#define glBindVertexArray(...)                             GLCHECK(gl3wProcs.gl.BindVertexArray(__VA_ARGS__))
#undef glBindVertexBuffer
#define glBindVertexBuffer(...)                            GLCHECK(gl3wProcs.gl.BindVertexBuffer(__VA_ARGS__))
#undef glBindVertexBuffers
#define glBindVertexBuffers(...)                           GLCHECK(gl3wProcs.gl.BindVertexBuffers(__VA_ARGS__))
#undef glBlendColor
#define glBlendColor(...)                                  GLCHECK(gl3wProcs.gl.BlendColor(__VA_ARGS__))
#undef glBlendEquation
#define glBlendEquation(...)                               GLCHECK(gl3wProcs.gl.BlendEquation(__VA_ARGS__))
#undef glBlendEquationSeparate
#define glBlendEquationSeparate(...)                       GLCHECK(gl3wProcs.gl.BlendEquationSeparate(__VA_ARGS__))
#undef glBlendEquationSeparatei
#define glBlendEquationSeparatei(...)                      GLCHECK(gl3wProcs.gl.BlendEquationSeparatei(__VA_ARGS__))
#undef glBlendEquationSeparateiARB
#define glBlendEquationSeparateiARB(...)                   GLCHECK(gl3wProcs.gl.BlendEquationSeparateiARB(__VA_ARGS__))
#undef glBlendEquationi
#define glBlendEquationi(...)                              GLCHECK(gl3wProcs.gl.BlendEquationi(__VA_ARGS__))
#undef glBlendEquationiARB
#define glBlendEquationiARB(...)                           GLCHECK(gl3wProcs.gl.BlendEquationiARB(__VA_ARGS__))
#undef glBlendFunc
#define glBlendFunc(...)                                   GLCHECK(gl3wProcs.gl.BlendFunc(__VA_ARGS__))
#undef glBlendFuncSeparate
#define glBlendFuncSeparate(...)                           GLCHECK(gl3wProcs.gl.BlendFuncSeparate(__VA_ARGS__))
#undef glBlendFuncSeparatei
#define glBlendFuncSeparatei(...)                          GLCHECK(gl3wProcs.gl.BlendFuncSeparatei(__VA_ARGS__))
#undef glBlendFuncSeparateiARB
#define glBlendFuncSeparateiARB(...)                       GLCHECK(gl3wProcs.gl.BlendFuncSeparateiARB(__VA_ARGS__))
#undef glBlendFunci
#define glBlendFunci(...)                                  GLCHECK(gl3wProcs.gl.BlendFunci(__VA_ARGS__))
#undef glBlendFunciARB
#define glBlendFunciARB(...)                               GLCHECK(gl3wProcs.gl.BlendFunciARB(__VA_ARGS__))
#undef glBlitFramebuffer
#define glBlitFramebuffer(...)                             GLCHECK(gl3wProcs.gl.BlitFramebuffer(__VA_ARGS__))
#undef glBlitNamedFramebuffer
#define glBlitNamedFramebuffer(...)                        GLCHECK(gl3wProcs.gl.BlitNamedFramebuffer(__VA_ARGS__))
#undef glBufferData
#define glBufferData(...)                                  GLCHECK(gl3wProcs.gl.BufferData(__VA_ARGS__))
#undef glBufferPageCommitmentARB
#define glBufferPageCommitmentARB(...)                     GLCHECK(gl3wProcs.gl.BufferPageCommitmentARB(__VA_ARGS__))
#undef glBufferStorage
#define glBufferStorage(...)                               GLCHECK(gl3wProcs.gl.BufferStorage(__VA_ARGS__))
#undef glBufferSubData
#define glBufferSubData(...)                               GLCHECK(gl3wProcs.gl.BufferSubData(__VA_ARGS__))
#undef glClampColor
#define glClampColor(...)                                  GLCHECK(gl3wProcs.gl.ClampColor(__VA_ARGS__))
#undef glClear
#define glClear(...)                                       GLCHECK(gl3wProcs.gl.Clear(__VA_ARGS__))
#undef glClearBufferData
#define glClearBufferData(...)                             GLCHECK(gl3wProcs.gl.ClearBufferData(__VA_ARGS__))
#undef glClearBufferSubData
#define glClearBufferSubData(...)                          GLCHECK(gl3wProcs.gl.ClearBufferSubData(__VA_ARGS__))
#undef glClearBufferfi
#define glClearBufferfi(...)                               GLCHECK(gl3wProcs.gl.ClearBufferfi(__VA_ARGS__))
#undef glClearBufferfv
#define glClearBufferfv(...)                               GLCHECK(gl3wProcs.gl.ClearBufferfv(__VA_ARGS__))
#undef glClearBufferiv
#define glClearBufferiv(...)                               GLCHECK(gl3wProcs.gl.ClearBufferiv(__VA_ARGS__))
#undef glClearBufferuiv
#define glClearBufferuiv(...)                              GLCHECK(gl3wProcs.gl.ClearBufferuiv(__VA_ARGS__))
#undef glClearColor
#define glClearColor(...)                                  GLCHECK(gl3wProcs.gl.ClearColor(__VA_ARGS__))
#undef glClearDepth
#define glClearDepth(...)                                  GLCHECK(gl3wProcs.gl.ClearDepth(__VA_ARGS__))
#undef glClearDepthf
#define glClearDepthf(...)                                 GLCHECK(gl3wProcs.gl.ClearDepthf(__VA_ARGS__))
#undef glClearNamedBufferData
#define glClearNamedBufferData(...)                        GLCHECK(gl3wProcs.gl.ClearNamedBufferData(__VA_ARGS__))
#undef glClearNamedBufferSubData
#define glClearNamedBufferSubData(...)                     GLCHECK(gl3wProcs.gl.ClearNamedBufferSubData(__VA_ARGS__))
#undef glClearNamedFramebufferfi
#define glClearNamedFramebufferfi(...)                     GLCHECK(gl3wProcs.gl.ClearNamedFramebufferfi(__VA_ARGS__))
#undef glClearNamedFramebufferfv
#define glClearNamedFramebufferfv(...)                     GLCHECK(gl3wProcs.gl.ClearNamedFramebufferfv(__VA_ARGS__))
#undef glClearNamedFramebufferiv
#define glClearNamedFramebufferiv(...)                     GLCHECK(gl3wProcs.gl.ClearNamedFramebufferiv(__VA_ARGS__))
#undef glClearNamedFramebufferuiv
#define glClearNamedFramebufferuiv(...)                    GLCHECK(gl3wProcs.gl.ClearNamedFramebufferuiv(__VA_ARGS__))
#undef glClearStencil
#define glClearStencil(...)                                GLCHECK(gl3wProcs.gl.ClearStencil(__VA_ARGS__))
#undef glClearTexImage
#define glClearTexImage(...)                               GLCHECK(gl3wProcs.gl.ClearTexImage(__VA_ARGS__))
#undef glClearTexSubImage
#define glClearTexSubImage(...)                            GLCHECK(gl3wProcs.gl.ClearTexSubImage(__VA_ARGS__))
#undef glClientWaitSync
#define glClientWaitSync(...)                              GLCHECK(gl3wProcs.gl.ClientWaitSync(__VA_ARGS__))
#undef glClipControl
#define glClipControl(...)                                 GLCHECK(gl3wProcs.gl.ClipControl(__VA_ARGS__))
#undef glColorMask
#define glColorMask(...)                                   GLCHECK(gl3wProcs.gl.ColorMask(__VA_ARGS__))
#undef glColorMaski
#define glColorMaski(...)                                  GLCHECK(gl3wProcs.gl.ColorMaski(__VA_ARGS__))
#undef glCompileShader
#define glCompileShader(...)                               GLCHECK(gl3wProcs.gl.CompileShader(__VA_ARGS__))
#undef glCompileShaderIncludeARB
#define glCompileShaderIncludeARB(...)                     GLCHECK(gl3wProcs.gl.CompileShaderIncludeARB(__VA_ARGS__))
#undef glCompressedTexImage1D
#define glCompressedTexImage1D(...)                        GLCHECK(gl3wProcs.gl.CompressedTexImage1D(__VA_ARGS__))
#undef glCompressedTexImage2D
#define glCompressedTexImage2D(...)                        GLCHECK(gl3wProcs.gl.CompressedTexImage2D(__VA_ARGS__))
#undef glCompressedTexImage3D
#define glCompressedTexImage3D(...)                        GLCHECK(gl3wProcs.gl.CompressedTexImage3D(__VA_ARGS__))
#undef glCompressedTexSubImage1D
#define glCompressedTexSubImage1D(...)                     GLCHECK(gl3wProcs.gl.CompressedTexSubImage1D(__VA_ARGS__))
#undef glCompressedTexSubImage2D
#define glCompressedTexSubImage2D(...)                     GLCHECK(gl3wProcs.gl.CompressedTexSubImage2D(__VA_ARGS__))
#undef glCompressedTexSubImage3D
#define glCompressedTexSubImage3D(...)                     GLCHECK(gl3wProcs.gl.CompressedTexSubImage3D(__VA_ARGS__))
#undef glCompressedTextureSubImage1D
#define glCompressedTextureSubImage1D(...)                 GLCHECK(gl3wProcs.gl.CompressedTextureSubImage1D(__VA_ARGS__))
#undef glCompressedTextureSubImage2D
#define glCompressedTextureSubImage2D(...)                 GLCHECK(gl3wProcs.gl.CompressedTextureSubImage2D(__VA_ARGS__))
#undef glCompressedTextureSubImage3D
#define glCompressedTextureSubImage3D(...)                 GLCHECK(gl3wProcs.gl.CompressedTextureSubImage3D(__VA_ARGS__))
#undef glCopyBufferSubData
#define glCopyBufferSubData(...)                           GLCHECK(gl3wProcs.gl.CopyBufferSubData(__VA_ARGS__))
#undef glCopyImageSubData
#define glCopyImageSubData(...)                            GLCHECK(gl3wProcs.gl.CopyImageSubData(__VA_ARGS__))
#undef glCopyNamedBufferSubData
#define glCopyNamedBufferSubData(...)                      GLCHECK(gl3wProcs.gl.CopyNamedBufferSubData(__VA_ARGS__))
#undef glCopyTexImage1D
#define glCopyTexImage1D(...)                              GLCHECK(gl3wProcs.gl.CopyTexImage1D(__VA_ARGS__))
#undef glCopyTexImage2D
#define glCopyTexImage2D(...)                              GLCHECK(gl3wProcs.gl.CopyTexImage2D(__VA_ARGS__))
#undef glCopyTexSubImage1D
#define glCopyTexSubImage1D(...)                           GLCHECK(gl3wProcs.gl.CopyTexSubImage1D(__VA_ARGS__))
#undef glCopyTexSubImage2D
#define glCopyTexSubImage2D(...)                           GLCHECK(gl3wProcs.gl.CopyTexSubImage2D(__VA_ARGS__))
#undef glCopyTexSubImage3D
#define glCopyTexSubImage3D(...)                           GLCHECK(gl3wProcs.gl.CopyTexSubImage3D(__VA_ARGS__))
#undef glCopyTextureSubImage1D
#define glCopyTextureSubImage1D(...)                       GLCHECK(gl3wProcs.gl.CopyTextureSubImage1D(__VA_ARGS__))
#undef glCopyTextureSubImage2D
#define glCopyTextureSubImage2D(...)                       GLCHECK(gl3wProcs.gl.CopyTextureSubImage2D(__VA_ARGS__))
#undef glCopyTextureSubImage3D
#define glCopyTextureSubImage3D(...)                       GLCHECK(gl3wProcs.gl.CopyTextureSubImage3D(__VA_ARGS__))
#undef glCreateBuffers
#define glCreateBuffers(...)                               GLCHECK(gl3wProcs.gl.CreateBuffers(__VA_ARGS__))
#undef glCreateFramebuffers
#define glCreateFramebuffers(...)                          GLCHECK(gl3wProcs.gl.CreateFramebuffers(__VA_ARGS__))
#undef glCreateProgramPipelines
#define glCreateProgramPipelines(...)                      GLCHECK(gl3wProcs.gl.CreateProgramPipelines(__VA_ARGS__))
#undef glCreateQueries
#define glCreateQueries(...)                               GLCHECK(gl3wProcs.gl.CreateQueries(__VA_ARGS__))
#undef glCreateRenderbuffers
#define glCreateRenderbuffers(...)                         GLCHECK(gl3wProcs.gl.CreateRenderbuffers(__VA_ARGS__))
#undef glCreateSamplers
#define glCreateSamplers(...)                              GLCHECK(gl3wProcs.gl.CreateSamplers(__VA_ARGS__))
#undef glCreateShaderProgramv
#define glCreateShaderProgramv(...)                        GLCHECK(gl3wProcs.gl.CreateShaderProgramv(__VA_ARGS__))
#undef glCreateSyncFromCLeventARB
#define glCreateSyncFromCLeventARB(...)                    GLCHECK(gl3wProcs.gl.CreateSyncFromCLeventARB(__VA_ARGS__))
#undef glCreateTextures
#define glCreateTextures(...)                              GLCHECK(gl3wProcs.gl.CreateTextures(__VA_ARGS__))
#undef glCreateTransformFeedbacks
#define glCreateTransformFeedbacks(...)                    GLCHECK(gl3wProcs.gl.CreateTransformFeedbacks(__VA_ARGS__))
#undef glCreateVertexArrays
#define glCreateVertexArrays(...)                          GLCHECK(gl3wProcs.gl.CreateVertexArrays(__VA_ARGS__))
#undef glCullFace
#define glCullFace(...)                                    GLCHECK(gl3wProcs.gl.CullFace(__VA_ARGS__))
#undef glDebugMessageCallback
#define glDebugMessageCallback(...)                        GLCHECK(gl3wProcs.gl.DebugMessageCallback(__VA_ARGS__))
#undef glDebugMessageCallbackARB
#define glDebugMessageCallbackARB(...)                     GLCHECK(gl3wProcs.gl.DebugMessageCallbackARB(__VA_ARGS__))
#undef glDebugMessageControl
#define glDebugMessageControl(...)                         GLCHECK(gl3wProcs.gl.DebugMessageControl(__VA_ARGS__))
#undef glDebugMessageControlARB
#define glDebugMessageControlARB(...)                      GLCHECK(gl3wProcs.gl.DebugMessageControlARB(__VA_ARGS__))
#undef glDebugMessageInsert
#define glDebugMessageInsert(...)                          GLCHECK(gl3wProcs.gl.DebugMessageInsert(__VA_ARGS__))
#undef glDebugMessageInsertARB
#define glDebugMessageInsertARB(...)                       GLCHECK(gl3wProcs.gl.DebugMessageInsertARB(__VA_ARGS__))
#undef glDeleteBuffers
#define glDeleteBuffers(...)                               GLCHECK(gl3wProcs.gl.DeleteBuffers(__VA_ARGS__))
#undef glDeleteFramebuffers
#define glDeleteFramebuffers(...)                          GLCHECK(gl3wProcs.gl.DeleteFramebuffers(__VA_ARGS__))
#undef glDeleteNamedStringARB
#define glDeleteNamedStringARB(...)                        GLCHECK(gl3wProcs.gl.DeleteNamedStringARB(__VA_ARGS__))
#undef glDeleteProgram
#define glDeleteProgram(...)                               GLCHECK(gl3wProcs.gl.DeleteProgram(__VA_ARGS__))
#undef glDeleteProgramPipelines
#define glDeleteProgramPipelines(...)                      GLCHECK(gl3wProcs.gl.DeleteProgramPipelines(__VA_ARGS__))
#undef glDeleteQueries
#define glDeleteQueries(...)                               GLCHECK(gl3wProcs.gl.DeleteQueries(__VA_ARGS__))
#undef glDeleteRenderbuffers
#define glDeleteRenderbuffers(...)                         GLCHECK(gl3wProcs.gl.DeleteRenderbuffers(__VA_ARGS__))
#undef glDeleteSamplers
#define glDeleteSamplers(...)                              GLCHECK(gl3wProcs.gl.DeleteSamplers(__VA_ARGS__))
#undef glDeleteShader
#define glDeleteShader(...)                                GLCHECK(gl3wProcs.gl.DeleteShader(__VA_ARGS__))
#undef glDeleteSync
#define glDeleteSync(...)                                  GLCHECK(gl3wProcs.gl.DeleteSync(__VA_ARGS__))
#undef glDeleteTextures
#define glDeleteTextures(...)                              GLCHECK(gl3wProcs.gl.DeleteTextures(__VA_ARGS__))
#undef glDeleteTransformFeedbacks
#define glDeleteTransformFeedbacks(...)                    GLCHECK(gl3wProcs.gl.DeleteTransformFeedbacks(__VA_ARGS__))
#undef glDeleteVertexArrays
#define glDeleteVertexArrays(...)                          GLCHECK(gl3wProcs.gl.DeleteVertexArrays(__VA_ARGS__))
#undef glDepthFunc
#define glDepthFunc(...)                                   GLCHECK(gl3wProcs.gl.DepthFunc(__VA_ARGS__))
#undef glDepthMask
#define glDepthMask(...)                                   GLCHECK(gl3wProcs.gl.DepthMask(__VA_ARGS__))
#undef glDepthRange
#define glDepthRange(...)                                  GLCHECK(gl3wProcs.gl.DepthRange(__VA_ARGS__))
#undef glDepthRangeArrayv
#define glDepthRangeArrayv(...)                            GLCHECK(gl3wProcs.gl.DepthRangeArrayv(__VA_ARGS__))
#undef glDepthRangeIndexed
#define glDepthRangeIndexed(...)                           GLCHECK(gl3wProcs.gl.DepthRangeIndexed(__VA_ARGS__))
#undef glDepthRangef
#define glDepthRangef(...)                                 GLCHECK(gl3wProcs.gl.DepthRangef(__VA_ARGS__))
#undef glDetachShader
#define glDetachShader(...)                                GLCHECK(gl3wProcs.gl.DetachShader(__VA_ARGS__))
#undef glDisable
#define glDisable(...)                                     GLCHECK(gl3wProcs.gl.Disable(__VA_ARGS__))
#undef glDisableVertexArrayAttrib
#define glDisableVertexArrayAttrib(...)                    GLCHECK(gl3wProcs.gl.DisableVertexArrayAttrib(__VA_ARGS__))
#undef glDisableVertexAttribArray
#define glDisableVertexAttribArray(...)                    GLCHECK(gl3wProcs.gl.DisableVertexAttribArray(__VA_ARGS__))
#undef glDisablei
#define glDisablei(...)                                    GLCHECK(gl3wProcs.gl.Disablei(__VA_ARGS__))
#undef glDispatchCompute
#define glDispatchCompute(...)                             GLCHECK(gl3wProcs.gl.DispatchCompute(__VA_ARGS__))
#undef glDispatchComputeGroupSizeARB
#define glDispatchComputeGroupSizeARB(...)                 GLCHECK(gl3wProcs.gl.DispatchComputeGroupSizeARB(__VA_ARGS__))
#undef glDispatchComputeIndirect
#define glDispatchComputeIndirect(...)                     GLCHECK(gl3wProcs.gl.DispatchComputeIndirect(__VA_ARGS__))
#undef glDrawArrays
#define glDrawArrays(...)                                  GLCHECK(gl3wProcs.gl.DrawArrays(__VA_ARGS__))
#undef glDrawArraysIndirect
#define glDrawArraysIndirect(...)                          GLCHECK(gl3wProcs.gl.DrawArraysIndirect(__VA_ARGS__))
#undef glDrawArraysInstanced
#define glDrawArraysInstanced(...)                         GLCHECK(gl3wProcs.gl.DrawArraysInstanced(__VA_ARGS__))
#undef glDrawArraysInstancedBaseInstance
#define glDrawArraysInstancedBaseInstance(...)             GLCHECK(gl3wProcs.gl.DrawArraysInstancedBaseInstance(__VA_ARGS__))
#undef glDrawBuffer
#define glDrawBuffer(...)                                  GLCHECK(gl3wProcs.gl.DrawBuffer(__VA_ARGS__))
#undef glDrawBuffers
#define glDrawBuffers(...)                                 GLCHECK(gl3wProcs.gl.DrawBuffers(__VA_ARGS__))
#undef glDrawElements
#define glDrawElements(...)                                GLCHECK(gl3wProcs.gl.DrawElements(__VA_ARGS__))
#undef glDrawElementsBaseVertex
#define glDrawElementsBaseVertex(...)                      GLCHECK(gl3wProcs.gl.DrawElementsBaseVertex(__VA_ARGS__))
#undef glDrawElementsIndirect
#define glDrawElementsIndirect(...)                        GLCHECK(gl3wProcs.gl.DrawElementsIndirect(__VA_ARGS__))
#undef glDrawElementsInstanced
#define glDrawElementsInstanced(...)                       GLCHECK(gl3wProcs.gl.DrawElementsInstanced(__VA_ARGS__))
#undef glDrawElementsInstancedBaseInstance
#define glDrawElementsInstancedBaseInstance(...)           GLCHECK(gl3wProcs.gl.DrawElementsInstancedBaseInstance(__VA_ARGS__))
#undef glDrawElementsInstancedBaseVertex
#define glDrawElementsInstancedBaseVertex(...)             GLCHECK(gl3wProcs.gl.DrawElementsInstancedBaseVertex(__VA_ARGS__))
#undef glDrawElementsInstancedBaseVertexBaseInstance
#define glDrawElementsInstancedBaseVertexBaseInstance(...) GLCHECK(gl3wProcs.gl.DrawElementsInstancedBaseVertexBaseInstance(__VA_ARGS__))
#undef glDrawRangeElements
#define glDrawRangeElements(...)                           GLCHECK(gl3wProcs.gl.DrawRangeElements(__VA_ARGS__))
#undef glDrawRangeElementsBaseVertex
#define glDrawRangeElementsBaseVertex(...)                 GLCHECK(gl3wProcs.gl.DrawRangeElementsBaseVertex(__VA_ARGS__))
#undef glDrawTransformFeedback
#define glDrawTransformFeedback(...)                       GLCHECK(gl3wProcs.gl.DrawTransformFeedback(__VA_ARGS__))
#undef glDrawTransformFeedbackInstanced
#define glDrawTransformFeedbackInstanced(...)              GLCHECK(gl3wProcs.gl.DrawTransformFeedbackInstanced(__VA_ARGS__))
#undef glDrawTransformFeedbackStream
#define glDrawTransformFeedbackStream(...)                 GLCHECK(gl3wProcs.gl.DrawTransformFeedbackStream(__VA_ARGS__))
#undef glDrawTransformFeedbackStreamInstanced
#define glDrawTransformFeedbackStreamInstanced(...)        GLCHECK(gl3wProcs.gl.DrawTransformFeedbackStreamInstanced(__VA_ARGS__))
#undef glEnable
#define glEnable(...)                                      GLCHECK(gl3wProcs.gl.Enable(__VA_ARGS__))
#undef glEnableVertexArrayAttrib
#define glEnableVertexArrayAttrib(...)                     GLCHECK(gl3wProcs.gl.EnableVertexArrayAttrib(__VA_ARGS__))
#undef glEnableVertexAttribArray
#define glEnableVertexAttribArray(...)                     GLCHECK(gl3wProcs.gl.EnableVertexAttribArray(__VA_ARGS__))
#undef glEnablei
#define glEnablei(...)                                     GLCHECK(gl3wProcs.gl.Enablei(__VA_ARGS__))
#undef glEndConditionalRender
#define glEndConditionalRender(...)                        GLCHECK(gl3wProcs.gl.EndConditionalRender(__VA_ARGS__))
#undef glEndQuery
#define glEndQuery(...)                                    GLCHECK(gl3wProcs.gl.EndQuery(__VA_ARGS__))
#undef glEndQueryIndexed
#define glEndQueryIndexed(...)                             GLCHECK(gl3wProcs.gl.EndQueryIndexed(__VA_ARGS__))
#undef glEndTransformFeedback
#define glEndTransformFeedback(...)                        GLCHECK(gl3wProcs.gl.EndTransformFeedback(__VA_ARGS__))
#undef glFenceSync
#define glFenceSync(...)                                   GLCHECK(gl3wProcs.gl.FenceSync(__VA_ARGS__))
#undef glFinish
#define glFinish(...)                                      GLCHECK(gl3wProcs.gl.Finish(__VA_ARGS__))
#undef glFlush
#define glFlush(...)                                       GLCHECK(gl3wProcs.gl.Flush(__VA_ARGS__))
#undef glFlushMappedBufferRange
#define glFlushMappedBufferRange(...)                      GLCHECK(gl3wProcs.gl.FlushMappedBufferRange(__VA_ARGS__))
#undef glFlushMappedNamedBufferRange
#define glFlushMappedNamedBufferRange(...)                 GLCHECK(gl3wProcs.gl.FlushMappedNamedBufferRange(__VA_ARGS__))
#undef glFramebufferParameteri
#define glFramebufferParameteri(...)                       GLCHECK(gl3wProcs.gl.FramebufferParameteri(__VA_ARGS__))
#undef glFramebufferRenderbuffer
#define glFramebufferRenderbuffer(...)                     GLCHECK(gl3wProcs.gl.FramebufferRenderbuffer(__VA_ARGS__))
#undef glFramebufferTexture
#define glFramebufferTexture(...)                          GLCHECK(gl3wProcs.gl.FramebufferTexture(__VA_ARGS__))
#undef glFramebufferTexture1D
#define glFramebufferTexture1D(...)                        GLCHECK(gl3wProcs.gl.FramebufferTexture1D(__VA_ARGS__))
#undef glFramebufferTexture2D
#define glFramebufferTexture2D(...)                        GLCHECK(gl3wProcs.gl.FramebufferTexture2D(__VA_ARGS__))
#undef glFramebufferTexture3D
#define glFramebufferTexture3D(...)                        GLCHECK(gl3wProcs.gl.FramebufferTexture3D(__VA_ARGS__))
#undef glFramebufferTextureLayer
#define glFramebufferTextureLayer(...)                     GLCHECK(gl3wProcs.gl.FramebufferTextureLayer(__VA_ARGS__))
#undef glFrontFace
#define glFrontFace(...)                                   GLCHECK(gl3wProcs.gl.FrontFace(__VA_ARGS__))
#undef glGenBuffers
#define glGenBuffers(...)                                  GLCHECK(gl3wProcs.gl.GenBuffers(__VA_ARGS__))
#undef glGenFramebuffers
#define glGenFramebuffers(...)                             GLCHECK(gl3wProcs.gl.GenFramebuffers(__VA_ARGS__))
#undef glGenProgramPipelines
#define glGenProgramPipelines(...)                         GLCHECK(gl3wProcs.gl.GenProgramPipelines(__VA_ARGS__))
#undef glGenQueries
#define glGenQueries(...)                                  GLCHECK(gl3wProcs.gl.GenQueries(__VA_ARGS__))
#undef glGenRenderbuffers
#define glGenRenderbuffers(...)                            GLCHECK(gl3wProcs.gl.GenRenderbuffers(__VA_ARGS__))
#undef glGenSamplers
#define glGenSamplers(...)                                 GLCHECK(gl3wProcs.gl.GenSamplers(__VA_ARGS__))
#undef glGenTextures
#define glGenTextures(...)                                 GLCHECK(gl3wProcs.gl.GenTextures(__VA_ARGS__))
#undef glGenTransformFeedbacks
#define glGenTransformFeedbacks(...)                       GLCHECK(gl3wProcs.gl.GenTransformFeedbacks(__VA_ARGS__))
#undef glGenVertexArrays
#define glGenVertexArrays(...)                             GLCHECK(gl3wProcs.gl.GenVertexArrays(__VA_ARGS__))
#undef glGenerateMipmap
#define glGenerateMipmap(...)                              GLCHECK(gl3wProcs.gl.GenerateMipmap(__VA_ARGS__))
#undef glGenerateTextureMipmap
#define glGenerateTextureMipmap(...)                       GLCHECK(gl3wProcs.gl.GenerateTextureMipmap(__VA_ARGS__))
#undef glGetActiveAtomicCounterBufferiv
#define glGetActiveAtomicCounterBufferiv(...)              GLCHECK(gl3wProcs.gl.GetActiveAtomicCounterBufferiv(__VA_ARGS__))
#undef glGetActiveAttrib
#define glGetActiveAttrib(...)                             GLCHECK(gl3wProcs.gl.GetActiveAttrib(__VA_ARGS__))
#undef glGetActiveSubroutineName
#define glGetActiveSubroutineName(...)                     GLCHECK(gl3wProcs.gl.GetActiveSubroutineName(__VA_ARGS__))
#undef glGetActiveSubroutineUniformName
#define glGetActiveSubroutineUniformName(...)              GLCHECK(gl3wProcs.gl.GetActiveSubroutineUniformName(__VA_ARGS__))
#undef glGetActiveSubroutineUniformiv
#define glGetActiveSubroutineUniformiv(...)                GLCHECK(gl3wProcs.gl.GetActiveSubroutineUniformiv(__VA_ARGS__))
#undef glGetActiveUniform
#define glGetActiveUniform(...)                            GLCHECK(gl3wProcs.gl.GetActiveUniform(__VA_ARGS__))
#undef glGetActiveUniformBlockName
#define glGetActiveUniformBlockName(...)                   GLCHECK(gl3wProcs.gl.GetActiveUniformBlockName(__VA_ARGS__))
#undef glGetActiveUniformBlockiv
#define glGetActiveUniformBlockiv(...)                     GLCHECK(gl3wProcs.gl.GetActiveUniformBlockiv(__VA_ARGS__))
#undef glGetActiveUniformName
#define glGetActiveUniformName(...)                        GLCHECK(gl3wProcs.gl.GetActiveUniformName(__VA_ARGS__))
#undef glGetActiveUniformsiv
#define glGetActiveUniformsiv(...)                         GLCHECK(gl3wProcs.gl.GetActiveUniformsiv(__VA_ARGS__))
#undef glGetAttachedShaders
#define glGetAttachedShaders(...)                          GLCHECK(gl3wProcs.gl.GetAttachedShaders(__VA_ARGS__))
#undef glGetBooleani_v
#define glGetBooleani_v(...)                               GLCHECK(gl3wProcs.gl.GetBooleani_v(__VA_ARGS__))
#undef glGetBooleanv
#define glGetBooleanv(...)                                 GLCHECK(gl3wProcs.gl.GetBooleanv(__VA_ARGS__))
#undef glGetBufferParameteri64v
#define glGetBufferParameteri64v(...)                      GLCHECK(gl3wProcs.gl.GetBufferParameteri64v(__VA_ARGS__))
#undef glGetBufferParameteriv
#define glGetBufferParameteriv(...)                        GLCHECK(gl3wProcs.gl.GetBufferParameteriv(__VA_ARGS__))
#undef glGetBufferPointerv
#define glGetBufferPointerv(...)                           GLCHECK(gl3wProcs.gl.GetBufferPointerv(__VA_ARGS__))
#undef glGetBufferSubData
#define glGetBufferSubData(...)                            GLCHECK(gl3wProcs.gl.GetBufferSubData(__VA_ARGS__))
#undef glGetCompressedTexImage
#define glGetCompressedTexImage(...)                       GLCHECK(gl3wProcs.gl.GetCompressedTexImage(__VA_ARGS__))
#undef glGetCompressedTextureImage
#define glGetCompressedTextureImage(...)                   GLCHECK(gl3wProcs.gl.GetCompressedTextureImage(__VA_ARGS__))
#undef glGetCompressedTextureSubImage
#define glGetCompressedTextureSubImage(...)                GLCHECK(gl3wProcs.gl.GetCompressedTextureSubImage(__VA_ARGS__))
#undef glGetDebugMessageLog
#define glGetDebugMessageLog(...)                          GLCHECK(gl3wProcs.gl.GetDebugMessageLog(__VA_ARGS__))
#undef glGetDebugMessageLogARB
#define glGetDebugMessageLogARB(...)                       GLCHECK(gl3wProcs.gl.GetDebugMessageLogARB(__VA_ARGS__))
#undef glGetDoublei_v
#define glGetDoublei_v(...)                                GLCHECK(gl3wProcs.gl.GetDoublei_v(__VA_ARGS__))
#undef glGetDoublev
#define glGetDoublev(...)                                  GLCHECK(gl3wProcs.gl.GetDoublev(__VA_ARGS__))
#undef glGetFloati_v
#define glGetFloati_v(...)                                 GLCHECK(gl3wProcs.gl.GetFloati_v(__VA_ARGS__))
#undef glGetFloatv
#define glGetFloatv(...)                                   GLCHECK(gl3wProcs.gl.GetFloatv(__VA_ARGS__))
#undef glGetFragDataIndex
#define glGetFragDataIndex(...)                            GLCHECK(gl3wProcs.gl.GetFragDataIndex(__VA_ARGS__))
#undef glGetFragDataLocation
#define glGetFragDataLocation(...)                         GLCHECK(gl3wProcs.gl.GetFragDataLocation(__VA_ARGS__))
#undef glGetFramebufferAttachmentParameteriv
#define glGetFramebufferAttachmentParameteriv(...)         GLCHECK(gl3wProcs.gl.GetFramebufferAttachmentParameteriv(__VA_ARGS__))
#undef glGetFramebufferParameteriv
#define glGetFramebufferParameteriv(...)                   GLCHECK(gl3wProcs.gl.GetFramebufferParameteriv(__VA_ARGS__))
#undef glGetGraphicsResetStatus
#define glGetGraphicsResetStatus(...)                      GLCHECK(gl3wProcs.gl.GetGraphicsResetStatus(__VA_ARGS__))
#undef glGetGraphicsResetStatusARB
#define glGetGraphicsResetStatusARB(...)                   GLCHECK(gl3wProcs.gl.GetGraphicsResetStatusARB(__VA_ARGS__))
#undef glGetImageHandleARB
#define glGetImageHandleARB(...)                           GLCHECK(gl3wProcs.gl.GetImageHandleARB(__VA_ARGS__))
#undef glGetInteger64i_v
#define glGetInteger64i_v(...)                             GLCHECK(gl3wProcs.gl.GetInteger64i_v(__VA_ARGS__))
#undef glGetInteger64v
#define glGetInteger64v(...)                               GLCHECK(gl3wProcs.gl.GetInteger64v(__VA_ARGS__))
#undef glGetIntegeri_v
#define glGetIntegeri_v(...)                               GLCHECK(gl3wProcs.gl.GetIntegeri_v(__VA_ARGS__))
#undef glGetIntegerv
#define glGetIntegerv(...)                                 GLCHECK(gl3wProcs.gl.GetIntegerv(__VA_ARGS__))
#undef glGetInternalformati64v
#define glGetInternalformati64v(...)                       GLCHECK(gl3wProcs.gl.GetInternalformati64v(__VA_ARGS__))
#undef glGetInternalformativ
#define glGetInternalformativ(...)                         GLCHECK(gl3wProcs.gl.GetInternalformativ(__VA_ARGS__))
#undef glGetMultisamplefv
#define glGetMultisamplefv(...)                            GLCHECK(gl3wProcs.gl.GetMultisamplefv(__VA_ARGS__))
#undef glGetNamedBufferParameteri64v
#define glGetNamedBufferParameteri64v(...)                 GLCHECK(gl3wProcs.gl.GetNamedBufferParameteri64v(__VA_ARGS__))
#undef glGetNamedBufferParameteriv
#define glGetNamedBufferParameteriv(...)                   GLCHECK(gl3wProcs.gl.GetNamedBufferParameteriv(__VA_ARGS__))
#undef glGetNamedBufferPointerv
#define glGetNamedBufferPointerv(...)                      GLCHECK(gl3wProcs.gl.GetNamedBufferPointerv(__VA_ARGS__))
#undef glGetNamedBufferSubData
#define glGetNamedBufferSubData(...)                       GLCHECK(gl3wProcs.gl.GetNamedBufferSubData(__VA_ARGS__))
#undef glGetNamedFramebufferAttachmentParameteriv
#define glGetNamedFramebufferAttachmentParameteriv(...)    GLCHECK(gl3wProcs.gl.GetNamedFramebufferAttachmentParameteriv(__VA_ARGS__))
#undef glGetNamedFramebufferParameteriv
#define glGetNamedFramebufferParameteriv(...)              GLCHECK(gl3wProcs.gl.GetNamedFramebufferParameteriv(__VA_ARGS__))
#undef glGetNamedRenderbufferParameteriv
#define glGetNamedRenderbufferParameteriv(...)             GLCHECK(gl3wProcs.gl.GetNamedRenderbufferParameteriv(__VA_ARGS__))
#undef glGetNamedStringARB
#define glGetNamedStringARB(...)                           GLCHECK(gl3wProcs.gl.GetNamedStringARB(__VA_ARGS__))
#undef glGetNamedStringivARB
#define glGetNamedStringivARB(...)                         GLCHECK(gl3wProcs.gl.GetNamedStringivARB(__VA_ARGS__))
#undef glGetObjectLabel
#define glGetObjectLabel(...)                              GLCHECK(gl3wProcs.gl.GetObjectLabel(__VA_ARGS__))
#undef glGetObjectPtrLabel
#define glGetObjectPtrLabel(...)                           GLCHECK(gl3wProcs.gl.GetObjectPtrLabel(__VA_ARGS__))
#undef glGetPointerv
#define glGetPointerv(...)                                 GLCHECK(gl3wProcs.gl.GetPointerv(__VA_ARGS__))
#undef glGetProgramBinary
#define glGetProgramBinary(...)                            GLCHECK(gl3wProcs.gl.GetProgramBinary(__VA_ARGS__))
#undef glGetProgramInfoLog
#define glGetProgramInfoLog(...)                           GLCHECK(gl3wProcs.gl.GetProgramInfoLog(__VA_ARGS__))
#undef glGetProgramInterfaceiv
#define glGetProgramInterfaceiv(...)                       GLCHECK(gl3wProcs.gl.GetProgramInterfaceiv(__VA_ARGS__))
#undef glGetProgramPipelineInfoLog
#define glGetProgramPipelineInfoLog(...)                   GLCHECK(gl3wProcs.gl.GetProgramPipelineInfoLog(__VA_ARGS__))
#undef glGetProgramPipelineiv
#define glGetProgramPipelineiv(...)                        GLCHECK(gl3wProcs.gl.GetProgramPipelineiv(__VA_ARGS__))
#undef glGetProgramResourceIndex
#define glGetProgramResourceIndex(...)                     GLCHECK(gl3wProcs.gl.GetProgramResourceIndex(__VA_ARGS__))
#undef glGetProgramResourceLocation
#define glGetProgramResourceLocation(...)                  GLCHECK(gl3wProcs.gl.GetProgramResourceLocation(__VA_ARGS__))
#undef glGetProgramResourceLocationIndex
#define glGetProgramResourceLocationIndex(...)             GLCHECK(gl3wProcs.gl.GetProgramResourceLocationIndex(__VA_ARGS__))
#undef glGetProgramResourceName
#define glGetProgramResourceName(...)                      GLCHECK(gl3wProcs.gl.GetProgramResourceName(__VA_ARGS__))
#undef glGetProgramResourceiv
#define glGetProgramResourceiv(...)                        GLCHECK(gl3wProcs.gl.GetProgramResourceiv(__VA_ARGS__))
#undef glGetProgramStageiv
#define glGetProgramStageiv(...)                           GLCHECK(gl3wProcs.gl.GetProgramStageiv(__VA_ARGS__))
#undef glGetProgramiv
#define glGetProgramiv(...)                                GLCHECK(gl3wProcs.gl.GetProgramiv(__VA_ARGS__))
#undef glGetQueryBufferObjecti64v
#define glGetQueryBufferObjecti64v(...)                    GLCHECK(gl3wProcs.gl.GetQueryBufferObjecti64v(__VA_ARGS__))
#undef glGetQueryBufferObjectiv
#define glGetQueryBufferObjectiv(...)                      GLCHECK(gl3wProcs.gl.GetQueryBufferObjectiv(__VA_ARGS__))
#undef glGetQueryBufferObjectui64v
#define glGetQueryBufferObjectui64v(...)                   GLCHECK(gl3wProcs.gl.GetQueryBufferObjectui64v(__VA_ARGS__))
#undef glGetQueryBufferObjectuiv
#define glGetQueryBufferObjectuiv(...)                     GLCHECK(gl3wProcs.gl.GetQueryBufferObjectuiv(__VA_ARGS__))
#undef glGetQueryIndexediv
#define glGetQueryIndexediv(...)                           GLCHECK(gl3wProcs.gl.GetQueryIndexediv(__VA_ARGS__))
#undef glGetQueryObjecti64v
#define glGetQueryObjecti64v(...)                          GLCHECK(gl3wProcs.gl.GetQueryObjecti64v(__VA_ARGS__))
#undef glGetQueryObjectiv
#define glGetQueryObjectiv(...)                            GLCHECK(gl3wProcs.gl.GetQueryObjectiv(__VA_ARGS__))
#undef glGetQueryObjectui64v
#define glGetQueryObjectui64v(...)                         GLCHECK(gl3wProcs.gl.GetQueryObjectui64v(__VA_ARGS__))
#undef glGetQueryObjectuiv
#define glGetQueryObjectuiv(...)                           GLCHECK(gl3wProcs.gl.GetQueryObjectuiv(__VA_ARGS__))
#undef glGetQueryiv
#define glGetQueryiv(...)                                  GLCHECK(gl3wProcs.gl.GetQueryiv(__VA_ARGS__))
#undef glGetRenderbufferParameteriv
#define glGetRenderbufferParameteriv(...)                  GLCHECK(gl3wProcs.gl.GetRenderbufferParameteriv(__VA_ARGS__))
#undef glGetSamplerParameterIiv
#define glGetSamplerParameterIiv(...)                      GLCHECK(gl3wProcs.gl.GetSamplerParameterIiv(__VA_ARGS__))
#undef glGetSamplerParameterIuiv
#define glGetSamplerParameterIuiv(...)                     GLCHECK(gl3wProcs.gl.GetSamplerParameterIuiv(__VA_ARGS__))
#undef glGetSamplerParameterfv
#define glGetSamplerParameterfv(...)                       GLCHECK(gl3wProcs.gl.GetSamplerParameterfv(__VA_ARGS__))
#undef glGetSamplerParameteriv
#define glGetSamplerParameteriv(...)                       GLCHECK(gl3wProcs.gl.GetSamplerParameteriv(__VA_ARGS__))
#undef glGetShaderInfoLog
#define glGetShaderInfoLog(...)                            GLCHECK(gl3wProcs.gl.GetShaderInfoLog(__VA_ARGS__))
#undef glGetShaderPrecisionFormat
#define glGetShaderPrecisionFormat(...)                    GLCHECK(gl3wProcs.gl.GetShaderPrecisionFormat(__VA_ARGS__))
#undef glGetShaderSource
#define glGetShaderSource(...)                             GLCHECK(gl3wProcs.gl.GetShaderSource(__VA_ARGS__))
#undef glGetShaderiv
#define glGetShaderiv(...)                                 GLCHECK(gl3wProcs.gl.GetShaderiv(__VA_ARGS__))
#undef glGetSubroutineIndex
#define glGetSubroutineIndex(...)                          GLCHECK(gl3wProcs.gl.GetSubroutineIndex(__VA_ARGS__))
#undef glGetSubroutineUniformLocation
#define glGetSubroutineUniformLocation(...)                GLCHECK(gl3wProcs.gl.GetSubroutineUniformLocation(__VA_ARGS__))
#undef glGetSynciv
#define glGetSynciv(...)                                   GLCHECK(gl3wProcs.gl.GetSynciv(__VA_ARGS__))
#undef glGetTexImage
#define glGetTexImage(...)                                 GLCHECK(gl3wProcs.gl.GetTexImage(__VA_ARGS__))
#undef glGetTexLevelParameterfv
#define glGetTexLevelParameterfv(...)                      GLCHECK(gl3wProcs.gl.GetTexLevelParameterfv(__VA_ARGS__))
#undef glGetTexLevelParameteriv
#define glGetTexLevelParameteriv(...)                      GLCHECK(gl3wProcs.gl.GetTexLevelParameteriv(__VA_ARGS__))
#undef glGetTexParameterIiv
#define glGetTexParameterIiv(...)                          GLCHECK(gl3wProcs.gl.GetTexParameterIiv(__VA_ARGS__))
#undef glGetTexParameterIuiv
#define glGetTexParameterIuiv(...)                         GLCHECK(gl3wProcs.gl.GetTexParameterIuiv(__VA_ARGS__))
#undef glGetTexParameterfv
#define glGetTexParameterfv(...)                           GLCHECK(gl3wProcs.gl.GetTexParameterfv(__VA_ARGS__))
#undef glGetTexParameteriv
#define glGetTexParameteriv(...)                           GLCHECK(gl3wProcs.gl.GetTexParameteriv(__VA_ARGS__))
#undef glGetTextureHandleARB
#define glGetTextureHandleARB(...)                         GLCHECK(gl3wProcs.gl.GetTextureHandleARB(__VA_ARGS__))
#undef glGetTextureImage
#define glGetTextureImage(...)                             GLCHECK(gl3wProcs.gl.GetTextureImage(__VA_ARGS__))
#undef glGetTextureLevelParameterfv
#define glGetTextureLevelParameterfv(...)                  GLCHECK(gl3wProcs.gl.GetTextureLevelParameterfv(__VA_ARGS__))
#undef glGetTextureLevelParameteriv
#define glGetTextureLevelParameteriv(...)                  GLCHECK(gl3wProcs.gl.GetTextureLevelParameteriv(__VA_ARGS__))
#undef glGetTextureParameterIiv
#define glGetTextureParameterIiv(...)                      GLCHECK(gl3wProcs.gl.GetTextureParameterIiv(__VA_ARGS__))
#undef glGetTextureParameterIuiv
#define glGetTextureParameterIuiv(...)                     GLCHECK(gl3wProcs.gl.GetTextureParameterIuiv(__VA_ARGS__))
#undef glGetTextureParameterfv
#define glGetTextureParameterfv(...)                       GLCHECK(gl3wProcs.gl.GetTextureParameterfv(__VA_ARGS__))
#undef glGetTextureParameteriv
#define glGetTextureParameteriv(...)                       GLCHECK(gl3wProcs.gl.GetTextureParameteriv(__VA_ARGS__))
#undef glGetTextureSamplerHandleARB
#define glGetTextureSamplerHandleARB(...)                  GLCHECK(gl3wProcs.gl.GetTextureSamplerHandleARB(__VA_ARGS__))
#undef glGetTextureSubImage
#define glGetTextureSubImage(...)                          GLCHECK(gl3wProcs.gl.GetTextureSubImage(__VA_ARGS__))
#undef glGetTransformFeedbackVarying
#define glGetTransformFeedbackVarying(...)                 GLCHECK(gl3wProcs.gl.GetTransformFeedbackVarying(__VA_ARGS__))
#undef glGetTransformFeedbacki64_v
#define glGetTransformFeedbacki64_v(...)                   GLCHECK(gl3wProcs.gl.GetTransformFeedbacki64_v(__VA_ARGS__))
#undef glGetTransformFeedbacki_v
#define glGetTransformFeedbacki_v(...)                     GLCHECK(gl3wProcs.gl.GetTransformFeedbacki_v(__VA_ARGS__))
#undef glGetTransformFeedbackiv
#define glGetTransformFeedbackiv(...)                      GLCHECK(gl3wProcs.gl.GetTransformFeedbackiv(__VA_ARGS__))
#undef glGetUniformBlockIndex
#define glGetUniformBlockIndex(...)                        GLCHECK(gl3wProcs.gl.GetUniformBlockIndex(__VA_ARGS__))
#undef glGetUniformIndices
#define glGetUniformIndices(...)                           GLCHECK(gl3wProcs.gl.GetUniformIndices(__VA_ARGS__))
#undef glGetUniformSubroutineuiv
#define glGetUniformSubroutineuiv(...)                     GLCHECK(gl3wProcs.gl.GetUniformSubroutineuiv(__VA_ARGS__))
#undef glGetUniformdv
#define glGetUniformdv(...)                                GLCHECK(gl3wProcs.gl.GetUniformdv(__VA_ARGS__))
#undef glGetUniformfv
#define glGetUniformfv(...)                                GLCHECK(gl3wProcs.gl.GetUniformfv(__VA_ARGS__))
#undef glGetUniformiv
#define glGetUniformiv(...)                                GLCHECK(gl3wProcs.gl.GetUniformiv(__VA_ARGS__))
#undef glGetUniformuiv
#define glGetUniformuiv(...)                               GLCHECK(gl3wProcs.gl.GetUniformuiv(__VA_ARGS__))
#undef glGetVertexArrayIndexed64iv
#define glGetVertexArrayIndexed64iv(...)                   GLCHECK(gl3wProcs.gl.GetVertexArrayIndexed64iv(__VA_ARGS__))
#undef glGetVertexArrayIndexediv
#define glGetVertexArrayIndexediv(...)                     GLCHECK(gl3wProcs.gl.GetVertexArrayIndexediv(__VA_ARGS__))
#undef glGetVertexArrayiv
#define glGetVertexArrayiv(...)                            GLCHECK(gl3wProcs.gl.GetVertexArrayiv(__VA_ARGS__))
#undef glGetVertexAttribIiv
#define glGetVertexAttribIiv(...)                          GLCHECK(gl3wProcs.gl.GetVertexAttribIiv(__VA_ARGS__))
#undef glGetVertexAttribIuiv
#define glGetVertexAttribIuiv(...)                         GLCHECK(gl3wProcs.gl.GetVertexAttribIuiv(__VA_ARGS__))
#undef glGetVertexAttribLdv
#define glGetVertexAttribLdv(...)                          GLCHECK(gl3wProcs.gl.GetVertexAttribLdv(__VA_ARGS__))
#undef glGetVertexAttribLui64vARB
#define glGetVertexAttribLui64vARB(...)                    GLCHECK(gl3wProcs.gl.GetVertexAttribLui64vARB(__VA_ARGS__))
#undef glGetVertexAttribPointerv
#define glGetVertexAttribPointerv(...)                     GLCHECK(gl3wProcs.gl.GetVertexAttribPointerv(__VA_ARGS__))
#undef glGetVertexAttribdv
#define glGetVertexAttribdv(...)                           GLCHECK(gl3wProcs.gl.GetVertexAttribdv(__VA_ARGS__))
#undef glGetVertexAttribfv
#define glGetVertexAttribfv(...)                           GLCHECK(gl3wProcs.gl.GetVertexAttribfv(__VA_ARGS__))
#undef glGetVertexAttribiv
#define glGetVertexAttribiv(...)                           GLCHECK(gl3wProcs.gl.GetVertexAttribiv(__VA_ARGS__))
#undef glGetnCompressedTexImage
#define glGetnCompressedTexImage(...)                      GLCHECK(gl3wProcs.gl.GetnCompressedTexImage(__VA_ARGS__))
#undef glGetnCompressedTexImageARB
#define glGetnCompressedTexImageARB(...)                   GLCHECK(gl3wProcs.gl.GetnCompressedTexImageARB(__VA_ARGS__))
#undef glGetnTexImage
#define glGetnTexImage(...)                                GLCHECK(gl3wProcs.gl.GetnTexImage(__VA_ARGS__))
#undef glGetnTexImageARB
#define glGetnTexImageARB(...)                             GLCHECK(gl3wProcs.gl.GetnTexImageARB(__VA_ARGS__))
#undef glGetnUniformdv
#define glGetnUniformdv(...)                               GLCHECK(gl3wProcs.gl.GetnUniformdv(__VA_ARGS__))
#undef glGetnUniformdvARB
#define glGetnUniformdvARB(...)                            GLCHECK(gl3wProcs.gl.GetnUniformdvARB(__VA_ARGS__))
#undef glGetnUniformfv
#define glGetnUniformfv(...)                               GLCHECK(gl3wProcs.gl.GetnUniformfv(__VA_ARGS__))
#undef glGetnUniformfvARB
#define glGetnUniformfvARB(...)                            GLCHECK(gl3wProcs.gl.GetnUniformfvARB(__VA_ARGS__))
#undef glGetnUniformiv
#define glGetnUniformiv(...)                               GLCHECK(gl3wProcs.gl.GetnUniformiv(__VA_ARGS__))
#undef glGetnUniformivARB
#define glGetnUniformivARB(...)                            GLCHECK(gl3wProcs.gl.GetnUniformivARB(__VA_ARGS__))
#undef glGetnUniformuiv
#define glGetnUniformuiv(...)                              GLCHECK(gl3wProcs.gl.GetnUniformuiv(__VA_ARGS__))
#undef glGetnUniformuivARB
#define glGetnUniformuivARB(...)                           GLCHECK(gl3wProcs.gl.GetnUniformuivARB(__VA_ARGS__))
#undef glHint
#define glHint(...)                                        GLCHECK(gl3wProcs.gl.Hint(__VA_ARGS__))
#undef glInvalidateBufferData
#define glInvalidateBufferData(...)                        GLCHECK(gl3wProcs.gl.InvalidateBufferData(__VA_ARGS__))
#undef glInvalidateBufferSubData
#define glInvalidateBufferSubData(...)                     GLCHECK(gl3wProcs.gl.InvalidateBufferSubData(__VA_ARGS__))
#undef glInvalidateFramebuffer
#define glInvalidateFramebuffer(...)                       GLCHECK(gl3wProcs.gl.InvalidateFramebuffer(__VA_ARGS__))
#undef glInvalidateNamedFramebufferData
#define glInvalidateNamedFramebufferData(...)              GLCHECK(gl3wProcs.gl.InvalidateNamedFramebufferData(__VA_ARGS__))
#undef glInvalidateNamedFramebufferSubData
#define glInvalidateNamedFramebufferSubData(...)           GLCHECK(gl3wProcs.gl.InvalidateNamedFramebufferSubData(__VA_ARGS__))
#undef glInvalidateSubFramebuffer
#define glInvalidateSubFramebuffer(...)                    GLCHECK(gl3wProcs.gl.InvalidateSubFramebuffer(__VA_ARGS__))
#undef glInvalidateTexImage
#define glInvalidateTexImage(...)                          GLCHECK(gl3wProcs.gl.InvalidateTexImage(__VA_ARGS__))
#undef glInvalidateTexSubImage
#define glInvalidateTexSubImage(...)                       GLCHECK(gl3wProcs.gl.InvalidateTexSubImage(__VA_ARGS__))
#undef glLineWidth
#define glLineWidth(...)                                   GLCHECK(gl3wProcs.gl.LineWidth(__VA_ARGS__))
#undef glLinkProgram
#define glLinkProgram(...)                                 GLCHECK(gl3wProcs.gl.LinkProgram(__VA_ARGS__))
#undef glLogicOp
#define glLogicOp(...)                                     GLCHECK(gl3wProcs.gl.LogicOp(__VA_ARGS__))
#undef glMakeImageHandleNonResidentARB
#define glMakeImageHandleNonResidentARB(...)               GLCHECK(gl3wProcs.gl.MakeImageHandleNonResidentARB(__VA_ARGS__))
#undef glMakeImageHandleResidentARB
#define glMakeImageHandleResidentARB(...)                  GLCHECK(gl3wProcs.gl.MakeImageHandleResidentARB(__VA_ARGS__))
#undef glMakeTextureHandleNonResidentARB
#define glMakeTextureHandleNonResidentARB(...)             GLCHECK(gl3wProcs.gl.MakeTextureHandleNonResidentARB(__VA_ARGS__))
#undef glMakeTextureHandleResidentARB
#define glMakeTextureHandleResidentARB(...)                GLCHECK(gl3wProcs.gl.MakeTextureHandleResidentARB(__VA_ARGS__))
#undef glMemoryBarrier
#define glMemoryBarrier(...)                               GLCHECK(gl3wProcs.gl.MemoryBarrier(__VA_ARGS__))
#undef glMemoryBarrierByRegion
#define glMemoryBarrierByRegion(...)                       GLCHECK(gl3wProcs.gl.MemoryBarrierByRegion(__VA_ARGS__))
#undef glMinSampleShading
#define glMinSampleShading(...)                            GLCHECK(gl3wProcs.gl.MinSampleShading(__VA_ARGS__))
#undef glMinSampleShadingARB
#define glMinSampleShadingARB(...)                         GLCHECK(gl3wProcs.gl.MinSampleShadingARB(__VA_ARGS__))
#undef glMultiDrawArrays
#define glMultiDrawArrays(...)                             GLCHECK(gl3wProcs.gl.MultiDrawArrays(__VA_ARGS__))
#undef glMultiDrawArraysIndirect
#define glMultiDrawArraysIndirect(...)                     GLCHECK(gl3wProcs.gl.MultiDrawArraysIndirect(__VA_ARGS__))
#undef glMultiDrawArraysIndirectCountARB
#define glMultiDrawArraysIndirectCountARB(...)             GLCHECK(gl3wProcs.gl.MultiDrawArraysIndirectCountARB(__VA_ARGS__))
#undef glMultiDrawElements
#define glMultiDrawElements(...)                           GLCHECK(gl3wProcs.gl.MultiDrawElements(__VA_ARGS__))
#undef glMultiDrawElementsBaseVertex
#define glMultiDrawElementsBaseVertex(...)                 GLCHECK(gl3wProcs.gl.MultiDrawElementsBaseVertex(__VA_ARGS__))
#undef glMultiDrawElementsIndirect
#define glMultiDrawElementsIndirect(...)                   GLCHECK(gl3wProcs.gl.MultiDrawElementsIndirect(__VA_ARGS__))
#undef glMultiDrawElementsIndirectCountARB
#define glMultiDrawElementsIndirectCountARB(...)           GLCHECK(gl3wProcs.gl.MultiDrawElementsIndirectCountARB(__VA_ARGS__))
#undef glNamedBufferData
#define glNamedBufferData(...)                             GLCHECK(gl3wProcs.gl.NamedBufferData(__VA_ARGS__))
#undef glNamedBufferPageCommitmentARB
#define glNamedBufferPageCommitmentARB(...)                GLCHECK(gl3wProcs.gl.NamedBufferPageCommitmentARB(__VA_ARGS__))
#undef glNamedBufferPageCommitmentEXT
#define glNamedBufferPageCommitmentEXT(...)                GLCHECK(gl3wProcs.gl.NamedBufferPageCommitmentEXT(__VA_ARGS__))
#undef glNamedBufferStorage
#define glNamedBufferStorage(...)                          GLCHECK(gl3wProcs.gl.NamedBufferStorage(__VA_ARGS__))
#undef glNamedBufferSubData
#define glNamedBufferSubData(...)                          GLCHECK(gl3wProcs.gl.NamedBufferSubData(__VA_ARGS__))
#undef glNamedFramebufferDrawBuffer
#define glNamedFramebufferDrawBuffer(...)                  GLCHECK(gl3wProcs.gl.NamedFramebufferDrawBuffer(__VA_ARGS__))
#undef glNamedFramebufferDrawBuffers
#define glNamedFramebufferDrawBuffers(...)                 GLCHECK(gl3wProcs.gl.NamedFramebufferDrawBuffers(__VA_ARGS__))
#undef glNamedFramebufferParameteri
#define glNamedFramebufferParameteri(...)                  GLCHECK(gl3wProcs.gl.NamedFramebufferParameteri(__VA_ARGS__))
#undef glNamedFramebufferReadBuffer
#define glNamedFramebufferReadBuffer(...)                  GLCHECK(gl3wProcs.gl.NamedFramebufferReadBuffer(__VA_ARGS__))
#undef glNamedFramebufferRenderbuffer
#define glNamedFramebufferRenderbuffer(...)                GLCHECK(gl3wProcs.gl.NamedFramebufferRenderbuffer(__VA_ARGS__))
#undef glNamedFramebufferTexture
#define glNamedFramebufferTexture(...)                     GLCHECK(gl3wProcs.gl.NamedFramebufferTexture(__VA_ARGS__))
#undef glNamedFramebufferTextureLayer
#define glNamedFramebufferTextureLayer(...)                GLCHECK(gl3wProcs.gl.NamedFramebufferTextureLayer(__VA_ARGS__))
#undef glNamedRenderbufferStorage
#define glNamedRenderbufferStorage(...)                    GLCHECK(gl3wProcs.gl.NamedRenderbufferStorage(__VA_ARGS__))
#undef glNamedRenderbufferStorageMultisample
#define glNamedRenderbufferStorageMultisample(...)         GLCHECK(gl3wProcs.gl.NamedRenderbufferStorageMultisample(__VA_ARGS__))
#undef glNamedStringARB
#define glNamedStringARB(...)                              GLCHECK(gl3wProcs.gl.NamedStringARB(__VA_ARGS__))
#undef glObjectLabel
#define glObjectLabel(...)                                 GLCHECK(gl3wProcs.gl.ObjectLabel(__VA_ARGS__))
#undef glObjectPtrLabel
#define glObjectPtrLabel(...)                              GLCHECK(gl3wProcs.gl.ObjectPtrLabel(__VA_ARGS__))
#undef glPatchParameterfv
#define glPatchParameterfv(...)                            GLCHECK(gl3wProcs.gl.PatchParameterfv(__VA_ARGS__))
#undef glPatchParameteri
#define glPatchParameteri(...)                             GLCHECK(gl3wProcs.gl.PatchParameteri(__VA_ARGS__))
#undef glPauseTransformFeedback
#define glPauseTransformFeedback(...)                      GLCHECK(gl3wProcs.gl.PauseTransformFeedback(__VA_ARGS__))
#undef glPixelStoref
#define glPixelStoref(...)                                 GLCHECK(gl3wProcs.gl.PixelStoref(__VA_ARGS__))
#undef glPixelStorei
#define glPixelStorei(...)                                 GLCHECK(gl3wProcs.gl.PixelStorei(__VA_ARGS__))
#undef glPointParameterf
#define glPointParameterf(...)                             GLCHECK(gl3wProcs.gl.PointParameterf(__VA_ARGS__))
#undef glPointParameterfv
#define glPointParameterfv(...)                            GLCHECK(gl3wProcs.gl.PointParameterfv(__VA_ARGS__))
#undef glPointParameteri
#define glPointParameteri(...)                             GLCHECK(gl3wProcs.gl.PointParameteri(__VA_ARGS__))
#undef glPointParameteriv
#define glPointParameteriv(...)                            GLCHECK(gl3wProcs.gl.PointParameteriv(__VA_ARGS__))
#undef glPointSize
#define glPointSize(...)                                   GLCHECK(gl3wProcs.gl.PointSize(__VA_ARGS__))
#undef glPolygonMode
#define glPolygonMode(...)                                 GLCHECK(gl3wProcs.gl.PolygonMode(__VA_ARGS__))
#undef glPolygonOffset
#define glPolygonOffset(...)                               GLCHECK(gl3wProcs.gl.PolygonOffset(__VA_ARGS__))
#undef glPopDebugGroup
#define glPopDebugGroup(...)                               GLCHECK(gl3wProcs.gl.PopDebugGroup(__VA_ARGS__))
#undef glPrimitiveRestartIndex
#define glPrimitiveRestartIndex(...)                       GLCHECK(gl3wProcs.gl.PrimitiveRestartIndex(__VA_ARGS__))
#undef glProgramBinary
#define glProgramBinary(...)                               GLCHECK(gl3wProcs.gl.ProgramBinary(__VA_ARGS__))
#undef glProgramParameteri
#define glProgramParameteri(...)                           GLCHECK(gl3wProcs.gl.ProgramParameteri(__VA_ARGS__))
#undef glProgramUniform1d
#define glProgramUniform1d(...)                            GLCHECK(gl3wProcs.gl.ProgramUniform1d(__VA_ARGS__))
#undef glProgramUniform1dv
#define glProgramUniform1dv(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform1dv(__VA_ARGS__))
#undef glProgramUniform1f
#define glProgramUniform1f(...)                            GLCHECK(gl3wProcs.gl.ProgramUniform1f(__VA_ARGS__))
#undef glProgramUniform1fv
#define glProgramUniform1fv(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform1fv(__VA_ARGS__))
#undef glProgramUniform1i
#define glProgramUniform1i(...)                            GLCHECK(gl3wProcs.gl.ProgramUniform1i(__VA_ARGS__))
#undef glProgramUniform1iv
#define glProgramUniform1iv(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform1iv(__VA_ARGS__))
#undef glProgramUniform1ui
#define glProgramUniform1ui(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform1ui(__VA_ARGS__))
#undef glProgramUniform1uiv
#define glProgramUniform1uiv(...)                          GLCHECK(gl3wProcs.gl.ProgramUniform1uiv(__VA_ARGS__))
#undef glProgramUniform2d
#define glProgramUniform2d(...)                            GLCHECK(gl3wProcs.gl.ProgramUniform2d(__VA_ARGS__))
#undef glProgramUniform2dv
#define glProgramUniform2dv(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform2dv(__VA_ARGS__))
#undef glProgramUniform2f
#define glProgramUniform2f(...)                            GLCHECK(gl3wProcs.gl.ProgramUniform2f(__VA_ARGS__))
#undef glProgramUniform2fv
#define glProgramUniform2fv(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform2fv(__VA_ARGS__))
#undef glProgramUniform2i
#define glProgramUniform2i(...)                            GLCHECK(gl3wProcs.gl.ProgramUniform2i(__VA_ARGS__))
#undef glProgramUniform2iv
#define glProgramUniform2iv(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform2iv(__VA_ARGS__))
#undef glProgramUniform2ui
#define glProgramUniform2ui(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform2ui(__VA_ARGS__))
#undef glProgramUniform2uiv
#define glProgramUniform2uiv(...)                          GLCHECK(gl3wProcs.gl.ProgramUniform2uiv(__VA_ARGS__))
#undef glProgramUniform3d
#define glProgramUniform3d(...)                            GLCHECK(gl3wProcs.gl.ProgramUniform3d(__VA_ARGS__))
#undef glProgramUniform3dv
#define glProgramUniform3dv(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform3dv(__VA_ARGS__))
#undef glProgramUniform3f
#define glProgramUniform3f(...)                            GLCHECK(gl3wProcs.gl.ProgramUniform3f(__VA_ARGS__))
#undef glProgramUniform3fv
#define glProgramUniform3fv(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform3fv(__VA_ARGS__))
#undef glProgramUniform3i
#define glProgramUniform3i(...)                            GLCHECK(gl3wProcs.gl.ProgramUniform3i(__VA_ARGS__))
#undef glProgramUniform3iv
#define glProgramUniform3iv(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform3iv(__VA_ARGS__))
#undef glProgramUniform3ui
#define glProgramUniform3ui(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform3ui(__VA_ARGS__))
#undef glProgramUniform3uiv
#define glProgramUniform3uiv(...)                          GLCHECK(gl3wProcs.gl.ProgramUniform3uiv(__VA_ARGS__))
#undef glProgramUniform4d
#define glProgramUniform4d(...)                            GLCHECK(gl3wProcs.gl.ProgramUniform4d(__VA_ARGS__))
#undef glProgramUniform4dv
#define glProgramUniform4dv(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform4dv(__VA_ARGS__))
#undef glProgramUniform4f
#define glProgramUniform4f(...)                            GLCHECK(gl3wProcs.gl.ProgramUniform4f(__VA_ARGS__))
#undef glProgramUniform4fv
#define glProgramUniform4fv(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform4fv(__VA_ARGS__))
#undef glProgramUniform4i
#define glProgramUniform4i(...)                            GLCHECK(gl3wProcs.gl.ProgramUniform4i(__VA_ARGS__))
#undef glProgramUniform4iv
#define glProgramUniform4iv(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform4iv(__VA_ARGS__))
#undef glProgramUniform4ui
#define glProgramUniform4ui(...)                           GLCHECK(gl3wProcs.gl.ProgramUniform4ui(__VA_ARGS__))
#undef glProgramUniform4uiv
#define glProgramUniform4uiv(...)                          GLCHECK(gl3wProcs.gl.ProgramUniform4uiv(__VA_ARGS__))
#undef glProgramUniformHandleui64ARB
#define glProgramUniformHandleui64ARB(...)                 GLCHECK(gl3wProcs.gl.ProgramUniformHandleui64ARB(__VA_ARGS__))
#undef glProgramUniformHandleui64vARB
#define glProgramUniformHandleui64vARB(...)                GLCHECK(gl3wProcs.gl.ProgramUniformHandleui64vARB(__VA_ARGS__))
#undef glProgramUniformMatrix2dv
#define glProgramUniformMatrix2dv(...)                     GLCHECK(gl3wProcs.gl.ProgramUniformMatrix2dv(__VA_ARGS__))
#undef glProgramUniformMatrix2fv
#define glProgramUniformMatrix2fv(...)                     GLCHECK(gl3wProcs.gl.ProgramUniformMatrix2fv(__VA_ARGS__))
#undef glProgramUniformMatrix2x3dv
#define glProgramUniformMatrix2x3dv(...)                   GLCHECK(gl3wProcs.gl.ProgramUniformMatrix2x3dv(__VA_ARGS__))
#undef glProgramUniformMatrix2x3fv
#define glProgramUniformMatrix2x3fv(...)                   GLCHECK(gl3wProcs.gl.ProgramUniformMatrix2x3fv(__VA_ARGS__))
#undef glProgramUniformMatrix2x4dv
#define glProgramUniformMatrix2x4dv(...)                   GLCHECK(gl3wProcs.gl.ProgramUniformMatrix2x4dv(__VA_ARGS__))
#undef glProgramUniformMatrix2x4fv
#define glProgramUniformMatrix2x4fv(...)                   GLCHECK(gl3wProcs.gl.ProgramUniformMatrix2x4fv(__VA_ARGS__))
#undef glProgramUniformMatrix3dv
#define glProgramUniformMatrix3dv(...)                     GLCHECK(gl3wProcs.gl.ProgramUniformMatrix3dv(__VA_ARGS__))
#undef glProgramUniformMatrix3fv
#define glProgramUniformMatrix3fv(...)                     GLCHECK(gl3wProcs.gl.ProgramUniformMatrix3fv(__VA_ARGS__))
#undef glProgramUniformMatrix3x2dv
#define glProgramUniformMatrix3x2dv(...)                   GLCHECK(gl3wProcs.gl.ProgramUniformMatrix3x2dv(__VA_ARGS__))
#undef glProgramUniformMatrix3x2fv
#define glProgramUniformMatrix3x2fv(...)                   GLCHECK(gl3wProcs.gl.ProgramUniformMatrix3x2fv(__VA_ARGS__))
#undef glProgramUniformMatrix3x4dv
#define glProgramUniformMatrix3x4dv(...)                   GLCHECK(gl3wProcs.gl.ProgramUniformMatrix3x4dv(__VA_ARGS__))
#undef glProgramUniformMatrix3x4fv
#define glProgramUniformMatrix3x4fv(...)                   GLCHECK(gl3wProcs.gl.ProgramUniformMatrix3x4fv(__VA_ARGS__))
#undef glProgramUniformMatrix4dv
#define glProgramUniformMatrix4dv(...)                     GLCHECK(gl3wProcs.gl.ProgramUniformMatrix4dv(__VA_ARGS__))
#undef glProgramUniformMatrix4fv
#define glProgramUniformMatrix4fv(...)                     GLCHECK(gl3wProcs.gl.ProgramUniformMatrix4fv(__VA_ARGS__))
#undef glProgramUniformMatrix4x2dv
#define glProgramUniformMatrix4x2dv(...)                   GLCHECK(gl3wProcs.gl.ProgramUniformMatrix4x2dv(__VA_ARGS__))
#undef glProgramUniformMatrix4x2fv
#define glProgramUniformMatrix4x2fv(...)                   GLCHECK(gl3wProcs.gl.ProgramUniformMatrix4x2fv(__VA_ARGS__))
#undef glProgramUniformMatrix4x3dv
#define glProgramUniformMatrix4x3dv(...)                   GLCHECK(gl3wProcs.gl.ProgramUniformMatrix4x3dv(__VA_ARGS__))
#undef glProgramUniformMatrix4x3fv
#define glProgramUniformMatrix4x3fv(...)                   GLCHECK(gl3wProcs.gl.ProgramUniformMatrix4x3fv(__VA_ARGS__))
#undef glProvokingVertex
#define glProvokingVertex(...)                             GLCHECK(gl3wProcs.gl.ProvokingVertex(__VA_ARGS__))
#undef glPushDebugGroup
#define glPushDebugGroup(...)                              GLCHECK(gl3wProcs.gl.PushDebugGroup(__VA_ARGS__))
#undef glQueryCounter
#define glQueryCounter(...)                                GLCHECK(gl3wProcs.gl.QueryCounter(__VA_ARGS__))
#undef glReadBuffer
#define glReadBuffer(...)                                  GLCHECK(gl3wProcs.gl.ReadBuffer(__VA_ARGS__))
#undef glReadPixels
#define glReadPixels(...)                                  GLCHECK(gl3wProcs.gl.ReadPixels(__VA_ARGS__))
#undef glReadnPixels
#define glReadnPixels(...)                                 GLCHECK(gl3wProcs.gl.ReadnPixels(__VA_ARGS__))
#undef glReadnPixelsARB
#define glReadnPixelsARB(...)                              GLCHECK(gl3wProcs.gl.ReadnPixelsARB(__VA_ARGS__))
#undef glReleaseShaderCompiler
#define glReleaseShaderCompiler(...)                       GLCHECK(gl3wProcs.gl.ReleaseShaderCompiler(__VA_ARGS__))
#undef glRenderbufferStorage
#define glRenderbufferStorage(...)                         GLCHECK(gl3wProcs.gl.RenderbufferStorage(__VA_ARGS__))
#undef glRenderbufferStorageMultisample
#define glRenderbufferStorageMultisample(...)              GLCHECK(gl3wProcs.gl.RenderbufferStorageMultisample(__VA_ARGS__))
#undef glResumeTransformFeedback
#define glResumeTransformFeedback(...)                     GLCHECK(gl3wProcs.gl.ResumeTransformFeedback(__VA_ARGS__))
#undef glSampleCoverage
#define glSampleCoverage(...)                              GLCHECK(gl3wProcs.gl.SampleCoverage(__VA_ARGS__))
#undef glSampleMaski
#define glSampleMaski(...)                                 GLCHECK(gl3wProcs.gl.SampleMaski(__VA_ARGS__))
#undef glSamplerParameterIiv
#define glSamplerParameterIiv(...)                         GLCHECK(gl3wProcs.gl.SamplerParameterIiv(__VA_ARGS__))
#undef glSamplerParameterIuiv
#define glSamplerParameterIuiv(...)                        GLCHECK(gl3wProcs.gl.SamplerParameterIuiv(__VA_ARGS__))
#undef glSamplerParameterf
#define glSamplerParameterf(...)                           GLCHECK(gl3wProcs.gl.SamplerParameterf(__VA_ARGS__))
#undef glSamplerParameterfv
#define glSamplerParameterfv(...)                          GLCHECK(gl3wProcs.gl.SamplerParameterfv(__VA_ARGS__))
#undef glSamplerParameteri
#define glSamplerParameteri(...)                           GLCHECK(gl3wProcs.gl.SamplerParameteri(__VA_ARGS__))
#undef glSamplerParameteriv
#define glSamplerParameteriv(...)                          GLCHECK(gl3wProcs.gl.SamplerParameteriv(__VA_ARGS__))
#undef glScissor
#define glScissor(...)                                     GLCHECK(gl3wProcs.gl.Scissor(__VA_ARGS__))
#undef glScissorArrayv
#define glScissorArrayv(...)                               GLCHECK(gl3wProcs.gl.ScissorArrayv(__VA_ARGS__))
#undef glScissorIndexed
#define glScissorIndexed(...)                              GLCHECK(gl3wProcs.gl.ScissorIndexed(__VA_ARGS__))
#undef glScissorIndexedv
#define glScissorIndexedv(...)                             GLCHECK(gl3wProcs.gl.ScissorIndexedv(__VA_ARGS__))
#undef glShaderBinary
#define glShaderBinary(...)                                GLCHECK(gl3wProcs.gl.ShaderBinary(__VA_ARGS__))
#undef glShaderSource
#define glShaderSource(...)                                GLCHECK(gl3wProcs.gl.ShaderSource(__VA_ARGS__))
#undef glShaderStorageBlockBinding
#define glShaderStorageBlockBinding(...)                   GLCHECK(gl3wProcs.gl.ShaderStorageBlockBinding(__VA_ARGS__))
#undef glStencilFunc
#define glStencilFunc(...)                                 GLCHECK(gl3wProcs.gl.StencilFunc(__VA_ARGS__))
#undef glStencilFuncSeparate
#define glStencilFuncSeparate(...)                         GLCHECK(gl3wProcs.gl.StencilFuncSeparate(__VA_ARGS__))
#undef glStencilMask
#define glStencilMask(...)                                 GLCHECK(gl3wProcs.gl.StencilMask(__VA_ARGS__))
#undef glStencilMaskSeparate
#define glStencilMaskSeparate(...)                         GLCHECK(gl3wProcs.gl.StencilMaskSeparate(__VA_ARGS__))
#undef glStencilOp
#define glStencilOp(...)                                   GLCHECK(gl3wProcs.gl.StencilOp(__VA_ARGS__))
#undef glStencilOpSeparate
#define glStencilOpSeparate(...)                           GLCHECK(gl3wProcs.gl.StencilOpSeparate(__VA_ARGS__))
#undef glTexBuffer
#define glTexBuffer(...)                                   GLCHECK(gl3wProcs.gl.TexBuffer(__VA_ARGS__))
#undef glTexBufferRange
#define glTexBufferRange(...)                              GLCHECK(gl3wProcs.gl.TexBufferRange(__VA_ARGS__))
#undef glTexImage1D
#define glTexImage1D(...)                                  GLCHECK(gl3wProcs.gl.TexImage1D(__VA_ARGS__))
#undef glTexImage2D
#define glTexImage2D(...)                                  GLCHECK(gl3wProcs.gl.TexImage2D(__VA_ARGS__))
#undef glTexImage2DMultisample
#define glTexImage2DMultisample(...)                       GLCHECK(gl3wProcs.gl.TexImage2DMultisample(__VA_ARGS__))
#undef glTexImage3D
#define glTexImage3D(...)                                  GLCHECK(gl3wProcs.gl.TexImage3D(__VA_ARGS__))
#undef glTexImage3DMultisample
#define glTexImage3DMultisample(...)                       GLCHECK(gl3wProcs.gl.TexImage3DMultisample(__VA_ARGS__))
#undef glTexPageCommitmentARB
#define glTexPageCommitmentARB(...)                        GLCHECK(gl3wProcs.gl.TexPageCommitmentARB(__VA_ARGS__))
#undef glTexParameterIiv
#define glTexParameterIiv(...)                             GLCHECK(gl3wProcs.gl.TexParameterIiv(__VA_ARGS__))
#undef glTexParameterIuiv
#define glTexParameterIuiv(...)                            GLCHECK(gl3wProcs.gl.TexParameterIuiv(__VA_ARGS__))
#undef glTexParameterf
#define glTexParameterf(...)                               GLCHECK(gl3wProcs.gl.TexParameterf(__VA_ARGS__))
#undef glTexParameterfv
#define glTexParameterfv(...)                              GLCHECK(gl3wProcs.gl.TexParameterfv(__VA_ARGS__))
#undef glTexParameteri
#define glTexParameteri(...)                               GLCHECK(gl3wProcs.gl.TexParameteri(__VA_ARGS__))
#undef glTexParameteriv
#define glTexParameteriv(...)                              GLCHECK(gl3wProcs.gl.TexParameteriv(__VA_ARGS__))
#undef glTexStorage1D
#define glTexStorage1D(...)                                GLCHECK(gl3wProcs.gl.TexStorage1D(__VA_ARGS__))
#undef glTexStorage2D
#define glTexStorage2D(...)                                GLCHECK(gl3wProcs.gl.TexStorage2D(__VA_ARGS__))
#undef glTexStorage2DMultisample
#define glTexStorage2DMultisample(...)                     GLCHECK(gl3wProcs.gl.TexStorage2DMultisample(__VA_ARGS__))
#undef glTexStorage3D
#define glTexStorage3D(...)                                GLCHECK(gl3wProcs.gl.TexStorage3D(__VA_ARGS__))
#undef glTexStorage3DMultisample
#define glTexStorage3DMultisample(...)                     GLCHECK(gl3wProcs.gl.TexStorage3DMultisample(__VA_ARGS__))
#undef glTexSubImage1D
#define glTexSubImage1D(...)                               GLCHECK(gl3wProcs.gl.TexSubImage1D(__VA_ARGS__))
#undef glTexSubImage2D
#define glTexSubImage2D(...)                               GLCHECK(gl3wProcs.gl.TexSubImage2D(__VA_ARGS__))
#undef glTexSubImage3D
#define glTexSubImage3D(...)                               GLCHECK(gl3wProcs.gl.TexSubImage3D(__VA_ARGS__))
#undef glTextureBarrier
#define glTextureBarrier(...)                              GLCHECK(gl3wProcs.gl.TextureBarrier(__VA_ARGS__))
#undef glTextureBuffer
#define glTextureBuffer(...)                               GLCHECK(gl3wProcs.gl.TextureBuffer(__VA_ARGS__))
#undef glTextureBufferRange
#define glTextureBufferRange(...)                          GLCHECK(gl3wProcs.gl.TextureBufferRange(__VA_ARGS__))
#undef glTextureParameterIiv
#define glTextureParameterIiv(...)                         GLCHECK(gl3wProcs.gl.TextureParameterIiv(__VA_ARGS__))
#undef glTextureParameterIuiv
#define glTextureParameterIuiv(...)                        GLCHECK(gl3wProcs.gl.TextureParameterIuiv(__VA_ARGS__))
#undef glTextureParameterf
#define glTextureParameterf(...)                           GLCHECK(gl3wProcs.gl.TextureParameterf(__VA_ARGS__))
#undef glTextureParameterfv
#define glTextureParameterfv(...)                          GLCHECK(gl3wProcs.gl.TextureParameterfv(__VA_ARGS__))
#undef glTextureParameteri
#define glTextureParameteri(...)                           GLCHECK(gl3wProcs.gl.TextureParameteri(__VA_ARGS__))
#undef glTextureParameteriv
#define glTextureParameteriv(...)                          GLCHECK(gl3wProcs.gl.TextureParameteriv(__VA_ARGS__))
#undef glTextureStorage1D
#define glTextureStorage1D(...)                            GLCHECK(gl3wProcs.gl.TextureStorage1D(__VA_ARGS__))
#undef glTextureStorage2D
#define glTextureStorage2D(...)                            GLCHECK(gl3wProcs.gl.TextureStorage2D(__VA_ARGS__))
#undef glTextureStorage2DMultisample
#define glTextureStorage2DMultisample(...)                 GLCHECK(gl3wProcs.gl.TextureStorage2DMultisample(__VA_ARGS__))
#undef glTextureStorage3D
#define glTextureStorage3D(...)                            GLCHECK(gl3wProcs.gl.TextureStorage3D(__VA_ARGS__))
#undef glTextureStorage3DMultisample
#define glTextureStorage3DMultisample(...)                 GLCHECK(gl3wProcs.gl.TextureStorage3DMultisample(__VA_ARGS__))
#undef glTextureSubImage1D
#define glTextureSubImage1D(...)                           GLCHECK(gl3wProcs.gl.TextureSubImage1D(__VA_ARGS__))
#undef glTextureSubImage2D
#define glTextureSubImage2D(...)                           GLCHECK(gl3wProcs.gl.TextureSubImage2D(__VA_ARGS__))
#undef glTextureSubImage3D
#define glTextureSubImage3D(...)                           GLCHECK(gl3wProcs.gl.TextureSubImage3D(__VA_ARGS__))
#undef glTextureView
#define glTextureView(...)                                 GLCHECK(gl3wProcs.gl.TextureView(__VA_ARGS__))
#undef glTransformFeedbackBufferBase
#define glTransformFeedbackBufferBase(...)                 GLCHECK(gl3wProcs.gl.TransformFeedbackBufferBase(__VA_ARGS__))
#undef glTransformFeedbackBufferRange
#define glTransformFeedbackBufferRange(...)                GLCHECK(gl3wProcs.gl.TransformFeedbackBufferRange(__VA_ARGS__))
#undef glTransformFeedbackVaryings
#define glTransformFeedbackVaryings(...)                   GLCHECK(gl3wProcs.gl.TransformFeedbackVaryings(__VA_ARGS__))
#undef glUniform1d
#define glUniform1d(...)                                   GLCHECK(gl3wProcs.gl.Uniform1d(__VA_ARGS__))
#undef glUniform1dv
#define glUniform1dv(...)                                  GLCHECK(gl3wProcs.gl.Uniform1dv(__VA_ARGS__))
#undef glUniform1f
#define glUniform1f(...)                                   GLCHECK(gl3wProcs.gl.Uniform1f(__VA_ARGS__))
#undef glUniform1fv
#define glUniform1fv(...)                                  GLCHECK(gl3wProcs.gl.Uniform1fv(__VA_ARGS__))
#undef glUniform1i
#define glUniform1i(...)                                   GLCHECK(gl3wProcs.gl.Uniform1i(__VA_ARGS__))
#undef glUniform1iv
#define glUniform1iv(...)                                  GLCHECK(gl3wProcs.gl.Uniform1iv(__VA_ARGS__))
#undef glUniform1ui
#define glUniform1ui(...)                                  GLCHECK(gl3wProcs.gl.Uniform1ui(__VA_ARGS__))
#undef glUniform1uiv
#define glUniform1uiv(...)                                 GLCHECK(gl3wProcs.gl.Uniform1uiv(__VA_ARGS__))
#undef glUniform2d
#define glUniform2d(...)                                   GLCHECK(gl3wProcs.gl.Uniform2d(__VA_ARGS__))
#undef glUniform2dv
#define glUniform2dv(...)                                  GLCHECK(gl3wProcs.gl.Uniform2dv(__VA_ARGS__))
#undef glUniform2f
#define glUniform2f(...)                                   GLCHECK(gl3wProcs.gl.Uniform2f(__VA_ARGS__))
#undef glUniform2fv
#define glUniform2fv(...)                                  GLCHECK(gl3wProcs.gl.Uniform2fv(__VA_ARGS__))
#undef glUniform2i
#define glUniform2i(...)                                   GLCHECK(gl3wProcs.gl.Uniform2i(__VA_ARGS__))
#undef glUniform2iv
#define glUniform2iv(...)                                  GLCHECK(gl3wProcs.gl.Uniform2iv(__VA_ARGS__))
#undef glUniform2ui
#define glUniform2ui(...)                                  GLCHECK(gl3wProcs.gl.Uniform2ui(__VA_ARGS__))
#undef glUniform2uiv
#define glUniform2uiv(...)                                 GLCHECK(gl3wProcs.gl.Uniform2uiv(__VA_ARGS__))
#undef glUniform3d
#define glUniform3d(...)                                   GLCHECK(gl3wProcs.gl.Uniform3d(__VA_ARGS__))
#undef glUniform3dv
#define glUniform3dv(...)                                  GLCHECK(gl3wProcs.gl.Uniform3dv(__VA_ARGS__))
#undef glUniform3f
#define glUniform3f(...)                                   GLCHECK(gl3wProcs.gl.Uniform3f(__VA_ARGS__))
#undef glUniform3fv
#define glUniform3fv(...)                                  GLCHECK(gl3wProcs.gl.Uniform3fv(__VA_ARGS__))
#undef glUniform3i
#define glUniform3i(...)                                   GLCHECK(gl3wProcs.gl.Uniform3i(__VA_ARGS__))
#undef glUniform3iv
#define glUniform3iv(...)                                  GLCHECK(gl3wProcs.gl.Uniform3iv(__VA_ARGS__))
#undef glUniform3ui
#define glUniform3ui(...)                                  GLCHECK(gl3wProcs.gl.Uniform3ui(__VA_ARGS__))
#undef glUniform3uiv
#define glUniform3uiv(...)                                 GLCHECK(gl3wProcs.gl.Uniform3uiv(__VA_ARGS__))
#undef glUniform4d
#define glUniform4d(...)                                   GLCHECK(gl3wProcs.gl.Uniform4d(__VA_ARGS__))
#undef glUniform4dv
#define glUniform4dv(...)                                  GLCHECK(gl3wProcs.gl.Uniform4dv(__VA_ARGS__))
#undef glUniform4f
#define glUniform4f(...)                                   GLCHECK(gl3wProcs.gl.Uniform4f(__VA_ARGS__))
#undef glUniform4fv
#define glUniform4fv(...)                                  GLCHECK(gl3wProcs.gl.Uniform4fv(__VA_ARGS__))
#undef glUniform4i
#define glUniform4i(...)                                   GLCHECK(gl3wProcs.gl.Uniform4i(__VA_ARGS__))
#undef glUniform4iv
#define glUniform4iv(...)                                  GLCHECK(gl3wProcs.gl.Uniform4iv(__VA_ARGS__))
#undef glUniform4ui
#define glUniform4ui(...)                                  GLCHECK(gl3wProcs.gl.Uniform4ui(__VA_ARGS__))
#undef glUniform4uiv
#define glUniform4uiv(...)                                 GLCHECK(gl3wProcs.gl.Uniform4uiv(__VA_ARGS__))
#undef glUniformBlockBinding
#define glUniformBlockBinding(...)                         GLCHECK(gl3wProcs.gl.UniformBlockBinding(__VA_ARGS__))
#undef glUniformHandleui64ARB
#define glUniformHandleui64ARB(...)                        GLCHECK(gl3wProcs.gl.UniformHandleui64ARB(__VA_ARGS__))
#undef glUniformHandleui64vARB
#define glUniformHandleui64vARB(...)                       GLCHECK(gl3wProcs.gl.UniformHandleui64vARB(__VA_ARGS__))
#undef glUniformMatrix2dv
#define glUniformMatrix2dv(...)                            GLCHECK(gl3wProcs.gl.UniformMatrix2dv(__VA_ARGS__))
#undef glUniformMatrix2fv
#define glUniformMatrix2fv(...)                            GLCHECK(gl3wProcs.gl.UniformMatrix2fv(__VA_ARGS__))
#undef glUniformMatrix2x3dv
#define glUniformMatrix2x3dv(...)                          GLCHECK(gl3wProcs.gl.UniformMatrix2x3dv(__VA_ARGS__))
#undef glUniformMatrix2x3fv
#define glUniformMatrix2x3fv(...)                          GLCHECK(gl3wProcs.gl.UniformMatrix2x3fv(__VA_ARGS__))
#undef glUniformMatrix2x4dv
#define glUniformMatrix2x4dv(...)                          GLCHECK(gl3wProcs.gl.UniformMatrix2x4dv(__VA_ARGS__))
#undef glUniformMatrix2x4fv
#define glUniformMatrix2x4fv(...)                          GLCHECK(gl3wProcs.gl.UniformMatrix2x4fv(__VA_ARGS__))
#undef glUniformMatrix3dv
#define glUniformMatrix3dv(...)                            GLCHECK(gl3wProcs.gl.UniformMatrix3dv(__VA_ARGS__))
#undef glUniformMatrix3fv
#define glUniformMatrix3fv(...)                            GLCHECK(gl3wProcs.gl.UniformMatrix3fv(__VA_ARGS__))
#undef glUniformMatrix3x2dv
#define glUniformMatrix3x2dv(...)                          GLCHECK(gl3wProcs.gl.UniformMatrix3x2dv(__VA_ARGS__))
#undef glUniformMatrix3x2fv
#define glUniformMatrix3x2fv(...)                          GLCHECK(gl3wProcs.gl.UniformMatrix3x2fv(__VA_ARGS__))
#undef glUniformMatrix3x4dv
#define glUniformMatrix3x4dv(...)                          GLCHECK(gl3wProcs.gl.UniformMatrix3x4dv(__VA_ARGS__))
#undef glUniformMatrix3x4fv
#define glUniformMatrix3x4fv(...)                          GLCHECK(gl3wProcs.gl.UniformMatrix3x4fv(__VA_ARGS__))
#undef glUniformMatrix4dv
#define glUniformMatrix4dv(...)                            GLCHECK(gl3wProcs.gl.UniformMatrix4dv(__VA_ARGS__))
#undef glUniformMatrix4fv
#define glUniformMatrix4fv(...)                            GLCHECK(gl3wProcs.gl.UniformMatrix4fv(__VA_ARGS__))
#undef glUniformMatrix4x2dv
#define glUniformMatrix4x2dv(...)                          GLCHECK(gl3wProcs.gl.UniformMatrix4x2dv(__VA_ARGS__))
#undef glUniformMatrix4x2fv
#define glUniformMatrix4x2fv(...)                          GLCHECK(gl3wProcs.gl.UniformMatrix4x2fv(__VA_ARGS__))
#undef glUniformMatrix4x3dv
#define glUniformMatrix4x3dv(...)                          GLCHECK(gl3wProcs.gl.UniformMatrix4x3dv(__VA_ARGS__))
#undef glUniformMatrix4x3fv
#define glUniformMatrix4x3fv(...)                          GLCHECK(gl3wProcs.gl.UniformMatrix4x3fv(__VA_ARGS__))
#undef glUniformSubroutinesuiv
#define glUniformSubroutinesuiv(...)                       GLCHECK(gl3wProcs.gl.UniformSubroutinesuiv(__VA_ARGS__))
#undef glUnmapBuffer
#define glUnmapBuffer(...)                                 GLCHECK(gl3wProcs.gl.UnmapBuffer(__VA_ARGS__))
#undef glUnmapNamedBuffer
#define glUnmapNamedBuffer(...)                            GLCHECK(gl3wProcs.gl.UnmapNamedBuffer(__VA_ARGS__))
#undef glUseProgram
#define glUseProgram(...)                                  GLCHECK(gl3wProcs.gl.UseProgram(__VA_ARGS__))
#undef glUseProgramStages
#define glUseProgramStages(...)                            GLCHECK(gl3wProcs.gl.UseProgramStages(__VA_ARGS__))
#undef glValidateProgram
#define glValidateProgram(...)                             GLCHECK(gl3wProcs.gl.ValidateProgram(__VA_ARGS__))
#undef glValidateProgramPipeline
#define glValidateProgramPipeline(...)                     GLCHECK(gl3wProcs.gl.ValidateProgramPipeline(__VA_ARGS__))
#undef glVertexArrayAttribBinding
#define glVertexArrayAttribBinding(...)                    GLCHECK(gl3wProcs.gl.VertexArrayAttribBinding(__VA_ARGS__))
#undef glVertexArrayAttribFormat
#define glVertexArrayAttribFormat(...)                     GLCHECK(gl3wProcs.gl.VertexArrayAttribFormat(__VA_ARGS__))
#undef glVertexArrayAttribIFormat
#define glVertexArrayAttribIFormat(...)                    GLCHECK(gl3wProcs.gl.VertexArrayAttribIFormat(__VA_ARGS__))
#undef glVertexArrayAttribLFormat
#define glVertexArrayAttribLFormat(...)                    GLCHECK(gl3wProcs.gl.VertexArrayAttribLFormat(__VA_ARGS__))
#undef glVertexArrayBindingDivisor
#define glVertexArrayBindingDivisor(...)                   GLCHECK(gl3wProcs.gl.VertexArrayBindingDivisor(__VA_ARGS__))
#undef glVertexArrayElementBuffer
#define glVertexArrayElementBuffer(...)                    GLCHECK(gl3wProcs.gl.VertexArrayElementBuffer(__VA_ARGS__))
#undef glVertexArrayVertexBuffer
#define glVertexArrayVertexBuffer(...)                     GLCHECK(gl3wProcs.gl.VertexArrayVertexBuffer(__VA_ARGS__))
#undef glVertexArrayVertexBuffers
#define glVertexArrayVertexBuffers(...)                    GLCHECK(gl3wProcs.gl.VertexArrayVertexBuffers(__VA_ARGS__))
#undef glVertexAttrib1d
#define glVertexAttrib1d(...)                              GLCHECK(gl3wProcs.gl.VertexAttrib1d(__VA_ARGS__))
#undef glVertexAttrib1dv
#define glVertexAttrib1dv(...)                             GLCHECK(gl3wProcs.gl.VertexAttrib1dv(__VA_ARGS__))
#undef glVertexAttrib1f
#define glVertexAttrib1f(...)                              GLCHECK(gl3wProcs.gl.VertexAttrib1f(__VA_ARGS__))
#undef glVertexAttrib1fv
#define glVertexAttrib1fv(...)                             GLCHECK(gl3wProcs.gl.VertexAttrib1fv(__VA_ARGS__))
#undef glVertexAttrib1s
#define glVertexAttrib1s(...)                              GLCHECK(gl3wProcs.gl.VertexAttrib1s(__VA_ARGS__))
#undef glVertexAttrib1sv
#define glVertexAttrib1sv(...)                             GLCHECK(gl3wProcs.gl.VertexAttrib1sv(__VA_ARGS__))
#undef glVertexAttrib2d
#define glVertexAttrib2d(...)                              GLCHECK(gl3wProcs.gl.VertexAttrib2d(__VA_ARGS__))
#undef glVertexAttrib2dv
#define glVertexAttrib2dv(...)                             GLCHECK(gl3wProcs.gl.VertexAttrib2dv(__VA_ARGS__))
#undef glVertexAttrib2f
#define glVertexAttrib2f(...)                              GLCHECK(gl3wProcs.gl.VertexAttrib2f(__VA_ARGS__))
#undef glVertexAttrib2fv
#define glVertexAttrib2fv(...)                             GLCHECK(gl3wProcs.gl.VertexAttrib2fv(__VA_ARGS__))
#undef glVertexAttrib2s
#define glVertexAttrib2s(...)                              GLCHECK(gl3wProcs.gl.VertexAttrib2s(__VA_ARGS__))
#undef glVertexAttrib2sv
#define glVertexAttrib2sv(...)                             GLCHECK(gl3wProcs.gl.VertexAttrib2sv(__VA_ARGS__))
#undef glVertexAttrib3d
#define glVertexAttrib3d(...)                              GLCHECK(gl3wProcs.gl.VertexAttrib3d(__VA_ARGS__))
#undef glVertexAttrib3dv
#define glVertexAttrib3dv(...)                             GLCHECK(gl3wProcs.gl.VertexAttrib3dv(__VA_ARGS__))
#undef glVertexAttrib3f
#define glVertexAttrib3f(...)                              GLCHECK(gl3wProcs.gl.VertexAttrib3f(__VA_ARGS__))
#undef glVertexAttrib3fv
#define glVertexAttrib3fv(...)                             GLCHECK(gl3wProcs.gl.VertexAttrib3fv(__VA_ARGS__))
#undef glVertexAttrib3s
#define glVertexAttrib3s(...)                              GLCHECK(gl3wProcs.gl.VertexAttrib3s(__VA_ARGS__))
#undef glVertexAttrib3sv
#define glVertexAttrib3sv(...)                             GLCHECK(gl3wProcs.gl.VertexAttrib3sv(__VA_ARGS__))
#undef glVertexAttrib4Nbv
#define glVertexAttrib4Nbv(...)                            GLCHECK(gl3wProcs.gl.VertexAttrib4Nbv(__VA_ARGS__))
#undef glVertexAttrib4Niv
#define glVertexAttrib4Niv(...)                            GLCHECK(gl3wProcs.gl.VertexAttrib4Niv(__VA_ARGS__))
#undef glVertexAttrib4Nsv
#define glVertexAttrib4Nsv(...)                            GLCHECK(gl3wProcs.gl.VertexAttrib4Nsv(__VA_ARGS__))
#undef glVertexAttrib4Nub
#define glVertexAttrib4Nub(...)                            GLCHECK(gl3wProcs.gl.VertexAttrib4Nub(__VA_ARGS__))
#undef glVertexAttrib4Nubv
#define glVertexAttrib4Nubv(...)                           GLCHECK(gl3wProcs.gl.VertexAttrib4Nubv(__VA_ARGS__))
#undef glVertexAttrib4Nuiv
#define glVertexAttrib4Nuiv(...)                           GLCHECK(gl3wProcs.gl.VertexAttrib4Nuiv(__VA_ARGS__))
#undef glVertexAttrib4Nusv
#define glVertexAttrib4Nusv(...)                           GLCHECK(gl3wProcs.gl.VertexAttrib4Nusv(__VA_ARGS__))
#undef glVertexAttrib4bv
#define glVertexAttrib4bv(...)                             GLCHECK(gl3wProcs.gl.VertexAttrib4bv(__VA_ARGS__))
#undef glVertexAttrib4d
#define glVertexAttrib4d(...)                              GLCHECK(gl3wProcs.gl.VertexAttrib4d(__VA_ARGS__))
#undef glVertexAttrib4dv
#define glVertexAttrib4dv(...)                             GLCHECK(gl3wProcs.gl.VertexAttrib4dv(__VA_ARGS__))
#undef glVertexAttrib4f
#define glVertexAttrib4f(...)                              GLCHECK(gl3wProcs.gl.VertexAttrib4f(__VA_ARGS__))
#undef glVertexAttrib4fv
#define glVertexAttrib4fv(...)                             GLCHECK(gl3wProcs.gl.VertexAttrib4fv(__VA_ARGS__))
#undef glVertexAttrib4iv
#define glVertexAttrib4iv(...)                             GLCHECK(gl3wProcs.gl.VertexAttrib4iv(__VA_ARGS__))
#undef glVertexAttrib4s
#define glVertexAttrib4s(...)                              GLCHECK(gl3wProcs.gl.VertexAttrib4s(__VA_ARGS__))
#undef glVertexAttrib4sv
#define glVertexAttrib4sv(...)                             GLCHECK(gl3wProcs.gl.VertexAttrib4sv(__VA_ARGS__))
#undef glVertexAttrib4ubv
#define glVertexAttrib4ubv(...)                            GLCHECK(gl3wProcs.gl.VertexAttrib4ubv(__VA_ARGS__))
#undef glVertexAttrib4uiv
#define glVertexAttrib4uiv(...)                            GLCHECK(gl3wProcs.gl.VertexAttrib4uiv(__VA_ARGS__))
#undef glVertexAttrib4usv
#define glVertexAttrib4usv(...)                            GLCHECK(gl3wProcs.gl.VertexAttrib4usv(__VA_ARGS__))
#undef glVertexAttribBinding
#define glVertexAttribBinding(...)                         GLCHECK(gl3wProcs.gl.VertexAttribBinding(__VA_ARGS__))
#undef glVertexAttribDivisor
#define glVertexAttribDivisor(...)                         GLCHECK(gl3wProcs.gl.VertexAttribDivisor(__VA_ARGS__))
#undef glVertexAttribFormat
#define glVertexAttribFormat(...)                          GLCHECK(gl3wProcs.gl.VertexAttribFormat(__VA_ARGS__))
#undef glVertexAttribI1i
#define glVertexAttribI1i(...)                             GLCHECK(gl3wProcs.gl.VertexAttribI1i(__VA_ARGS__))
#undef glVertexAttribI1iv
#define glVertexAttribI1iv(...)                            GLCHECK(gl3wProcs.gl.VertexAttribI1iv(__VA_ARGS__))
#undef glVertexAttribI1ui
#define glVertexAttribI1ui(...)                            GLCHECK(gl3wProcs.gl.VertexAttribI1ui(__VA_ARGS__))
#undef glVertexAttribI1uiv
#define glVertexAttribI1uiv(...)                           GLCHECK(gl3wProcs.gl.VertexAttribI1uiv(__VA_ARGS__))
#undef glVertexAttribI2i
#define glVertexAttribI2i(...)                             GLCHECK(gl3wProcs.gl.VertexAttribI2i(__VA_ARGS__))
#undef glVertexAttribI2iv
#define glVertexAttribI2iv(...)                            GLCHECK(gl3wProcs.gl.VertexAttribI2iv(__VA_ARGS__))
#undef glVertexAttribI2ui
#define glVertexAttribI2ui(...)                            GLCHECK(gl3wProcs.gl.VertexAttribI2ui(__VA_ARGS__))
#undef glVertexAttribI2uiv
#define glVertexAttribI2uiv(...)                           GLCHECK(gl3wProcs.gl.VertexAttribI2uiv(__VA_ARGS__))
#undef glVertexAttribI3i
#define glVertexAttribI3i(...)                             GLCHECK(gl3wProcs.gl.VertexAttribI3i(__VA_ARGS__))
#undef glVertexAttribI3iv
#define glVertexAttribI3iv(...)                            GLCHECK(gl3wProcs.gl.VertexAttribI3iv(__VA_ARGS__))
#undef glVertexAttribI3ui
#define glVertexAttribI3ui(...)                            GLCHECK(gl3wProcs.gl.VertexAttribI3ui(__VA_ARGS__))
#undef glVertexAttribI3uiv
#define glVertexAttribI3uiv(...)                           GLCHECK(gl3wProcs.gl.VertexAttribI3uiv(__VA_ARGS__))
#undef glVertexAttribI4bv
#define glVertexAttribI4bv(...)                            GLCHECK(gl3wProcs.gl.VertexAttribI4bv(__VA_ARGS__))
#undef glVertexAttribI4i
#define glVertexAttribI4i(...)                             GLCHECK(gl3wProcs.gl.VertexAttribI4i(__VA_ARGS__))
#undef glVertexAttribI4iv
#define glVertexAttribI4iv(...)                            GLCHECK(gl3wProcs.gl.VertexAttribI4iv(__VA_ARGS__))
#undef glVertexAttribI4sv
#define glVertexAttribI4sv(...)                            GLCHECK(gl3wProcs.gl.VertexAttribI4sv(__VA_ARGS__))
#undef glVertexAttribI4ubv
#define glVertexAttribI4ubv(...)                           GLCHECK(gl3wProcs.gl.VertexAttribI4ubv(__VA_ARGS__))
#undef glVertexAttribI4ui
#define glVertexAttribI4ui(...)                            GLCHECK(gl3wProcs.gl.VertexAttribI4ui(__VA_ARGS__))
#undef glVertexAttribI4uiv
#define glVertexAttribI4uiv(...)                           GLCHECK(gl3wProcs.gl.VertexAttribI4uiv(__VA_ARGS__))
#undef glVertexAttribI4usv
#define glVertexAttribI4usv(...)                           GLCHECK(gl3wProcs.gl.VertexAttribI4usv(__VA_ARGS__))
#undef glVertexAttribIFormat
#define glVertexAttribIFormat(...)                         GLCHECK(gl3wProcs.gl.VertexAttribIFormat(__VA_ARGS__))
#undef glVertexAttribIPointer
#define glVertexAttribIPointer(...)                        GLCHECK(gl3wProcs.gl.VertexAttribIPointer(__VA_ARGS__))
#undef glVertexAttribL1d
#define glVertexAttribL1d(...)                             GLCHECK(gl3wProcs.gl.VertexAttribL1d(__VA_ARGS__))
#undef glVertexAttribL1dv
#define glVertexAttribL1dv(...)                            GLCHECK(gl3wProcs.gl.VertexAttribL1dv(__VA_ARGS__))
#undef glVertexAttribL1ui64ARB
#define glVertexAttribL1ui64ARB(...)                       GLCHECK(gl3wProcs.gl.VertexAttribL1ui64ARB(__VA_ARGS__))
#undef glVertexAttribL1ui64vARB
#define glVertexAttribL1ui64vARB(...)                      GLCHECK(gl3wProcs.gl.VertexAttribL1ui64vARB(__VA_ARGS__))
#undef glVertexAttribL2d
#define glVertexAttribL2d(...)                             GLCHECK(gl3wProcs.gl.VertexAttribL2d(__VA_ARGS__))
#undef glVertexAttribL2dv
#define glVertexAttribL2dv(...)                            GLCHECK(gl3wProcs.gl.VertexAttribL2dv(__VA_ARGS__))
#undef glVertexAttribL3d
#define glVertexAttribL3d(...)                             GLCHECK(gl3wProcs.gl.VertexAttribL3d(__VA_ARGS__))
#undef glVertexAttribL3dv
#define glVertexAttribL3dv(...)                            GLCHECK(gl3wProcs.gl.VertexAttribL3dv(__VA_ARGS__))
#undef glVertexAttribL4d
#define glVertexAttribL4d(...)                             GLCHECK(gl3wProcs.gl.VertexAttribL4d(__VA_ARGS__))
#undef glVertexAttribL4dv
#define glVertexAttribL4dv(...)                            GLCHECK(gl3wProcs.gl.VertexAttribL4dv(__VA_ARGS__))
#undef glVertexAttribLFormat
#define glVertexAttribLFormat(...)                         GLCHECK(gl3wProcs.gl.VertexAttribLFormat(__VA_ARGS__))
#undef glVertexAttribLPointer
#define glVertexAttribLPointer(...)                        GLCHECK(gl3wProcs.gl.VertexAttribLPointer(__VA_ARGS__))
#undef glVertexAttribP1ui
#define glVertexAttribP1ui(...)                            GLCHECK(gl3wProcs.gl.VertexAttribP1ui(__VA_ARGS__))
#undef glVertexAttribP1uiv
#define glVertexAttribP1uiv(...)                           GLCHECK(gl3wProcs.gl.VertexAttribP1uiv(__VA_ARGS__))
#undef glVertexAttribP2ui
#define glVertexAttribP2ui(...)                            GLCHECK(gl3wProcs.gl.VertexAttribP2ui(__VA_ARGS__))
#undef glVertexAttribP2uiv
#define glVertexAttribP2uiv(...)                           GLCHECK(gl3wProcs.gl.VertexAttribP2uiv(__VA_ARGS__))
#undef glVertexAttribP3ui
#define glVertexAttribP3ui(...)                            GLCHECK(gl3wProcs.gl.VertexAttribP3ui(__VA_ARGS__))
#undef glVertexAttribP3uiv
#define glVertexAttribP3uiv(...)                           GLCHECK(gl3wProcs.gl.VertexAttribP3uiv(__VA_ARGS__))
#undef glVertexAttribP4ui
#define glVertexAttribP4ui(...)                            GLCHECK(gl3wProcs.gl.VertexAttribP4ui(__VA_ARGS__))
#undef glVertexAttribP4uiv
#define glVertexAttribP4uiv(...)                           GLCHECK(gl3wProcs.gl.VertexAttribP4uiv(__VA_ARGS__))
#undef glVertexAttribPointer
#define glVertexAttribPointer(...)                         GLCHECK(gl3wProcs.gl.VertexAttribPointer(__VA_ARGS__))
#undef glVertexBindingDivisor
#define glVertexBindingDivisor(...)                        GLCHECK(gl3wProcs.gl.VertexBindingDivisor(__VA_ARGS__))
#undef glViewport
#define glViewport(...)                                    GLCHECK(gl3wProcs.gl.Viewport(__VA_ARGS__))
#undef glViewportArrayv
#define glViewportArrayv(...)                              GLCHECK(gl3wProcs.gl.ViewportArrayv(__VA_ARGS__))
#undef glViewportIndexedf
#define glViewportIndexedf(...)                            GLCHECK(gl3wProcs.gl.ViewportIndexedf(__VA_ARGS__))
#undef glViewportIndexedfv
#define glViewportIndexedfv(...)                           GLCHECK(gl3wProcs.gl.ViewportIndexedfv(__VA_ARGS__))
#undef glWaitSync
#define glWaitSync(...)                                    GLCHECK(gl3wProcs.gl.WaitSync(__VA_ARGS__))

#endif
