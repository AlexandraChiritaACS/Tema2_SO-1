#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>	/* open */
#include <sys/stat.h>	/* open */
#include <fcntl.h>	/* O_RDWR, O_CREAT, O_TRUNC, O_WRONLY */
#include <unistd.h>	/* close */

#include "so_stdio.h"

#define DEFAULT_BUF_SIZE 1024
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
        int8_t mode;
};

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
        int path_size = strlen(pathname) + 1;
        int cursor_pos = 0;
        int fd, offset = 0;
        char possible_modes[6][2] = {"r", "r+", "w", "w+", "a", "a+"};
        char exists = 0, i;
        SO_FILE *file;

        // check if file open mode is correct
        for (i = 0; i < 6; i++)
                if (!strcmp(possible_modes[i], mode)) {
                        exists = 1;
                        break;
                }

        if (!exists)
                return NULL;

        // check if it's reading mode and the file doesn't exist
        if (!strcmp(mode, "r") || !strcmp(mode, "r+")) {
                if (access( fname, F_OK ) == -1)
                        return NULL;
        }

        // allocate memory for the pointer and check if there are errors
        file = (SO_FILE *) malloc(sizeof(SO_FILE));

        if (file == NULL)
                return NULL;

        // check if the file exists
        file->pathname = (char *) malloc(path_size);
        if (file->pathname == NULL)
                return NULL;

        memset(file->pathname, 0, path_size);
        strcpy(file->pathname, pathname);

        // create buffer
        file->buffer = (char *) malloc(DEFAULT_BUF_SIZE);
        if (file->buffer == NULL) {
                free(file->pathname);
                free(file);
                return NULL;
        }
        memset(file->buffer, 0, DEFAULT_BUF_SIZE);

        // position cursor depending on file open mode
        file->read_pos = 0;
        file->write_pos = 0;
        if (!strcmp(mode, "a") || !strcmp(mode, "a+")) {
                fd = open(pathname, O_RDONLY);
                if (fd > 0)
                        offset = lseek(fd, 0, SEEK_END);
                file->write_pos = offset;
        }
        return file;
}

int so_fgetc(SO_FILE *stream)
{

}
