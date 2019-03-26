#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>	/* open */
#include <sys/stat.h>	/* open */
#include <fcntl.h>	/* O_RDWR, O_CREAT, O_TRUNC, O_WRONLY */
#include <unistd.h>	/* close */

#include "so_stdio.h"

#define DEFAULT_BUF_SIZE 4096
#define READ 1
#define WRITE 2
#define APPEND 3
#define TRUNC 4

struct _so_file
{
        char *pathname;
        char *buffer;
        unsigned int read_pos;
        unsigned int write_pos;
        unsigned int buff_pos;
        char mode[3];
        int fd;
        char reached_end;
        char had_error;
        char last_op;
};

FUNC_DECL_PREFIX int so_fgetc(SO_FILE *stream)
{
        char possible_modes[4][3] = {"r", "r+", "w+", "a+"};
        char exists = 0;
        int i;

        if (stream->reached_end)
                return SO_EOF;

        for (i = 0; i < 4; i++)
                if (!strcmp(possible_modes[i], stream->mode)) {
                        exists = 1;
                        break;
                }

        if (!exists) {
                stream->had_error = 1;
                return 0;
        }

        if (strlen(stream->buffer) == 0) {
                int count = read(stream->fd, stream->buffer, DEFAULT_BUF_SIZE);
                if (count == 0) {
                        stream->reached_end = 1;
                        return SO_EOF;
                }

                stream->buff_pos++;
                stream->read_pos++;

        } else {
                if (stream->buff_pos == strlen(stream->buffer) - 1) {
                        memset(stream->buffer, 0, DEFAULT_BUF_SIZE);
                        int count = read(stream->fd, stream->buffer, DEFAULT_BUF_SIZE);
                        if (count == 0) {
                                stream->reached_end = 1;
                                return SO_EOF;
                        }

                        stream->buff_pos = 0;
                        stream->read_pos++;
                } else {
                        stream->buff_pos++;
                        stream->read_pos++;
                }
        }

        stream->last_op = 2;
        return (int) stream->buffer[stream->buff_pos];
}

FUNC_DECL_PREFIX int so_fclose(SO_FILE *stream)
{
        if (stream->last_op == 1) {
                int cnt = so_fflush(stream);
                if (cnt == SO_EOF)
                        return -1;
        }
        int fd = stream->fd;
        free(stream->pathname);
        free(stream->buffer);
        free(stream);

        return close(fd);
}

FUNC_DECL_PREFIX SO_FILE *so_fopen(const char *pathname, const char *mode)
{
        int path_size = strlen(pathname) + 1;
        int cursor_pos = 0;
        int fd = -1, offset = 0;
        char possible_modes[6][3] = {"r", "r+", "w", "w+", "a", "a+"};
        char exists = 0, i;
        mode_t mod = 0;
        SO_FILE *file;

        // check if file open mode is correct
        for (i = 0; i < 6; i++)
                if (!strcmp(possible_modes[i], mode)) {
                        exists = 1;
                        break;
                }

        if (!exists || strlen(mode) > 2)
                return NULL;

        if (!strcmp(mode, "r"))
                mod = O_RDONLY;
        if (!strcmp(mode, "r+"))
                mod = O_RDWR;
        if (!strcmp(mode, "w"))
                mod = O_WRONLY | O_TRUNC;
        if (!strcmp(mode, "w+"))
                mod = O_RDWR | O_CREAT | O_TRUNC;
        if (!strcmp(mode, "a"))
                mod = O_WRONLY | O_CREAT | O_APPEND;
        if (!strcmp(mode, "a+"))
                mod = O_RDWR | O_CREAT | O_APPEND;
        //file->mode = mod;

        // check if it's reading mode and the file doesn't exist
        if (access(pathname, F_OK ) == -1) {
                if (!strcmp(mode, "r") || !strcmp(mode, "r+")) {
                        return NULL;
                }
                fd = creat(pathname, 0644);
                if (fd < 0)
                        return NULL;
        }

        // allocate memory for the pointer and check if there are errors
        file = (SO_FILE *) malloc(sizeof(SO_FILE));

        if (file == NULL)
                return NULL;

        strcpy(file->mode, mode);

        // check if the file exists
        file->pathname = (char *) malloc(path_size);
        if (file->pathname == NULL)
                return NULL;

        memset(file->pathname, 0, path_size);
        strcpy(file->pathname, pathname);

        // create buffer
        file->buffer = (char *) malloc(DEFAULT_BUF_SIZE + 1);
        if (file->buffer == NULL) {
                free(file->pathname);
                free(file);
                return NULL;
        }
        memset(file->buffer, 0, DEFAULT_BUF_SIZE + 1);

        // position cursor depending on file open mode
        file->read_pos = 0;
        file->write_pos = 0;
        file->buff_pos = -1;
        file->reached_end = 0;
        file->last_op = 0;
        if (!strcmp(mode, "a") || !strcmp(mode, "a+")) {
                fd = open(pathname, O_RDONLY);
                if (fd > 0)
                        offset = lseek(fd, 0, SEEK_END);
                file->write_pos = offset;
        }

        if (fd < 0) {
                file->fd = open(pathname, mod, 0644);
                if (file->fd < 0) {
                        free(file->pathname);
                        free(file->buffer);
                        free(file);
                        return NULL;
                }
        } else
                file->fd = fd;

        return file;
}

FUNC_DECL_PREFIX int so_fileno(SO_FILE *stream)
{
        return stream->fd;
}

FUNC_DECL_PREFIX int so_feof(SO_FILE *stream)
{
        return stream->reached_end;
}

FUNC_DECL_PREFIX int so_fputc(int c, SO_FILE *stream)
{
        char possible_modes[4][3] = {"r+", "w", "w+", "a+"};
        char exists = 0;
        int i;
        ssize_t count = 0;

        for (i = 0; i < 4; i++)
                if (!strcmp(possible_modes[i], stream->mode)) {
                        exists = 1;
                        break;
                }

        if (!exists) {
                stream->had_error = 1;
                return 0;
        }

        if (strlen(stream->buffer) == DEFAULT_BUF_SIZE) {
                count = write(stream->fd, stream->buffer, DEFAULT_BUF_SIZE);

                if (count == 0) {
                        stream->had_error = 1;
                        return SO_EOF;
                }

                memset(stream->buffer, 0, DEFAULT_BUF_SIZE + 1);
                stream->buff_pos = 0;
                stream->write_pos += count;

        } else {
                stream->buff_pos++;
        }

        stream->buffer[stream->buff_pos] = (unsigned char) c;
        stream->last_op = 1;
        return c;
}

FUNC_DECL_PREFIX int so_fflush(SO_FILE *stream)
{
        char possible_modes[4][3] = {"r+", "w", "w+", "a+"};
        char exists = 0;
        int i, start = 0;
        ssize_t count = 0;

        for (i = 0; i < 4; i++)
                if (!strcmp(possible_modes[i], stream->mode)) {
                        exists = 1;
                        break;
                }

        if (!exists) {
                stream->had_error = 1;
                return SO_EOF;
        }

        count = write(stream->fd, stream->buffer, strlen(stream->buffer));
        if (count == 0) {
                stream->had_error = 1;
                return SO_EOF;
        }

        memset(stream->buffer, 0, DEFAULT_BUF_SIZE + 1);
        stream->buff_pos = -1;

        return 0;
}

FUNC_DECL_PREFIX
size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
        char *p = ptr;
        size_t i, count = 0;
        for (i = 0; i < nmemb * size; i++) {
                unsigned char ch = (unsigned char) so_fgetc(stream);
                if (ch == SO_EOF)
                        break;
                memcpy(p + i, &ch, 1);
                count++;
        }

        return count;
}

FUNC_DECL_PREFIX
size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
        char *p = ptr;
        size_t i, count = 0;

        for (i = 0; i < nmemb * size; i++) {
                unsigned char ch = (unsigned char) so_fputc((int) *(p + i), stream);
                if (ch == SO_EOF)
                        break;
                count++;
        }

        return count;
}

/*int main() {
        char name[] = "test_file.txt";
        SO_FILE *file = so_fopen(name, "r");

        if (file == NULL) {
                printf("Failed to open file!\n");
                return 0;
        }

        printf("pathname: %s\n", file->pathname);
        printf("buffer: %s\n", file->buffer);
        printf("mode: %s\n", file->mode);
        printf("fd: %d\n", file->fd);
        printf("read_pos: %d\n", file->read_pos);
        printf("write_pos: %d\n", file->write_pos);
        printf("buff_pos: %d\n\n\n", file->buff_pos);

        int c = 0;
        printf("Text:\n");

        char buff[50];
        memset(buff, 0, 50);
        int cnt = so_fread(buff, 1, 50, file);
        printf("Cnt = %d\n", cnt);
        if (cnt == 0) {
                printf("Eroare de citire!\n");
        } else {
                printf("%s\n", buff);
        }
        //so_fflush(file);
        so_fclose(file);

        return 0;
}*/
