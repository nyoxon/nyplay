#ifndef SOUND_ENGINE_H
#define SOUND_ENGINE_H

#include "types.h"

void audio_shutdown(struct player_state* st);
void apply_volume(struct player_state* st, size_t volume);
int apply_offset(struct player_state* st, int64_t offset);
int audio_init(struct player_state* st);
int convert_wav_to_32(struct player_state* st, size_t frames);
int play_wav_stream(struct player_state* st;);

#endif