#pragma once

#include "Runtime.hpp"

#include <gl\gl3w.h>

namespace ReShade
{
	namespace Runtimes
	{
		class GLRuntime : public Runtime
		{
		public:
			GLRuntime(HDC device, HGLRC context);
			~GLRuntime();

			bool OnInit(unsigned int width, unsigned int height);
			void OnReset() override;
			void OnPresent() override;
			void OnDrawCall(unsigned int vertices) override;
			void OnApplyEffect() override;
			void OnApplyEffectTechnique(const Technique *technique) override;
			void OnFramebufferAttachment(GLenum target, GLenum attachment, GLenum objecttarget, GLuint object, GLint level);

			void Screenshot(unsigned char *buffer) const override;
			bool UpdateEffect(const FX::Tree &ast, const std::vector<std::string> &pragmas, std::string &errors) override;
			bool UpdateTexture(Texture *texture, const unsigned char *data, std::size_t size) override;

			void DetectDepthSource();
			void CreateDepthTexture(GLuint width, GLuint height, GLenum format);

			inline Texture *GetTexture(const std::string &name) const
			{
				const auto it = std::find_if(this->mTextures.cbegin(), this->mTextures.cend(), [name](const std::unique_ptr<Texture> &it) { return it->Name == name; });

				return it != this->mTextures.cend() ? it->get() : nullptr;
			}
			inline void EnlargeConstantStorage()
			{
				this->mConstantStorage = static_cast<unsigned char *>(::realloc(this->mConstantStorage, this->mConstantStorageSize += 128));
			}
			inline unsigned char *GetConstantStorage() const
			{
				return this->mConstantStorage;
			}
			inline size_t GetConstantStorageSize() const
			{
				return this->mConstantStorageSize;
			}
			inline void AddTexture(Texture *texture)
			{
				this->mTextures.push_back(std::unique_ptr<Texture>(texture));
			}
			inline void AddConstant(Constant *constant)
			{
				this->mConstants.push_back(std::unique_ptr<Constant>(constant));
			}
			inline void AddTechnique(Technique *technique)
			{
				this->mTechniques.push_back(std::unique_ptr<Technique>(technique));
			}

			HDC mDeviceContext;
			HGLRC mRenderContext;

			GLuint mReferenceCount, mCurrentVertexCount;
			GLuint mDefaultBackBufferFBO, mDefaultBackBufferRBO[2], mBackBufferTexture[2];
			GLuint mDepthSourceFBO, mDepthSource, mDepthTexture, mBlitFBO;
			std::vector<struct GLSampler> mEffectSamplers;
			GLuint mDefaultVAO, mDefaultVBO, mEffectUBO;

		private:
			struct DepthSourceInfo
			{
				GLint Width, Height, Level, Format;
				GLfloat DrawCallCount, DrawVerticesCount;
			};

			std::unique_ptr<class GLStateBlock> mStateBlock;
			std::unordered_map<GLuint, DepthSourceInfo> mDepthSourceTable;
			bool mPresenting;
		};
	}
}