#pragma once

#include "Runtime.hpp"
#include "Effect.hpp"

#include <gl\gl3w.h>
#include <vector>
#include <unordered_map>

#ifdef _DEBUG
	#define GLCHECK(call) { glGetError(); call; GLenum __e = glGetError(); if (__e != GL_NO_ERROR) { char __m[1024]; sprintf_s(__m, "OpenGL Error %x at line %d: %s", __e, __LINE__, #call); MessageBoxA(nullptr, __m, 0, MB_ICONERROR); } }
#else
	#define GLCHECK(call) call
#endif

namespace ReShade
{
	struct GLStateBlock
	{
		GLStateBlock()
		{
			ZeroMemory(this, sizeof(this));
		}

		void Capture()
		{
			GLCHECK(glGetIntegerv(GL_VIEWPORT, this->mViewport));
			GLCHECK(this->mStencilTest = glIsEnabled(GL_STENCIL_TEST));
			GLCHECK(this->mScissorTest = glIsEnabled(GL_SCISSOR_TEST));
			GLCHECK(glGetIntegerv(GL_FRONT_FACE, reinterpret_cast<GLint *>(&this->mFrontFace)));
			GLCHECK(glGetIntegerv(GL_POLYGON_MODE, reinterpret_cast<GLint *>(&this->mPolygonMode)));
			GLCHECK(this->mCullFace = glIsEnabled(GL_CULL_FACE));
			GLCHECK(glGetIntegerv(GL_CULL_FACE_MODE, reinterpret_cast<GLint *>(&this->mCullFaceMode)));
			GLCHECK(glGetBooleanv(GL_COLOR_WRITEMASK, this->mColorMask));
			GLCHECK(this->mFramebufferSRGB = glIsEnabled(GL_FRAMEBUFFER_SRGB));
			GLCHECK(this->mBlend = glIsEnabled(GL_BLEND));
			GLCHECK(glGetIntegerv(GL_BLEND_SRC, reinterpret_cast<GLint *>(&this->mBlendFuncSrc)));
			GLCHECK(glGetIntegerv(GL_BLEND_DST, reinterpret_cast<GLint *>(&this->mBlendFuncDest)));
			GLCHECK(glGetIntegerv(GL_BLEND_EQUATION_RGB, reinterpret_cast<GLint *>(&this->mBlendEqColor)));
			GLCHECK(glGetIntegerv(GL_BLEND_EQUATION_ALPHA, reinterpret_cast<GLint *>(&this->mBlendEqAlpha)));
			GLCHECK(this->mDepthTest = glIsEnabled(GL_DEPTH_TEST));
			GLCHECK(glGetBooleanv(GL_DEPTH_WRITEMASK, &this->mDepthMask));
			GLCHECK(glGetIntegerv(GL_DEPTH_FUNC, reinterpret_cast<GLint *>(&this->mDepthFunc)));
			GLCHECK(glGetIntegerv(GL_STENCIL_VALUE_MASK, reinterpret_cast<GLint *>(&this->mStencilReadMask)));
			GLCHECK(glGetIntegerv(GL_STENCIL_WRITEMASK, reinterpret_cast<GLint *>(&this->mStencilMask)));
			GLCHECK(glGetIntegerv(GL_STENCIL_FUNC, reinterpret_cast<GLint *>(&this->mStencilFunc)));
			GLCHECK(glGetIntegerv(GL_STENCIL_FAIL, reinterpret_cast<GLint *>(&this->mStencilOpFail)));
			GLCHECK(glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, reinterpret_cast<GLint *>(&this->mStencilOpZFail)));
			GLCHECK(glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, reinterpret_cast<GLint *>(&this->mStencilOpZPass)));
			GLCHECK(glGetIntegerv(GL_STENCIL_REF, &this->mStencilRef));
			GLCHECK(glGetIntegerv(GL_ACTIVE_TEXTURE, reinterpret_cast<GLint *>(&this->mActiveTexture)));

			for (GLuint i = 0; i < 8; ++i)
			{
				glGetIntegerv(GL_DRAW_BUFFER0 + i, reinterpret_cast<GLint *>(&this->mDrawBuffers[i]));
			}

			GLCHECK(glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint *>(&this->mProgram)));
			GLCHECK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&this->mFBO)));
			GLCHECK(glGetIntegerv(GL_VERTEX_ARRAY_BINDING, reinterpret_cast<GLint *>(&this->mVAO)));
			GLCHECK(glGetIntegerv(GL_ARRAY_BUFFER_BINDING, reinterpret_cast<GLint *>(&this->mVBO)));
			GLCHECK(glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, reinterpret_cast<GLint *>(&this->mUBO)));
				
			for (GLuint i = 0; i < ARRAYSIZE(this->mTextures2D); ++i)
			{
				glActiveTexture(GL_TEXTURE0 + i);
				glGetIntegerv(GL_TEXTURE_BINDING_2D, reinterpret_cast<GLint *>(&this->mTextures2D[i]));
				glGetIntegerv(GL_SAMPLER_BINDING, reinterpret_cast<GLint *>(&this->mSamplers[i]));
			}
		}
		void Apply() const
		{
			GLCHECK(glUseProgram(glIsProgram(this->mProgram) ? this->mProgram : 0));
			GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, glIsFramebuffer(this->mFBO) ? this->mFBO : 0));
			GLCHECK(glBindVertexArray(glIsVertexArray(this->mVAO) ? this->mVAO : 0));
			GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, glIsBuffer(this->mVBO) ? this->mVBO : 0));
			GLCHECK(glBindBuffer(GL_UNIFORM_BUFFER, glIsBuffer(this->mUBO) ? this->mUBO : 0));

			for (GLuint i = 0; i < ARRAYSIZE(this->mTextures2D); ++i)
			{
				GLCHECK(glActiveTexture(GL_TEXTURE0 + i));
				GLCHECK(glBindTexture(GL_TEXTURE_2D, glIsTexture(this->mTextures2D[i]) ? this->mTextures2D[i] : 0));
				GLCHECK(glBindSampler(i, glIsSampler(this->mSamplers[i]) ? this->mSamplers[i] : 0));
			}

#define glEnableb(cap, value) if ((value)) glEnable(cap); else glDisable(cap);

			GLCHECK(glViewport(this->mViewport[0], this->mViewport[1], this->mViewport[2], this->mViewport[3]));
			GLCHECK(glEnableb(GL_STENCIL_TEST, this->mStencilTest));
			GLCHECK(glEnableb(GL_SCISSOR_TEST, this->mScissorTest));
			GLCHECK(glFrontFace(this->mFrontFace));
			GLCHECK(glPolygonMode(GL_FRONT_AND_BACK, this->mPolygonMode));
			GLCHECK(glEnableb(GL_CULL_FACE, this->mCullFace));
			GLCHECK(glCullFace(this->mCullFaceMode));
			GLCHECK(glColorMask(this->mColorMask[0], this->mColorMask[1], this->mColorMask[2], this->mColorMask[3]));
			GLCHECK(glEnableb(GL_FRAMEBUFFER_SRGB, this->mFramebufferSRGB));
			GLCHECK(glEnableb(GL_BLEND, this->mBlend));
			GLCHECK(glBlendFunc(this->mBlendFuncSrc, this->mBlendFuncDest));
			GLCHECK(glBlendEquationSeparate(this->mBlendEqColor, this->mBlendEqAlpha));
			GLCHECK(glEnableb(GL_DEPTH_TEST, this->mDepthTest));
			GLCHECK(glDepthMask(this->mDepthMask));
			GLCHECK(glDepthFunc(this->mDepthFunc));
			GLCHECK(glStencilMask(this->mStencilMask));
			GLCHECK(glStencilFunc(this->mStencilFunc, this->mStencilRef, this->mStencilReadMask));
			GLCHECK(glStencilOp(this->mStencilOpFail, this->mStencilOpZFail, this->mStencilOpZPass));
			GLCHECK(glActiveTexture(this->mActiveTexture));

			if (this->mDrawBuffers[1] == GL_NONE &&
				this->mDrawBuffers[2] == GL_NONE &&
				this->mDrawBuffers[3] == GL_NONE &&
				this->mDrawBuffers[4] == GL_NONE &&
				this->mDrawBuffers[5] == GL_NONE &&
				this->mDrawBuffers[6] == GL_NONE &&
				this->mDrawBuffers[7] == GL_NONE)
			{
				glDrawBuffer(this->mDrawBuffers[0]);
			}
			else
			{
				glDrawBuffers(8, this->mDrawBuffers);
			}
		}

		GLint mStencilRef, mViewport[4];
		GLuint mStencilMask, mStencilReadMask;
		GLuint mProgram, mFBO, mVAO, mVBO, mUBO, mTextures2D[8], mSamplers[8];
		GLenum mDrawBuffers[8], mCullFace, mCullFaceMode, mPolygonMode, mBlendEqColor, mBlendEqAlpha, mBlendFuncSrc, mBlendFuncDest, mDepthFunc, mStencilFunc, mStencilOpFail, mStencilOpZFail, mStencilOpZPass, mFrontFace, mActiveTexture;
		GLboolean mScissorTest, mBlend, mDepthTest, mDepthMask, mStencilTest, mColorMask[4], mFramebufferSRGB;
	};

	struct GLFramebufferInfo
	{
		GLfloat DrawCallCount, DrawVerticesCount;
		GLuint DepthStencilName;
		GLenum DepthStencilTarget;
		GLint DepthStencilWidth, DepthStencilHeight, DepthStencilFormat;
	};
	struct GLRuntime : public Runtime, public std::enable_shared_from_this<GLRuntime>
	{
		friend struct GLEffect;
		friend struct GLTexture;
		friend struct GLSampler;
		friend struct GLConstant;
		friend struct GLTechnique;

		GLRuntime(HDC device, HGLRC context);
		~GLRuntime();

		bool OnCreateInternal(unsigned int width, unsigned int height);
		void OnDeleteInternal();
		void OnDrawInternal(unsigned int vertices);
		void OnPresentInternal();

		virtual std::unique_ptr<Effect> CreateEffect(const EffectTree &ast, std::string &errors) const override;
		virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const override;

		void CreateDepthStencil(GLenum target, GLenum objecttarget, GLuint object, GLint level);
		void DetectBestDepthStencil();

		HDC mDeviceContext;
		HGLRC mRenderContext;
		GLStateBlock mStateBlock;
		GLuint mBackBufferFBO, mBackBufferRBO, mDepthStencilFBO, mDepthStencilTexture, mBlitFBO;
		std::unordered_map<GLuint, GLFramebufferInfo> mFramebufferTable;
		bool mLost, mPresenting;
	};
	struct GLEffect : public Effect
	{
		friend struct GLTexture;
		friend struct GLSampler;
		friend struct GLConstant;
		friend struct GLTechnique;

		GLEffect(std::shared_ptr<const GLRuntime> context);
		~GLEffect();

		virtual const Texture *GetTexture(const std::string &name) const override;
		virtual std::vector<std::string> GetTextureNames() const override;
		virtual const Constant *GetConstant(const std::string &name) const override;
		virtual std::vector<std::string> GetConstantNames() const override;
		virtual const Technique *GetTechnique(const std::string &name) const override;
		virtual std::vector<std::string> GetTechniqueNames() const override;

		void UpdateConstants() const;

		std::shared_ptr<const GLRuntime> mEffectContext;
		std::unordered_map<std::string, std::unique_ptr<GLTexture>> mTextures;
		std::vector<std::shared_ptr<GLSampler>> mSamplers;
		std::unordered_map<std::string, std::unique_ptr<GLConstant>> mConstants;
		std::unordered_map<std::string, std::unique_ptr<GLTechnique>> mTechniques;
		GLuint mDefaultVAO, mDefaultVBO, mDepthStencil;
		std::vector<GLuint> mUniformBuffers;
		std::vector<std::pair<unsigned char *, std::size_t>> mUniformStorages;
		mutable bool mUniformDirty;
	};
	struct GLTexture : public Effect::Texture
	{
		GLTexture(GLEffect *effect, const Description &desc);
		~GLTexture();

		inline bool AddAnnotation(const std::string &name, const Effect::Annotation &value)
		{
			return this->mAnnotations.emplace(name, value).second;
		}

		virtual bool Update(unsigned int level, const unsigned char *data, std::size_t size) override;
		virtual void UpdateFromColorBuffer() override;
		virtual void UpdateFromDepthBuffer() override;

		GLEffect *mEffect;
		GLuint mID[2];
		bool mNoDelete;
	};
	struct GLSampler
	{
		GLSampler() : mID(0)
		{
		}
		~GLSampler()
		{
			glDeleteSamplers(1, &this->mID);
		}

		GLuint mID;
		GLTexture *mTexture;
		bool mSRGB;
	};
	struct GLConstant : public Effect::Constant
	{
		GLConstant(GLEffect *effect, const Description &desc);
		~GLConstant();

		inline bool AddAnnotation(const std::string &name, const Effect::Annotation &value)
		{
			return this->mAnnotations.emplace(name, value).second;
		}

		virtual void GetValue(unsigned char *data, std::size_t size) const override;
		virtual void SetValue(const unsigned char *data, std::size_t size) override;

		GLEffect *mEffect;
		std::size_t mBuffer, mBufferOffset;
	};
	struct GLTechnique : public Effect::Technique
	{
		struct Pass
		{
			GLuint Program;
			GLuint Framebuffer;
			GLint StencilRef;
			GLuint StencilMask, StencilReadMask;
			GLsizei ViewportWidth, ViewportHeight;
			GLenum DrawBuffers[8], BlendEqColor, BlendEqAlpha, BlendFuncSrc, BlendFuncDest, DepthFunc, StencilFunc, StencilOpFail, StencilOpZFail, StencilOpZPass;
			GLboolean FramebufferSRGB, Blend, DepthMask, DepthTest, StencilTest, ColorMaskR, ColorMaskG, ColorMaskB, ColorMaskA;
		};

		GLTechnique(GLEffect *effect);
		~GLTechnique();

		inline bool AddAnnotation(const std::string &name, const Effect::Annotation &value)
		{
			return this->mAnnotations.emplace(name, value).second;
		}
		inline void AddPass(const Pass &pass)
		{
			this->mPasses.push_back(pass);
		}

		virtual bool Begin(unsigned int &passes) const override;
		virtual void End() const override;
		virtual void RenderPass(unsigned int index) const override;

		GLEffect *mEffect;
		std::vector<Pass> mPasses;
	};
}