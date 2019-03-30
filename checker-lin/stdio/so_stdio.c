#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>	/* open */
#include <sys/stat.h>	/* open */
#include <fcntl.h>	/* O_RDWR, O_CREAT, O_TRUNC, O_WRONLY */
#include <unistd.h>	/* close */

#include "so_stdio.h"

#define DEFAULT_BUF_SIZE 4096
#define WRITE 1
#define READ 2
#define APPEND 3
#define TRUNC 4

struct _so_file
{
        char *pathname;
        char *buffer;
        long read_pos;
        long write_pos;
        int buff_pos;
        int buff_size;
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

        if (stream->buff_pos >= stream->buff_size - 1 && stream->buff_pos > -1 && stream->buff_size < DEFAULT_BUF_SIZE - 1) {
                stream->reached_end = 1;
                return SO_EOF;
        }

        if (stream->buff_pos == DEFAULT_BUF_SIZE - 1 || stream->buff_pos == -1) {
                memset(stream->buffer, 0, DEFAULT_BUF_SIZE);
                int count = read(stream->fd, stream->buffer, DEFAULT_BUF_SIZE);

                if (count == -1) {
                        stream->had_error = 1;
                        return SO_EOF;
                }

                if (count == 0) {
                        stream->reached_end = 1;
                        return SO_EOF;
                }

                stream->buff_size = count;
                stream->buff_pos = 0;
                stream->read_pos++;

        } else {
                stream->buff_pos++;
                stream->read_pos++;
        }

        stream->last_op = READ;
        return (int) stream->buffer[stream->buff_pos];
}

FUNC_DECL_PREFIX int so_fclose(SO_FILE *stream)
{
        stream->had_error = 0;
        if (stream->last_op == WRITE) {
                int cnt = so_fflush(stream);
                if (cnt == SO_EOF)
                        return -1;
        }
        int fd = stream->fd;
        int err = stream->had_error;
        free(stream->pathname);
        free(stream->buffer);
        free(stream);

        int ret = close(fd);

        if (err)
                return -1;
        return ret;
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
                mod = O_RDWR | O_CREAT;// | O_APPEND;
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
        file->buff_size = 0;
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
        return (stream->reached_end) ? SO_EOF : 0;
}

FUNC_DECL_PREFIX int so_fputc(int c, SO_FILE *stream)
{
        char possible_modes[5][3] = {"r+", "w", "w+", "a+", "a"};
        char exists = 0;
        int i;
        size_t count = 0;

        for (i = 0; i < 5; i++)
                if (!strcmp(possible_modes[i], stream->mode)) {
                        exists = 1;
                        break;
                }

        if (!exists) {
                stream->had_error = 1;
                return 0;
        }

        if (stream->buff_pos == DEFAULT_BUF_SIZE - 1) {
                count = write(stream->fd, stream->buffer, DEFAULT_BUF_SIZE);

                if (count == -1){
                        stream->had_error = 1;
                }

                if (count == 0) {
                        //stream->had_error = 1;
                        return SO_EOF;
                }

                memset(stream->buffer, 0, DEFAULT_BUF_SIZE + 1);
                stream->buff_pos = 0;
                stream->write_pos += count;

        } else {
                stream->buff_pos++;
        }

        stream->buffer[stream->buff_pos] = (unsigned char) c;
        stream->last_op = WRITE;
        return c;
}

FUNC_DECL_PREFIX int so_fflush(SO_FILE *stream)
{
        char possible_modes[5][3] = {"r+", "w", "w+", "a+", "a"};
        char exists = 0;
        int i, start = 0;
        ssize_t count = 0;

        for (i = 0; i < 5; i++)
                if (!strcmp(possible_modes[i], stream->mode)) {
                        exists = 1;
                        break;
                }

        if (!exists) {
                stream->had_error = 1;
                return SO_EOF;
        }

        count = write(stream->fd, stream->buffer, stream->buff_pos + 1);
        if (count == -1)
                stream->had_error = 1;

        if (count == 0)
                return 0;

        memset(stream->buffer, 0, DEFAULT_BUF_SIZE + 1);
        stream->buff_pos = -1;

        return 0;
}

FUNC_DECL_PREFIX
size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
        char *p = ptr;
        size_t i, count = 0;
        stream->had_error = 0;

        for (i = 0; i < nmemb * size; i++) {
                unsigned char ch = (unsigned char) so_fgetc(stream);
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

FUNC_DECL_PREFIX
size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
        char *p = ptr;
        size_t i, count = 0;
        stream->had_error = 0;

        for (i = 0; i < nmemb * size; i++) {
                unsigned char ch = (unsigned char) so_fputc((int) *(p + i), stream);
                if (ch == SO_EOF)
                        break;
                count++;
        }

        if (stream->had_error)
                return 0;

        return count;
}

FUNC_DECL_PREFIX long so_ftell(SO_FILE *stream)
{
        if (!strcmp("w", stream->mode) || !strcmp("a", stream->mode))
                return stream->write_pos + strlen(stream->buffer);
        if (!strcmp("r", stream->mode))
                return stream->read_pos;
        return 0;
}

FUNC_DECL_PREFIX int so_fseek(SO_FILE *stream, long offset, int whence)
{
        off_t count = lseek(stream->fd, offset, whence);

        if (!strcmp("w", stream->mode) || !strcmp("a", stream->mode))
                stream->write_pos = count;
        if (!strcmp("r", stream->mode))
                stream->read_pos = count;
        return (count >= 0) ? 0 : -1;
}

FUNC_DECL_PREFIX int so_ferror(SO_FILE *stream)
{
        return (stream->had_error) ? SO_EOF : 0;
}

FUNC_DECL_PREFIX SO_FILE *so_popen(const char *command, const char *type)
{

}

FUNC_DECL_PREFIX int so_pclose(SO_FILE *stream)
{
        
}

/*int main() {
        char name[] = "test_file.txt";
        int fd = open(name, O_RDWR | O_CREAT, 0644);

        int cnt= write(fd, name, strlen(name));
        printf("%d\n", cnt);
        char bf[] = "TEXT";
        cnt = lseek(fd, -5, SEEK_END);
        write(fd, bf, strlen(bf));
        //char c[100];
        //read(fd, c, 5);
        //printf("Read:%s\n", c);

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

        char buf[] = "Ana are mere";
        int cnt = so_fwrite(buf, 1, strlen(buf), file);//so_fwrite(buf, 1, buf_len, file);
        printf("%d\n", cnt);
        so_fflush(file);

        lseek(file->fd, -50, SEEK_END);
        //so_fseek(file, -5, SEEK_SET);
        char buff[] = "Lidia";
        cnt = so_fwrite(buff, 1, strlen(buff), file);//so_fwrite(buf, 1, buf_len, file);
        printf("%d\n", cnt);
        //so_fflush(file);
        so_fclose(file);

        return 0;
}*/
