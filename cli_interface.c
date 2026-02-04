#include "cli_interface.h"
#include "fd_handle.h"
#include "sound_engine.h"
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>

/*
3 -> pause
2 -> loop
1 -> finished
0 -> operation success
-1 -> error/stop playing
-2 -> error/continue playing
*/

static int is_wav(const char* name) {
	const char* dot = strrchr(name, '.');

	if (!dot) {
		return 0;
	}

	return strcasecmp(dot, ".wav") == 0;
}

void list_wavs
(
	const char* path,
	int recursive,
	void (*on_wav) (const char* fullpath, const char* fullname, void* userdata),
	void* userdata
)
{
	DIR* dir = opendir(path); // a pointer to the beggining of the dir

	if (!dir) {
		return;
	}

	struct dirent* ent;

	while ((ent = readdir(dir))) { // increments the pointer
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
			continue;
		}

		char fullpath[PATH_MAX_LENGTH];
		snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);

		struct stat st;

		if (stat(fullpath, &st) != 0) {
			continue;
		}

		if (S_ISDIR(st.st_mode) && recursive) { // actual file is a dir?
			list_wavs(fullpath, recursive, on_wav, userdata);
		} else if (S_ISREG(st.st_mode) && is_wav(ent->d_name)) {
			on_wav(fullpath, ent->d_name, userdata);
		}
	}

	closedir(dir);
}

int command_to_player(struct player_state* st) {
	st->mode = PLAYER;
	st->state = PLAYING;
	audio_init(st);
}

static void drain_stdin(void) {
	char buf[64];

	while (read(STDIN_FILENO, buf, sizeof(buf)) > 0) {

	}
}

static int player_to_command(struct player_state* st) {
	st->mode = COMMAND;
	st->state = STOPPED;
	st->current_track = 0;
	st->track_loop = 0;
	st->playlist_loop = 0;
	close(st->fd);
	st->fd = -1;
	st->playlist_random = 0;

	free(st->wav.buf);
	free(st->wav.buf32);

	if (st->random_list.indexes) {
		random_list_free(&st->random_list);
	}

	audio_shutdown(st);

	drain_stdin();
}

struct track* get_current_music(struct player_state* st) {
	return &st->playlist.items[st->current_track];
}

struct track* get_nth_music(struct player_state* st, size_t index) {
	if (index >= st->playlist.len) {
		fprintf(stderr, "index out of bounds\n");
		return NULL;
	}

	return &st->playlist.items[index];
}

int set_current_music(struct player_state* st, size_t index) {
	if (index >= st->playlist.len) {
		fprintf(stderr, "index out of bounds\n");
		return -1;
	}
 
	if (st->wav.buf && st->fd > 0) {
		free(st->wav.buf);
		free(st->wav.buf32);
	}

	struct track* t = get_nth_music(st, index);

	int fd = get_wav_information(t->path, &st->wav);


	if (init_wav_buf(&st->wav) < 0) {
		return -1;
	}

	if (fd < 0) {
		fprintf(stderr, "reading wav failed\n");
		return -1;
	}

	st->fd = fd;
	st->current_track = index;

	return 0;
}

static size_t get_full_duration(const struct player_state* st) {
	size_t total_frames = st->wav.data_size / st->wav.frame_size;
	return total_frames / st->wav.sample_rate;
}

static size_t get_duration_until_now(const struct player_state* st) {
	return st->wav.frames_played / st->wav.sample_rate;	
}

int prev_music(struct player_state* st) {
	st->played++;

	double duration = get_duration_until_now(st);

	if (duration >= 2) {
		if (set_current_music(st, st->current_track) < 0) {
			return -1;
		}

		return 0;
	}

	if (st->current_track == 0) {
		if (set_current_music(st, 0) < 0) {
			return -1;
		}

		return 0;
	}

	if (st->track_loop) {
		if (set_current_music(st, st->current_track) < 0) {
			return -1;
		}

		return 2;
	}

	if (st->playlist_random) {
		uint32_t current = 0;

		if (st->random_list.current == 0) {
			if (set_current_music(st, (st->random_list.indexes[st->random_list.size])) < 0) {
				return -1;
			}

			st->random_list.waiting = 1;
			return 0;
		}

		(st->random_list.current)--;
		int ret = random_list_get_current(&st->random_list, &current);

		if (ret != 0) {
			return -1;
		}

		if (set_current_music(st, current) < 0) {
			return -1;
		}

		st->current_track = current;
		return 0;
	}

	if (set_current_music(st, st->current_track - 1) < 0) {
		return -1;
	}

	return 0;
}

int next_music(struct player_state* st) {
	st->played++;

	if (st->track_loop) {
		if (set_current_music(st, st->current_track) < 0) {
			fprintf(stderr, "playing wav failed\n");
			return -1;
		}

		return 2;
	}

	if (st->playlist_random) {
		uint32_t current;

		if (st->random_list.waiting) {
			random_list_get_current(&st->random_list, &current);

			if (set_current_music(st, current) < 0) {
				return -1;
			}

			st->random_list.waiting = 0;
			st->current_track = current;
			return 0;
		}

		(st->random_list.current)++;
		int ret = random_list_get_current(&st->random_list, &current);

		if (ret == -1) { // error
			return -1;
		} else if (ret == 1) { // end of the list
			if (st->playlist_loop) {
				current = (st->random_list.indexes)[0];

				if (set_current_music(st, (st->random_list.indexes[st->random_list.size])) < 0) {
					return -1;
				}

				st->random_list.current = 0;
				st->random_list.waiting = 1;
				return 2;
			} else {
				return -1; // not a error, but end of player state
			}
		}

		if (set_current_music(st, current) < 0) {
			return -1;
		}

		st->current_track = current;
		return 0;
	}

	if (st->current_track >= st->playlist.len - 1) {
		if (st->playlist_loop) {
			if (set_current_music(st, 0) < 0) {
				fprintf(stderr, "playing wav failed\n");
				return -1;
			}

			return 0;
		} else {
			return -1; // not a error, but end of player state
		}
	}


	if (set_current_music(st, st->current_track + 1) < 0) {
		fprintf(stderr, "playing wav failed\n");
		return -1;
	}

	return 0;
}

void print_help() {
	printf("\033[H\033[J");
	printf("commands for command mode:\n\n");
	printf("(play number_track) -> play track of number number_track\n");
	printf("(play) -> (play 0)\n");
	printf("(list) -> list all wav files\n");
	printf("(loop) -> enable/disable playlist loop\n");
	printf("(clear) -> clean the terminal\n");
	printf("(help) -> list all possible commands\n");
	printf("(about) -> about the program\n");
	printf("(quit) -> quit the program\n\n");
	printf("if you add a new WAV in the directory, restart the program\n");
	printf("you don't need to write (command) inside the parentheses\n");
	printf("the use in here is just a way to distinguish a command from a normal text\n\n");
}

void print_about() {
	printf("\033[H\033[J");
	printf("	--- ABOUT ---\n\n");
	printf("this is a simple wav player written in C and using ALSA\n\n");
	printf("in this player you can play a list of.wav files\n");
	printf("within a directory (recursively if you enable this option)\n\n");
	printf("a file is identified as .wav only by its name, which means\n");
	printf("that the program does not perform a security check to ensure\n");
	printf("that a file with a .wav name is in fact a .wav\n\n");
	printf("	--- OPERATION MODES ---	 \n\n");
	printf("Command mode:\n");
	printf("it's the mode you're in right now, where you set\n");
	printf("certain settings like playlistloop and dictate \n");
	printf("specific commands for specific needs\n\n");
	printf("Player mode:\n");
	printf("this is the mode you find yourself in while a .wav\n");
	printf("s playing. in it there is some information about the current track\n");
	printf("a progress bar tha t updates at a constant rate and a list of\n");
	printf("commands (simpler to write) that you can write to get specific results\n\n");
}

void process_command_input(char* line, struct player_state* st) {
	char cmd[16];
	int flag;

	int count = sscanf(line, "%15s %d", cmd, &flag);

	if (strncmp(cmd, "quit", 4) == 0) {
		st->running = 0;
	} else if (strncmp(cmd, "help", 4) == 0) {
		print_help();
	} else if (strncmp(cmd, "list", 4) == 0) {
		printf("current directory: %s (recursive=%d)\n\n",
		 st->dir_path, st->recursive);
		playlist_print(&st->playlist);
	} else if (strncmp(cmd, "play", 4) == 0) {
		if (st->playlist.len == 0) {
			printf("current playlist is empty\n\n");
			return;
		}

		if (st->playlist_random) {
			next_music(st);
			command_to_player(st);
			return;
		}

		if (count == 1) {
			if (set_current_music(st, 0) < 0) {
				fprintf(stderr, "playing wav failed\n");
				return;
			}
		} else if (count == 2) {
			if (flag < 1 || flag > (int) st->playlist.len) {
				fprintf(stderr, "playing wav failed\n");
				return;
			}

			if (set_current_music(st, flag - 1) < 0) {
				fprintf(stderr, "playing wav failed\n");
				return;		
			}
		}

		command_to_player(st);
	} else if (strcmp(cmd, "loop") == 0) {
		st->playlist_loop = (st->playlist_loop) ? 0 : 1;

		if (st->playlist_loop) {
			printf("playlistloop: enabled\n");
		} else {
			printf("playlistloop: disabled\n");
		}
	} else if (strcmp(cmd, "clear") == 0) {
		printf("\033[H\033[J");		
	} else if(strcmp(cmd, "about") == 0) {
		print_about();
	} else if (*cmd) {
		printf("\ninvalid command: %s\n", cmd);
		printf("(help) for possible commands\n");
	}
}

void command_loop(struct player_state* st, volatile sig_atomic_t* should_exit) {
	// stdin is blocking during player loop
	char line[256];
	int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
	printf("\033[H\033[J");
	printf("COMMAND MODE (help for list of commands)\n\n");

	while (st->running && (st->mode == COMMAND)) {
		if (*should_exit) {
			st->running = 0;
		}

		printf("> ");
		fflush(stdout);

		if (!fgets(line, sizeof(line), stdin)) {
			break;
		}

		process_command_input(line, st);
	}

	if (st->random_list.indexes) {
		random_list_free(&st->random_list);
	}
}

static void render_progress_bar(struct player_state* st, int width) {
	if (st->wav.frames_left == 0) {
		return;
	}

	size_t total = st->wav.data_size / st->wav.frame_size;
	size_t current = total - st->wav.frames_left;

	float ratio = (float) current / (float) total;

	if (ratio > 1.0f) {
		ratio = 1.0f;
	}

	int filled = (int) (ratio * width);

	int duration = (int) get_duration_until_now(st);
	int minutes =  duration / 60;
	int seconds = duration % 60;

	if (seconds < 10) {
		printf("%d:0%d ", minutes, seconds);
	} else {
		printf("%d:%d ", minutes, seconds);
	}

	putchar('[');

	for (int i = 0; i < width; i++) {
		if (i < filled) {
			putchar('#');
		} else {
			putchar('-');
		}
	}

	putchar(']');
	putchar(' ');

	duration = (int) get_full_duration(st);
	minutes =  duration / 60;
	seconds = duration % 60;

	if (seconds < 10) {
		printf("%d:0%d ", minutes, seconds);
	} else {
		printf("%d:%d ", minutes, seconds);
	}
}

static void render_ui(struct player_state* st) {
	if (st->state == PAUSED) {
		return;
	}

	printf("\033[H\033[J");

	if (st->state == PLAYING) {
		struct track* t = get_current_music(st);
		printf("current track [%d/%d]: %s\n",
			st->current_track + 1, st->playlist.len, t->name);
		printf("volume: %.1f%\n", st->player_gain * 100.0);

		if (st->playlist_loop) {
			printf("playlistloop: enabled\n");
		} else {
			printf("playlistloop: disabled\n");
		}

		if (st->track_loop) {
			printf("looptrack: enabled\n");
		} else {
			printf("looptrack: disabled\n");
		}

		if (st->playlist_random) {
			printf("random: enabled\n");
		} else {
			printf("random: disabled\n");
		}

		render_progress_bar(st, UI_WIDTH);
		printf("\n(space) play/pause  (d) next  (a) prev  (l) loop  (q) quit\n");
		printf("(w) volume up  (s) volume down  (,) -5 seconds  (.) +5 seconds  (r) random\n\n");
	}
}

static int handle_random_playlist(struct player_state* st) {
	if (!st->playlist_random) {
		st->playlist_random = 1;

		struct random_list random_list = {0};
		random_list.indexes = NULL;

		int ret = random_list_init(&random_list,
			st->playlist.len, (uint32_t) st->current_track);

		if (ret < 0) {
			return -1;
		} else if (ret == 1) { // just one wav
			return 0;
		}

		st->random_list = random_list;

		return 0;
	} else {
		st->playlist_random = 0;

		if (st->random_list.indexes) {
			random_list_free(&st->random_list);
		}

		return 0;
	}
}

static int process_key(struct player_state* st, char c) {
	if (c == ' ') {
		st->state = (st->state == PAUSED) ? PLAYING : PAUSED;
		return 3;
	}

	if (c == 'd') {
		return next_music(st);
	}

	if (c == 'q') {
		return -1;
	}

	if (c == 'w') {
		st->player_gain += 0.1;
		return 0;
	}

	if (c == 's') {
		st->player_gain -= 0.1;
		return 0;
	}

	if (c == 'a') {
		return prev_music(st);
	}

	if (c == '.') {
		apply_offset(st, 5 * (int32_t) st->wav.sample_rate);
		return 0;
	}

	if (c == ',') {
		apply_offset(st, -5 * (int32_t) st->wav.sample_rate);
		return 0;
	}

	if (c == 'l') {
		st->track_loop = (st->track_loop) ? 0 : 1;
		return 0;
	}

	if (c == 'r') {
		return handle_random_playlist(st);
	}

	return 0;
}

int process_player_input(struct player_state* st) {
	char input;

	if (read(STDIN_FILENO, &input, sizeof(input)) != sizeof(char)) {
		return -2;
	}

	return process_key(st, input);
}

void player_loop(struct player_state* st, volatile sig_atomic_t* should_exit) {
	/*
	stdin is nonblocking during player loop
	*/
	int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

	struct timespec ts = {
		.tv_sec = 0,
		.tv_nsec = 16 * 1000 * 1000
	};

	while (st->running && (st->mode == PLAYER)) {
		if (*should_exit) {
			st->running = 0;

			if (st->mode == PLAYER) {
				player_to_command(st);
			}

			break;
		}

		int ret = process_player_input(st);

		if (ret == 3) { // pause
			continue;
		} else if (ret == 1) { // finished
			ret = next_music(st);

			if (ret == -1) { // error/quit
				player_to_command(st);
				break;
			}
		} else if (ret == -1) { // error/quit
			player_to_command(st);
			break;
		}

		ret = play_wav_stream(st);

		if (ret == 1) { // finished
			ret = next_music(st);

			if (ret == -1) { // error
				player_to_command(st);
				break;
			}
		}

		render_ui(st);
		nanosleep(&ts, NULL);
	}
}

/*	--- CALLBACKS --- */

void print_wav(const char* path, const char* fullname, void* userdata) {
	(void) userdata;
	printf("%s\n", path);
}

