#pragma once
#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

static constexpr int     PORT     = 9090;
static constexpr size_t  BUF_SIZE = 65536;

// Wire header for every transfer (sent before the file payload)
struct FileHeader {
    char     cmd[8];       // "UPLOAD" or "DOWNLOAD", fixed width, not NUL-terminated
    char     name[256];    // filename (null-terminated)
    uint64_t size;         // file size in bytes (0 for download requests)
};

inline bool send_all(int fd, const void* buf, size_t len) {
    const char* p = static_cast<const char*>(buf);
    while (len > 0) {
        ssize_t n = send(fd, p, len, 0);
        if (n <= 0) return false;
        p += n; len -= n;
    }
    return true;
}

inline bool recv_all(int fd, void* buf, size_t len) {
    char* p = static_cast<char*>(buf);
    while (len > 0) {
        ssize_t n = recv(fd, p, len, 0);
        if (n <= 0) return false;
        p += n; len -= n;
    }
    return true;
}
