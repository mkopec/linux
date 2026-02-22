/*
 * HPO HDMI link sequencing
 *
 * Copyright 2026 Michał Kopeć <michal@nozomi.space>
 */

#include "link_hwss_hpo_frl.h"
#include "core_types.h"
#include "dccg.h"
#include "dm_helpers.h"

#define DC_LOGGER link->ctx->logger

bool can_use_hpo_hdmi_frl_link_hwss(const struct dc_link *link,
				    const struct link_resource *link_res)
{
	return link_res->hpo_hdmi_link_enc != NULL;
}

static void enable_hpo_hdmi_frl_link_output(
	struct dc_link *link, const struct link_resource *link_res,
	enum signal_type signal, const struct dc_link_settings *link_settings)
{
	struct hpo_hdmi_link_encoder *enc;
	uint8_t frl_rate;
	uint8_t lane_count;

	enc = link_res->hpo_hdmi_link_enc;
	frl_rate = link_settings->frl_rate;
	lane_count = (frl_rate <= 2) ? 3 : 4;

	enc->funcs->configure_transmitter(enc, link, frl_rate,
					  link->link_enc->transmitter,
					  link->link_enc->hpd_source);

	enc->funcs->link_enable(enc, lane_count);
}

static void
disable_hpo_hdmi_frl_link_output(struct dc_link *link,
				 const struct link_resource *link_res,
				 enum signal_type signal)
{
	struct hpo_hdmi_link_encoder *enc;

	if (!link_res->hpo_hdmi_link_enc)
		return;

	enc = link_res->hpo_hdmi_link_enc;

	enc->funcs->link_disable(enc);
}

static void setup_hpo_hdmi_frl_stream_encoder(struct pipe_ctx *pipe_ctx)
{
	struct hpo_hdmi_stream_encoder *stream_enc =
		pipe_ctx->stream_res.hpo_hdmi_stream_enc;

	uint32_t otg_inst = pipe_ctx->stream_res.tg->inst;
	stream_enc->funcs->enable(stream_enc, otg_inst);
}

static void reset_hpo_hdmi_frl_stream_encoder(struct pipe_ctx *pipe_ctx)
{
	struct hpo_hdmi_stream_encoder *stream_enc =
		pipe_ctx->stream_res.hpo_hdmi_stream_enc;

	if (!stream_enc)
		return;

	stream_enc->funcs->stop_hdmi_info_packets(stream_enc);
	stream_enc->funcs->disable(stream_enc);
}

static void setup_hpo_hdmi_frl_stream_attribute(struct pipe_ctx *pipe_ctx)
{
	struct hpo_hdmi_stream_encoder *stream_enc =
		pipe_ctx->stream_res.hpo_hdmi_stream_enc;
	struct dc_crtc_timing *timing = &pipe_ctx->stream->timing;
	enum dc_color_space output_color_space =
		pipe_ctx->stream->output_color_space;

	if (!stream_enc)
		return;

	stream_enc->funcs->setup_stream_attribute(stream_enc, timing,
						  output_color_space);
}

static void setup_hpo_hdmi_frl_audio_output(struct pipe_ctx *pipe_ctx,
				     struct audio_output *audio_output,
				     uint32_t audio_inst)
{
	pipe_ctx->stream_res.hpo_hdmi_stream_enc->funcs->hdmi_audio_setup(
		pipe_ctx->stream_res.hpo_hdmi_stream_enc, audio_inst,
		&pipe_ctx->stream->audio_info, &audio_output->crtc_info,
		pipe_ctx->link_config.dp_link_settings.frl_rate);
}

static void enable_hpo_hdmi_frl_audio_packet(struct pipe_ctx *pipe_ctx)
{
	pipe_ctx->stream_res.hpo_hdmi_stream_enc->funcs->hdmi_audio_enable(
		pipe_ctx->stream_res.hpo_hdmi_stream_enc);
}

static void disable_hpo_hdmi_frl_audio_packet(struct pipe_ctx *pipe_ctx)
{
	if (pipe_ctx->stream_res.audio)
		pipe_ctx->stream_res.hpo_hdmi_stream_enc->funcs
			->hdmi_audio_disable(
				pipe_ctx->stream_res.hpo_hdmi_stream_enc);
}

static const struct link_hwss hpo_hdmi_frl_link_hwss = {
	.setup_stream_encoder = setup_hpo_hdmi_frl_stream_encoder,
	.reset_stream_encoder = reset_hpo_hdmi_frl_stream_encoder,
	.setup_stream_attribute = setup_hpo_hdmi_frl_stream_attribute,

	.setup_audio_output = setup_hpo_hdmi_frl_audio_output,
	.enable_audio_packet = enable_hpo_hdmi_frl_audio_packet,
	.disable_audio_packet = disable_hpo_hdmi_frl_audio_packet,

	.disable_link_output = disable_hpo_hdmi_frl_link_output,
	.ext = {
		.enable_hdmi_link_output = enable_hpo_hdmi_frl_link_output,
	},
};

const struct link_hwss *get_hpo_hdmi_frl_link_hwss(void)
{
	return &hpo_hdmi_frl_link_hwss;
}
