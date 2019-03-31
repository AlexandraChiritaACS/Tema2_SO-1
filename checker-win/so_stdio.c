#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "so_stdio.h"

#define DEFAULT_BUF_SIZE 4096
#define WRITE 1
#define READ 2
#define APPEND 3
#define TRUNC 4

struct _so_file {
	char *pathname;
	char *buffer;
	long read_pos;
	long write_pos;
	int buff_pos;
	int buff_size;
	unsigned char mode[3];
	HANDLE fd;
	char reached_end;
	char had_error;
	char last_op;
};

int so_feof(SO_FILE *stream)
{
	return (stream->reached_end) ? SO_EOF : 0;
}

long so_ftell(SO_FILE *stream)
{
	if (!strcmp("w", stream->mode) || !strcmp("a", stream->mode))
		return stream->write_pos + strlen(stream->buffer);
	if (!strcmp("r", stream->mode))
		return stream->read_pos;
	return 0;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	DWORD count = SetFilePointer(
								stream->fd,
								offset,
								NULL,
								whence
								);

	if (!strcmp("w", stream->mode) || !strcmp("a", stream->mode))
		stream->write_pos = count;
	if (!strcmp("r", stream->mode))
		stream->read_pos = count;
	return (count >= 0) ? 0 : -1;
}

int so_ferror(SO_FILE *stream)
{
	return (stream->had_error) ? SO_EOF : 0;
}

SO_FILE *so_popen(const char *command, const char *type)
{

}

int so_pclose(SO_FILE *stream)
{

}

int so_fflush(SO_FILE *stream)
{
	int start = 0;
	BOOL count = FALSE;
	LPWORD lpNumberOfBytesWritten = 0;

	count = WriteFile(
				stream->fd,
				stream->buffer,
				stream->buff_pos + 1,
				&lpNumberOfBytesWritten,
				NULL
				);
	if (count == FALSE)
		stream->had_error = 1;

	if (lpNumberOfBytesWritten == 0)
		return 0;

	memset(stream->buffer, 0, DEFAULT_BUF_SIZE + 1);
	stream->buff_pos = -1;

	return 0;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	char *p = ptr;
	size_t i, count = 0;
	unsigned char ch;

	stream->had_error = 0;

	for (i = 0; i < nmemb * size; i++) {
		ch = (unsigned char) so_fputc((int) *(p + i), stream);
		if (ch == SO_EOF)
			break;
		count++;
	}

	if (stream->had_error)
		return 0;

	return count;
}

int so_fputc(int c, SO_FILE *stream)
{
	char exists = 0;
	int i;
	BOOL count = FALSE;
	LPWORD lpNumberOfBytesWritten = 0;

	if (stream->buff_pos == DEFAULT_BUF_SIZE - 1) {
		count = WriteFile(
				stream->fd,
				stream->buffer,
				DEFAULT_BUF_SIZE,
				&lpNumberOfBytesWritten,
				NULL
				);

		if (count == FALSE)
			stream->had_error = 1;

		if (lpNumberOfBytesWritten == 0)
			return SO_EOF;

		memset(stream->buffer, 0, DEFAULT_BUF_SIZE + 1);
		stream->buff_pos = 0;
		stream->write_pos += count;

	} else
		stream->buff_pos++;

	stream->buffer[stream->buff_pos] = (unsigned char) c;
	stream->last_op = WRITE;
	return c;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	char *p = ptr;
	size_t i, count = 0;
	unsigned char ch;

	stream->had_error = 0;

	for (i = 0; i < nmemb * size; i++) {
		ch = (unsigned char) so_fgetc(stream);

		if (ch == SO_EOF)
			break;

		memcpy(p + i, &ch, 1);

		if (!stream->had_error)
			count++;
	}

	if (stream->had_error)
		return 0;

	return count;
}

HANDLE so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

int so_fgetc(SO_FILE *stream)
{
	char exists = 0, good = 0;
	int i = 0, j = 0;
	BOOL count = FALSE;
	DWORD dwBytesToRead = DEFAULT_BUF_SIZE, dwBytesRead = 0;
	
	if (stream->reached_end)
		return SO_EOF;

	if (stream->buff_pos >= stream->buff_size - 1 && stream->buff_pos > -1
		&& stream->buff_size < DEFAULT_BUF_SIZE - 1) {
		stream->reached_end = 1;
		return SO_EOF;
	}

	if (stream->buff_pos == DEFAULT_BUF_SIZE - 1
		|| stream->buff_pos == -1) {
		memset(stream->buffer, 0, DEFAULT_BUF_SIZE);
		count = ReadFile(
						stream->fd,
						stream->buffer,
						dwBytesToRead,
						&dwBytesRead,
						NULL);

		if (count == FALSE) {
			stream->had_error = 1;
			return SO_EOF;
		}

		if (dwBytesRead == 0) {
			stream->reached_end = 1;
			return SO_EOF;
		}

		stream->buff_size = dwBytesRead;
		stream->buff_pos = 0;
		stream->read_pos++;

	} else {
		stream->buff_pos++;
		stream->read_pos++;
	}

	stream->last_op = READ;
	return (int) stream->buffer[stream->buff_pos];
}

int so_fclose(SO_FILE *stream)
{
	BOOL bRet = FALSE;
	HANDLE handle = stream->fd;
	char hadError = stream->had_error;
	int cnt = 0;

	stream->had_error = 0;
	if (stream->last_op == WRITE) {
		cnt = so_fflush(stream);

		if (cnt == SO_EOF)
			return -1;
	}

	free(stream->pathname);
	free(stream->buffer);
	bRet = CloseHandle(handle);
	free(stream);

	return 0;
}

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	int path_size = strlen(pathname) + 1;
	int cursor_pos = 0;
	int fd = -1, offset = 0, i = 0;
	char possible_modes[6][3] = {"r", "r+", "w", "w+", "a", "a+"};
	char exists = 0;
	SO_FILE *file = NULL;
	DWORD fileAttr = 0;
	DWORD creation = 0, access = 0;
	HANDLE handle = 0;

	for (i = 0; i < 6; i++)
		if (!strcmp(possible_modes[i], mode)) {
			exists = 1;
			break;
		}

	if (!exists || strlen(mode) > 2)
		return NULL;

	if (!strcmp(mode, "r")) {
		access = GENERIC_READ;
		creation = OPEN_EXISTING;
	}
	if (!strcmp(mode, "r+"))
		access = GENERIC_READ | GENERIC_WRITE;

		if (!strcmp(mode, "w")) {
		access = GENERIC_WRITE;
		creation = CREATE_ALWAYS;
	}
	if (!strcmp(mode, "w+")) {
		access = GENERIC_READ | GENERIC_WRITE;
		creation = TRUNCATE_EXISTING;
	}
	if (!strcmp(mode, "a")) {
		access = FILE_APPEND_DATA;
		creation = TRUNCATE_EXISTING | CREATE_NEW;
	}
	if (!strcmp(mode, "a+")) {
		access = FILE_APPEND_DATA | GENERIC_READ;
		creation = CREATE_NEW;
	}

	fileAttr = GetFileAttributes(pathname);
	if (fileAttr == 0xFFFFFFFF) {
		if (!strcmp(mode, "r") || !strcmp(mode, "r+"))
			return NULL;
	}

	// allocate memory for the pointer and check if there are errors
	file = (SO_FILE *) malloc(sizeof(SO_FILE));

	if (file == NULL)
		return NULL;

	for (i = 0; i < strlen(mode); i++)
		file->mode[i] = mode[i];
	file->mode[2] = '\0';

	// check if the file exists
	file->pathname = (char *) malloc(path_size);
	if (file->pathname == NULL)
		return NULL;

	memset(file->pathname, 0, path_size);
	for (i = 0; i < strlen(pathname); i++)
		file->pathname[i] = pathname[i];
	file->pathname[strlen(pathname)] = '\0';

	// create buffer
	file->buffer = (char *) malloc(DEFAULT_BUF_SIZE * 2);
	if (file->buffer == NULL) {
		free(file->pathname);
		free(file);
		return NULL;
	}
	memset(file->buffer, 0, DEFAULT_BUF_SIZE * 2);

	// position cursor depending on file open mode
	file->read_pos = 0;
	file->write_pos = 0;
	file->buff_pos = -1;
	file->reached_end = 0;
	file->last_op = 0;
	file->buff_size = 0;

	handle = CreateFile(
		pathname,
		access,
		FILE_SHARE_WRITE | FILE_SHARE_WRITE,
		NULL,
		creation,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if (handle == INVALID_HANDLE_VALUE) {
		free(file->pathname);
		free(file->buffer);
		free(file);
		return NULL;
	}

	file->fd = handle;

	return file;
}
