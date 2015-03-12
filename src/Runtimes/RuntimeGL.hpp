#pragma once

#include "Runtime.hpp"

#include <atomic>
#include <gl\gl3w.h>

namespace ReShade
{
	namespace Runtimes
	{
		struct GLRuntime : public Runtime, public std::enable_shared_from_this<GLRuntime>
		{
			friend struct GLTechnique;

			struct DepthSourceInfo
			{
				GLint Width, Height, Level, Format;
				GLfloat DrawCallCount, DrawVerticesCount;
			};

			GLRuntime(HDC device, HGLRC context);
			~GLRuntime();

			bool OnCreateInternal(unsigned int width, unsigned int height);
			void OnDeleteInternal();
			void OnDrawInternal(unsigned int vertices);
			void OnPresentInternal();
			void OnFramebufferAttachment(GLenum target, GLenum attachment, GLenum objecttarget, GLuint object, GLint level);

			void DetectDepthSource();
			void CreateDepthTexture(GLuint width, GLuint height, GLenum format);

			virtual std::unique_ptr<FX::Effect> CompileEffect(const FX::Tree &ast, std::string &errors) const override;
			virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const override;

			HDC mDeviceContext;
			HGLRC mRenderContext;
			std::atomic<GLuint> mReferenceCount;
			GLuint mCurrentVertexCount;
			std::unique_ptr<class GLStateBlock> mStateBlock;
			GLuint mDefaultBackBufferFBO, mDefaultBackBufferRBO[2], mBackBufferTexture[2];
			GLuint mDepthSourceFBO, mDepthSource, mDepthTexture, mBlitFBO;
			std::unordered_map<GLuint, DepthSourceInfo> mDepthSourceTable;
			bool mLost, mPresenting;
		};

		struct GLEffect : public FX::Effect
		{
			friend struct GLRuntime;

			GLEffect(std::shared_ptr<const GLRuntime> runtime);
			~GLEffect();

			inline bool AddTexture(const std::string &name, Texture *texture)
			{
				return this->mTextures.emplace(name, std::unique_ptr<Texture>(texture)).second;
			}
			inline bool AddConstant(const std::string &name, Constant *constant)
			{
				return this->mConstants.emplace(name, std::unique_ptr<Constant>(constant)).second;
			}
			inline bool AddTechnique(const std::string &name, Technique *technique)
			{
				return this->mTechniques.emplace(name, std::unique_ptr<Technique>(technique)).second;
			}

			virtual void Enter() const override;
			virtual void Leave() const override;

			std::shared_ptr<const GLRuntime> mRuntime;
			std::vector<struct GLSampler> mSamplers;
			GLuint mDefaultVAO, mDefaultVBO, mUBO;
			GLubyte *mUniformStorage;
			std::size_t mUniformStorageSize;
			mutable bool mUniformDirty;
		};
		struct GLTexture : public FX::Effect::Texture
		{
			enum class Source
			{
				Memory,
				BackBuffer,
				DepthStencil
			};

			GLTexture(GLEffect *effect, const Description &desc);
			~GLTexture();

			inline bool AddAnnotation(const std::string &name, const FX::Effect::Annotation &value)
			{
				return this->mAnnotations.emplace(name, value).second;
			}

			virtual bool Update(unsigned int level, const unsigned char *data, std::size_t size) override;
			void ChangeSource(GLuint texture, GLuint textureSRGB);

			GLEffect *mEffect;
			Source mSource;
			GLuint mID[2];
		};
		struct GLSampler
		{
			GLuint mID;
			GLTexture *mTexture;
			bool mSRGB;
		};
		struct GLConstant : public FX::Effect::Constant
		{
			GLConstant(GLEffect *effect, const Description &desc);
			~GLConstant();

			inline bool AddAnnotation(const std::string &name, const FX::Effect::Annotation &value)
			{
				return this->mAnnotations.emplace(name, value).second;
			}

			virtual void GetValue(unsigned char *data, std::size_t size) const override;
			virtual void SetValue(const unsigned char *data, std::size_t size) override;

			GLEffect *mEffect;
		};
		struct GLTechnique : public FX::Effect::Technique
		{
			struct Pass
			{
				GLuint Program;
				GLuint Framebuffer, DrawTextures[8];
				GLint StencilRef;
				GLuint StencilMask, StencilReadMask;
				GLsizei ViewportWidth, ViewportHeight;
				GLenum DrawBuffers[8], BlendEqColor, BlendEqAlpha, BlendFuncSrc, BlendFuncDest, DepthFunc, StencilFunc, StencilOpFail, StencilOpZFail, StencilOpZPass;
				GLboolean FramebufferSRGB, Blend, DepthMask, DepthTest, StencilTest, ColorMaskR, ColorMaskG, ColorMaskB, ColorMaskA;
			};

			GLTechnique(GLEffect *effect, const Description &desc);
			~GLTechnique();

			inline bool AddAnnotation(const std::string &name, const FX::Effect::Annotation &value)
			{
				return this->mAnnotations.emplace(name, value).second;
			}
			inline void AddPass(const Pass &pass)
			{
				this->mPasses.push_back(pass);
			}

			virtual void RenderPass(unsigned int index) const override;

			GLEffect *mEffect;
			std::vector<Pass> mPasses;
		};
	}
}