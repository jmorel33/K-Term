# KTerm Networking Module (`kt_net.h`)

The `kt_net.h` module provides a lightweight, single-header networking abstraction for KTerm. It enables KTerm instances to connect to remote hosts via TCP (raw sockets) or SSH (via libssh), integrating seamlessly with the KTerm event loop and Gateway Protocol.

## Features

- **Client Mode:** Connect to remote TCP servers or SSH hosts.
- **Non-Blocking Architecture:** Connection attempts and I/O are non-blocking to prevent UI freezes.
- **Gateway Integration:** Control networking via DCS Gateway commands (e.g., `DCS GATE ... connect ... ST`).
- **Secure Shell (SSH):** Optional support for SSHv2 via `libssh` (requires `KTERM_USE_LIBSSH`).
- **Clean Architecture:** Decoupled from the core terminal logic; plugs in via the `output_sink` interface.

## Usage

### 1. Initialization

Include the header and define the implementation in **one** source file:

```c
#define KTERM_NET_IMPLEMENTATION
#include "kt_net.h"
```

Initialize the networking subsystem after creating your KTerm instance:

```c
KTerm* term = KTerm_Create(config);
KTerm_Net_Init(term); // Sets up output sink and networking context
```

### 2. The Event Loop

To handle network I/O, you must call `KTerm_Net_Process` periodically in your main loop (e.g., every frame or tick). This function handles:
- Connection state machine (Resolving -> Connecting -> Auth -> Connected).
- Non-blocking socket I/O (sending buffered data, reading incoming data).
- Reading from the network and injecting into KTerm via `KTerm_WriteCharToSession`.

```c
while (running) {
    // 1. Process Network I/O
    KTerm_Net_Process(term);

    // 2. Process Terminal Logic
    KTerm_Update(term);

    // 3. Render...
}
```

### 3. Connecting (Client Mode)

You can initiate a connection programmatically or via Gateway commands.

**Programmatic:**
```c
KTermSession* session = &term->sessions[0];
KTerm_Net_Connect(term, session, "192.168.1.50", 23, "user", "password");
```

**Gateway Command (from inside the terminal or script):**
```
DCS GATE;KTERM;1;EXT;ssh;connect;192.168.1.50:23 ST
```
*(Note: The `ssh` extension name is used for both raw TCP and SSH connections currently, pending rename to `net`)*

### 4. SSH Support

To enable SSH, define `KTERM_USE_LIBSSH` before including `kt_net.h` and link against `libssh`.

```c
#define KTERM_USE_LIBSSH
#define KTERM_NET_IMPLEMENTATION
#include "kt_net.h"
```

When enabled, `KTerm_Net_Connect` attempts an SSH handshake. If disabled, it falls back to raw TCP sockets.

## Architecture

### Non-Blocking Connection
The module uses non-blocking sockets. When `KTerm_Net_Connect` is called:
1.  **RESOLVING:** Hostname resolution (currently blocking, future work to async).
2.  **CONNECTING:** Socket is set to non-blocking mode. `connect()` is called. If it returns `EINPROGRESS`/`EWOULDBLOCK`, the state machine waits in this state, polling the socket for writeability via `select()`.
3.  **CONNECTED:** Once writable, the connection is established.

### Output Routing
`KTerm_Net_Init` registers `KTerm_Net_Sink` as the terminal's output sink.
- **If Connected:** Terminal output is buffered in `tx_buffer` and sent to the socket in `KTerm_Net_Process`.
- **If Disconnected:** Terminal output falls back to `term->response_callback` (local echo or host application handling).

## Server Mode (Hosting)

`kt_net.h` is primarily a client implementation. To implement a server (accepting connections and spawning KTerm instances), you should write your own `accept()` loop and pipe data into KTerm.

See `example/net_server.c` for a reference implementation of a non-blocking TCP server.
