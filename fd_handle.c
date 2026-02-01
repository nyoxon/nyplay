#include "fd_handle.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

ssize_t read_bytes_from_file(int fd, void* buf, size_t size) {
	ssize_t total_read = 0;
	ssize_t n;

	while (total_read < size) {
		n = read(fd, buf + total_read, size - total_read);

		if (n < 0) {
			perror("read");
			break;
		}

		if (n == 0) {
			break;
		}

		total_read += n;
	}

	return total_read;
}

// this functions considerate little endianness architectures
static uint16_t le16(const uint8_t* b) {
	/*
	if bytes is LE => LSB comes first
	bytes(RE) = 0x1234 (LSB = 34, MSB = 12)
	bytes(LE) = 0x3412 (LSB = 34, MSB = 12)
	0x1234 = 00010010 00110100
	0x3412 = 00110100 00010010

	bytes[0] = 0x34 = 00000000 00110100
	bytes[1] = 0x12 = 00000000 00010010
	bytes[1] << 8 = 00010010 00000000

	bytes[0] | (bytes[1] << 8) = 00000000 00110100 | 00010010 00000000 =
	00010010 00110100 = 0x1234

	is easy to understanding the algorithm for just 2 bytes, but
	for more, the reasoning is the same
	*/

	return (uint16_t)b[0] | (uint16_t)b[1] << 8;
}

static uint32_t le32(const uint8_t* b) {
	return (uint32_t)b[0]
		| (uint32_t)b[1] << 8
		| (uint32_t)b[2] << 16
		| (uint32_t)b[3] << 24;
}

int get_wav_information(const char* path, struct wav_information* wav) {
	int fd = open(path, O_RDONLY);

	if (fd < 0) {
		perror("open");
		return -1;
	}

	uint8_t hdr_riff[12];
	struct riff_header riff;

	if (read_bytes_from_file(fd, hdr_riff, sizeof(riff)) != sizeof(riff)) {
		fprintf(stderr, "failed to read riff header\n");
		goto fail;
	}

	memcpy(riff.chunk_id, hdr_riff, 4);
	riff.chunk_size = le32(hdr_riff + 4);
	memcpy(riff.format, hdr_riff + 8, 4);

	if (memcmp(riff.chunk_id, "RIFF", 4) != 0) {
		fprintf(stderr, "invalid riff header\n");
		goto fail;
	}

	if (memcmp(riff.format, "WAVE", 4) != 0) {
		fprintf(stderr, "invalid file format\n");
		goto fail;
	}

	uint8_t hdr[8];
	struct chunk_header fmt;
	
	if (read_bytes_from_file(fd, hdr, sizeof(fmt)) != sizeof(fmt)) {
		fprintf(stderr, "failed to read format sub chunk\n");
		goto fail;
	}

	memcpy(fmt.id, hdr, 4);
	fmt.size = le32(hdr + 4);

	if (memcmp(fmt.id, "fmt ", 4) != 0) {
		fprintf(stderr, "invalid format_sub_chunk\n");
		goto fail;
	}

	uint32_t bytes_read = 0;
	uint8_t buf[4];

	if (read_bytes_from_file(fd, buf, 2) != 2) { // audio_format
		fprintf(stderr, "failed to read audio format\n");
		goto fail;
	}

	uint16_t audio_format = le16(buf);

	if (audio_format != 1 && audio_format != 0xFFFE) {
		fprintf(stderr, "unsupported audio format (not pcm)\n");
		goto fail;
	}

	bytes_read += 2;

	if (read_bytes_from_file(fd, buf, 2) != 2) {
		fprintf(stderr, "failed to read channels\n");
		goto fail;
	}

	wav->channels = le16(buf);
	bytes_read += 2;

	if (read_bytes_from_file(fd, buf, 4) != 4) {
		fprintf(stderr, "failed to read sample_rate\n");
		goto fail;
	}

	wav->sample_rate = le32(buf);
	bytes_read += 4;

	if (read_bytes_from_file(fd, buf, 4) != 4) {
		fprintf(stderr, "failed to read byte_rate\n");
		goto fail;
	}

	wav->byte_rate = le32(buf);
	bytes_read += 4;

	if (lseek(fd, 2,SEEK_CUR) < 0) { // byte_align
		perror("lseek");
		goto fail;
	}

	bytes_read += 2;

	if (read_bytes_from_file(fd, buf, 2) != 2) {
		fprintf(stderr, "failed to read bits_per_sample\n");
		goto fail;
	}

	wav->bits_per_sample = le16(buf);
	bytes_read += 2;

	if (fmt.size > bytes_read) {
		size_t remaining = fmt.size - bytes_read;

		if (lseek(fd, remaining, SEEK_CUR) < 0) {
			perror("lseek");
			goto fail;
		}
	}

	wav->frame_size = wav->channels * (wav->bits_per_sample / 8);

	struct chunk_header chunk = {0};

	while (read_bytes_from_file(fd, hdr, sizeof(struct chunk_header)) == sizeof(struct chunk_header)) {
		memcpy(chunk.id, hdr, 4);
		chunk.size = le32(hdr + 4);

		if (memcmp(chunk.id, "data", 4) == 0) {
			wav->data_size = chunk.size;
			wav->frames_played = 0;
			wav->data_offset = lseek(fd, 0, SEEK_CUR);
			wav->frames_left = wav->data_size / wav->frame_size;
			return fd;
		} else {
			off_t skip = chunk.size;

			// consider chunk padding
			if (skip & 1) {
				skip++;
			}

			if (lseek(fd, skip, SEEK_CUR) == -1) {
				perror("lseek");
				goto fail;
			}
		}
	}

	close(fd);
	return -1;

	fail:
		if (fd >= 0) {
			close(fd);
		}

		return -1;
}