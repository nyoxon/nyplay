#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <unistd.h>
#include <alsa/asoundlib.h>

#define PATH_MAX_LENGTH 1024
#define FRAMES_PER_TICK 1024

struct riff_header {
	char chunk_id[4]; // "RIFF"
	uint32_t chunk_size; // file_size - 8 bytes
	char format[4]; // "WAVE"
}__attribute__((packed));

struct fmt_sub_chunk {
	char subchunk1_id[4]; // "fmt "
	uint32_t subchunk1_size; // chunk size - 8 bytes (here, 16 bytes)
	uint16_t audio_format; // (1: PCM integer, 3: IEEE 754 float)
	uint16_t num_channels; // mono/stereo
	uint32_t sample_rate; // sample rate in hz
	uint32_t byte_rate; // bytes to read per sec (sample_rate * byte_align)
	uint16_t byte_align; // bytes per block (num_channels * bits_per_sample)
	uint16_t bits_per_sample; // bits per sample
}__attribute__((packed));

struct data_sub_chunk {
	char subchunk2_id[4]; // "data"
	uint32_t subchunk2_size; // sampled_data size
	// sampled_data
}__attribute__((packed));

struct chunk_header {
	char id[4];
	uint32_t size;
}__attribute__((packed));

struct track {
	char* path;
	char* name;
	double duration;
};

struct playlist { // a vector
	struct track* items;
	size_t len;
	size_t cap;
};

enum ui_mode {
	PLAYER,
	COMMAND
};

enum play_state {
	STOPPED,
	PLAYING,
	PAUSED
};

struct wav_information {
	off_t data_offset;
	size_t data_size;
	size_t frames_left;
	size_t frame_size;
	size_t frames_played;
	uint32_t byte_rate;
	uint32_t sample_rate;
	uint16_t channels;
	uint16_t bits_per_sample;
	int32_t* buf32;
	int8_t* buf;
};

struct random_list {
	uint32_t* indexes;
	uint32_t current;
	int waiting;
	size_t size;
};

struct player_state {
	int running; // controls main loop
	int fd; // fd of the current archive
	char dir_path[PATH_MAX_LENGTH]; // path of the current directory
	int recursive; // read directory recursively
	int playlist_loop; // playlist will play on loop
	int track_loop; // track will play on loop
	size_t played; // how many tracks were played
	int playlist_random;
	struct random_list random_list;

	enum ui_mode mode; // PLAYER or COMMAND
	enum play_state state; // STOPPED or PLAYING or PAUSED

	struct playlist playlist; // list of tracks
	size_t current_track; // number of tracks
	float player_gain;

	snd_pcm_t *pcm;
	struct wav_information wav;
};

void print_riff_header(const struct riff_header* rhdr);
void print_fmt_sub_chunk(const struct fmt_sub_chunk* fmt);
void print_data_sub_chunk(const struct data_sub_chunk* data);

/* --- PLAYLIST FUNCTIONS --- */

void playlist_init(struct playlist* pl);
void playlist_free(struct playlist* pl);
int playlist_push(struct playlist* pl, struct track t);
void playlist_print(struct playlist* pl);
void track_print(struct track* t);

/* --- RANDOM LIST FUNCTIONS --- */

int random_list_init(struct random_list* rl, size_t size, uint32_t current);
int random_list_get_current(struct random_list* rl, uint32_t* current);
int random_list_set_current(struct random_list* rl, uint32_t index);
void random_list_free(struct random_list* rl);

/* --- WAV ---*/

void printf_wav_information(struct wav_information* wav);
int init_wav_buf(struct wav_information* wav);

#endif