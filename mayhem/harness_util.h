// Scratch helpers shared by the harnesses: everything lives under $TMPDIR (default /tmp), is unique per
// call, and is removed again before the input returns. A harness that lets the library create sidecar files
// (SFileCompactArchive's "<archive>.tmp", stream-provider files) gives each input its own directory
// (make_scratch_dir) and removes the whole tree at the end (remove_tree).
#pragma once
#include <cstdint>
#include <fcntl.h>
#include <ftw.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

static inline std::string scratch_dir()
{
    const char * d = getenv("TMPDIR");
    return (d != NULL && *d != 0) ? d : "/tmp";
}

// Creates (or truncates) the file at path and fills it with data[0..size).
static inline bool write_file(const std::string & path, const uint8_t * data, size_t size)
{
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if(fd < 0)
        return false;
    size_t done = 0;
    while(done < size)
    {
        ssize_t n = write(fd, data + done, size - done);
        if(n <= 0)
            break;
        done += (size_t)n;
    }
    close(fd);
    return done == size;
}

// Creates a unique file holding data[0..size); returns its path, or "" on failure.
static inline std::string write_scratch(const uint8_t * data, size_t size, const char * tag)
{
    std::string path = scratch_dir() + "/sl_" + tag + "_XXXXXX";
    int fd = mkstemp(&path[0]);
    if(fd < 0)
        return "";
    close(fd);
    if(!write_file(path, data, size))
    {
        unlink(path.c_str());
        return "";
    }
    return path;
}

// Reserves a unique, empty path (the file exists, empty) for a library to create or overwrite.
static inline std::string reserve_scratch(const char * tag)
{
    return write_scratch(NULL, 0, tag);
}

// Creates a unique empty directory; returns its path, or "" on failure.
static inline std::string make_scratch_dir(const char * tag)
{
    std::string path = scratch_dir() + "/sl_" + tag + "_XXXXXX";
    return mkdtemp(&path[0]) != NULL ? path : "";
}

static inline int remove_tree_entry(const char * p, const struct stat *, int, struct FTW *)
{
    remove(p);
    return 0;
}

// Removes a directory and everything the library left in it.
static inline void remove_tree(const std::string & dir)
{
    if(!dir.empty())
        nftw(dir.c_str(), remove_tree_entry, 16, FTW_DEPTH | FTW_PHYS);
}
