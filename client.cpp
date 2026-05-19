#include "common.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace fs = std::filesystem;

static int connect_to(const char* host) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port   = htons(PORT);
    if (inet_pton(AF_INET, host, &srv.sin_addr) <= 0) {
        std::cerr << "invalid host: " << host << "\n"; close(fd); return -1;
    }
    if (connect(fd, (sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("connect"); close(fd); return -1;
    }
    return fd;
}

static std::string read_line(int fd) {
    std::string s;
    char c;
    while (recv(fd, &c, 1, 0) == 1 && c != '\n') s += c;
    if (!s.empty() && s.back() == '\r') s.pop_back();
    return s;
}

static int do_upload(const char* host, const std::string& filepath) {
    if (!fs::exists(filepath)) { std::cerr << "file not found: " << filepath << "\n"; return 1; }

    uint64_t size = fs::file_size(filepath);
    std::string fname = fs::path(filepath).filename().string();

    int fd = connect_to(host);
    if (fd < 0) return 1;

    FileHeader hdr{};
    std::strncpy(hdr.cmd, "UPLOAD", sizeof(hdr.cmd));
    std::strncpy(hdr.name, fname.c_str(), sizeof(hdr.name) - 1);
    hdr.size = size;
    send_all(fd, &hdr, sizeof(hdr));

    std::string resp = read_line(fd);
    if (resp.substr(0, 2) != "OK") { std::cerr << resp << "\n"; close(fd); return 1; }
    std::cout << "Server ready. Sending " << fname << " (" << size << " bytes)...\n";

    std::ifstream in(filepath, std::ios::binary);
    char buf[BUF_SIZE];
    uint64_t sent = 0;
    while (in.read(buf, BUF_SIZE) || in.gcount() > 0) {
        size_t n = in.gcount();
        if (!send_all(fd, buf, n)) { std::cerr << "send error\n"; close(fd); return 1; }
        sent += n;
        std::cout << "\r  " << sent << " / " << size << " bytes" << std::flush;
    }
    std::cout << "\n";
    resp = read_line(fd);
    std::cout << resp << "\n";
    close(fd);
    return 0;
}

static int do_download(const char* host, const std::string& fname) {
    int fd = connect_to(host);
    if (fd < 0) return 1;

    FileHeader hdr{};
    std::strncpy(hdr.cmd, "DOWNLOAD", sizeof(hdr.cmd));
    std::strncpy(hdr.name, fname.c_str(), sizeof(hdr.name) - 1);
    hdr.size = 0;
    send_all(fd, &hdr, sizeof(hdr));

    std::string resp = read_line(fd);
    if (resp.substr(0, 2) != "OK") { std::cerr << resp << "\n"; close(fd); return 1; }

    uint64_t size = std::stoull(resp.substr(3));
    std::cout << "Downloading " << fname << " (" << size << " bytes)...\n";

    std::ofstream out(fname, std::ios::binary | std::ios::trunc);
    if (!out) { std::cerr << "cannot write " << fname << "\n"; close(fd); return 1; }

    char buf[BUF_SIZE];
    uint64_t received = 0;
    while (received < size) {
        size_t chunk = std::min((uint64_t)BUF_SIZE, size - received);
        ssize_t n = recv(fd, buf, chunk, 0);
        if (n <= 0) { std::cerr << "receive error\n"; close(fd); return 1; }
        out.write(buf, n);
        received += n;
        std::cout << "\r  " << received << " / " << size << " bytes" << std::flush;
    }
    std::cout << "\nDownload complete: " << fname << "\n";
    close(fd);
    return 0;
}

static void usage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " <host> upload <filepath>\n"
              << "  " << prog << " <host> download <filename>\n";
}

int main(int argc, char* argv[]) {
    if (argc < 4) { usage(argv[0]); return 1; }

    const char* host = argv[1];
    std::string cmd  = argv[2];
    std::string file = argv[3];

    if (cmd == "upload")   return do_upload(host, file);
    if (cmd == "download") return do_download(host, file);

    usage(argv[0]);
    return 1;
}
