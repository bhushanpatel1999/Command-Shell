#include "io61.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <climits>
#include <cerrno>

// Data structure for io61 file wrappers. Add your own stuff.

struct io61_file {
    // File descriptor
    int fd;
    // Size of cache
    static constexpr off_t cache_size = 8192;
    // Create cache
    unsigned char cache[cache_size]; 
    // Cache tags from section
    off_t tag;      
    off_t end_tag; 
    off_t pos_tag;
    // Mode of file (read or write)
    int mode;
};

// io61_fdopen(fd, mode)
//    Return a new io61_file for file descriptor `fd`. `mode` is
//    either O_RDONLY for a read-only file or O_WRONLY for a
//    write-only file. You need not support read/write files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = new io61_file;
    f->fd = fd;
    // Add mode to struct to mark file type
    f->mode = mode;
    // Reset tags
    f->tag = 0;
    f->end_tag = 0;
    f->pos_tag = 0;
    return f;
}

// io61_close(f)
// Close the io61_file `f` and release all its resources.

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
    // Temporary buffer
    unsigned char temp[1];
    int check = io61_read(f, temp, 1);
    if (check == (int) 1) {
        return temp[0];
    } else {
        return EOF;
    }
}

// Use this function to fill cache when reading

int io61_fill(io61_file* f) {
    // Clear cache
    f->tag = f->pos_tag = f->end_tag;
    // Declare variable to hold "read" return value
    int bytes_read;
    bytes_read = read(f->fd, f->cache, f->cache_size);
    if (bytes_read >= 0) {
        f->end_tag = (f->tag + (off_t) bytes_read); 
    }
    return bytes_read;
}

// io61_read(f, buf, sz)
//    Read up to `sz` characters from `f` into `buf`. Returns the number of
//    characters read on success; normally this is `sz`. Returns a short
//    count, which might be zero, if the file ended before `sz` characters
//    could be read. Returns -1 if an error occurred before any characters
//    were read.


ssize_t io61_read(io61_file* f, unsigned char* buf, size_t sz) {
    // Check invariants.
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->cache_size);
    // Keep track of bytes read to buffer
    int bytes_read = 0;
    // Declare variable to track how many bytes to memcpy
    int count_bytes;
    while (bytes_read < (int) sz) {
        // Check if cache is even filled. If not, fill it
        if (f->pos_tag == f->end_tag) {
            int check = io61_fill(f);
            // Check to see if it was filled. If not, you're at EOF so exit
            if (check == 0) {
                // Means no more bytes to read
                break;
            }
            if (check == -1) {
                // Means error
                return -1;
            }
        }
        // According to Ed #936, bytes left to read can be smaller than "sz" if the file
        // is smaller than "sz" (like in blockcat), so must check the minimum 
        // of (bytes left to read and cache size)
        if ( (sz - (size_t) bytes_read) < (size_t) (f->end_tag - f->pos_tag) ) {
            count_bytes = sz - bytes_read;
        }
        else {
            count_bytes = f->end_tag - f->pos_tag;
        }
        // Use memcpy to copy count_bytes from cache
        memcpy(&buf[bytes_read], &f->cache[f->pos_tag - f->tag], count_bytes);
        // Increment by # of bytes copied
        bytes_read += count_bytes;
        // Increment pos_tag 
        f->pos_tag += count_bytes;
    }
    return bytes_read;
    // Note: This function never returns -1 because `io61_readc`
    // does not distinguish between error and end-of-file.
    // Your final version should return -1 if a system call indicates
    // an error.
}

// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success or
//    -1 on error.

int io61_writec(io61_file* f, int ch) {
    // Temporary buffer
    unsigned char temp[1];
    temp[0] = ch;
    int check = io61_write(f, temp, 1);
    if (check == 1) {
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
    //Check invariants.
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->cache_size);
    assert(f->pos_tag == f->end_tag);
    // Keep track of bytes written to buffer
    int bytes_written = 0;
    // Declare variable to track how many bytes to memcpy
    int count_bytes;
    while (bytes_written < (int) sz) {
        // Check if cache filled. If so, flush it
        if (f->pos_tag == f->tag + f->cache_size) {
            io61_flush(f);
        }
        // Similar minimum logic as io61_read above
        if ( (sz - (size_t) bytes_written) < (size_t) (f->tag + f->cache_size - f->pos_tag) ) {
            count_bytes = sz - bytes_written;
        }
        else {
            count_bytes = f->tag + f->cache_size - f->pos_tag;
        }
        // Use memcpy to write count_bytes to cache
        memcpy(&f->cache[f->pos_tag - f->tag], &buf[bytes_written], count_bytes);
        // Increment by # of bytes copied
        bytes_written += count_bytes;
        // Increment end_tag and pos_tag 
        f->end_tag = f->pos_tag += count_bytes;
    }
    return bytes_written;
}

// io61_flush(f)
//    Forces a write of all buffered data written to `f`.
//    If `f` was opened read-only, io61_flush(f) may either drop all
//    data buffered for reading, or do nothing.

int io61_flush(io61_file* f) {
    if (f->mode == O_WRONLY) {
        // Write to output file
        int check = write(f->fd, f->cache, f->pos_tag - f->tag);
        if (check < 0) {
            return -1;
        }
        // Mover start tag to match bytes written
        f->tag = f->pos_tag;
        return 0;
    }
    else {
        return 0;
    }
}

// io61_seek(f, pos)
//    Change the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t pos) {
    // Aligned position variable
    off_t align = pos;
    // Flush to output file
    if (f->mode == O_WRONLY) {
        io61_flush(f);
    }
    else {
        // Align to closest, smaller cache_size-divisible position
        off_t leftover = pos % f->cache_size;
        align -= leftover;
    }
    // If we're already aligned then just move tag and exit
    if (f->tag == align) {
        f->pos_tag = pos;
        return 0;
    }
    // lseek to "align", verify with if-statement, and set tags
    if (lseek(f->fd, align, SEEK_SET) == (int) align) {
        f->tag = f->end_tag = align;
        if (f->mode == O_RDONLY) {
            io61_fill(f);
        }
        f->pos_tag = pos;
        return 0;
    }
    return 0;
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