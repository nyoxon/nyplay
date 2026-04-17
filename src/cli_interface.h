/*
there are 3 I/O lines that must me implemented:
- audio output
- user input
- UI update

the main loop must be:

loop {
	wait_timeout_events()
	process_user_input() // stdin
	feed_audio_output() // data_buf
	update_ui()
}

*/

#ifndef CLI_INTERFACE_H
#define CLI_INTERFACE_H

#include "types.h"
#include <stdatomic.h>
#include <signal.h>
#include <alsa/asoundlib.h>

#define UI_WIDTH 20

struct track* get_current_music(struct player_state* st);
int set_current_music(struct player_state* st, size_t index);

/* search and list .wav files */
void list_wavs
(
	const char* path,
	int recursive,
	void (*on_wav) (const char* fullpath, const char* fullname, void* userdata),
	void* userdata
);

// loop during UI_COMMAND
void command_loop(struct player_state* st, volatile sig_atomic_t* should_exit);

// handle user input on command mode
void process_command_input(char* line, struct player_state* st);

// loop during UI_PLAYER
void player_loop(struct player_state* st, volatile sig_atomic_t* should_exit);

// handle user input on player mode
int process_player_input(struct player_state* st);

void enable_raw_mode();
void disable_raw_mode();

/*	--- CALLBACKS --- */

void print_wav(const char* path, const char* fullname, void* userdata);

#endif