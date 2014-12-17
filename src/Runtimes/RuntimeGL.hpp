#pragma once

#include "Runtime.hpp"

#include <gl\gl3w.h>
#include <vector>
#include <unordered_map>

namespace ReShade { namespace Runtimes
{
	struct GLRuntime : public Runtime, public std::enable_shared_from_this<GLRuntime>
	{
		friend struct GLEffect;
		friend struct GLTexture;
		friend struct GLSampler;
		friend struct GLConstant;
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

		virtual std::unique_ptr<Effect> CompileEffect(const EffectTree &ast, std::string &errors) const override;
		virtual void CreateScreenshot(unsigned char *buffer, std::size_t size) const override;

		HDC mDeviceContext;
		HGLRC mRenderContext;
		std::unique_ptr<class GLStateBlock> mStateBlock;
		GLuint mDefaultBackBufferFBO, mDefaultBackBufferRBO[2], mBackBufferTexture[2];
		GLuint mDepthSourceFBO, mDepthSource, mDepthTexture, mBlitFBO;
		std::unordered_map<GLuint, DepthSourceInfo> mDepthSourceTable;
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
		GLuint mDefaultVAO, mDefaultVBO;
		std::vector<GLuint> mUniformBuffers;
		std::vector<std::pair<unsigned char *, std::size_t>> mUniformStorages;
		mutable bool mUniformDirty;
	};
	struct GLTexture : public Effect::Texture
	{
		enum class Source
		{
			Memory,
			BackBuffer,
			DepthStencil
		};

		GLTexture(GLEffect *effect, const Description &desc);
		~GLTexture();

		inline bool AddAnnotation(const std::string &name, const Effect::Annotation &value)
		{
			return this->mAnnotations.emplace(name, value).second;
		}

		virtual bool Update(unsigned int level, const unsigned char *data, std::size_t size) override;
		void UpdateSource(GLuint texture, GLuint textureSRGB);

		GLEffect *mEffect;
		Source mSource;
		GLuint mID[2];
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
		std::size_t mBufferIndex, mBufferOffset;
	};
	struct GLTechnique : public Effect::Technique
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
} }