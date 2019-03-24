#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>	/* open */
#include <sys/stat.h>	/* open */
#include <fcntl.h>	/* O_RDWR, O_CREAT, O_TRUNC, O_WRONLY */
#include <unistd.h>	/* close */

#include "so_stdio.h"

#define DEFAULT_BUF_SIZE 4
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
};

FUNC_DECL_PREFIX int so_fgetc(SO_FILE *stream)
{
        char possible_modes[4][3] = {"r", "r+", "w+", "a+"};
        char exists = 0;
        int i;

        for (i = 0; i < 4; i++)
                if (!strcmp(possible_modes[i], stream->mode)) {
                        exists = 1;
                        break;
                }

        if (!exists)
                return 0;

        if (strlen(stream->buffer) == 0) {
                int count = read(stream->fd, stream->buffer, DEFAULT_BUF_SIZE);
                if (count == 0) {
                        printf("EOF\n");
                        return SO_EOF;
                }

                stream->buff_pos++;
                stream->read_pos++;

        } else {
                if (stream->buff_pos == strlen(stream->buffer) - 1) {
                        memset(stream->buffer, 0, DEFAULT_BUF_SIZE);
                        int count = read(stream->fd, stream->buffer, DEFAULT_BUF_SIZE);
                        if (count == 0) {
                                return SO_EOF;
                        }

                        stream->buff_pos = 0;
                        stream->read_pos++;
                } else {
                        stream->buff_pos++;
                        stream->read_pos++;
                }
        }

        return (int) stream->buffer[stream->buff_pos];
}

FUNC_DECL_PREFIX int so_fclose(SO_FILE *stream)
{
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
        int fd, offset = 0;
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
                mod = O_WRONLY;
        if (!strcmp(mode, "w+"))
                mod = O_RDWR | O_CREAT | O_TRUNC;
        if (!strcmp(mode, "a"))
                mod = O_WRONLY | O_CREAT | O_APPEND;
        if (!strcmp(mode, "a+"))
                mod = O_RDWR | O_CREAT | O_APPEND;
        //file->mode = mod;

        // check if it's reading mode and the file doesn't exist
        if (!strcmp(mode, "r") || !strcmp(mode, "r+")) {
                if (access(pathname, F_OK ) == -1)
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
        if (!strcmp(mode, "a") || !strcmp(mode, "a+")) {
                fd = open(pathname, O_RDONLY);
                if (fd > 0)
                        offset = lseek(fd, 0, SEEK_END);
                file->write_pos = offset;
        }

        file->fd = open(pathname, mod, 0644);
        if (file->fd < 0) {
                free(file->pathname);
                free(file->buffer);
                free(file);
                return NULL;
        }

        return file;
}

int main() {
        char name[] = "test_file.txt";
        SO_FILE *file = so_fopen(name, "a+");

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
        while ((c = so_fgetc(file)) != SO_EOF) {
                printf("%c", (char) c);
        }

        so_fclose(file);

        return 0;
}
