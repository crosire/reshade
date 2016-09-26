#pragma once

#include <gl/gl3w.h>

#ifndef NDEBUG

#include <cstdio>

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

#undef glActiveShaderProgram
#define glActiveShaderProgram(...)                         GLCHECK(gl3wActiveShaderProgram(__VA_ARGS__))
#undef glActiveTexture
#define glActiveTexture(...)                               GLCHECK(gl3wActiveTexture(__VA_ARGS__))
#undef glAttachShader
#define glAttachShader(...)                                GLCHECK(gl3wAttachShader(__VA_ARGS__))
#undef glBeginConditionalRender
#define glBeginConditionalRender(...)                      GLCHECK(gl3wBeginConditionalRender(__VA_ARGS__))
#undef glBeginQuery
#define glBeginQuery(...)                                  GLCHECK(gl3wBeginQuery(__VA_ARGS__))
#undef glBeginQueryIndexed
#define glBeginQueryIndexed(...)                           GLCHECK(gl3wBeginQueryIndexed(__VA_ARGS__))
#undef glBeginTransformFeedback
#define glBeginTransformFeedback(...)                      GLCHECK(gl3wBeginTransformFeedback(__VA_ARGS__))
#undef glBindAttribLocation
#define glBindAttribLocation(...)                          GLCHECK(gl3wBindAttribLocation(__VA_ARGS__))
#undef glBindBuffer
#define glBindBuffer(...)                                  GLCHECK(gl3wBindBuffer(__VA_ARGS__))
#undef glBindBufferBase
#define glBindBufferBase(...)                              GLCHECK(gl3wBindBufferBase(__VA_ARGS__))
#undef glBindBufferRange
#define glBindBufferRange(...)                             GLCHECK(gl3wBindBufferRange(__VA_ARGS__))
#undef glBindBuffersBase
#define glBindBuffersBase(...)                             GLCHECK(gl3wBindBuffersBase(__VA_ARGS__))
#undef glBindBuffersRange
#define glBindBuffersRange(...)                            GLCHECK(gl3wBindBuffersRange(__VA_ARGS__))
#undef glBindFragDataLocation
#define glBindFragDataLocation(...)                        GLCHECK(gl3wBindFragDataLocation(__VA_ARGS__))
#undef glBindFragDataLocationIndexed
#define glBindFragDataLocationIndexed(...)                 GLCHECK(gl3wBindFragDataLocationIndexed(__VA_ARGS__))
#undef glBindFramebuffer
#define glBindFramebuffer(...)                             GLCHECK(gl3wBindFramebuffer(__VA_ARGS__))
#undef glBindImageTexture
#define glBindImageTexture(...)                            GLCHECK(gl3wBindImageTexture(__VA_ARGS__))
#undef glBindImageTextures
#define glBindImageTextures(...)                           GLCHECK(gl3wBindImageTextures(__VA_ARGS__))
#undef glBindProgramPipeline
#define glBindProgramPipeline(...)                         GLCHECK(gl3wBindProgramPipeline(__VA_ARGS__))
#undef glBindRenderbuffer
#define glBindRenderbuffer(...)                            GLCHECK(gl3wBindRenderbuffer(__VA_ARGS__))
#undef glBindSampler
#define glBindSampler(...)                                 GLCHECK(gl3wBindSampler(__VA_ARGS__))
#undef glBindSamplers
#define glBindSamplers(...)                                GLCHECK(gl3wBindSamplers(__VA_ARGS__))
#undef glBindTexture
#define glBindTexture(...)                                 GLCHECK(gl3wBindTexture(__VA_ARGS__))
#undef glBindTextureUnit
#define glBindTextureUnit(...)                             GLCHECK(gl3wBindTextureUnit(__VA_ARGS__))
#undef glBindTextures
#define glBindTextures(...)                                GLCHECK(gl3wBindTextures(__VA_ARGS__))
#undef glBindTransformFeedback
#define glBindTransformFeedback(...)                       GLCHECK(gl3wBindTransformFeedback(__VA_ARGS__))
#undef glBindVertexArray
#define glBindVertexArray(...)                             GLCHECK(gl3wBindVertexArray(__VA_ARGS__))
#undef glBindVertexBuffer
#define glBindVertexBuffer(...)                            GLCHECK(gl3wBindVertexBuffer(__VA_ARGS__))
#undef glBindVertexBuffers
#define glBindVertexBuffers(...)                           GLCHECK(gl3wBindVertexBuffers(__VA_ARGS__))
#undef glBlendColor
#define glBlendColor(...)                                  GLCHECK(gl3wBlendColor(__VA_ARGS__))
#undef glBlendEquation
#define glBlendEquation(...)                               GLCHECK(gl3wBlendEquation(__VA_ARGS__))
#undef glBlendEquationSeparate
#define glBlendEquationSeparate(...)                       GLCHECK(gl3wBlendEquationSeparate(__VA_ARGS__))
#undef glBlendEquationSeparatei
#define glBlendEquationSeparatei(...)                      GLCHECK(gl3wBlendEquationSeparatei(__VA_ARGS__))
#undef glBlendEquationSeparateiARB
#define glBlendEquationSeparateiARB(...)                   GLCHECK(gl3wBlendEquationSeparateiARB(__VA_ARGS__))
#undef glBlendEquationi
#define glBlendEquationi(...)                              GLCHECK(gl3wBlendEquationi(__VA_ARGS__))
#undef glBlendEquationiARB
#define glBlendEquationiARB(...)                           GLCHECK(gl3wBlendEquationiARB(__VA_ARGS__))
#undef glBlendFunc
#define glBlendFunc(...)                                   GLCHECK(gl3wBlendFunc(__VA_ARGS__))
#undef glBlendFuncSeparate
#define glBlendFuncSeparate(...)                           GLCHECK(gl3wBlendFuncSeparate(__VA_ARGS__))
#undef glBlendFuncSeparatei
#define glBlendFuncSeparatei(...)                          GLCHECK(gl3wBlendFuncSeparatei(__VA_ARGS__))
#undef glBlendFuncSeparateiARB
#define glBlendFuncSeparateiARB(...)                       GLCHECK(gl3wBlendFuncSeparateiARB(__VA_ARGS__))
#undef glBlendFunci
#define glBlendFunci(...)                                  GLCHECK(gl3wBlendFunci(__VA_ARGS__))
#undef glBlendFunciARB
#define glBlendFunciARB(...)                               GLCHECK(gl3wBlendFunciARB(__VA_ARGS__))
#undef glBlitFramebuffer
#define glBlitFramebuffer(...)                             GLCHECK(gl3wBlitFramebuffer(__VA_ARGS__))
#undef glBlitNamedFramebuffer
#define glBlitNamedFramebuffer(...)                        GLCHECK(gl3wBlitNamedFramebuffer(__VA_ARGS__))
#undef glBufferData
#define glBufferData(...)                                  GLCHECK(gl3wBufferData(__VA_ARGS__))
#undef glBufferPageCommitmentARB
#define glBufferPageCommitmentARB(...)                     GLCHECK(gl3wBufferPageCommitmentARB(__VA_ARGS__))
#undef glBufferStorage
#define glBufferStorage(...)                               GLCHECK(gl3wBufferStorage(__VA_ARGS__))
#undef glBufferSubData
#define glBufferSubData(...)                               GLCHECK(gl3wBufferSubData(__VA_ARGS__))
#undef glClampColor
#define glClampColor(...)                                  GLCHECK(gl3wClampColor(__VA_ARGS__))
#undef glClear
#define glClear(...)                                       GLCHECK(gl3wClear(__VA_ARGS__))
#undef glClearBufferData
#define glClearBufferData(...)                             GLCHECK(gl3wClearBufferData(__VA_ARGS__))
#undef glClearBufferSubData
#define glClearBufferSubData(...)                          GLCHECK(gl3wClearBufferSubData(__VA_ARGS__))
#undef glClearBufferfi
#define glClearBufferfi(...)                               GLCHECK(gl3wClearBufferfi(__VA_ARGS__))
#undef glClearBufferfv
#define glClearBufferfv(...)                               GLCHECK(gl3wClearBufferfv(__VA_ARGS__))
#undef glClearBufferiv
#define glClearBufferiv(...)                               GLCHECK(gl3wClearBufferiv(__VA_ARGS__))
#undef glClearBufferuiv
#define glClearBufferuiv(...)                              GLCHECK(gl3wClearBufferuiv(__VA_ARGS__))
#undef glClearColor
#define glClearColor(...)                                  GLCHECK(gl3wClearColor(__VA_ARGS__))
#undef glClearDepth
#define glClearDepth(...)                                  GLCHECK(gl3wClearDepth(__VA_ARGS__))
#undef glClearDepthf
#define glClearDepthf(...)                                 GLCHECK(gl3wClearDepthf(__VA_ARGS__))
#undef glClearNamedBufferData
#define glClearNamedBufferData(...)                        GLCHECK(gl3wClearNamedBufferData(__VA_ARGS__))
#undef glClearNamedBufferSubData
#define glClearNamedBufferSubData(...)                     GLCHECK(gl3wClearNamedBufferSubData(__VA_ARGS__))
#undef glClearNamedFramebufferfi
#define glClearNamedFramebufferfi(...)                     GLCHECK(gl3wClearNamedFramebufferfi(__VA_ARGS__))
#undef glClearNamedFramebufferfv
#define glClearNamedFramebufferfv(...)                     GLCHECK(gl3wClearNamedFramebufferfv(__VA_ARGS__))
#undef glClearNamedFramebufferiv
#define glClearNamedFramebufferiv(...)                     GLCHECK(gl3wClearNamedFramebufferiv(__VA_ARGS__))
#undef glClearNamedFramebufferuiv
#define glClearNamedFramebufferuiv(...)                    GLCHECK(gl3wClearNamedFramebufferuiv(__VA_ARGS__))
#undef glClearStencil
#define glClearStencil(...)                                GLCHECK(gl3wClearStencil(__VA_ARGS__))
#undef glClearTexImage
#define glClearTexImage(...)                               GLCHECK(gl3wClearTexImage(__VA_ARGS__))
#undef glClearTexSubImage
#define glClearTexSubImage(...)                            GLCHECK(gl3wClearTexSubImage(__VA_ARGS__))
#undef glClientWaitSync
#define glClientWaitSync(...)                              GLCHECK(gl3wClientWaitSync(__VA_ARGS__))
#undef glClipControl
#define glClipControl(...)                                 GLCHECK(gl3wClipControl(__VA_ARGS__))
#undef glColorMask
#define glColorMask(...)                                   GLCHECK(gl3wColorMask(__VA_ARGS__))
#undef glColorMaski
#define glColorMaski(...)                                  GLCHECK(gl3wColorMaski(__VA_ARGS__))
#undef glCompileShader
#define glCompileShader(...)                               GLCHECK(gl3wCompileShader(__VA_ARGS__))
#undef glCompileShaderIncludeARB
#define glCompileShaderIncludeARB(...)                     GLCHECK(gl3wCompileShaderIncludeARB(__VA_ARGS__))
#undef glCompressedTexImage1D
#define glCompressedTexImage1D(...)                        GLCHECK(gl3wCompressedTexImage1D(__VA_ARGS__))
#undef glCompressedTexImage2D
#define glCompressedTexImage2D(...)                        GLCHECK(gl3wCompressedTexImage2D(__VA_ARGS__))
#undef glCompressedTexImage3D
#define glCompressedTexImage3D(...)                        GLCHECK(gl3wCompressedTexImage3D(__VA_ARGS__))
#undef glCompressedTexSubImage1D
#define glCompressedTexSubImage1D(...)                     GLCHECK(gl3wCompressedTexSubImage1D(__VA_ARGS__))
#undef glCompressedTexSubImage2D
#define glCompressedTexSubImage2D(...)                     GLCHECK(gl3wCompressedTexSubImage2D(__VA_ARGS__))
#undef glCompressedTexSubImage3D
#define glCompressedTexSubImage3D(...)                     GLCHECK(gl3wCompressedTexSubImage3D(__VA_ARGS__))
#undef glCompressedTextureSubImage1D
#define glCompressedTextureSubImage1D(...)                 GLCHECK(gl3wCompressedTextureSubImage1D(__VA_ARGS__))
#undef glCompressedTextureSubImage2D
#define glCompressedTextureSubImage2D(...)                 GLCHECK(gl3wCompressedTextureSubImage2D(__VA_ARGS__))
#undef glCompressedTextureSubImage3D
#define glCompressedTextureSubImage3D(...)                 GLCHECK(gl3wCompressedTextureSubImage3D(__VA_ARGS__))
#undef glCopyBufferSubData
#define glCopyBufferSubData(...)                           GLCHECK(gl3wCopyBufferSubData(__VA_ARGS__))
#undef glCopyImageSubData
#define glCopyImageSubData(...)                            GLCHECK(gl3wCopyImageSubData(__VA_ARGS__))
#undef glCopyNamedBufferSubData
#define glCopyNamedBufferSubData(...)                      GLCHECK(gl3wCopyNamedBufferSubData(__VA_ARGS__))
#undef glCopyTexImage1D
#define glCopyTexImage1D(...)                              GLCHECK(gl3wCopyTexImage1D(__VA_ARGS__))
#undef glCopyTexImage2D
#define glCopyTexImage2D(...)                              GLCHECK(gl3wCopyTexImage2D(__VA_ARGS__))
#undef glCopyTexSubImage1D
#define glCopyTexSubImage1D(...)                           GLCHECK(gl3wCopyTexSubImage1D(__VA_ARGS__))
#undef glCopyTexSubImage2D
#define glCopyTexSubImage2D(...)                           GLCHECK(gl3wCopyTexSubImage2D(__VA_ARGS__))
#undef glCopyTexSubImage3D
#define glCopyTexSubImage3D(...)                           GLCHECK(gl3wCopyTexSubImage3D(__VA_ARGS__))
#undef glCopyTextureSubImage1D
#define glCopyTextureSubImage1D(...)                       GLCHECK(gl3wCopyTextureSubImage1D(__VA_ARGS__))
#undef glCopyTextureSubImage2D
#define glCopyTextureSubImage2D(...)                       GLCHECK(gl3wCopyTextureSubImage2D(__VA_ARGS__))
#undef glCopyTextureSubImage3D
#define glCopyTextureSubImage3D(...)                       GLCHECK(gl3wCopyTextureSubImage3D(__VA_ARGS__))
#undef glCreateBuffers
#define glCreateBuffers(...)                               GLCHECK(gl3wCreateBuffers(__VA_ARGS__))
#undef glCreateFramebuffers
#define glCreateFramebuffers(...)                          GLCHECK(gl3wCreateFramebuffers(__VA_ARGS__))
#undef glCreateProgramPipelines
#define glCreateProgramPipelines(...)                      GLCHECK(gl3wCreateProgramPipelines(__VA_ARGS__))
#undef glCreateQueries
#define glCreateQueries(...)                               GLCHECK(gl3wCreateQueries(__VA_ARGS__))
#undef glCreateRenderbuffers
#define glCreateRenderbuffers(...)                         GLCHECK(gl3wCreateRenderbuffers(__VA_ARGS__))
#undef glCreateSamplers
#define glCreateSamplers(...)                              GLCHECK(gl3wCreateSamplers(__VA_ARGS__))
#undef glCreateShaderProgramv
#define glCreateShaderProgramv(...)                        GLCHECK(gl3wCreateShaderProgramv(__VA_ARGS__))
#undef glCreateSyncFromCLeventARB
#define glCreateSyncFromCLeventARB(...)                    GLCHECK(gl3wCreateSyncFromCLeventARB(__VA_ARGS__))
#undef glCreateTextures
#define glCreateTextures(...)                              GLCHECK(gl3wCreateTextures(__VA_ARGS__))
#undef glCreateTransformFeedbacks
#define glCreateTransformFeedbacks(...)                    GLCHECK(gl3wCreateTransformFeedbacks(__VA_ARGS__))
#undef glCreateVertexArrays
#define glCreateVertexArrays(...)                          GLCHECK(gl3wCreateVertexArrays(__VA_ARGS__))
#undef glCullFace
#define glCullFace(...)                                    GLCHECK(gl3wCullFace(__VA_ARGS__))
#undef glDebugMessageCallback
#define glDebugMessageCallback(...)                        GLCHECK(gl3wDebugMessageCallback(__VA_ARGS__))
#undef glDebugMessageCallbackARB
#define glDebugMessageCallbackARB(...)                     GLCHECK(gl3wDebugMessageCallbackARB(__VA_ARGS__))
#undef glDebugMessageControl
#define glDebugMessageControl(...)                         GLCHECK(gl3wDebugMessageControl(__VA_ARGS__))
#undef glDebugMessageControlARB
#define glDebugMessageControlARB(...)                      GLCHECK(gl3wDebugMessageControlARB(__VA_ARGS__))
#undef glDebugMessageInsert
#define glDebugMessageInsert(...)                          GLCHECK(gl3wDebugMessageInsert(__VA_ARGS__))
#undef glDebugMessageInsertARB
#define glDebugMessageInsertARB(...)                       GLCHECK(gl3wDebugMessageInsertARB(__VA_ARGS__))
#undef glDeleteBuffers
#define glDeleteBuffers(...)                               GLCHECK(gl3wDeleteBuffers(__VA_ARGS__))
#undef glDeleteFramebuffers
#define glDeleteFramebuffers(...)                          GLCHECK(gl3wDeleteFramebuffers(__VA_ARGS__))
#undef glDeleteNamedStringARB
#define glDeleteNamedStringARB(...)                        GLCHECK(gl3wDeleteNamedStringARB(__VA_ARGS__))
#undef glDeleteProgram
#define glDeleteProgram(...)                               GLCHECK(gl3wDeleteProgram(__VA_ARGS__))
#undef glDeleteProgramPipelines
#define glDeleteProgramPipelines(...)                      GLCHECK(gl3wDeleteProgramPipelines(__VA_ARGS__))
#undef glDeleteQueries
#define glDeleteQueries(...)                               GLCHECK(gl3wDeleteQueries(__VA_ARGS__))
#undef glDeleteRenderbuffers
#define glDeleteRenderbuffers(...)                         GLCHECK(gl3wDeleteRenderbuffers(__VA_ARGS__))
#undef glDeleteSamplers
#define glDeleteSamplers(...)                              GLCHECK(gl3wDeleteSamplers(__VA_ARGS__))
#undef glDeleteShader
#define glDeleteShader(...)                                GLCHECK(gl3wDeleteShader(__VA_ARGS__))
#undef glDeleteSync
#define glDeleteSync(...)                                  GLCHECK(gl3wDeleteSync(__VA_ARGS__))
#undef glDeleteTextures
#define glDeleteTextures(...)                              GLCHECK(gl3wDeleteTextures(__VA_ARGS__))
#undef glDeleteTransformFeedbacks
#define glDeleteTransformFeedbacks(...)                    GLCHECK(gl3wDeleteTransformFeedbacks(__VA_ARGS__))
#undef glDeleteVertexArrays
#define glDeleteVertexArrays(...)                          GLCHECK(gl3wDeleteVertexArrays(__VA_ARGS__))
#undef glDepthFunc
#define glDepthFunc(...)                                   GLCHECK(gl3wDepthFunc(__VA_ARGS__))
#undef glDepthMask
#define glDepthMask(...)                                   GLCHECK(gl3wDepthMask(__VA_ARGS__))
#undef glDepthRange
#define glDepthRange(...)                                  GLCHECK(gl3wDepthRange(__VA_ARGS__))
#undef glDepthRangeArrayv
#define glDepthRangeArrayv(...)                            GLCHECK(gl3wDepthRangeArrayv(__VA_ARGS__))
#undef glDepthRangeIndexed
#define glDepthRangeIndexed(...)                           GLCHECK(gl3wDepthRangeIndexed(__VA_ARGS__))
#undef glDepthRangef
#define glDepthRangef(...)                                 GLCHECK(gl3wDepthRangef(__VA_ARGS__))
#undef glDetachShader
#define glDetachShader(...)                                GLCHECK(gl3wDetachShader(__VA_ARGS__))
#undef glDisable
#define glDisable(...)                                     GLCHECK(gl3wDisable(__VA_ARGS__))
#undef glDisableVertexArrayAttrib
#define glDisableVertexArrayAttrib(...)                    GLCHECK(gl3wDisableVertexArrayAttrib(__VA_ARGS__))
#undef glDisableVertexAttribArray
#define glDisableVertexAttribArray(...)                    GLCHECK(gl3wDisableVertexAttribArray(__VA_ARGS__))
#undef glDisablei
#define glDisablei(...)                                    GLCHECK(gl3wDisablei(__VA_ARGS__))
#undef glDispatchCompute
#define glDispatchCompute(...)                             GLCHECK(gl3wDispatchCompute(__VA_ARGS__))
#undef glDispatchComputeGroupSizeARB
#define glDispatchComputeGroupSizeARB(...)                 GLCHECK(gl3wDispatchComputeGroupSizeARB(__VA_ARGS__))
#undef glDispatchComputeIndirect
#define glDispatchComputeIndirect(...)                     GLCHECK(gl3wDispatchComputeIndirect(__VA_ARGS__))
#undef glDrawArrays
#define glDrawArrays(...)                                  GLCHECK(gl3wDrawArrays(__VA_ARGS__))
#undef glDrawArraysIndirect
#define glDrawArraysIndirect(...)                          GLCHECK(gl3wDrawArraysIndirect(__VA_ARGS__))
#undef glDrawArraysInstanced
#define glDrawArraysInstanced(...)                         GLCHECK(gl3wDrawArraysInstanced(__VA_ARGS__))
#undef glDrawArraysInstancedBaseInstance
#define glDrawArraysInstancedBaseInstance(...)             GLCHECK(gl3wDrawArraysInstancedBaseInstance(__VA_ARGS__))
#undef glDrawBuffer
#define glDrawBuffer(...)                                  GLCHECK(gl3wDrawBuffer(__VA_ARGS__))
#undef glDrawBuffers
#define glDrawBuffers(...)                                 GLCHECK(gl3wDrawBuffers(__VA_ARGS__))
#undef glDrawElements
#define glDrawElements(...)                                GLCHECK(gl3wDrawElements(__VA_ARGS__))
#undef glDrawElementsBaseVertex
#define glDrawElementsBaseVertex(...)                      GLCHECK(gl3wDrawElementsBaseVertex(__VA_ARGS__))
#undef glDrawElementsIndirect
#define glDrawElementsIndirect(...)                        GLCHECK(gl3wDrawElementsIndirect(__VA_ARGS__))
#undef glDrawElementsInstanced
#define glDrawElementsInstanced(...)                       GLCHECK(gl3wDrawElementsInstanced(__VA_ARGS__))
#undef glDrawElementsInstancedBaseInstance
#define glDrawElementsInstancedBaseInstance(...)           GLCHECK(gl3wDrawElementsInstancedBaseInstance(__VA_ARGS__))
#undef glDrawElementsInstancedBaseVertex
#define glDrawElementsInstancedBaseVertex(...)             GLCHECK(gl3wDrawElementsInstancedBaseVertex(__VA_ARGS__))
#undef glDrawElementsInstancedBaseVertexBaseInstance
#define glDrawElementsInstancedBaseVertexBaseInstance(...) GLCHECK(gl3wDrawElementsInstancedBaseVertexBaseInstance(__VA_ARGS__))
#undef glDrawRangeElements
#define glDrawRangeElements(...)                           GLCHECK(gl3wDrawRangeElements(__VA_ARGS__))
#undef glDrawRangeElementsBaseVertex
#define glDrawRangeElementsBaseVertex(...)                 GLCHECK(gl3wDrawRangeElementsBaseVertex(__VA_ARGS__))
#undef glDrawTransformFeedback
#define glDrawTransformFeedback(...)                       GLCHECK(gl3wDrawTransformFeedback(__VA_ARGS__))
#undef glDrawTransformFeedbackInstanced
#define glDrawTransformFeedbackInstanced(...)              GLCHECK(gl3wDrawTransformFeedbackInstanced(__VA_ARGS__))
#undef glDrawTransformFeedbackStream
#define glDrawTransformFeedbackStream(...)                 GLCHECK(gl3wDrawTransformFeedbackStream(__VA_ARGS__))
#undef glDrawTransformFeedbackStreamInstanced
#define glDrawTransformFeedbackStreamInstanced(...)        GLCHECK(gl3wDrawTransformFeedbackStreamInstanced(__VA_ARGS__))
#undef glEnable
#define glEnable(...)                                      GLCHECK(gl3wEnable(__VA_ARGS__))
#undef glEnableVertexArrayAttrib
#define glEnableVertexArrayAttrib(...)                     GLCHECK(gl3wEnableVertexArrayAttrib(__VA_ARGS__))
#undef glEnableVertexAttribArray
#define glEnableVertexAttribArray(...)                     GLCHECK(gl3wEnableVertexAttribArray(__VA_ARGS__))
#undef glEnablei
#define glEnablei(...)                                     GLCHECK(gl3wEnablei(__VA_ARGS__))
#undef glEndConditionalRender
#define glEndConditionalRender(...)                        GLCHECK(gl3wEndConditionalRender(__VA_ARGS__))
#undef glEndQuery
#define glEndQuery(...)                                    GLCHECK(gl3wEndQuery(__VA_ARGS__))
#undef glEndQueryIndexed
#define glEndQueryIndexed(...)                             GLCHECK(gl3wEndQueryIndexed(__VA_ARGS__))
#undef glEndTransformFeedback
#define glEndTransformFeedback(...)                        GLCHECK(gl3wEndTransformFeedback(__VA_ARGS__))
#undef glFenceSync
#define glFenceSync(...)                                   GLCHECK(gl3wFenceSync(__VA_ARGS__))
#undef glFinish
#define glFinish(...)                                      GLCHECK(gl3wFinish(__VA_ARGS__))
#undef glFlush
#define glFlush(...)                                       GLCHECK(gl3wFlush(__VA_ARGS__))
#undef glFlushMappedBufferRange
#define glFlushMappedBufferRange(...)                      GLCHECK(gl3wFlushMappedBufferRange(__VA_ARGS__))
#undef glFlushMappedNamedBufferRange
#define glFlushMappedNamedBufferRange(...)                 GLCHECK(gl3wFlushMappedNamedBufferRange(__VA_ARGS__))
#undef glFramebufferParameteri
#define glFramebufferParameteri(...)                       GLCHECK(gl3wFramebufferParameteri(__VA_ARGS__))
#undef glFramebufferRenderbuffer
#define glFramebufferRenderbuffer(...)                     GLCHECK(gl3wFramebufferRenderbuffer(__VA_ARGS__))
#undef glFramebufferTexture
#define glFramebufferTexture(...)                          GLCHECK(gl3wFramebufferTexture(__VA_ARGS__))
#undef glFramebufferTexture1D
#define glFramebufferTexture1D(...)                        GLCHECK(gl3wFramebufferTexture1D(__VA_ARGS__))
#undef glFramebufferTexture2D
#define glFramebufferTexture2D(...)                        GLCHECK(gl3wFramebufferTexture2D(__VA_ARGS__))
#undef glFramebufferTexture3D
#define glFramebufferTexture3D(...)                        GLCHECK(gl3wFramebufferTexture3D(__VA_ARGS__))
#undef glFramebufferTextureLayer
#define glFramebufferTextureLayer(...)                     GLCHECK(gl3wFramebufferTextureLayer(__VA_ARGS__))
#undef glFrontFace
#define glFrontFace(...)                                   GLCHECK(gl3wFrontFace(__VA_ARGS__))
#undef glGenBuffers
#define glGenBuffers(...)                                  GLCHECK(gl3wGenBuffers(__VA_ARGS__))
#undef glGenFramebuffers
#define glGenFramebuffers(...)                             GLCHECK(gl3wGenFramebuffers(__VA_ARGS__))
#undef glGenProgramPipelines
#define glGenProgramPipelines(...)                         GLCHECK(gl3wGenProgramPipelines(__VA_ARGS__))
#undef glGenQueries
#define glGenQueries(...)                                  GLCHECK(gl3wGenQueries(__VA_ARGS__))
#undef glGenRenderbuffers
#define glGenRenderbuffers(...)                            GLCHECK(gl3wGenRenderbuffers(__VA_ARGS__))
#undef glGenSamplers
#define glGenSamplers(...)                                 GLCHECK(gl3wGenSamplers(__VA_ARGS__))
#undef glGenTextures
#define glGenTextures(...)                                 GLCHECK(gl3wGenTextures(__VA_ARGS__))
#undef glGenTransformFeedbacks
#define glGenTransformFeedbacks(...)                       GLCHECK(gl3wGenTransformFeedbacks(__VA_ARGS__))
#undef glGenVertexArrays
#define glGenVertexArrays(...)                             GLCHECK(gl3wGenVertexArrays(__VA_ARGS__))
#undef glGenerateMipmap
#define glGenerateMipmap(...)                              GLCHECK(gl3wGenerateMipmap(__VA_ARGS__))
#undef glGenerateTextureMipmap
#define glGenerateTextureMipmap(...)                       GLCHECK(gl3wGenerateTextureMipmap(__VA_ARGS__))
#undef glGetActiveAtomicCounterBufferiv
#define glGetActiveAtomicCounterBufferiv(...)              GLCHECK(gl3wGetActiveAtomicCounterBufferiv(__VA_ARGS__))
#undef glGetActiveAttrib
#define glGetActiveAttrib(...)                             GLCHECK(gl3wGetActiveAttrib(__VA_ARGS__))
#undef glGetActiveSubroutineName
#define glGetActiveSubroutineName(...)                     GLCHECK(gl3wGetActiveSubroutineName(__VA_ARGS__))
#undef glGetActiveSubroutineUniformName
#define glGetActiveSubroutineUniformName(...)              GLCHECK(gl3wGetActiveSubroutineUniformName(__VA_ARGS__))
#undef glGetActiveSubroutineUniformiv
#define glGetActiveSubroutineUniformiv(...)                GLCHECK(gl3wGetActiveSubroutineUniformiv(__VA_ARGS__))
#undef glGetActiveUniform
#define glGetActiveUniform(...)                            GLCHECK(gl3wGetActiveUniform(__VA_ARGS__))
#undef glGetActiveUniformBlockName
#define glGetActiveUniformBlockName(...)                   GLCHECK(gl3wGetActiveUniformBlockName(__VA_ARGS__))
#undef glGetActiveUniformBlockiv
#define glGetActiveUniformBlockiv(...)                     GLCHECK(gl3wGetActiveUniformBlockiv(__VA_ARGS__))
#undef glGetActiveUniformName
#define glGetActiveUniformName(...)                        GLCHECK(gl3wGetActiveUniformName(__VA_ARGS__))
#undef glGetActiveUniformsiv
#define glGetActiveUniformsiv(...)                         GLCHECK(gl3wGetActiveUniformsiv(__VA_ARGS__))
#undef glGetAttachedShaders
#define glGetAttachedShaders(...)                          GLCHECK(gl3wGetAttachedShaders(__VA_ARGS__))
#undef glGetBooleani_v
#define glGetBooleani_v(...)                               GLCHECK(gl3wGetBooleani_v(__VA_ARGS__))
#undef glGetBooleanv
#define glGetBooleanv(...)                                 GLCHECK(gl3wGetBooleanv(__VA_ARGS__))
#undef glGetBufferParameteri64v
#define glGetBufferParameteri64v(...)                      GLCHECK(gl3wGetBufferParameteri64v(__VA_ARGS__))
#undef glGetBufferParameteriv
#define glGetBufferParameteriv(...)                        GLCHECK(gl3wGetBufferParameteriv(__VA_ARGS__))
#undef glGetBufferPointerv
#define glGetBufferPointerv(...)                           GLCHECK(gl3wGetBufferPointerv(__VA_ARGS__))
#undef glGetBufferSubData
#define glGetBufferSubData(...)                            GLCHECK(gl3wGetBufferSubData(__VA_ARGS__))
#undef glGetCompressedTexImage
#define glGetCompressedTexImage(...)                       GLCHECK(gl3wGetCompressedTexImage(__VA_ARGS__))
#undef glGetCompressedTextureImage
#define glGetCompressedTextureImage(...)                   GLCHECK(gl3wGetCompressedTextureImage(__VA_ARGS__))
#undef glGetCompressedTextureSubImage
#define glGetCompressedTextureSubImage(...)                GLCHECK(gl3wGetCompressedTextureSubImage(__VA_ARGS__))
#undef glGetDebugMessageLog
#define glGetDebugMessageLog(...)                          GLCHECK(gl3wGetDebugMessageLog(__VA_ARGS__))
#undef glGetDebugMessageLogARB
#define glGetDebugMessageLogARB(...)                       GLCHECK(gl3wGetDebugMessageLogARB(__VA_ARGS__))
#undef glGetDoublei_v
#define glGetDoublei_v(...)                                GLCHECK(gl3wGetDoublei_v(__VA_ARGS__))
#undef glGetDoublev
#define glGetDoublev(...)                                  GLCHECK(gl3wGetDoublev(__VA_ARGS__))
#undef glGetFloati_v
#define glGetFloati_v(...)                                 GLCHECK(gl3wGetFloati_v(__VA_ARGS__))
#undef glGetFloatv
#define glGetFloatv(...)                                   GLCHECK(gl3wGetFloatv(__VA_ARGS__))
#undef glGetFragDataIndex
#define glGetFragDataIndex(...)                            GLCHECK(gl3wGetFragDataIndex(__VA_ARGS__))
#undef glGetFragDataLocation
#define glGetFragDataLocation(...)                         GLCHECK(gl3wGetFragDataLocation(__VA_ARGS__))
#undef glGetFramebufferAttachmentParameteriv
#define glGetFramebufferAttachmentParameteriv(...)         GLCHECK(gl3wGetFramebufferAttachmentParameteriv(__VA_ARGS__))
#undef glGetFramebufferParameteriv
#define glGetFramebufferParameteriv(...)                   GLCHECK(gl3wGetFramebufferParameteriv(__VA_ARGS__))
#undef glGetGraphicsResetStatus
#define glGetGraphicsResetStatus(...)                      GLCHECK(gl3wGetGraphicsResetStatus(__VA_ARGS__))
#undef glGetGraphicsResetStatusARB
#define glGetGraphicsResetStatusARB(...)                   GLCHECK(gl3wGetGraphicsResetStatusARB(__VA_ARGS__))
#undef glGetImageHandleARB
#define glGetImageHandleARB(...)                           GLCHECK(gl3wGetImageHandleARB(__VA_ARGS__))
#undef glGetInteger64i_v
#define glGetInteger64i_v(...)                             GLCHECK(gl3wGetInteger64i_v(__VA_ARGS__))
#undef glGetInteger64v
#define glGetInteger64v(...)                               GLCHECK(gl3wGetInteger64v(__VA_ARGS__))
#undef glGetIntegeri_v
#define glGetIntegeri_v(...)                               GLCHECK(gl3wGetIntegeri_v(__VA_ARGS__))
#undef glGetIntegerv
#define glGetIntegerv(...)                                 GLCHECK(gl3wGetIntegerv(__VA_ARGS__))
#undef glGetInternalformati64v
#define glGetInternalformati64v(...)                       GLCHECK(gl3wGetInternalformati64v(__VA_ARGS__))
#undef glGetInternalformativ
#define glGetInternalformativ(...)                         GLCHECK(gl3wGetInternalformativ(__VA_ARGS__))
#undef glGetMultisamplefv
#define glGetMultisamplefv(...)                            GLCHECK(gl3wGetMultisamplefv(__VA_ARGS__))
#undef glGetNamedBufferParameteri64v
#define glGetNamedBufferParameteri64v(...)                 GLCHECK(gl3wGetNamedBufferParameteri64v(__VA_ARGS__))
#undef glGetNamedBufferParameteriv
#define glGetNamedBufferParameteriv(...)                   GLCHECK(gl3wGetNamedBufferParameteriv(__VA_ARGS__))
#undef glGetNamedBufferPointerv
#define glGetNamedBufferPointerv(...)                      GLCHECK(gl3wGetNamedBufferPointerv(__VA_ARGS__))
#undef glGetNamedBufferSubData
#define glGetNamedBufferSubData(...)                       GLCHECK(gl3wGetNamedBufferSubData(__VA_ARGS__))
#undef glGetNamedFramebufferAttachmentParameteriv
#define glGetNamedFramebufferAttachmentParameteriv(...)    GLCHECK(gl3wGetNamedFramebufferAttachmentParameteriv(__VA_ARGS__))
#undef glGetNamedFramebufferParameteriv
#define glGetNamedFramebufferParameteriv(...)              GLCHECK(gl3wGetNamedFramebufferParameteriv(__VA_ARGS__))
#undef glGetNamedRenderbufferParameteriv
#define glGetNamedRenderbufferParameteriv(...)             GLCHECK(gl3wGetNamedRenderbufferParameteriv(__VA_ARGS__))
#undef glGetNamedStringARB
#define glGetNamedStringARB(...)                           GLCHECK(gl3wGetNamedStringARB(__VA_ARGS__))
#undef glGetNamedStringivARB
#define glGetNamedStringivARB(...)                         GLCHECK(gl3wGetNamedStringivARB(__VA_ARGS__))
#undef glGetObjectLabel
#define glGetObjectLabel(...)                              GLCHECK(gl3wGetObjectLabel(__VA_ARGS__))
#undef glGetObjectPtrLabel
#define glGetObjectPtrLabel(...)                           GLCHECK(gl3wGetObjectPtrLabel(__VA_ARGS__))
#undef glGetPointerv
#define glGetPointerv(...)                                 GLCHECK(gl3wGetPointerv(__VA_ARGS__))
#undef glGetProgramBinary
#define glGetProgramBinary(...)                            GLCHECK(gl3wGetProgramBinary(__VA_ARGS__))
#undef glGetProgramInfoLog
#define glGetProgramInfoLog(...)                           GLCHECK(gl3wGetProgramInfoLog(__VA_ARGS__))
#undef glGetProgramInterfaceiv
#define glGetProgramInterfaceiv(...)                       GLCHECK(gl3wGetProgramInterfaceiv(__VA_ARGS__))
#undef glGetProgramPipelineInfoLog
#define glGetProgramPipelineInfoLog(...)                   GLCHECK(gl3wGetProgramPipelineInfoLog(__VA_ARGS__))
#undef glGetProgramPipelineiv
#define glGetProgramPipelineiv(...)                        GLCHECK(gl3wGetProgramPipelineiv(__VA_ARGS__))
#undef glGetProgramResourceIndex
#define glGetProgramResourceIndex(...)                     GLCHECK(gl3wGetProgramResourceIndex(__VA_ARGS__))
#undef glGetProgramResourceLocation
#define glGetProgramResourceLocation(...)                  GLCHECK(gl3wGetProgramResourceLocation(__VA_ARGS__))
#undef glGetProgramResourceLocationIndex
#define glGetProgramResourceLocationIndex(...)             GLCHECK(gl3wGetProgramResourceLocationIndex(__VA_ARGS__))
#undef glGetProgramResourceName
#define glGetProgramResourceName(...)                      GLCHECK(gl3wGetProgramResourceName(__VA_ARGS__))
#undef glGetProgramResourceiv
#define glGetProgramResourceiv(...)                        GLCHECK(gl3wGetProgramResourceiv(__VA_ARGS__))
#undef glGetProgramStageiv
#define glGetProgramStageiv(...)                           GLCHECK(gl3wGetProgramStageiv(__VA_ARGS__))
#undef glGetProgramiv
#define glGetProgramiv(...)                                GLCHECK(gl3wGetProgramiv(__VA_ARGS__))
#undef glGetQueryBufferObjecti64v
#define glGetQueryBufferObjecti64v(...)                    GLCHECK(gl3wGetQueryBufferObjecti64v(__VA_ARGS__))
#undef glGetQueryBufferObjectiv
#define glGetQueryBufferObjectiv(...)                      GLCHECK(gl3wGetQueryBufferObjectiv(__VA_ARGS__))
#undef glGetQueryBufferObjectui64v
#define glGetQueryBufferObjectui64v(...)                   GLCHECK(gl3wGetQueryBufferObjectui64v(__VA_ARGS__))
#undef glGetQueryBufferObjectuiv
#define glGetQueryBufferObjectuiv(...)                     GLCHECK(gl3wGetQueryBufferObjectuiv(__VA_ARGS__))
#undef glGetQueryIndexediv
#define glGetQueryIndexediv(...)                           GLCHECK(gl3wGetQueryIndexediv(__VA_ARGS__))
#undef glGetQueryObjecti64v
#define glGetQueryObjecti64v(...)                          GLCHECK(gl3wGetQueryObjecti64v(__VA_ARGS__))
#undef glGetQueryObjectiv
#define glGetQueryObjectiv(...)                            GLCHECK(gl3wGetQueryObjectiv(__VA_ARGS__))
#undef glGetQueryObjectui64v
#define glGetQueryObjectui64v(...)                         GLCHECK(gl3wGetQueryObjectui64v(__VA_ARGS__))
#undef glGetQueryObjectuiv
#define glGetQueryObjectuiv(...)                           GLCHECK(gl3wGetQueryObjectuiv(__VA_ARGS__))
#undef glGetQueryiv
#define glGetQueryiv(...)                                  GLCHECK(gl3wGetQueryiv(__VA_ARGS__))
#undef glGetRenderbufferParameteriv
#define glGetRenderbufferParameteriv(...)                  GLCHECK(gl3wGetRenderbufferParameteriv(__VA_ARGS__))
#undef glGetSamplerParameterIiv
#define glGetSamplerParameterIiv(...)                      GLCHECK(gl3wGetSamplerParameterIiv(__VA_ARGS__))
#undef glGetSamplerParameterIuiv
#define glGetSamplerParameterIuiv(...)                     GLCHECK(gl3wGetSamplerParameterIuiv(__VA_ARGS__))
#undef glGetSamplerParameterfv
#define glGetSamplerParameterfv(...)                       GLCHECK(gl3wGetSamplerParameterfv(__VA_ARGS__))
#undef glGetSamplerParameteriv
#define glGetSamplerParameteriv(...)                       GLCHECK(gl3wGetSamplerParameteriv(__VA_ARGS__))
#undef glGetShaderInfoLog
#define glGetShaderInfoLog(...)                            GLCHECK(gl3wGetShaderInfoLog(__VA_ARGS__))
#undef glGetShaderPrecisionFormat
#define glGetShaderPrecisionFormat(...)                    GLCHECK(gl3wGetShaderPrecisionFormat(__VA_ARGS__))
#undef glGetShaderSource
#define glGetShaderSource(...)                             GLCHECK(gl3wGetShaderSource(__VA_ARGS__))
#undef glGetShaderiv
#define glGetShaderiv(...)                                 GLCHECK(gl3wGetShaderiv(__VA_ARGS__))
#undef glGetSubroutineIndex
#define glGetSubroutineIndex(...)                          GLCHECK(gl3wGetSubroutineIndex(__VA_ARGS__))
#undef glGetSubroutineUniformLocation
#define glGetSubroutineUniformLocation(...)                GLCHECK(gl3wGetSubroutineUniformLocation(__VA_ARGS__))
#undef glGetSynciv
#define glGetSynciv(...)                                   GLCHECK(gl3wGetSynciv(__VA_ARGS__))
#undef glGetTexImage
#define glGetTexImage(...)                                 GLCHECK(gl3wGetTexImage(__VA_ARGS__))
#undef glGetTexLevelParameterfv
#define glGetTexLevelParameterfv(...)                      GLCHECK(gl3wGetTexLevelParameterfv(__VA_ARGS__))
#undef glGetTexLevelParameteriv
#define glGetTexLevelParameteriv(...)                      GLCHECK(gl3wGetTexLevelParameteriv(__VA_ARGS__))
#undef glGetTexParameterIiv
#define glGetTexParameterIiv(...)                          GLCHECK(gl3wGetTexParameterIiv(__VA_ARGS__))
#undef glGetTexParameterIuiv
#define glGetTexParameterIuiv(...)                         GLCHECK(gl3wGetTexParameterIuiv(__VA_ARGS__))
#undef glGetTexParameterfv
#define glGetTexParameterfv(...)                           GLCHECK(gl3wGetTexParameterfv(__VA_ARGS__))
#undef glGetTexParameteriv
#define glGetTexParameteriv(...)                           GLCHECK(gl3wGetTexParameteriv(__VA_ARGS__))
#undef glGetTextureHandleARB
#define glGetTextureHandleARB(...)                         GLCHECK(gl3wGetTextureHandleARB(__VA_ARGS__))
#undef glGetTextureImage
#define glGetTextureImage(...)                             GLCHECK(gl3wGetTextureImage(__VA_ARGS__))
#undef glGetTextureLevelParameterfv
#define glGetTextureLevelParameterfv(...)                  GLCHECK(gl3wGetTextureLevelParameterfv(__VA_ARGS__))
#undef glGetTextureLevelParameteriv
#define glGetTextureLevelParameteriv(...)                  GLCHECK(gl3wGetTextureLevelParameteriv(__VA_ARGS__))
#undef glGetTextureParameterIiv
#define glGetTextureParameterIiv(...)                      GLCHECK(gl3wGetTextureParameterIiv(__VA_ARGS__))
#undef glGetTextureParameterIuiv
#define glGetTextureParameterIuiv(...)                     GLCHECK(gl3wGetTextureParameterIuiv(__VA_ARGS__))
#undef glGetTextureParameterfv
#define glGetTextureParameterfv(...)                       GLCHECK(gl3wGetTextureParameterfv(__VA_ARGS__))
#undef glGetTextureParameteriv
#define glGetTextureParameteriv(...)                       GLCHECK(gl3wGetTextureParameteriv(__VA_ARGS__))
#undef glGetTextureSamplerHandleARB
#define glGetTextureSamplerHandleARB(...)                  GLCHECK(gl3wGetTextureSamplerHandleARB(__VA_ARGS__))
#undef glGetTextureSubImage
#define glGetTextureSubImage(...)                          GLCHECK(gl3wGetTextureSubImage(__VA_ARGS__))
#undef glGetTransformFeedbackVarying
#define glGetTransformFeedbackVarying(...)                 GLCHECK(gl3wGetTransformFeedbackVarying(__VA_ARGS__))
#undef glGetTransformFeedbacki64_v
#define glGetTransformFeedbacki64_v(...)                   GLCHECK(gl3wGetTransformFeedbacki64_v(__VA_ARGS__))
#undef glGetTransformFeedbacki_v
#define glGetTransformFeedbacki_v(...)                     GLCHECK(gl3wGetTransformFeedbacki_v(__VA_ARGS__))
#undef glGetTransformFeedbackiv
#define glGetTransformFeedbackiv(...)                      GLCHECK(gl3wGetTransformFeedbackiv(__VA_ARGS__))
#undef glGetUniformBlockIndex
#define glGetUniformBlockIndex(...)                        GLCHECK(gl3wGetUniformBlockIndex(__VA_ARGS__))
#undef glGetUniformIndices
#define glGetUniformIndices(...)                           GLCHECK(gl3wGetUniformIndices(__VA_ARGS__))
#undef glGetUniformSubroutineuiv
#define glGetUniformSubroutineuiv(...)                     GLCHECK(gl3wGetUniformSubroutineuiv(__VA_ARGS__))
#undef glGetUniformdv
#define glGetUniformdv(...)                                GLCHECK(gl3wGetUniformdv(__VA_ARGS__))
#undef glGetUniformfv
#define glGetUniformfv(...)                                GLCHECK(gl3wGetUniformfv(__VA_ARGS__))
#undef glGetUniformiv
#define glGetUniformiv(...)                                GLCHECK(gl3wGetUniformiv(__VA_ARGS__))
#undef glGetUniformuiv
#define glGetUniformuiv(...)                               GLCHECK(gl3wGetUniformuiv(__VA_ARGS__))
#undef glGetVertexArrayIndexed64iv
#define glGetVertexArrayIndexed64iv(...)                   GLCHECK(gl3wGetVertexArrayIndexed64iv(__VA_ARGS__))
#undef glGetVertexArrayIndexediv
#define glGetVertexArrayIndexediv(...)                     GLCHECK(gl3wGetVertexArrayIndexediv(__VA_ARGS__))
#undef glGetVertexArrayiv
#define glGetVertexArrayiv(...)                            GLCHECK(gl3wGetVertexArrayiv(__VA_ARGS__))
#undef glGetVertexAttribIiv
#define glGetVertexAttribIiv(...)                          GLCHECK(gl3wGetVertexAttribIiv(__VA_ARGS__))
#undef glGetVertexAttribIuiv
#define glGetVertexAttribIuiv(...)                         GLCHECK(gl3wGetVertexAttribIuiv(__VA_ARGS__))
#undef glGetVertexAttribLdv
#define glGetVertexAttribLdv(...)                          GLCHECK(gl3wGetVertexAttribLdv(__VA_ARGS__))
#undef glGetVertexAttribLui64vARB
#define glGetVertexAttribLui64vARB(...)                    GLCHECK(gl3wGetVertexAttribLui64vARB(__VA_ARGS__))
#undef glGetVertexAttribPointerv
#define glGetVertexAttribPointerv(...)                     GLCHECK(gl3wGetVertexAttribPointerv(__VA_ARGS__))
#undef glGetVertexAttribdv
#define glGetVertexAttribdv(...)                           GLCHECK(gl3wGetVertexAttribdv(__VA_ARGS__))
#undef glGetVertexAttribfv
#define glGetVertexAttribfv(...)                           GLCHECK(gl3wGetVertexAttribfv(__VA_ARGS__))
#undef glGetVertexAttribiv
#define glGetVertexAttribiv(...)                           GLCHECK(gl3wGetVertexAttribiv(__VA_ARGS__))
#undef glGetnCompressedTexImage
#define glGetnCompressedTexImage(...)                      GLCHECK(gl3wGetnCompressedTexImage(__VA_ARGS__))
#undef glGetnCompressedTexImageARB
#define glGetnCompressedTexImageARB(...)                   GLCHECK(gl3wGetnCompressedTexImageARB(__VA_ARGS__))
#undef glGetnTexImage
#define glGetnTexImage(...)                                GLCHECK(gl3wGetnTexImage(__VA_ARGS__))
#undef glGetnTexImageARB
#define glGetnTexImageARB(...)                             GLCHECK(gl3wGetnTexImageARB(__VA_ARGS__))
#undef glGetnUniformdv
#define glGetnUniformdv(...)                               GLCHECK(gl3wGetnUniformdv(__VA_ARGS__))
#undef glGetnUniformdvARB
#define glGetnUniformdvARB(...)                            GLCHECK(gl3wGetnUniformdvARB(__VA_ARGS__))
#undef glGetnUniformfv
#define glGetnUniformfv(...)                               GLCHECK(gl3wGetnUniformfv(__VA_ARGS__))
#undef glGetnUniformfvARB
#define glGetnUniformfvARB(...)                            GLCHECK(gl3wGetnUniformfvARB(__VA_ARGS__))
#undef glGetnUniformiv
#define glGetnUniformiv(...)                               GLCHECK(gl3wGetnUniformiv(__VA_ARGS__))
#undef glGetnUniformivARB
#define glGetnUniformivARB(...)                            GLCHECK(gl3wGetnUniformivARB(__VA_ARGS__))
#undef glGetnUniformuiv
#define glGetnUniformuiv(...)                              GLCHECK(gl3wGetnUniformuiv(__VA_ARGS__))
#undef glGetnUniformuivARB
#define glGetnUniformuivARB(...)                           GLCHECK(gl3wGetnUniformuivARB(__VA_ARGS__))
#undef glHint
#define glHint(...)                                        GLCHECK(gl3wHint(__VA_ARGS__))
#undef glInvalidateBufferData
#define glInvalidateBufferData(...)                        GLCHECK(gl3wInvalidateBufferData(__VA_ARGS__))
#undef glInvalidateBufferSubData
#define glInvalidateBufferSubData(...)                     GLCHECK(gl3wInvalidateBufferSubData(__VA_ARGS__))
#undef glInvalidateFramebuffer
#define glInvalidateFramebuffer(...)                       GLCHECK(gl3wInvalidateFramebuffer(__VA_ARGS__))
#undef glInvalidateNamedFramebufferData
#define glInvalidateNamedFramebufferData(...)              GLCHECK(gl3wInvalidateNamedFramebufferData(__VA_ARGS__))
#undef glInvalidateNamedFramebufferSubData
#define glInvalidateNamedFramebufferSubData(...)           GLCHECK(gl3wInvalidateNamedFramebufferSubData(__VA_ARGS__))
#undef glInvalidateSubFramebuffer
#define glInvalidateSubFramebuffer(...)                    GLCHECK(gl3wInvalidateSubFramebuffer(__VA_ARGS__))
#undef glInvalidateTexImage
#define glInvalidateTexImage(...)                          GLCHECK(gl3wInvalidateTexImage(__VA_ARGS__))
#undef glInvalidateTexSubImage
#define glInvalidateTexSubImage(...)                       GLCHECK(gl3wInvalidateTexSubImage(__VA_ARGS__))
#undef glLineWidth
#define glLineWidth(...)                                   GLCHECK(gl3wLineWidth(__VA_ARGS__))
#undef glLinkProgram
#define glLinkProgram(...)                                 GLCHECK(gl3wLinkProgram(__VA_ARGS__))
#undef glLogicOp
#define glLogicOp(...)                                     GLCHECK(gl3wLogicOp(__VA_ARGS__))
#undef glMakeImageHandleNonResidentARB
#define glMakeImageHandleNonResidentARB(...)               GLCHECK(gl3wMakeImageHandleNonResidentARB(__VA_ARGS__))
#undef glMakeImageHandleResidentARB
#define glMakeImageHandleResidentARB(...)                  GLCHECK(gl3wMakeImageHandleResidentARB(__VA_ARGS__))
#undef glMakeTextureHandleNonResidentARB
#define glMakeTextureHandleNonResidentARB(...)             GLCHECK(gl3wMakeTextureHandleNonResidentARB(__VA_ARGS__))
#undef glMakeTextureHandleResidentARB
#define glMakeTextureHandleResidentARB(...)                GLCHECK(gl3wMakeTextureHandleResidentARB(__VA_ARGS__))
#undef glMemoryBarrier
#define glMemoryBarrier(...)                               GLCHECK(gl3wMemoryBarrier(__VA_ARGS__))
#undef glMemoryBarrierByRegion
#define glMemoryBarrierByRegion(...)                       GLCHECK(gl3wMemoryBarrierByRegion(__VA_ARGS__))
#undef glMinSampleShading
#define glMinSampleShading(...)                            GLCHECK(gl3wMinSampleShading(__VA_ARGS__))
#undef glMinSampleShadingARB
#define glMinSampleShadingARB(...)                         GLCHECK(gl3wMinSampleShadingARB(__VA_ARGS__))
#undef glMultiDrawArrays
#define glMultiDrawArrays(...)                             GLCHECK(gl3wMultiDrawArrays(__VA_ARGS__))
#undef glMultiDrawArraysIndirect
#define glMultiDrawArraysIndirect(...)                     GLCHECK(gl3wMultiDrawArraysIndirect(__VA_ARGS__))
#undef glMultiDrawArraysIndirectCountARB
#define glMultiDrawArraysIndirectCountARB(...)             GLCHECK(gl3wMultiDrawArraysIndirectCountARB(__VA_ARGS__))
#undef glMultiDrawElements
#define glMultiDrawElements(...)                           GLCHECK(gl3wMultiDrawElements(__VA_ARGS__))
#undef glMultiDrawElementsBaseVertex
#define glMultiDrawElementsBaseVertex(...)                 GLCHECK(gl3wMultiDrawElementsBaseVertex(__VA_ARGS__))
#undef glMultiDrawElementsIndirect
#define glMultiDrawElementsIndirect(...)                   GLCHECK(gl3wMultiDrawElementsIndirect(__VA_ARGS__))
#undef glMultiDrawElementsIndirectCountARB
#define glMultiDrawElementsIndirectCountARB(...)           GLCHECK(gl3wMultiDrawElementsIndirectCountARB(__VA_ARGS__))
#undef glNamedBufferData
#define glNamedBufferData(...)                             GLCHECK(gl3wNamedBufferData(__VA_ARGS__))
#undef glNamedBufferPageCommitmentARB
#define glNamedBufferPageCommitmentARB(...)                GLCHECK(gl3wNamedBufferPageCommitmentARB(__VA_ARGS__))
#undef glNamedBufferPageCommitmentEXT
#define glNamedBufferPageCommitmentEXT(...)                GLCHECK(gl3wNamedBufferPageCommitmentEXT(__VA_ARGS__))
#undef glNamedBufferStorage
#define glNamedBufferStorage(...)                          GLCHECK(gl3wNamedBufferStorage(__VA_ARGS__))
#undef glNamedBufferSubData
#define glNamedBufferSubData(...)                          GLCHECK(gl3wNamedBufferSubData(__VA_ARGS__))
#undef glNamedFramebufferDrawBuffer
#define glNamedFramebufferDrawBuffer(...)                  GLCHECK(gl3wNamedFramebufferDrawBuffer(__VA_ARGS__))
#undef glNamedFramebufferDrawBuffers
#define glNamedFramebufferDrawBuffers(...)                 GLCHECK(gl3wNamedFramebufferDrawBuffers(__VA_ARGS__))
#undef glNamedFramebufferParameteri
#define glNamedFramebufferParameteri(...)                  GLCHECK(gl3wNamedFramebufferParameteri(__VA_ARGS__))
#undef glNamedFramebufferReadBuffer
#define glNamedFramebufferReadBuffer(...)                  GLCHECK(gl3wNamedFramebufferReadBuffer(__VA_ARGS__))
#undef glNamedFramebufferRenderbuffer
#define glNamedFramebufferRenderbuffer(...)                GLCHECK(gl3wNamedFramebufferRenderbuffer(__VA_ARGS__))
#undef glNamedFramebufferTexture
#define glNamedFramebufferTexture(...)                     GLCHECK(gl3wNamedFramebufferTexture(__VA_ARGS__))
#undef glNamedFramebufferTextureLayer
#define glNamedFramebufferTextureLayer(...)                GLCHECK(gl3wNamedFramebufferTextureLayer(__VA_ARGS__))
#undef glNamedRenderbufferStorage
#define glNamedRenderbufferStorage(...)                    GLCHECK(gl3wNamedRenderbufferStorage(__VA_ARGS__))
#undef glNamedRenderbufferStorageMultisample
#define glNamedRenderbufferStorageMultisample(...)         GLCHECK(gl3wNamedRenderbufferStorageMultisample(__VA_ARGS__))
#undef glNamedStringARB
#define glNamedStringARB(...)                              GLCHECK(gl3wNamedStringARB(__VA_ARGS__))
#undef glObjectLabel
#define glObjectLabel(...)                                 GLCHECK(gl3wObjectLabel(__VA_ARGS__))
#undef glObjectPtrLabel
#define glObjectPtrLabel(...)                              GLCHECK(gl3wObjectPtrLabel(__VA_ARGS__))
#undef glPatchParameterfv
#define glPatchParameterfv(...)                            GLCHECK(gl3wPatchParameterfv(__VA_ARGS__))
#undef glPatchParameteri
#define glPatchParameteri(...)                             GLCHECK(gl3wPatchParameteri(__VA_ARGS__))
#undef glPauseTransformFeedback
#define glPauseTransformFeedback(...)                      GLCHECK(gl3wPauseTransformFeedback(__VA_ARGS__))
#undef glPixelStoref
#define glPixelStoref(...)                                 GLCHECK(gl3wPixelStoref(__VA_ARGS__))
#undef glPixelStorei
#define glPixelStorei(...)                                 GLCHECK(gl3wPixelStorei(__VA_ARGS__))
#undef glPointParameterf
#define glPointParameterf(...)                             GLCHECK(gl3wPointParameterf(__VA_ARGS__))
#undef glPointParameterfv
#define glPointParameterfv(...)                            GLCHECK(gl3wPointParameterfv(__VA_ARGS__))
#undef glPointParameteri
#define glPointParameteri(...)                             GLCHECK(gl3wPointParameteri(__VA_ARGS__))
#undef glPointParameteriv
#define glPointParameteriv(...)                            GLCHECK(gl3wPointParameteriv(__VA_ARGS__))
#undef glPointSize
#define glPointSize(...)                                   GLCHECK(gl3wPointSize(__VA_ARGS__))
#undef glPolygonMode
#define glPolygonMode(...)                                 GLCHECK(gl3wPolygonMode(__VA_ARGS__))
#undef glPolygonOffset
#define glPolygonOffset(...)                               GLCHECK(gl3wPolygonOffset(__VA_ARGS__))
#undef glPopDebugGroup
#define glPopDebugGroup(...)                               GLCHECK(gl3wPopDebugGroup(__VA_ARGS__))
#undef glPrimitiveRestartIndex
#define glPrimitiveRestartIndex(...)                       GLCHECK(gl3wPrimitiveRestartIndex(__VA_ARGS__))
#undef glProgramBinary
#define glProgramBinary(...)                               GLCHECK(gl3wProgramBinary(__VA_ARGS__))
#undef glProgramParameteri
#define glProgramParameteri(...)                           GLCHECK(gl3wProgramParameteri(__VA_ARGS__))
#undef glProgramUniform1d
#define glProgramUniform1d(...)                            GLCHECK(gl3wProgramUniform1d(__VA_ARGS__))
#undef glProgramUniform1dv
#define glProgramUniform1dv(...)                           GLCHECK(gl3wProgramUniform1dv(__VA_ARGS__))
#undef glProgramUniform1f
#define glProgramUniform1f(...)                            GLCHECK(gl3wProgramUniform1f(__VA_ARGS__))
#undef glProgramUniform1fv
#define glProgramUniform1fv(...)                           GLCHECK(gl3wProgramUniform1fv(__VA_ARGS__))
#undef glProgramUniform1i
#define glProgramUniform1i(...)                            GLCHECK(gl3wProgramUniform1i(__VA_ARGS__))
#undef glProgramUniform1iv
#define glProgramUniform1iv(...)                           GLCHECK(gl3wProgramUniform1iv(__VA_ARGS__))
#undef glProgramUniform1ui
#define glProgramUniform1ui(...)                           GLCHECK(gl3wProgramUniform1ui(__VA_ARGS__))
#undef glProgramUniform1uiv
#define glProgramUniform1uiv(...)                          GLCHECK(gl3wProgramUniform1uiv(__VA_ARGS__))
#undef glProgramUniform2d
#define glProgramUniform2d(...)                            GLCHECK(gl3wProgramUniform2d(__VA_ARGS__))
#undef glProgramUniform2dv
#define glProgramUniform2dv(...)                           GLCHECK(gl3wProgramUniform2dv(__VA_ARGS__))
#undef glProgramUniform2f
#define glProgramUniform2f(...)                            GLCHECK(gl3wProgramUniform2f(__VA_ARGS__))
#undef glProgramUniform2fv
#define glProgramUniform2fv(...)                           GLCHECK(gl3wProgramUniform2fv(__VA_ARGS__))
#undef glProgramUniform2i
#define glProgramUniform2i(...)                            GLCHECK(gl3wProgramUniform2i(__VA_ARGS__))
#undef glProgramUniform2iv
#define glProgramUniform2iv(...)                           GLCHECK(gl3wProgramUniform2iv(__VA_ARGS__))
#undef glProgramUniform2ui
#define glProgramUniform2ui(...)                           GLCHECK(gl3wProgramUniform2ui(__VA_ARGS__))
#undef glProgramUniform2uiv
#define glProgramUniform2uiv(...)                          GLCHECK(gl3wProgramUniform2uiv(__VA_ARGS__))
#undef glProgramUniform3d
#define glProgramUniform3d(...)                            GLCHECK(gl3wProgramUniform3d(__VA_ARGS__))
#undef glProgramUniform3dv
#define glProgramUniform3dv(...)                           GLCHECK(gl3wProgramUniform3dv(__VA_ARGS__))
#undef glProgramUniform3f
#define glProgramUniform3f(...)                            GLCHECK(gl3wProgramUniform3f(__VA_ARGS__))
#undef glProgramUniform3fv
#define glProgramUniform3fv(...)                           GLCHECK(gl3wProgramUniform3fv(__VA_ARGS__))
#undef glProgramUniform3i
#define glProgramUniform3i(...)                            GLCHECK(gl3wProgramUniform3i(__VA_ARGS__))
#undef glProgramUniform3iv
#define glProgramUniform3iv(...)                           GLCHECK(gl3wProgramUniform3iv(__VA_ARGS__))
#undef glProgramUniform3ui
#define glProgramUniform3ui(...)                           GLCHECK(gl3wProgramUniform3ui(__VA_ARGS__))
#undef glProgramUniform3uiv
#define glProgramUniform3uiv(...)                          GLCHECK(gl3wProgramUniform3uiv(__VA_ARGS__))
#undef glProgramUniform4d
#define glProgramUniform4d(...)                            GLCHECK(gl3wProgramUniform4d(__VA_ARGS__))
#undef glProgramUniform4dv
#define glProgramUniform4dv(...)                           GLCHECK(gl3wProgramUniform4dv(__VA_ARGS__))
#undef glProgramUniform4f
#define glProgramUniform4f(...)                            GLCHECK(gl3wProgramUniform4f(__VA_ARGS__))
#undef glProgramUniform4fv
#define glProgramUniform4fv(...)                           GLCHECK(gl3wProgramUniform4fv(__VA_ARGS__))
#undef glProgramUniform4i
#define glProgramUniform4i(...)                            GLCHECK(gl3wProgramUniform4i(__VA_ARGS__))
#undef glProgramUniform4iv
#define glProgramUniform4iv(...)                           GLCHECK(gl3wProgramUniform4iv(__VA_ARGS__))
#undef glProgramUniform4ui
#define glProgramUniform4ui(...)                           GLCHECK(gl3wProgramUniform4ui(__VA_ARGS__))
#undef glProgramUniform4uiv
#define glProgramUniform4uiv(...)                          GLCHECK(gl3wProgramUniform4uiv(__VA_ARGS__))
#undef glProgramUniformHandleui64ARB
#define glProgramUniformHandleui64ARB(...)                 GLCHECK(gl3wProgramUniformHandleui64ARB(__VA_ARGS__))
#undef glProgramUniformHandleui64vARB
#define glProgramUniformHandleui64vARB(...)                GLCHECK(gl3wProgramUniformHandleui64vARB(__VA_ARGS__))
#undef glProgramUniformMatrix2dv
#define glProgramUniformMatrix2dv(...)                     GLCHECK(gl3wProgramUniformMatrix2dv(__VA_ARGS__))
#undef glProgramUniformMatrix2fv
#define glProgramUniformMatrix2fv(...)                     GLCHECK(gl3wProgramUniformMatrix2fv(__VA_ARGS__))
#undef glProgramUniformMatrix2x3dv
#define glProgramUniformMatrix2x3dv(...)                   GLCHECK(gl3wProgramUniformMatrix2x3dv(__VA_ARGS__))
#undef glProgramUniformMatrix2x3fv
#define glProgramUniformMatrix2x3fv(...)                   GLCHECK(gl3wProgramUniformMatrix2x3fv(__VA_ARGS__))
#undef glProgramUniformMatrix2x4dv
#define glProgramUniformMatrix2x4dv(...)                   GLCHECK(gl3wProgramUniformMatrix2x4dv(__VA_ARGS__))
#undef glProgramUniformMatrix2x4fv
#define glProgramUniformMatrix2x4fv(...)                   GLCHECK(gl3wProgramUniformMatrix2x4fv(__VA_ARGS__))
#undef glProgramUniformMatrix3dv
#define glProgramUniformMatrix3dv(...)                     GLCHECK(gl3wProgramUniformMatrix3dv(__VA_ARGS__))
#undef glProgramUniformMatrix3fv
#define glProgramUniformMatrix3fv(...)                     GLCHECK(gl3wProgramUniformMatrix3fv(__VA_ARGS__))
#undef glProgramUniformMatrix3x2dv
#define glProgramUniformMatrix3x2dv(...)                   GLCHECK(gl3wProgramUniformMatrix3x2dv(__VA_ARGS__))
#undef glProgramUniformMatrix3x2fv
#define glProgramUniformMatrix3x2fv(...)                   GLCHECK(gl3wProgramUniformMatrix3x2fv(__VA_ARGS__))
#undef glProgramUniformMatrix3x4dv
#define glProgramUniformMatrix3x4dv(...)                   GLCHECK(gl3wProgramUniformMatrix3x4dv(__VA_ARGS__))
#undef glProgramUniformMatrix3x4fv
#define glProgramUniformMatrix3x4fv(...)                   GLCHECK(gl3wProgramUniformMatrix3x4fv(__VA_ARGS__))
#undef glProgramUniformMatrix4dv
#define glProgramUniformMatrix4dv(...)                     GLCHECK(gl3wProgramUniformMatrix4dv(__VA_ARGS__))
#undef glProgramUniformMatrix4fv
#define glProgramUniformMatrix4fv(...)                     GLCHECK(gl3wProgramUniformMatrix4fv(__VA_ARGS__))
#undef glProgramUniformMatrix4x2dv
#define glProgramUniformMatrix4x2dv(...)                   GLCHECK(gl3wProgramUniformMatrix4x2dv(__VA_ARGS__))
#undef glProgramUniformMatrix4x2fv
#define glProgramUniformMatrix4x2fv(...)                   GLCHECK(gl3wProgramUniformMatrix4x2fv(__VA_ARGS__))
#undef glProgramUniformMatrix4x3dv
#define glProgramUniformMatrix4x3dv(...)                   GLCHECK(gl3wProgramUniformMatrix4x3dv(__VA_ARGS__))
#undef glProgramUniformMatrix4x3fv
#define glProgramUniformMatrix4x3fv(...)                   GLCHECK(gl3wProgramUniformMatrix4x3fv(__VA_ARGS__))
#undef glProvokingVertex
#define glProvokingVertex(...)                             GLCHECK(gl3wProvokingVertex(__VA_ARGS__))
#undef glPushDebugGroup
#define glPushDebugGroup(...)                              GLCHECK(gl3wPushDebugGroup(__VA_ARGS__))
#undef glQueryCounter
#define glQueryCounter(...)                                GLCHECK(gl3wQueryCounter(__VA_ARGS__))
#undef glReadBuffer
#define glReadBuffer(...)                                  GLCHECK(gl3wReadBuffer(__VA_ARGS__))
#undef glReadPixels
#define glReadPixels(...)                                  GLCHECK(gl3wReadPixels(__VA_ARGS__))
#undef glReadnPixels
#define glReadnPixels(...)                                 GLCHECK(gl3wReadnPixels(__VA_ARGS__))
#undef glReadnPixelsARB
#define glReadnPixelsARB(...)                              GLCHECK(gl3wReadnPixelsARB(__VA_ARGS__))
#undef glReleaseShaderCompiler
#define glReleaseShaderCompiler(...)                       GLCHECK(gl3wReleaseShaderCompiler(__VA_ARGS__))
#undef glRenderbufferStorage
#define glRenderbufferStorage(...)                         GLCHECK(gl3wRenderbufferStorage(__VA_ARGS__))
#undef glRenderbufferStorageMultisample
#define glRenderbufferStorageMultisample(...)              GLCHECK(gl3wRenderbufferStorageMultisample(__VA_ARGS__))
#undef glResumeTransformFeedback
#define glResumeTransformFeedback(...)                     GLCHECK(gl3wResumeTransformFeedback(__VA_ARGS__))
#undef glSampleCoverage
#define glSampleCoverage(...)                              GLCHECK(gl3wSampleCoverage(__VA_ARGS__))
#undef glSampleMaski
#define glSampleMaski(...)                                 GLCHECK(gl3wSampleMaski(__VA_ARGS__))
#undef glSamplerParameterIiv
#define glSamplerParameterIiv(...)                         GLCHECK(gl3wSamplerParameterIiv(__VA_ARGS__))
#undef glSamplerParameterIuiv
#define glSamplerParameterIuiv(...)                        GLCHECK(gl3wSamplerParameterIuiv(__VA_ARGS__))
#undef glSamplerParameterf
#define glSamplerParameterf(...)                           GLCHECK(gl3wSamplerParameterf(__VA_ARGS__))
#undef glSamplerParameterfv
#define glSamplerParameterfv(...)                          GLCHECK(gl3wSamplerParameterfv(__VA_ARGS__))
#undef glSamplerParameteri
#define glSamplerParameteri(...)                           GLCHECK(gl3wSamplerParameteri(__VA_ARGS__))
#undef glSamplerParameteriv
#define glSamplerParameteriv(...)                          GLCHECK(gl3wSamplerParameteriv(__VA_ARGS__))
#undef glScissor
#define glScissor(...)                                     GLCHECK(gl3wScissor(__VA_ARGS__))
#undef glScissorArrayv
#define glScissorArrayv(...)                               GLCHECK(gl3wScissorArrayv(__VA_ARGS__))
#undef glScissorIndexed
#define glScissorIndexed(...)                              GLCHECK(gl3wScissorIndexed(__VA_ARGS__))
#undef glScissorIndexedv
#define glScissorIndexedv(...)                             GLCHECK(gl3wScissorIndexedv(__VA_ARGS__))
#undef glShaderBinary
#define glShaderBinary(...)                                GLCHECK(gl3wShaderBinary(__VA_ARGS__))
#undef glShaderSource
#define glShaderSource(...)                                GLCHECK(gl3wShaderSource(__VA_ARGS__))
#undef glShaderStorageBlockBinding
#define glShaderStorageBlockBinding(...)                   GLCHECK(gl3wShaderStorageBlockBinding(__VA_ARGS__))
#undef glStencilFunc
#define glStencilFunc(...)                                 GLCHECK(gl3wStencilFunc(__VA_ARGS__))
#undef glStencilFuncSeparate
#define glStencilFuncSeparate(...)                         GLCHECK(gl3wStencilFuncSeparate(__VA_ARGS__))
#undef glStencilMask
#define glStencilMask(...)                                 GLCHECK(gl3wStencilMask(__VA_ARGS__))
#undef glStencilMaskSeparate
#define glStencilMaskSeparate(...)                         GLCHECK(gl3wStencilMaskSeparate(__VA_ARGS__))
#undef glStencilOp
#define glStencilOp(...)                                   GLCHECK(gl3wStencilOp(__VA_ARGS__))
#undef glStencilOpSeparate
#define glStencilOpSeparate(...)                           GLCHECK(gl3wStencilOpSeparate(__VA_ARGS__))
#undef glTexBuffer
#define glTexBuffer(...)                                   GLCHECK(gl3wTexBuffer(__VA_ARGS__))
#undef glTexBufferRange
#define glTexBufferRange(...)                              GLCHECK(gl3wTexBufferRange(__VA_ARGS__))
#undef glTexImage1D
#define glTexImage1D(...)                                  GLCHECK(gl3wTexImage1D(__VA_ARGS__))
#undef glTexImage2D
#define glTexImage2D(...)                                  GLCHECK(gl3wTexImage2D(__VA_ARGS__))
#undef glTexImage2DMultisample
#define glTexImage2DMultisample(...)                       GLCHECK(gl3wTexImage2DMultisample(__VA_ARGS__))
#undef glTexImage3D
#define glTexImage3D(...)                                  GLCHECK(gl3wTexImage3D(__VA_ARGS__))
#undef glTexImage3DMultisample
#define glTexImage3DMultisample(...)                       GLCHECK(gl3wTexImage3DMultisample(__VA_ARGS__))
#undef glTexPageCommitmentARB
#define glTexPageCommitmentARB(...)                        GLCHECK(gl3wTexPageCommitmentARB(__VA_ARGS__))
#undef glTexParameterIiv
#define glTexParameterIiv(...)                             GLCHECK(gl3wTexParameterIiv(__VA_ARGS__))
#undef glTexParameterIuiv
#define glTexParameterIuiv(...)                            GLCHECK(gl3wTexParameterIuiv(__VA_ARGS__))
#undef glTexParameterf
#define glTexParameterf(...)                               GLCHECK(gl3wTexParameterf(__VA_ARGS__))
#undef glTexParameterfv
#define glTexParameterfv(...)                              GLCHECK(gl3wTexParameterfv(__VA_ARGS__))
#undef glTexParameteri
#define glTexParameteri(...)                               GLCHECK(gl3wTexParameteri(__VA_ARGS__))
#undef glTexParameteriv
#define glTexParameteriv(...)                              GLCHECK(gl3wTexParameteriv(__VA_ARGS__))
#undef glTexStorage1D
#define glTexStorage1D(...)                                GLCHECK(gl3wTexStorage1D(__VA_ARGS__))
#undef glTexStorage2D
#define glTexStorage2D(...)                                GLCHECK(gl3wTexStorage2D(__VA_ARGS__))
#undef glTexStorage2DMultisample
#define glTexStorage2DMultisample(...)                     GLCHECK(gl3wTexStorage2DMultisample(__VA_ARGS__))
#undef glTexStorage3D
#define glTexStorage3D(...)                                GLCHECK(gl3wTexStorage3D(__VA_ARGS__))
#undef glTexStorage3DMultisample
#define glTexStorage3DMultisample(...)                     GLCHECK(gl3wTexStorage3DMultisample(__VA_ARGS__))
#undef glTexSubImage1D
#define glTexSubImage1D(...)                               GLCHECK(gl3wTexSubImage1D(__VA_ARGS__))
#undef glTexSubImage2D
#define glTexSubImage2D(...)                               GLCHECK(gl3wTexSubImage2D(__VA_ARGS__))
#undef glTexSubImage3D
#define glTexSubImage3D(...)                               GLCHECK(gl3wTexSubImage3D(__VA_ARGS__))
#undef glTextureBarrier
#define glTextureBarrier(...)                              GLCHECK(gl3wTextureBarrier(__VA_ARGS__))
#undef glTextureBuffer
#define glTextureBuffer(...)                               GLCHECK(gl3wTextureBuffer(__VA_ARGS__))
#undef glTextureBufferRange
#define glTextureBufferRange(...)                          GLCHECK(gl3wTextureBufferRange(__VA_ARGS__))
#undef glTextureParameterIiv
#define glTextureParameterIiv(...)                         GLCHECK(gl3wTextureParameterIiv(__VA_ARGS__))
#undef glTextureParameterIuiv
#define glTextureParameterIuiv(...)                        GLCHECK(gl3wTextureParameterIuiv(__VA_ARGS__))
#undef glTextureParameterf
#define glTextureParameterf(...)                           GLCHECK(gl3wTextureParameterf(__VA_ARGS__))
#undef glTextureParameterfv
#define glTextureParameterfv(...)                          GLCHECK(gl3wTextureParameterfv(__VA_ARGS__))
#undef glTextureParameteri
#define glTextureParameteri(...)                           GLCHECK(gl3wTextureParameteri(__VA_ARGS__))
#undef glTextureParameteriv
#define glTextureParameteriv(...)                          GLCHECK(gl3wTextureParameteriv(__VA_ARGS__))
#undef glTextureStorage1D
#define glTextureStorage1D(...)                            GLCHECK(gl3wTextureStorage1D(__VA_ARGS__))
#undef glTextureStorage2D
#define glTextureStorage2D(...)                            GLCHECK(gl3wTextureStorage2D(__VA_ARGS__))
#undef glTextureStorage2DMultisample
#define glTextureStorage2DMultisample(...)                 GLCHECK(gl3wTextureStorage2DMultisample(__VA_ARGS__))
#undef glTextureStorage3D
#define glTextureStorage3D(...)                            GLCHECK(gl3wTextureStorage3D(__VA_ARGS__))
#undef glTextureStorage3DMultisample
#define glTextureStorage3DMultisample(...)                 GLCHECK(gl3wTextureStorage3DMultisample(__VA_ARGS__))
#undef glTextureSubImage1D
#define glTextureSubImage1D(...)                           GLCHECK(gl3wTextureSubImage1D(__VA_ARGS__))
#undef glTextureSubImage2D
#define glTextureSubImage2D(...)                           GLCHECK(gl3wTextureSubImage2D(__VA_ARGS__))
#undef glTextureSubImage3D
#define glTextureSubImage3D(...)                           GLCHECK(gl3wTextureSubImage3D(__VA_ARGS__))
#undef glTextureView
#define glTextureView(...)                                 GLCHECK(gl3wTextureView(__VA_ARGS__))
#undef glTransformFeedbackBufferBase
#define glTransformFeedbackBufferBase(...)                 GLCHECK(gl3wTransformFeedbackBufferBase(__VA_ARGS__))
#undef glTransformFeedbackBufferRange
#define glTransformFeedbackBufferRange(...)                GLCHECK(gl3wTransformFeedbackBufferRange(__VA_ARGS__))
#undef glTransformFeedbackVaryings
#define glTransformFeedbackVaryings(...)                   GLCHECK(gl3wTransformFeedbackVaryings(__VA_ARGS__))
#undef glUniform1d
#define glUniform1d(...)                                   GLCHECK(gl3wUniform1d(__VA_ARGS__))
#undef glUniform1dv
#define glUniform1dv(...)                                  GLCHECK(gl3wUniform1dv(__VA_ARGS__))
#undef glUniform1f
#define glUniform1f(...)                                   GLCHECK(gl3wUniform1f(__VA_ARGS__))
#undef glUniform1fv
#define glUniform1fv(...)                                  GLCHECK(gl3wUniform1fv(__VA_ARGS__))
#undef glUniform1i
#define glUniform1i(...)                                   GLCHECK(gl3wUniform1i(__VA_ARGS__))
#undef glUniform1iv
#define glUniform1iv(...)                                  GLCHECK(gl3wUniform1iv(__VA_ARGS__))
#undef glUniform1ui
#define glUniform1ui(...)                                  GLCHECK(gl3wUniform1ui(__VA_ARGS__))
#undef glUniform1uiv
#define glUniform1uiv(...)                                 GLCHECK(gl3wUniform1uiv(__VA_ARGS__))
#undef glUniform2d
#define glUniform2d(...)                                   GLCHECK(gl3wUniform2d(__VA_ARGS__))
#undef glUniform2dv
#define glUniform2dv(...)                                  GLCHECK(gl3wUniform2dv(__VA_ARGS__))
#undef glUniform2f
#define glUniform2f(...)                                   GLCHECK(gl3wUniform2f(__VA_ARGS__))
#undef glUniform2fv
#define glUniform2fv(...)                                  GLCHECK(gl3wUniform2fv(__VA_ARGS__))
#undef glUniform2i
#define glUniform2i(...)                                   GLCHECK(gl3wUniform2i(__VA_ARGS__))
#undef glUniform2iv
#define glUniform2iv(...)                                  GLCHECK(gl3wUniform2iv(__VA_ARGS__))
#undef glUniform2ui
#define glUniform2ui(...)                                  GLCHECK(gl3wUniform2ui(__VA_ARGS__))
#undef glUniform2uiv
#define glUniform2uiv(...)                                 GLCHECK(gl3wUniform2uiv(__VA_ARGS__))
#undef glUniform3d
#define glUniform3d(...)                                   GLCHECK(gl3wUniform3d(__VA_ARGS__))
#undef glUniform3dv
#define glUniform3dv(...)                                  GLCHECK(gl3wUniform3dv(__VA_ARGS__))
#undef glUniform3f
#define glUniform3f(...)                                   GLCHECK(gl3wUniform3f(__VA_ARGS__))
#undef glUniform3fv
#define glUniform3fv(...)                                  GLCHECK(gl3wUniform3fv(__VA_ARGS__))
#undef glUniform3i
#define glUniform3i(...)                                   GLCHECK(gl3wUniform3i(__VA_ARGS__))
#undef glUniform3iv
#define glUniform3iv(...)                                  GLCHECK(gl3wUniform3iv(__VA_ARGS__))
#undef glUniform3ui
#define glUniform3ui(...)                                  GLCHECK(gl3wUniform3ui(__VA_ARGS__))
#undef glUniform3uiv
#define glUniform3uiv(...)                                 GLCHECK(gl3wUniform3uiv(__VA_ARGS__))
#undef glUniform4d
#define glUniform4d(...)                                   GLCHECK(gl3wUniform4d(__VA_ARGS__))
#undef glUniform4dv
#define glUniform4dv(...)                                  GLCHECK(gl3wUniform4dv(__VA_ARGS__))
#undef glUniform4f
#define glUniform4f(...)                                   GLCHECK(gl3wUniform4f(__VA_ARGS__))
#undef glUniform4fv
#define glUniform4fv(...)                                  GLCHECK(gl3wUniform4fv(__VA_ARGS__))
#undef glUniform4i
#define glUniform4i(...)                                   GLCHECK(gl3wUniform4i(__VA_ARGS__))
#undef glUniform4iv
#define glUniform4iv(...)                                  GLCHECK(gl3wUniform4iv(__VA_ARGS__))
#undef glUniform4ui
#define glUniform4ui(...)                                  GLCHECK(gl3wUniform4ui(__VA_ARGS__))
#undef glUniform4uiv
#define glUniform4uiv(...)                                 GLCHECK(gl3wUniform4uiv(__VA_ARGS__))
#undef glUniformBlockBinding
#define glUniformBlockBinding(...)                         GLCHECK(gl3wUniformBlockBinding(__VA_ARGS__))
#undef glUniformHandleui64ARB
#define glUniformHandleui64ARB(...)                        GLCHECK(gl3wUniformHandleui64ARB(__VA_ARGS__))
#undef glUniformHandleui64vARB
#define glUniformHandleui64vARB(...)                       GLCHECK(gl3wUniformHandleui64vARB(__VA_ARGS__))
#undef glUniformMatrix2dv
#define glUniformMatrix2dv(...)                            GLCHECK(gl3wUniformMatrix2dv(__VA_ARGS__))
#undef glUniformMatrix2fv
#define glUniformMatrix2fv(...)                            GLCHECK(gl3wUniformMatrix2fv(__VA_ARGS__))
#undef glUniformMatrix2x3dv
#define glUniformMatrix2x3dv(...)                          GLCHECK(gl3wUniformMatrix2x3dv(__VA_ARGS__))
#undef glUniformMatrix2x3fv
#define glUniformMatrix2x3fv(...)                          GLCHECK(gl3wUniformMatrix2x3fv(__VA_ARGS__))
#undef glUniformMatrix2x4dv
#define glUniformMatrix2x4dv(...)                          GLCHECK(gl3wUniformMatrix2x4dv(__VA_ARGS__))
#undef glUniformMatrix2x4fv
#define glUniformMatrix2x4fv(...)                          GLCHECK(gl3wUniformMatrix2x4fv(__VA_ARGS__))
#undef glUniformMatrix3dv
#define glUniformMatrix3dv(...)                            GLCHECK(gl3wUniformMatrix3dv(__VA_ARGS__))
#undef glUniformMatrix3fv
#define glUniformMatrix3fv(...)                            GLCHECK(gl3wUniformMatrix3fv(__VA_ARGS__))
#undef glUniformMatrix3x2dv
#define glUniformMatrix3x2dv(...)                          GLCHECK(gl3wUniformMatrix3x2dv(__VA_ARGS__))
#undef glUniformMatrix3x2fv
#define glUniformMatrix3x2fv(...)                          GLCHECK(gl3wUniformMatrix3x2fv(__VA_ARGS__))
#undef glUniformMatrix3x4dv
#define glUniformMatrix3x4dv(...)                          GLCHECK(gl3wUniformMatrix3x4dv(__VA_ARGS__))
#undef glUniformMatrix3x4fv
#define glUniformMatrix3x4fv(...)                          GLCHECK(gl3wUniformMatrix3x4fv(__VA_ARGS__))
#undef glUniformMatrix4dv
#define glUniformMatrix4dv(...)                            GLCHECK(gl3wUniformMatrix4dv(__VA_ARGS__))
#undef glUniformMatrix4fv
#define glUniformMatrix4fv(...)                            GLCHECK(gl3wUniformMatrix4fv(__VA_ARGS__))
#undef glUniformMatrix4x2dv
#define glUniformMatrix4x2dv(...)                          GLCHECK(gl3wUniformMatrix4x2dv(__VA_ARGS__))
#undef glUniformMatrix4x2fv
#define glUniformMatrix4x2fv(...)                          GLCHECK(gl3wUniformMatrix4x2fv(__VA_ARGS__))
#undef glUniformMatrix4x3dv
#define glUniformMatrix4x3dv(...)                          GLCHECK(gl3wUniformMatrix4x3dv(__VA_ARGS__))
#undef glUniformMatrix4x3fv
#define glUniformMatrix4x3fv(...)                          GLCHECK(gl3wUniformMatrix4x3fv(__VA_ARGS__))
#undef glUniformSubroutinesuiv
#define glUniformSubroutinesuiv(...)                       GLCHECK(gl3wUniformSubroutinesuiv(__VA_ARGS__))
#undef glUnmapBuffer
#define glUnmapBuffer(...)                                 GLCHECK(gl3wUnmapBuffer(__VA_ARGS__))
#undef glUnmapNamedBuffer
#define glUnmapNamedBuffer(...)                            GLCHECK(gl3wUnmapNamedBuffer(__VA_ARGS__))
#undef glUseProgram
#define glUseProgram(...)                                  GLCHECK(gl3wUseProgram(__VA_ARGS__))
#undef glUseProgramStages
#define glUseProgramStages(...)                            GLCHECK(gl3wUseProgramStages(__VA_ARGS__))
#undef glValidateProgram
#define glValidateProgram(...)                             GLCHECK(gl3wValidateProgram(__VA_ARGS__))
#undef glValidateProgramPipeline
#define glValidateProgramPipeline(...)                     GLCHECK(gl3wValidateProgramPipeline(__VA_ARGS__))
#undef glVertexArrayAttribBinding
#define glVertexArrayAttribBinding(...)                    GLCHECK(gl3wVertexArrayAttribBinding(__VA_ARGS__))
#undef glVertexArrayAttribFormat
#define glVertexArrayAttribFormat(...)                     GLCHECK(gl3wVertexArrayAttribFormat(__VA_ARGS__))
#undef glVertexArrayAttribIFormat
#define glVertexArrayAttribIFormat(...)                    GLCHECK(gl3wVertexArrayAttribIFormat(__VA_ARGS__))
#undef glVertexArrayAttribLFormat
#define glVertexArrayAttribLFormat(...)                    GLCHECK(gl3wVertexArrayAttribLFormat(__VA_ARGS__))
#undef glVertexArrayBindingDivisor
#define glVertexArrayBindingDivisor(...)                   GLCHECK(gl3wVertexArrayBindingDivisor(__VA_ARGS__))
#undef glVertexArrayElementBuffer
#define glVertexArrayElementBuffer(...)                    GLCHECK(gl3wVertexArrayElementBuffer(__VA_ARGS__))
#undef glVertexArrayVertexBuffer
#define glVertexArrayVertexBuffer(...)                     GLCHECK(gl3wVertexArrayVertexBuffer(__VA_ARGS__))
#undef glVertexArrayVertexBuffers
#define glVertexArrayVertexBuffers(...)                    GLCHECK(gl3wVertexArrayVertexBuffers(__VA_ARGS__))
#undef glVertexAttrib1d
#define glVertexAttrib1d(...)                              GLCHECK(gl3wVertexAttrib1d(__VA_ARGS__))
#undef glVertexAttrib1dv
#define glVertexAttrib1dv(...)                             GLCHECK(gl3wVertexAttrib1dv(__VA_ARGS__))
#undef glVertexAttrib1f
#define glVertexAttrib1f(...)                              GLCHECK(gl3wVertexAttrib1f(__VA_ARGS__))
#undef glVertexAttrib1fv
#define glVertexAttrib1fv(...)                             GLCHECK(gl3wVertexAttrib1fv(__VA_ARGS__))
#undef glVertexAttrib1s
#define glVertexAttrib1s(...)                              GLCHECK(gl3wVertexAttrib1s(__VA_ARGS__))
#undef glVertexAttrib1sv
#define glVertexAttrib1sv(...)                             GLCHECK(gl3wVertexAttrib1sv(__VA_ARGS__))
#undef glVertexAttrib2d
#define glVertexAttrib2d(...)                              GLCHECK(gl3wVertexAttrib2d(__VA_ARGS__))
#undef glVertexAttrib2dv
#define glVertexAttrib2dv(...)                             GLCHECK(gl3wVertexAttrib2dv(__VA_ARGS__))
#undef glVertexAttrib2f
#define glVertexAttrib2f(...)                              GLCHECK(gl3wVertexAttrib2f(__VA_ARGS__))
#undef glVertexAttrib2fv
#define glVertexAttrib2fv(...)                             GLCHECK(gl3wVertexAttrib2fv(__VA_ARGS__))
#undef glVertexAttrib2s
#define glVertexAttrib2s(...)                              GLCHECK(gl3wVertexAttrib2s(__VA_ARGS__))
#undef glVertexAttrib2sv
#define glVertexAttrib2sv(...)                             GLCHECK(gl3wVertexAttrib2sv(__VA_ARGS__))
#undef glVertexAttrib3d
#define glVertexAttrib3d(...)                              GLCHECK(gl3wVertexAttrib3d(__VA_ARGS__))
#undef glVertexAttrib3dv
#define glVertexAttrib3dv(...)                             GLCHECK(gl3wVertexAttrib3dv(__VA_ARGS__))
#undef glVertexAttrib3f
#define glVertexAttrib3f(...)                              GLCHECK(gl3wVertexAttrib3f(__VA_ARGS__))
#undef glVertexAttrib3fv
#define glVertexAttrib3fv(...)                             GLCHECK(gl3wVertexAttrib3fv(__VA_ARGS__))
#undef glVertexAttrib3s
#define glVertexAttrib3s(...)                              GLCHECK(gl3wVertexAttrib3s(__VA_ARGS__))
#undef glVertexAttrib3sv
#define glVertexAttrib3sv(...)                             GLCHECK(gl3wVertexAttrib3sv(__VA_ARGS__))
#undef glVertexAttrib4Nbv
#define glVertexAttrib4Nbv(...)                            GLCHECK(gl3wVertexAttrib4Nbv(__VA_ARGS__))
#undef glVertexAttrib4Niv
#define glVertexAttrib4Niv(...)                            GLCHECK(gl3wVertexAttrib4Niv(__VA_ARGS__))
#undef glVertexAttrib4Nsv
#define glVertexAttrib4Nsv(...)                            GLCHECK(gl3wVertexAttrib4Nsv(__VA_ARGS__))
#undef glVertexAttrib4Nub
#define glVertexAttrib4Nub(...)                            GLCHECK(gl3wVertexAttrib4Nub(__VA_ARGS__))
#undef glVertexAttrib4Nubv
#define glVertexAttrib4Nubv(...)                           GLCHECK(gl3wVertexAttrib4Nubv(__VA_ARGS__))
#undef glVertexAttrib4Nuiv
#define glVertexAttrib4Nuiv(...)                           GLCHECK(gl3wVertexAttrib4Nuiv(__VA_ARGS__))
#undef glVertexAttrib4Nusv
#define glVertexAttrib4Nusv(...)                           GLCHECK(gl3wVertexAttrib4Nusv(__VA_ARGS__))
#undef glVertexAttrib4bv
#define glVertexAttrib4bv(...)                             GLCHECK(gl3wVertexAttrib4bv(__VA_ARGS__))
#undef glVertexAttrib4d
#define glVertexAttrib4d(...)                              GLCHECK(gl3wVertexAttrib4d(__VA_ARGS__))
#undef glVertexAttrib4dv
#define glVertexAttrib4dv(...)                             GLCHECK(gl3wVertexAttrib4dv(__VA_ARGS__))
#undef glVertexAttrib4f
#define glVertexAttrib4f(...)                              GLCHECK(gl3wVertexAttrib4f(__VA_ARGS__))
#undef glVertexAttrib4fv
#define glVertexAttrib4fv(...)                             GLCHECK(gl3wVertexAttrib4fv(__VA_ARGS__))
#undef glVertexAttrib4iv
#define glVertexAttrib4iv(...)                             GLCHECK(gl3wVertexAttrib4iv(__VA_ARGS__))
#undef glVertexAttrib4s
#define glVertexAttrib4s(...)                              GLCHECK(gl3wVertexAttrib4s(__VA_ARGS__))
#undef glVertexAttrib4sv
#define glVertexAttrib4sv(...)                             GLCHECK(gl3wVertexAttrib4sv(__VA_ARGS__))
#undef glVertexAttrib4ubv
#define glVertexAttrib4ubv(...)                            GLCHECK(gl3wVertexAttrib4ubv(__VA_ARGS__))
#undef glVertexAttrib4uiv
#define glVertexAttrib4uiv(...)                            GLCHECK(gl3wVertexAttrib4uiv(__VA_ARGS__))
#undef glVertexAttrib4usv
#define glVertexAttrib4usv(...)                            GLCHECK(gl3wVertexAttrib4usv(__VA_ARGS__))
#undef glVertexAttribBinding
#define glVertexAttribBinding(...)                         GLCHECK(gl3wVertexAttribBinding(__VA_ARGS__))
#undef glVertexAttribDivisor
#define glVertexAttribDivisor(...)                         GLCHECK(gl3wVertexAttribDivisor(__VA_ARGS__))
#undef glVertexAttribFormat
#define glVertexAttribFormat(...)                          GLCHECK(gl3wVertexAttribFormat(__VA_ARGS__))
#undef glVertexAttribI1i
#define glVertexAttribI1i(...)                             GLCHECK(gl3wVertexAttribI1i(__VA_ARGS__))
#undef glVertexAttribI1iv
#define glVertexAttribI1iv(...)                            GLCHECK(gl3wVertexAttribI1iv(__VA_ARGS__))
#undef glVertexAttribI1ui
#define glVertexAttribI1ui(...)                            GLCHECK(gl3wVertexAttribI1ui(__VA_ARGS__))
#undef glVertexAttribI1uiv
#define glVertexAttribI1uiv(...)                           GLCHECK(gl3wVertexAttribI1uiv(__VA_ARGS__))
#undef glVertexAttribI2i
#define glVertexAttribI2i(...)                             GLCHECK(gl3wVertexAttribI2i(__VA_ARGS__))
#undef glVertexAttribI2iv
#define glVertexAttribI2iv(...)                            GLCHECK(gl3wVertexAttribI2iv(__VA_ARGS__))
#undef glVertexAttribI2ui
#define glVertexAttribI2ui(...)                            GLCHECK(gl3wVertexAttribI2ui(__VA_ARGS__))
#undef glVertexAttribI2uiv
#define glVertexAttribI2uiv(...)                           GLCHECK(gl3wVertexAttribI2uiv(__VA_ARGS__))
#undef glVertexAttribI3i
#define glVertexAttribI3i(...)                             GLCHECK(gl3wVertexAttribI3i(__VA_ARGS__))
#undef glVertexAttribI3iv
#define glVertexAttribI3iv(...)                            GLCHECK(gl3wVertexAttribI3iv(__VA_ARGS__))
#undef glVertexAttribI3ui
#define glVertexAttribI3ui(...)                            GLCHECK(gl3wVertexAttribI3ui(__VA_ARGS__))
#undef glVertexAttribI3uiv
#define glVertexAttribI3uiv(...)                           GLCHECK(gl3wVertexAttribI3uiv(__VA_ARGS__))
#undef glVertexAttribI4bv
#define glVertexAttribI4bv(...)                            GLCHECK(gl3wVertexAttribI4bv(__VA_ARGS__))
#undef glVertexAttribI4i
#define glVertexAttribI4i(...)                             GLCHECK(gl3wVertexAttribI4i(__VA_ARGS__))
#undef glVertexAttribI4iv
#define glVertexAttribI4iv(...)                            GLCHECK(gl3wVertexAttribI4iv(__VA_ARGS__))
#undef glVertexAttribI4sv
#define glVertexAttribI4sv(...)                            GLCHECK(gl3wVertexAttribI4sv(__VA_ARGS__))
#undef glVertexAttribI4ubv
#define glVertexAttribI4ubv(...)                           GLCHECK(gl3wVertexAttribI4ubv(__VA_ARGS__))
#undef glVertexAttribI4ui
#define glVertexAttribI4ui(...)                            GLCHECK(gl3wVertexAttribI4ui(__VA_ARGS__))
#undef glVertexAttribI4uiv
#define glVertexAttribI4uiv(...)                           GLCHECK(gl3wVertexAttribI4uiv(__VA_ARGS__))
#undef glVertexAttribI4usv
#define glVertexAttribI4usv(...)                           GLCHECK(gl3wVertexAttribI4usv(__VA_ARGS__))
#undef glVertexAttribIFormat
#define glVertexAttribIFormat(...)                         GLCHECK(gl3wVertexAttribIFormat(__VA_ARGS__))
#undef glVertexAttribIPointer
#define glVertexAttribIPointer(...)                        GLCHECK(gl3wVertexAttribIPointer(__VA_ARGS__))
#undef glVertexAttribL1d
#define glVertexAttribL1d(...)                             GLCHECK(gl3wVertexAttribL1d(__VA_ARGS__))
#undef glVertexAttribL1dv
#define glVertexAttribL1dv(...)                            GLCHECK(gl3wVertexAttribL1dv(__VA_ARGS__))
#undef glVertexAttribL1ui64ARB
#define glVertexAttribL1ui64ARB(...)                       GLCHECK(gl3wVertexAttribL1ui64ARB(__VA_ARGS__))
#undef glVertexAttribL1ui64vARB
#define glVertexAttribL1ui64vARB(...)                      GLCHECK(gl3wVertexAttribL1ui64vARB(__VA_ARGS__))
#undef glVertexAttribL2d
#define glVertexAttribL2d(...)                             GLCHECK(gl3wVertexAttribL2d(__VA_ARGS__))
#undef glVertexAttribL2dv
#define glVertexAttribL2dv(...)                            GLCHECK(gl3wVertexAttribL2dv(__VA_ARGS__))
#undef glVertexAttribL3d
#define glVertexAttribL3d(...)                             GLCHECK(gl3wVertexAttribL3d(__VA_ARGS__))
#undef glVertexAttribL3dv
#define glVertexAttribL3dv(...)                            GLCHECK(gl3wVertexAttribL3dv(__VA_ARGS__))
#undef glVertexAttribL4d
#define glVertexAttribL4d(...)                             GLCHECK(gl3wVertexAttribL4d(__VA_ARGS__))
#undef glVertexAttribL4dv
#define glVertexAttribL4dv(...)                            GLCHECK(gl3wVertexAttribL4dv(__VA_ARGS__))
#undef glVertexAttribLFormat
#define glVertexAttribLFormat(...)                         GLCHECK(gl3wVertexAttribLFormat(__VA_ARGS__))
#undef glVertexAttribLPointer
#define glVertexAttribLPointer(...)                        GLCHECK(gl3wVertexAttribLPointer(__VA_ARGS__))
#undef glVertexAttribP1ui
#define glVertexAttribP1ui(...)                            GLCHECK(gl3wVertexAttribP1ui(__VA_ARGS__))
#undef glVertexAttribP1uiv
#define glVertexAttribP1uiv(...)                           GLCHECK(gl3wVertexAttribP1uiv(__VA_ARGS__))
#undef glVertexAttribP2ui
#define glVertexAttribP2ui(...)                            GLCHECK(gl3wVertexAttribP2ui(__VA_ARGS__))
#undef glVertexAttribP2uiv
#define glVertexAttribP2uiv(...)                           GLCHECK(gl3wVertexAttribP2uiv(__VA_ARGS__))
#undef glVertexAttribP3ui
#define glVertexAttribP3ui(...)                            GLCHECK(gl3wVertexAttribP3ui(__VA_ARGS__))
#undef glVertexAttribP3uiv
#define glVertexAttribP3uiv(...)                           GLCHECK(gl3wVertexAttribP3uiv(__VA_ARGS__))
#undef glVertexAttribP4ui
#define glVertexAttribP4ui(...)                            GLCHECK(gl3wVertexAttribP4ui(__VA_ARGS__))
#undef glVertexAttribP4uiv
#define glVertexAttribP4uiv(...)                           GLCHECK(gl3wVertexAttribP4uiv(__VA_ARGS__))
#undef glVertexAttribPointer
#define glVertexAttribPointer(...)                         GLCHECK(gl3wVertexAttribPointer(__VA_ARGS__))
#undef glVertexBindingDivisor
#define glVertexBindingDivisor(...)                        GLCHECK(gl3wVertexBindingDivisor(__VA_ARGS__))
#undef glViewport
#define glViewport(...)                                    GLCHECK(gl3wViewport(__VA_ARGS__))
#undef glViewportArrayv
#define glViewportArrayv(...)                              GLCHECK(gl3wViewportArrayv(__VA_ARGS__))
#undef glViewportIndexedf
#define glViewportIndexedf(...)                            GLCHECK(gl3wViewportIndexedf(__VA_ARGS__))
#undef glViewportIndexedfv
#define glViewportIndexedfv(...)                           GLCHECK(gl3wViewportIndexedfv(__VA_ARGS__))
#undef glWaitSync
#define glWaitSync(...)                                    GLCHECK(gl3wWaitSync(__VA_ARGS__))

#endif
