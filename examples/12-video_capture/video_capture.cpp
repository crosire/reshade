/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <reshade.hpp>
#include <chrono>

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

struct __declspec(uuid("0d7525f9-c4e1-426e-bc99-15bbd5fd51f2")) video_capture
{
	AVCodecContext *codec_ctx = nullptr;
	AVFormatContext *output_ctx = nullptr;
	AVFrame *frame = nullptr;

	reshade::api::resource host_resource;

	std::chrono::system_clock::time_point last_time;
	std::chrono::system_clock::time_point start_time;

	bool init_codec_ctx(const reshade::api::resource_desc &buffer_desc);
	void destroy_codec_ctx();
	bool init_format_ctx(const char *filename);
	void destroy_format_ctx();
};

bool video_capture::init_codec_ctx(const reshade::api::resource_desc &buffer_desc)
{
	const AVCodec *codec = nullptr;
	void *i = nullptr;
	while ((codec = av_codec_iterate(&i)) != nullptr)
	{
		if (codec->id != AV_CODEC_ID_H264 || !av_codec_is_encoder(codec))
			continue;

		bool supports_rgb0 = false;
		for (const AVPixelFormat *fmt = codec->pix_fmts; *fmt != AV_PIX_FMT_NONE; ++fmt)
			if (*fmt == AV_PIX_FMT_0BGR32 || *fmt == AV_PIX_FMT_0RGB32)
				supports_rgb0 = true;

		 if (supports_rgb0)
			break; // Found a codec that passes requirements
	}

	if (codec == nullptr)
	{
		reshade::log_message(reshade::log_level::error, "Failed to find a H.264 encoder that passes requirements!");
		return false;
	}

	codec_ctx = avcodec_alloc_context3(codec);

	codec_ctx->bit_rate = 400000;
	codec_ctx->width = buffer_desc.texture.width;
	codec_ctx->height = buffer_desc.texture.height;
	codec_ctx->time_base = { 1, 30 }; // Frames per second
	codec_ctx->color_range = AVCOL_RANGE_JPEG;
	codec_ctx->gop_size = 250;
	codec_ctx->max_b_frames = 2;

	switch (buffer_desc.texture.format)
	{
	case reshade::api::format::r8g8b8a8_unorm:
	case reshade::api::format::r8g8b8a8_unorm_srgb:
	case reshade::api::format::r8g8b8x8_unorm:
	case reshade::api::format::r8g8b8x8_unorm_srgb:
		codec_ctx->pix_fmt = AV_PIX_FMT_0BGR32;
		break;
	case reshade::api::format::b8g8r8a8_unorm:
	case reshade::api::format::b8g8r8a8_unorm_srgb:
	case reshade::api::format::b8g8r8x8_unorm:
	case reshade::api::format::b8g8r8x8_unorm_srgb:
		codec_ctx->pix_fmt = AV_PIX_FMT_0RGB32;
		break;
	default:
		destroy_codec_ctx();
		reshade::log_message(reshade::log_level::error, "Unsupported texture format!");
		return false;
	}

	if (int err = avcodec_open2(codec_ctx, codec, nullptr); err < 0)
	{
		destroy_codec_ctx();

		char errbuf[32 + AV_ERROR_MAX_STRING_SIZE] = "Failed to initialize encoder: ";
		av_make_error_string(errbuf + strlen(errbuf), sizeof(errbuf) - strlen(errbuf), err);
		reshade::log_message(reshade::log_level::error, errbuf);
		return false;
	}

	frame = av_frame_alloc();

	if (frame == nullptr)
	{
		destroy_codec_ctx();
		return false;
	}

	frame->width = codec_ctx->width;
	frame->height = codec_ctx->height;
	frame->format = codec_ctx->pix_fmt;
	frame->color_range = codec_ctx->color_range;

	if (int err = av_frame_get_buffer(frame, 0); err < 0)
	{
		destroy_codec_ctx();

		char errbuf[32 + AV_ERROR_MAX_STRING_SIZE] = "Failed to get frame buffer: ";
		av_make_error_string(errbuf + strlen(errbuf), sizeof(errbuf) - strlen(errbuf), err);
		reshade::log_message(reshade::log_level::error, errbuf);
		return false;
	}

	return true;
}
void video_capture::destroy_codec_ctx()
{
	if (frame != nullptr)
	{
		av_frame_free(&frame);
		frame = nullptr;
	}

	if (codec_ctx != nullptr)
	{
		avcodec_free_context(&codec_ctx);
		codec_ctx = nullptr;
	}
}

bool video_capture::init_format_ctx(const char *filename)
{
	if (int err = avformat_alloc_output_context2(&output_ctx, nullptr, nullptr, filename); err < 0)
	{
		char errbuf[32 + AV_ERROR_MAX_STRING_SIZE] = "Failed to initialize ffmpeg output context: ";
		av_make_error_string(errbuf + strlen(errbuf), sizeof(errbuf) - strlen(errbuf), err);
		reshade::log_message(reshade::log_level::error, errbuf);
		return false;
	}

	if (int err = avio_open(&output_ctx->pb, filename, AVIO_FLAG_WRITE); err < 0)
	{
		avformat_free_context(output_ctx);
		output_ctx = nullptr;

		reshade::log_message(reshade::log_level::error, "Failed to open output file!");
		return false;
	}

	// Add video stream with this codec to the output
	AVStream *const stream = avformat_new_stream(output_ctx, nullptr);
	stream->id = 0;
	stream->time_base = codec_ctx->time_base;
	avcodec_parameters_from_context(stream->codecpar, codec_ctx);

	avformat_write_header(output_ctx, nullptr);

	return true;
}
void video_capture::destroy_format_ctx()
{
	if (output_ctx != nullptr)
	{
		av_write_trailer(output_ctx);

		avio_context_free(&output_ctx->pb);
		avformat_free_context(output_ctx);
		output_ctx = nullptr;
	}
}

static void encode_frame(AVCodecContext *enc, AVFormatContext *s, AVFrame *frame, int stream_index = 0)
{
	if (int err = avcodec_send_frame(enc, frame); err < 0)
	{
		char errbuf[32 + AV_ERROR_MAX_STRING_SIZE] = "Failed to send frame for encoding: ";
		av_make_error_string(errbuf + strlen(errbuf), sizeof(errbuf) - strlen(errbuf), err);
		reshade::log_message(reshade::log_level::error, errbuf);
		return;
	}

	AVStream *const stream = s->streams[stream_index];

	for (AVPacket pkt = {}; avcodec_receive_packet(enc, &pkt) >= 0; av_packet_unref(&pkt))
	{
		av_packet_rescale_ts(&pkt, enc->time_base, stream->time_base);
		pkt.stream_index = stream_index;

		av_interleaved_write_frame(s, &pkt);
	}
}

static void on_init(reshade::api::effect_runtime *runtime)
{
	runtime->create_private_data<video_capture>();
}
static void on_destroy(reshade::api::effect_runtime *runtime)
{
	video_capture &data = runtime->get_private_data<video_capture>();

	if (data.host_resource != 0)
		runtime->get_device()->destroy_resource(data.host_resource);

	// Flush the encoder
	if (data.output_ctx != nullptr)
		encode_frame(data.codec_ctx, data.output_ctx, nullptr);

	data.destroy_format_ctx(); data.destroy_codec_ctx();

	runtime->destroy_private_data<video_capture>();
}

static void on_reshade_finish_effects(reshade::api::effect_runtime *runtime, reshade::api::command_list *, reshade::api::resource_view rtv, reshade::api::resource_view)
{
	video_capture &data = runtime->get_private_data<video_capture>();

	reshade::api::device *const device = runtime->get_device();

	const reshade::api::resource rtv_resource = device->get_resource_from_view(rtv);

	if (runtime->is_key_pressed(VK_F11))
	{
		if (data.output_ctx != nullptr)
		{
			reshade::log_message(reshade::log_level::info, "Stopping video recording ...");

			runtime->get_command_queue()->wait_idle();

			device->destroy_resource(data.host_resource);
			data.host_resource = { 0 };

			// Flush the encoder
			encode_frame(data.codec_ctx, data.output_ctx, nullptr);

			data.destroy_format_ctx(); data.destroy_codec_ctx();
		}
		else
		{
			reshade::api::resource_desc desc = device->get_resource_desc(rtv_resource);

			if (!data.init_codec_ctx(desc))
				return;
			if (!data.init_format_ctx("video.mp4"))
				return;

			desc.type = reshade::api::resource_type::texture_2d;
			desc.heap = reshade::api::memory_heap::gpu_to_cpu;
			desc.usage = reshade::api::resource_usage::copy_dest;
			desc.flags = reshade::api::resource_flags::none;

			if (device->create_resource(desc, nullptr, reshade::api::resource_usage::copy_dest, &data.host_resource))
			{
				reshade::log_message(reshade::log_level::info, "Starting video recording ...");

				data.start_time = data.last_time = std::chrono::system_clock::now();
			}
			else
			{
				reshade::log_message(reshade::log_level::error, "Failed to create host resource!");

				data.destroy_format_ctx(); data.destroy_codec_ctx();
				return;
			}
		}
	}

	if (data.codec_ctx == nullptr || data.output_ctx == nullptr || data.host_resource == 0)
		return;

	// Only encode a frame every few frames, depending on the set codec framerate
	const auto time = std::chrono::system_clock::now();
	if ((time - data.last_time) < (std::chrono::milliseconds(data.codec_ctx->time_base.num * std::milli::den) / data.codec_ctx->time_base.den))
		return;
	data.last_time = time;

	// TODO: This is slow/incorrect, because blocking for the frame and copy to finish.
	// Instead multiple resources should be created and copied into in a round-robin fashion, only the oldest of which is mapped below and copied into the video frame.
	// See the OBS capture add-on example which does this.
	reshade::api::command_list *const cmd_list = runtime->get_command_queue()->get_immediate_command_list();
	cmd_list->barrier(rtv_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
	cmd_list->copy_texture_region(rtv_resource, 0, nullptr, data.host_resource, 0, nullptr);
	cmd_list->barrier(rtv_resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);

	runtime->get_command_queue()->flush_immediate_command_list();

	// Wait for the above copy command to finish executing on the GPU
	runtime->get_command_queue()->wait_idle();

	if (av_frame_make_writable(data.frame) < 0)
		return;

	reshade::api::subresource_data host_data;
	if (!device->map_texture_region(data.host_resource, 0, nullptr, reshade::api::map_access::read_only, &host_data))
		return;

	for (int y = 0; y < data.codec_ctx->height; ++y)
	{
		for (int x = 0; x < data.codec_ctx->width; ++x)
		{
			const size_t host_data_index = y * host_data.row_pitch + x * 4;
			const size_t frame_data_index = y * data.frame->linesize[0] + x * 4;

			data.frame->data[0][frame_data_index + 0] = static_cast<const uint8_t *>(host_data.data)[host_data_index + 0];
			data.frame->data[0][frame_data_index + 1] = static_cast<const uint8_t *>(host_data.data)[host_data_index + 1];
			data.frame->data[0][frame_data_index + 2] = static_cast<const uint8_t *>(host_data.data)[host_data_index + 2];
		}
	}

	device->unmap_texture_region(data.host_resource, 0);

	data.frame->pts = av_rescale_q(
		std::chrono::duration_cast<std::chrono::milliseconds>(time - data.start_time).count(),
		AVRational { std::milli::num, std::milli::den },
		data.codec_ctx->time_base);

	encode_frame(data.codec_ctx, data.output_ctx, data.frame);
}

extern "C" __declspec(dllexport) const char *NAME = "Video Capture";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Example add-on that captures the screen after effects were rendered and uses FFmpeg to create a video file from that.";

extern "C" __declspec(dllexport) bool AddonInit(HMODULE addon_module, HMODULE reshade_module)
{
	if (!reshade::register_addon(addon_module, reshade_module))
		return false;

	reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init);
	reshade::register_event<reshade::addon_event::destroy_effect_runtime>(on_destroy);
	reshade::register_event<reshade::addon_event::reshade_finish_effects>(on_reshade_finish_effects);

	return true;
}
extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon_module, HMODULE reshade_module)
{
	reshade::unregister_addon(addon_module, reshade_module);
}
