# Client-Server File Transfer System

A multithreaded C++ file transfer application using TCP sockets. The server handles concurrent uploads and downloads — each client connection runs in its own `std::thread` — with path sanitisation and reliable chunked I/O.

## Architecture

```
 ┌──────────────────────────────────┐        ┌──────────────────────────────────┐
 │           client                 │        │            server                │
 │                                  │        │                                  │
 │  ./client <host> upload  file ───┼──────► │  thread 1: recv chunks ──► disk │
 │  ./client <host> download file ◄─┼──────  │  thread 2: send chunks ◄── disk │
 │                                  │  TCP   │  thread N: ...                  │
 └──────────────────────────────────┘ :9090  └──────────────────────────────────┘
```

### Wire protocol

Each connection sends a fixed-size `FileHeader` struct first:

```
cmd[8]    — "UPLOAD" or "DOWNLOAD"
name[256] — filename (null-terminated, path component stripped by server)
size      — file size in bytes (upload) or 0 (download request)
```

Followed by the raw file bytes (upload) or a size-prefixed response + file bytes (download).

### Concurrency

- Thread-per-connection: `std::thread(...).detach()` on each `accept()`
- Server log protected by `std::mutex` — no interleaved output
- Each thread owns its socket fd and file stream — no shared mutable state during transfer

## Build

```bash
make          # builds ./server and ./client
make clean    # removes binaries and uploads/ dir
```

Requires C++17 (`g++ 7+` or `clang++ 5+`) on Linux/macOS.

## Run

**Terminal 1 — server:**
```bash
./server
# File Transfer Server listening on port 9090
# Uploads saved to ./uploads/
```

**Terminal 2 — client:**
```bash
# Upload a file
./client 127.0.0.1 upload myfile.txt

# Download it back
./client 127.0.0.1 download myfile.txt
```

The server saves all uploaded files under `./uploads/`. Path components in filenames are stripped (e.g. `../../etc/passwd` → `passwd`) to prevent directory traversal.

## Testing concurrent transfers

```bash
# Upload 4 files in parallel
for f in a.txt b.txt c.txt d.txt; do
  ./client 127.0.0.1 upload "$f" &
done
wait
```

## Tech

C++17 · POSIX TCP sockets · `std::thread` · `std::mutex` · `std::filesystem` · chunked I/O
