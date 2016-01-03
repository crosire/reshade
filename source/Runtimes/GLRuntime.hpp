#pragma once

#include "Runtime.hpp"

#include <algorithm>
#include <gl\gl3w.h>

namespace ReShade
{
	namespace Runtimes
	{
		class GLRuntime : public Runtime
		{
		public:
			GLRuntime(HDC device);

			bool OnInit(unsigned int width, unsigned int height);
			void OnReset() override;
			void OnResetEffect() override;
			void OnPresent() override;
			void OnDrawCall(unsigned int vertices) override;
			void OnApplyEffect() override;
			void OnApplyEffectTechnique(const Technique *technique) override;
			void OnFramebufferAttachment(GLenum target, GLenum attachment, GLenum objecttarget, GLuint object, GLint level);

			Texture *GetTexture(const std::string &name) const
			{
				const auto it = std::find_if(_textures.cbegin(), _textures.cend(), [name](const std::unique_ptr<Texture> &it) { return it->Name == name; });

				return it != _textures.cend() ? it->get() : nullptr;
			}
			void EnlargeConstantStorage()
			{
				_uniformDataStorage.resize(_uniformDataStorage.size() + 128);
			}
			unsigned char *GetConstantStorage()
			{
				return _uniformDataStorage.data();
			}
			size_t GetConstantStorageSize() const
			{
				return _uniformDataStorage.size();
			}
			void AddTexture(Texture *texture)
			{
				_textures.push_back(std::unique_ptr<Texture>(texture));
			}
			void AddConstant(Uniform *constant)
			{
				_uniforms.push_back(std::unique_ptr<Uniform>(constant));
			}
			void AddTechnique(Technique *technique)
			{
				_techniques.push_back(std::unique_ptr<Technique>(technique));
			}

			HDC _hdc;

			GLuint _referenceCount, _currentVertexCount;
			GLuint _defaultBackBufferFBO, _defaultBackBufferRBO[2], _backbufferTexture[2];
			GLuint _depthSourceFBO, _depthSource, _depthTexture, _blitFBO;
			std::vector<struct GLSampler> _effectSamplers;
			GLuint _defaultVAO, _defaultVBO, _effectUBO;

		private:
			struct DepthSourceInfo
			{
				GLint Width, Height, Level, Format;
				GLfloat DrawCallCount, DrawVerticesCount;
			};

			void Screenshot(unsigned char *buffer) const override;
			bool UpdateEffect(const FX::nodetree &ast, const std::vector<std::string> &pragmas, std::string &errors) override;
			bool UpdateTexture(Texture *texture, const unsigned char *data, size_t size) override;

			void DetectDepthSource();
			void CreateDepthTexture(GLuint width, GLuint height, GLenum format);

			std::unique_ptr<class GLStateBlock> _stateBlock;
			std::unordered_map<GLuint, DepthSourceInfo> _depthSourceTable;
		};
	}
}