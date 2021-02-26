/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_GUI

#include "runtime_gl.hpp"
#include "runtime_gl_objects.hpp"
#include <imgui.h>

void reshade::opengl::runtime_impl::init_imgui_resources()
{
	if (_imgui.program != 0)
		return;

	assert(_app_state.has_state);

	const GLchar *vertex_shader[] = {
		"#version 430\n"
		"layout(location = 1) uniform mat4 proj;\n"
		"layout(location = 0) in vec2 pos;\n"
		"layout(location = 1) in vec2 tex;\n"
		"layout(location = 2) in vec4 col;\n"
		"out vec4 frag_col;\n"
		"out vec2 frag_tex;\n"
		"void main()\n"
		"{\n"
		"	frag_col = col;\n"
		"	frag_tex = tex;\n"
		"	gl_Position = proj * vec4(pos.xy, 0, 1);\n"
		"}\n"
	};
	const GLchar *fragment_shader[] = {
		"#version 430\n"
		"layout(binding = 0) uniform sampler2D s0;\n"
		"in vec4 frag_col;\n"
		"in vec2 frag_tex;\n"
		"out vec4 col;\n"
		"void main()\n"
		"{\n"
		"	col = frag_col * texture(s0, frag_tex.st);\n"
		"}\n"
	};

	const GLuint imgui_vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(imgui_vs, 1, vertex_shader, 0);
	glCompileShader(imgui_vs);
	const GLuint imgui_fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(imgui_fs, 1, fragment_shader, 0);
	glCompileShader(imgui_fs);

	_imgui.program = glCreateProgram();
	glAttachShader(_imgui.program, imgui_vs);
	glAttachShader(_imgui.program, imgui_fs);
	 glLinkProgram(_imgui.program);
	glDeleteShader(imgui_vs);
	glDeleteShader(imgui_fs);
}

void reshade::opengl::runtime_impl::render_imgui_draw_data(ImDrawData *draw_data)
{
	assert(_app_state.has_state);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	if (_compatibility_context)
		glDisable(GL_ALPHA_TEST);
	glFrontFace(GL_CCW);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);
	glEnable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_FALSE);

	glViewport(0, 0, GLsizei(draw_data->DisplaySize.x), GLsizei(draw_data->DisplaySize.y));

	glUseProgram(_imgui.program);
	glBindSampler(0, 0); // Do not use separate sampler object, since state is already set in texture
	glActiveTexture(GL_TEXTURE0); // s0

	const float ortho_projection[16] = {
		2.0f / draw_data->DisplaySize.x, 0.0f,   0.0f, 0.0f,
		0.0f, -2.0f / draw_data->DisplaySize.y,  0.0f, 0.0f,
		0.0f,                            0.0f,  -1.0f, 0.0f,
		-(2 * draw_data->DisplayPos.x + draw_data->DisplaySize.x) / draw_data->DisplaySize.x,
		+(2 * draw_data->DisplayPos.y + draw_data->DisplaySize.y) / draw_data->DisplaySize.y, 0.0f, 1.0f,
	};
	glUniformMatrix4fv(1 /* proj */, 1, GL_FALSE, ortho_projection);

	glBindVertexArray(_vao[VAO_IMGUI]);
	// Need to rebuild vertex array object every frame
	// Doing so fixes weird interaction with 'glEnableClientState' and 'glVertexPointer' (e.g. in the first Call of Duty)
	glBindVertexBuffer(0, _buf[VBO_IMGUI], 0, sizeof(ImDrawVert));
	glEnableVertexAttribArray(0 /* pos */);
	glVertexAttribFormat(0, 2, GL_FLOAT, GL_FALSE, offsetof(ImDrawVert, pos));
	glVertexAttribBinding(0, 0);
	glEnableVertexAttribArray(1 /* tex */);
	glVertexAttribFormat(1, 2, GL_FLOAT, GL_FALSE, offsetof(ImDrawVert, uv ));
	glVertexAttribBinding(1, 0);
	glEnableVertexAttribArray(2 /* col */);
	glVertexAttribFormat(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(ImDrawVert, col));
	glVertexAttribBinding(2, 0);

	glBindBuffer(GL_ARRAY_BUFFER, _buf[VBO_IMGUI]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _buf[IBO_IMGUI]);

	for (int n = 0; n < draw_data->CmdListsCount; ++n)
	{
		ImDrawList *const draw_list = draw_data->CmdLists[n];

		glBufferData(GL_ARRAY_BUFFER, draw_list->VtxBuffer.Size * sizeof(ImDrawVert), draw_list->VtxBuffer.Data, GL_STREAM_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx), draw_list->IdxBuffer.Data, GL_STREAM_DRAW);

		for (const ImDrawCmd &cmd : draw_list->CmdBuffer)
		{
			assert(cmd.TextureId != 0);
			assert(cmd.UserCallback == nullptr);

			const ImVec4 scissor_rect(
				cmd.ClipRect.x - draw_data->DisplayPos.x,
				cmd.ClipRect.y - draw_data->DisplayPos.y,
				cmd.ClipRect.z - draw_data->DisplayPos.x,
				cmd.ClipRect.w - draw_data->DisplayPos.y);
			glScissor(
				static_cast<GLint>(scissor_rect.x),
				static_cast<GLint>(_height - scissor_rect.w),
				static_cast<GLint>(scissor_rect.z - scissor_rect.x),
				static_cast<GLint>(scissor_rect.w - scissor_rect.y));

			glBindTexture(GL_TEXTURE_2D,
				static_cast<const tex_data *>(cmd.TextureId)->id[0]);

			glDrawElementsBaseVertex(GL_TRIANGLES, cmd.ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
				reinterpret_cast<const void *>(static_cast<uintptr_t>(cmd.IdxOffset * sizeof(ImDrawIdx))), cmd.VtxOffset);
		}
	}
}

#endif
