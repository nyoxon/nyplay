#ifndef FD_HANDLE_H
#define FD_HANDLE_H

#include "types.h"

ssize_t read_bytes_from_file(int fd, void* buf, size_t size);
int get_wav_information(const char* path, struct wav_information* wav);

#endif