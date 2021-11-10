#include "io61.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <climits>
#include <cerrno>

// io61.c
//    YOUR CODE HERE!


// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.

struct io61_file {
    int fd;
    // Create cache
    char cache[4096]; 
    // Cache tags from section
    off_t tag;      
    off_t end_tag; 
    off_t pos_tag;
    // Size of cache
    int cache_size = 4096;
};


// io61_fdopen(fd, mode)
//    Return a new io61_file for file descriptor `fd`. `mode` is
//    either O_RDONLY for a read-only file or O_WRONLY for a
//    write-only file. You need not support read/write files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = new io61_file;
    f->fd = fd;
    (void) mode;
    return f;
}


// io61_close(f)
//    Close the io61_file `f` and release all its resources.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    delete f;
    return r;
}


// io61_readc(f)
//    Read a single (unsigned) character from `f` and return it. Returns EOF
//    (which is -1) on error or end-of-file.

int io61_readc(io61_file* f) {
    unsigned char buf[1];
    if (read(f->fd, buf, 1) == 1) {
        return buf[0];
    } else {
        return EOF;
    }
}


// io61_read(f, buf, sz)
//    Read up to `sz` characters from `f` into `buf`. Returns the number of
//    characters read on success; normally this is `sz`. Returns a short
//    count, which might be zero, if the file ended before `sz` characters
//    could be read. Returns -1 if an error occurred before any characters
//    were read.

// Use this to prefetch and fill cache
int io61_fill(io61_file* f) {
    // Clear cache
    f->tag = f->pos_tag = f->end_tag;
    // Declare variable to hold "read" return value
    int bytes_read;
    bytes_read = read(f->fd, f->cache, 4096);
    if (bytes_read != 0) {
        f->end_tag = (f->tag + bytes_read); 
    }
    return bytes_read;
}

ssize_t io61_read(io61_file* f, unsigned char* buf, size_t sz) {

    // size_t nread = 0;
    // while (nread != sz) {
    //     int ch = read(f->fd, buf, sz);
    //     if (ch == EOF) {
    //         break;
    //     }
    //     buf[nread] = ch;
    //     ++nread;
    // }
    // return nread;
    // nread = read(f->fd, buf, sz);
    // if (nread == -1) {
    //     return -1;
    // }
    // else {
    //     return nread;
    // }

    // Keep track of bytes read to buffer
    int bytes_read = 0;
    while (bytes_read < sz) {
        // Check if cache is even filled. If not, fill it
        if (f->pos_tag == f->end_tag) {
            io61_fill(f);
            // Check to see if it was filed. If not, you're at EOF so exit
            if (f->pos_tag == f->end_tag) {
                break;
            }
        }
        // Declare variable to track how many bytes to memcpy
        int count_bytes;
        // According to Ed #936, bytes left to read can be smaller than "sz" if the file
        // is smaller than "sz" (like in blockcat), so must check the minimum 
        // of (cache size and bytes left to read)
        if ( (sz - bytes_read) < (f->end_tag - f->pos_tag) ) {
            count_bytes = sz - bytes_read;
        }
        else {
            count_bytes = f->end_tag - f->pos_tag;
        }

        // Use memcpy to copy count_bytes from cache
        memcpy(&buf[bytes_read], &f->cache[f->end_tag - f->pos_tag], count_bytes);
        // Increment by # of bytes copied
        bytes_read += count_bytes;
        // Increment pos_tag so the if-statement at the 
        f->pos_tag += count_bytes;
    }
    return bytes_read;
    

    // if (f->pos_tag == f->end_tag) {
    //     return bytes_read;
    // }
    // if (sz <= f->cache_size) {
    //     memcpy(buf[bytes_read], &f->cache, sz);
    //     f->pos_tag += sz;
    //     bytes_read += sz;
    //     return bytes_read;
    // }
    // else {
    //     int bytes_left = sz;
    //     while (bytes_left >= f->cache_size) {
    //         memcpy(buf[bytes_read], &f->cache, f->cache_size);
    //         f->pos_tag += f->cache_size;
    //         bytes_left -= f->cache_size;
    //         bytes_read += f->cache_size;
    //         io61_fill(f);
    //         if (f->pos_tag == f->end_tag) {
    //             return bytes_read;
    //         }
    //     }
    //     memcpy(buf[bytes_read], &f->cache, bytes_left);
    //     f->pos_tag += bytes_left;
    //     bytes_read += bytes_left;
    //     return bytes_read;
    // }


    // Note: This function never returns -1 because `io61_readc`
    // does not distinguish between error and end-of-file.
    // Your final version should return -1 if a system call indicates
    // an error.
}


// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success or
//    -1 on error.

int io61_writec(io61_file* f, int ch) {
    unsigned char buf[1];
    buf[0] = ch;
    if (write(f->fd, buf, 1) == 1) {
        return 0;
    } else {
        return -1;
    }
}


// io61_write(f, buf, sz)
//    Write `sz` characters from `buf` to `f`. Returns the number of
//    characters written on success; normally this is `sz`. Returns -1 if
//    an error occurred before any characters were written.

ssize_t io61_write(io61_file* f, const unsigned char* buf, size_t sz) {
    // size_t nwritten = 0;
    // while (nwritten != sz) {
    //     if (write(f->fd, buf, sz) == -1) {
    //         break;
    //     }
    //     ++nwritten;
    // }
    // if (nwritten != 0 || sz == 0) {
    //     return nwritten;
    // } else {
    //     return -1;
    // }


    // nwritten = write(f->fd, buf, sz);
    // if (nwritten == -1) {
    //     return -1;
    // }
    // else {
    //     return nwritten;
    // }

    // assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    // assert(f->end_tag - f->pos_tag <= f->cache_size);

    // // Write cache invariant.
    // assert(f->pos_tag == f->end_tag);

    // // The desired data is guaranteed to lie within this cache slot.
    // assert(sz <= f->cache_size && f->pos_tag + sz <= f->tag + f->cache_size);

    /* ANSWER */
    size_t bytes_read;
    memcpy(&f->cache[f->pos_tag - f->tag], buf, sz);
    f->pos_tag += sz;
    f->end_tag += sz;
    return sz;
}


// io61_flush(f)
//    Forces a write of all buffered data written to `f`.
//    If `f` was opened read-only, io61_flush(f) may either drop all
//    data buffered for reading, or do nothing.

int io61_flush(io61_file* f) {
    // assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    // assert(f->end_tag - f->pos_tag <= f->cache_size);

    // // Write cache invariant.
    // assert(f->pos_tag == f->end_tag);

    /* ANSWER */
    int bytes_written = write(f->fd, f->cache, f->pos_tag - f->tag);
    f->tag = f->pos_tag;
    return bytes_written;
}


// io61_seek(f, pos)
//    Change the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t pos) {
    off_t r = lseek(f->fd, (off_t) pos, SEEK_SET);
    if (r == (off_t) pos) {
        return 0;
    } else {
        return -1;
    }
}


// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Open the file corresponding to `filename` and return its io61_file.
//    If `!filename`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != nullptr` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    if (filename) {
        fd = open(filename, mode, 0666);
    } else if ((mode & O_ACCMODE) == O_RDONLY) {
        fd = STDIN_FILENO;
    } else {
        fd = STDOUT_FILENO;
    }
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}


// io61_filesize(f)
//    Return the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode)) {
        return s.st_size;
    } else {
        return -1;
    }
}
