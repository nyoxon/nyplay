#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

void print_riff_header(const struct riff_header* rhdr) {
	printf("	--- RIFF HEADER --- 	\n");
	printf("chunk_id: %.4s\n", rhdr->chunk_id);
	printf("chunk_size: %d\n", rhdr->chunk_size);
	printf("format: %.4s\n\n", rhdr->format);
}

void print_fmt_sub_chunk(const struct fmt_sub_chunk* fmt) {
	printf("	--- FMT SUB CHUNK --- 	\n");
	printf("subchunk1_id: %.4s\n", fmt->subchunk1_id);
	printf("subchunk1_size: %d\n", fmt->subchunk1_size);
	printf("audio_format: %u\n", fmt->audio_format);
	printf("num_channels: %u\n", fmt->num_channels);
	printf("sample_rate: %d\n", fmt->sample_rate);
	printf("byte_rate: %d\n", fmt->byte_rate);
	printf("byte_align: %u\n", fmt->byte_align);
	printf("bits_per_sample: %u\n\n", fmt->bits_per_sample);
}

void print_data_sub_chunk(const struct data_sub_chunk* data) {
	printf("	--- DATA SUB CHUNK --- 	\n");
	printf("subchunk2_id: %.4s\n", data->subchunk2_id);
	printf("subchunk2_size: %d\n\n", data->subchunk2_size);
}

/* --- PLAYLIST FUNCTIONS --- */

static int playlist_grow(struct playlist* pl) {
	size_t new_cap = pl->cap ? pl->cap * 2 : 8;

	struct track* new_items = realloc(pl->items, new_cap * sizeof(*new_items));

	if (!new_items) {
		return -1;
	}

	pl->items = new_items;
	pl->cap = new_cap;

	return 0;
}

void playlist_init(struct playlist* pl) {
	pl->items = NULL;
	pl->len = 0;
	pl->cap = 0;
}

int playlist_push(struct playlist* pl, struct track t) {
	if (pl->len == pl->cap) {
		if (playlist_grow(pl) < 0) {
			return -1;
		}
	}

	pl->items[pl->len++] = t;
	return 0;
}

void playlist_free(struct playlist* pl) {
	for (size_t i = 0; i < pl->len; i++) {
		free(pl->items[i].path);
		free(pl->items[i].name);
	}

	free(pl->items);
}

void track_print(struct track* t) {
	if (!t) {
		return;
	}

	printf("path: %s\n", t->path);
	printf("name: %s\n", t->name);

	int duration = (int) t->duration;
	int minutes =  duration / 60;
	int seconds = duration % 60;

	if (seconds < 10) {
		printf("duration:	%d:0%d\n\n", minutes, seconds);
	} else {
		printf("duration:	%d:%d\n\n", minutes, seconds);
	}
}

void playlist_print(struct playlist* pl) {
	if (!pl) {
		return;
	}

	for (size_t i = 0; i < pl->len; i++) {
		struct track* t = &pl->items[i];
		printf("track %d\n", i + 1);

		track_print(t);
	}
}

void printf_wav_information(struct wav_information* wav) {
	printf("	---	 WAV INFORMATION ---\n");
	printf("data_size: %d\n", wav->data_size);
	printf("channels: %d\n", wav->channels);
	printf("sample_rate: %d\n", wav->sample_rate);
	printf("bits_per_sample: %d\n\n", wav->bits_per_sample);
}

int init_wav_buf(struct wav_information* wav) {
	wav->buf = malloc(FRAMES_PER_TICK * wav->frame_size);
	wav->buf32 = malloc(FRAMES_PER_TICK * wav->channels * sizeof(int32_t));

    if (!wav->buf || !wav->buf32) {
        perror("malloc");
        return -1;
    }

	return 0;
}

static int create_random_sequence(struct random_list* rl, size_t size, uint32_t current) {
	if (size < 2) {
		return 1;
	}

	srand((unsigned) time(NULL));

	rl->indexes = malloc(size * sizeof(uint32_t));

	if (!(rl->indexes)) {
		return -1;
	}

	for (size_t i = 0; i < size; i++) {
		(rl->indexes)[i] = i;
	}


	/*
	if current != -1, then the user set the random flag during playing
	some track.
	therefore, we change the last number of the list (size - 1) with current
	and decrement rl->size, ensuring that the current track won't play again.

	ex: current = 5, size = 10
	indexes = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

	indexes[size - 1] = indexes[9] := current = 5
	indexes[current] = indexes[5] := size - 1 = 9

	result:
	indexes = [0, 1, 2, 3, 4, 9, 6, 7, 8, 5]
	(size--)

	effective indexes:
	indexes = [0, 1, 2, 3, 4, 9, 6, 7, 8]
	*/

	if (current >= size) {
		free(rl->indexes);
		rl->indexes = NULL;
		return -1;
	}

	(rl->indexes)[size - 1] = current;
	(rl->indexes)[current] = size - 1;
	(size--);

	// fisher-yates
	for (size_t i = size - 1; i > 0; i--) {
		size_t j = rand() % (i + 1);
		uint32_t tmp = (rl->indexes)[i];
		(rl->indexes)[i] = (rl->indexes)[j];
		(rl->indexes)[j] = tmp;
	}

	rl->current = 0;
	rl->size = size;
	rl->waiting = 1;

	return 0;
}

int random_list_init(struct random_list* rl, size_t size, uint32_t current) {
	return create_random_sequence(rl, size, current);
}

int random_list_get_current(struct random_list* rl, uint32_t* current) {
	if (!rl || !(rl->indexes)) {
		return -1; // error
	}

	if (rl->current >= rl->size) {
		return 1; // end
	}

	*current = (rl->indexes)[rl->current];

	return 0; // success
}

int random_list_set_current(struct random_list* rl, uint32_t index) {
	if (!rl || !(rl->indexes)) {
		return -1;
	}

	if (index >= rl->size) {
		return -1;
	}

	rl->current = index;
	return 0;
}

void random_list_free(struct random_list* rl) {
	if (!rl || !(rl->indexes)) {
		return;
	}

	free(rl->indexes);
	rl->indexes = NULL;
	rl->current = 0;
	rl->size = 0;
}