#include "common.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <filesystem>
#include <cstring>
#include <ctime>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace fs = std::filesystem;

static std::mutex log_mu;

static void log(const std::string& addr, const std::string& msg) {
    auto t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
    std::lock_guard<std::mutex> lk(log_mu);
    std::cout << "[" << buf << "] [" << addr << "] " << msg << "\n" << std::flush;
}

static void send_status(int fd, bool ok, const std::string& msg) {
    std::string s = (ok ? "OK " : "ERR ") + msg + "\n";
    send_all(fd, s.c_str(), s.size());
}

static void handle_upload(int fd, const FileHeader& hdr, const std::string& addr) {
    std::string fname(hdr.name);
    uint64_t    total = hdr.size;
    log(addr, "UPLOAD " + fname + " (" + std::to_string(total) + " bytes)");

    // Sanitise: strip any path component
    fname = fs::path(fname).filename().string();
    if (fname.empty() || fname[0] == '.') { send_status(fd, false, "invalid filename"); return; }

    fs::create_directories("uploads");
    std::ofstream out("uploads/" + fname, std::ios::binary | std::ios::trunc);
    if (!out) { send_status(fd, false, "server could not open file"); return; }

    send_status(fd, true, "ready");

    char buf[BUF_SIZE];
    uint64_t received = 0;
    while (received < total) {
        size_t chunk = std::min((uint64_t)BUF_SIZE, total - received);
        ssize_t n = recv(fd, buf, chunk, 0);
        if (n <= 0) { log(addr, "upload interrupted at " + std::to_string(received) + " bytes"); return; }
        out.write(buf, n);
        received += n;
    }
    out.close();
    log(addr, "upload complete: uploads/" + fname);
    send_status(fd, true, "upload complete (" + std::to_string(received) + " bytes)");
}

static void handle_download(int fd, const FileHeader& hdr, const std::string& addr) {
    std::string fname = fs::path(std::string(hdr.name)).filename().string();
    log(addr, "DOWNLOAD " + fname);

    std::string path = "uploads/" + fname;
    if (!fs::exists(path)) { send_status(fd, false, "file not found"); return; }

    uint64_t size = fs::file_size(path);
    std::ifstream in(path, std::ios::binary);
    if (!in) { send_status(fd, false, "server could not read file"); return; }

    // Send size first, then data
    std::string header = "OK " + std::to_string(size) + "\n";
    send_all(fd, header.c_str(), header.size());

    char buf[BUF_SIZE];
    uint64_t sent = 0;
    while (in.read(buf, BUF_SIZE) || in.gcount() > 0) {
        size_t n = in.gcount();
        if (!send_all(fd, buf, n)) { log(addr, "download interrupted"); return; }
        sent += n;
    }
    log(addr, "download complete: " + fname + " (" + std::to_string(sent) + " bytes)");
}

static void client_session(int fd, const std::string& addr) {
    log(addr, "connected");
    FileHeader hdr{};
    if (!recv_all(fd, &hdr, sizeof(hdr))) {
        log(addr, "failed to read header");
        close(fd);
        return;
    }

    if (std::strncmp(hdr.cmd, "UPLOAD",   6) == 0) handle_upload(fd, hdr, addr);
    else if (std::strncmp(hdr.cmd, "DOWNLOAD", 8) == 0) handle_download(fd, hdr, addr);
    else send_status(fd, false, "unknown command");

    close(fd);
    log(addr, "disconnected");
}

int main() {
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }

    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(srv, 32) < 0) { perror("listen"); return 1; }

    std::cout << "File Transfer Server listening on port " << PORT << "\n"
              << "Uploads saved to ./uploads/\n";

    while (true) {
        sockaddr_in cli{};
        socklen_t len = sizeof(cli);
        int fd = accept(srv, (sockaddr*)&cli, &len);
        if (fd < 0) { perror("accept"); continue; }
        std::string cli_addr = std::string(inet_ntoa(cli.sin_addr))
                             + ":" + std::to_string(ntohs(cli.sin_port));
        std::thread(client_session, fd, cli_addr).detach();
    }
}
