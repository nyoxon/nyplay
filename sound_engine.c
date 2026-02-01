#include "types.h"
#include "sound_engine.h"
#include <limits.h>

static inline int32_t clamp_s32(int64_t v) {
	if (v > INT32_MAX) {
		return INT32_MAX;
	}

	if (v < INT32_MIN) {
		return INT32_MIN;
	}

	return (int32_t) v;
}

void apply_volume(struct player_state* st, size_t frames)
{
	if (st->player_gain == 1.0f) {
		return;
	}

	size_t total_samples = frames * st->wav.channels;

	for (size_t i = 0; i < total_samples; i++) {
		int64_t v = (int64_t)(st->wav.buf32[i] * st->player_gain);
		st->wav.buf32[i] = clamp_s32(v);
	}
}

int apply_offset(struct player_state* st, int64_t offset) {
	int64_t frames_played_signed = (int64_t) st->wav.frames_played;
	int64_t new_frame_pos = frames_played_signed + offset;
	int64_t total_frames = (int64_t) st->wav.data_size / st->wav.frame_size;

	if (new_frame_pos < 0) {
		new_frame_pos = 0;
	} else if (new_frame_pos > total_frames) {
		new_frame_pos = total_frames;
	}

	off_t byte_offset = st->wav.data_offset + new_frame_pos * st->wav.frame_size;

	if (lseek(st->fd, byte_offset, SEEK_SET) == -1) {
		perror("lseek");
		return -1;
	}

	st->wav.frames_played = new_frame_pos;

	st->wav.frames_left = total_frames - st->wav.frames_played;

	return 0;
}

int convert_wav_to_32
(
	struct player_state* st,
	size_t frames
) 
{
	size_t total_samples = frames * st->wav.channels;

	const uint8_t* src = st->wav.buf;
	int32_t* dst = st->wav.buf32;

	for (size_t i = 0; i < total_samples; i++) {
		int32_t sample = 0;

		if (st->wav.bits_per_sample == 8) {
			uint8_t v = *src++;
			sample = ((int32_t)v - 128) << 24;
		} else if (st->wav.bits_per_sample == 16) {
			int16_t v = (int16_t)(src[0] | (src[1] << 8));
			src += 2;
			sample = ((int32_t) v) << 16;
		} else if (st->wav.bits_per_sample == 24) {
			int32_t v = (src[0]) | (src[1] << 8) | (src[2] << 16);

			if (v & 0x00800000) {
				v |= 0xFF000000;
			}

			src += 3;
			sample = v << 8;
		} else {			
			return -1;
		}

		*dst++ = sample;
	}

	return 0;
}

int audio_init(struct player_state* st) 
{
	int err;


	if ((err = snd_pcm_open(&st->pcm, "default",
		SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		return -1;
	}

	if ((err = snd_pcm_set_params(
		st->pcm,
		SND_PCM_FORMAT_S32_LE,
		SND_PCM_ACCESS_RW_INTERLEAVED,
		st->wav.channels,
		st->wav.sample_rate,
		1,
		500000)) < 0) {
		return -1;
	}

	st->mode = PLAYER;
	st->state = PLAYING;

	return 0;
}

void audio_shutdown(struct player_state* st) {
	if (!st->pcm) {
		return;
	}

	snd_pcm_drain(st->pcm);
	snd_pcm_close(st->pcm);
	st->pcm = NULL;
	st->mode = COMMAND;
	st->state = STOPPED;
}

int play_wav_stream
(
	struct player_state* st
)
{
	if (st->state != PLAYING) {
		return 3;
	}

	if (st->fd < 0) {
		fprintf(stderr, "invalid file descriptor\n");
		return -1;
	}

	if (st->wav.frames_left == 0) {
		return 1;
	}

	ssize_t n;

	size_t frames = (st->wav.frames_left < FRAMES_PER_TICK) 
		? st->wav.frames_left
		: FRAMES_PER_TICK;

	n = read(st->fd, st->wav.buf, frames * st->wav.frame_size);

	if (n <= 0) {
		return -1;
	}

	size_t offset = 0;
	size_t frames_to_write = n / st->wav.frame_size;

	if (convert_wav_to_32(st, frames_to_write) < 0) {
		return -1;
	}

	apply_volume(st, frames_to_write);

	while (frames_to_write > 0) {
		snd_pcm_sframes_t written = 
			snd_pcm_writei(st->pcm, 
				st->wav.buf32 + offset * st->wav.channels, frames_to_write);

		if (written < 0) {
			if (written == -EPIPE) {
				snd_pcm_prepare(st->pcm);
				continue;
			}

			return -1;
		}

		frames_to_write -= written;
		st->wav.frames_played += written;
		offset += written;
	}

	st->wav.frames_left -= n / st->wav.frame_size;

	return 0;
}
