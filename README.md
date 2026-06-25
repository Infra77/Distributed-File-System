# Distributed File System

A client-server distributed file system implemented in **C**, featuring user authentication, role-based access control, concurrent file operations, and action logging — all over raw TCP sockets.

---

## Features

- **User Authentication** — Login and signup with username/password credentials stored persistently on the server
- **Role-Based Access Control** — Two roles: `admin` (full access) and `user` (owns only their own files)
- **File Operations** — Upload, download, update, delete, and list files on the remote server
- **Local Sandbox** — Per-user local directory with `ls`, `touch`, and `cat` commands
- **Concurrency Control** — Semaphores limit concurrent uploads/downloads to 3 each; mutexes guard shared metadata and user files
- **File Locking** — POSIX advisory locks (`fcntl`) prevent simultaneous conflicting reads and writes
- **Action Logging** — All user actions (login, signup, upload, download, update, delete, list) are timestamped and appended to `logs.txt`
- **Soft Deletion** — Deleted files are marked in metadata rather than removed from disk, preserving data integrity

---

## Project Structure

```
Distributed-File-System/
├── client/
│   ├── client.c          # Client entry point: auth UI and main command loop
│   ├── client_fn.c       # Command implementations (upload, download, update, delete, list, local commands)
│   └── client_fn.h       # Client structs and function prototypes
└── server/
    ├── server.c          # Server entry point: socket setup, connection acceptance, thread dispatch
    ├── server_fn.c       # Handler implementations (auth, list, upload, download, update, delete)
    └── server_fn.h       # Server structs, extern declarations, and function prototypes
```

---

## Architecture

```
Client                          Server
  │                               │
  │──── TCP connect (port 8080) ──▶│
  │                               │
  │──── auth (login / signup) ───▶│── users.txt (binary records)
  │◀─── result ───────────────────│
  │                               │
  │──── command (list/upload/…) ──▶│── metadata.txt (binary records)
  │                               │── files/  (raw file data)
  │◀─── response ─────────────────│── logs.txt (action log)
```

Each accepted client connection spawns a dedicated POSIX thread on the server. File operations that exceed the semaphore limit (3 concurrent uploads or downloads) block until a slot is free.

---

## Data Files (Server-side)

| File | Purpose |
|---|---|
| `users.txt` | Binary file of `Session` structs (username, password, role) |
| `metadata.txt` | Binary file of `Meta` structs (filename, author, is_deleted flag) |
| `files/` | Directory holding the raw uploaded file data |
| `logs.txt` | Append-only plaintext action log with timestamps |

---

## Building

### Prerequisites

- GCC
- POSIX-compliant system (Linux / macOS)
- `pthread` library

### Compile the Server

```bash
cd server
gcc -o server server.c server_fn.c -lpthread
```

### Compile the Client

```bash
cd client
gcc -o client client.c client_fn.c -lpthread
```

---

## Running

### 1. Start the Server

```bash
cd server
./server
# Server is listening on port 8080...
```

The server listens on port `8080` by default (defined as `PORT` in `server.c`). It creates a `files/` directory in the current working directory on first run.

### 2. Start the Client

```bash
cd client
./client
```

The client connects to `127.0.0.1:8080` by default (defined as `SERVER_IP` and `SERVER_PORT` in `client.c`).

> To connect to a remote server, update `SERVER_IP` in `client.c` before compiling.

---

## Usage

### Authentication

On launch, the client presents a menu:

```
=== Distributed File System ===
1. Login
2. Signup
3. Exit
Choice:
```

Enter your username, password, and role (`admin` or `user`). Note: signing up as `admin` is not permitted — admin accounts must be created directly.

### Command Prompt

After successful authentication, you enter the interactive shell:

```
<username>/dfs>
```

#### Remote Server Commands

| Command | Description |
|---|---|
| `list` | List all files on the server (filename and author) |
| `upload <file>` | Upload `<file>` from your local sandbox to the server |
| `download <file>` | Download `<file>` from the server to your local sandbox |
| `update <file>` | Replace a server file with the local version (author or admin only) |
| `delete <file>` | Soft-delete a file on the server (author or admin only) |

#### Local Sandbox Commands

Each user has a local directory (`./<username>/`) that acts as a personal workspace.

| Command | Description |
|---|---|
| `ls` | List files in your local sandbox |
| `touch <file>` | Create an empty file in your local sandbox |
| `cat <file>` | Print the contents of a local file |

#### Other

| Command | Description |
|---|---|
| `help` | Print available commands |
| `exit` | Disconnect from the server and quit |

---

## Access Control

| Operation | Regular User | Admin |
|---|---|---|
| Upload | ✅ | ✅ |
| Download | ✅ | ✅ |
| List | ✅ | ✅ |
| Update own file | ✅ | ✅ |
| Update any file | ❌ | ✅ |
| Delete own file | ✅ | ✅ |
| Delete any file | ❌ | ✅ |

---

## Concurrency Model

- **Per-client thread** — each accepted connection runs in its own `pthread`
- **Upload semaphore** — max 3 concurrent uploads (`sem_init(&upload_sem, 0, 3)`)
- **Download semaphore** — max 3 concurrent downloads (`sem_init(&download_sem, 0, 3)`)
- **`meta_mutex`** — protects reads and writes to `metadata.txt`
- **`user_mutex`** — protects reads and writes to `users.txt`
- **`log_mutex`** — protects append operations to `logs.txt`
- **POSIX file locks (`fcntl`)** — write-lock on upload/update, read-lock on download to prevent data corruption

---

## Configuration

All configuration is done via `#define` constants at the top of each source file — no external config file is needed.

| Constant | File | Default | Description |
|---|---|---|---|
| `PORT` | `server.c` / `server_fn.c` | `8080` | Server listening port |
| `BUFFER_SIZE` | `server_fn.c` / `client_fn.h` | `1000` | I/O buffer size in bytes |
| `SERVER_IP` | `client.c` | `"127.0.0.1"` | Server IP address |
| `SERVER_PORT` | `client.c` | `8080` | Server port |

---

## License

This project is not currently distributed under an open-source license. All rights reserved by the author.
