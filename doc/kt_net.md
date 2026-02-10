# KTerm Networking Module (`kt_net.h`)

The `kt_net.h` module provides a lightweight, single-header networking abstraction for KTerm. It enables KTerm instances to connect to remote hosts via TCP (raw sockets) or SSH (via libssh), integrating seamlessly with the KTerm event loop and Gateway Protocol.

## Features

- **Client Mode:** Connect to remote TCP servers or SSH hosts.
- **Non-Blocking Architecture:** Connection attempts and I/O are non-blocking to prevent UI freezes.
- **Event-Driven:** Callbacks for connection status (`on_connect`, `on_disconnect`) and data reception.
- **Security Hooks:** Interface for plugging in TLS/SSL (e.g., OpenSSL, mbedTLS) or custom encryption.
- **Protocol Framing:** Optional binary framing protocol for multiplexing resizing, gateway commands, and data over a single stream.
- **Gateway Integration:** Control networking via DCS Gateway commands (e.g., `DCS GATE ... connect ... ST`).
- **Session Multiplexing:** Route network traffic to specific local sessions via `ATTACH` command.

## Usage

### 1. Initialization

Include the header and define the implementation in **one** source file:

```c
#define KTERM_NET_IMPLEMENTATION
#include "kt_net.h"
```

Initialize the networking subsystem (automatically called by `KTerm_Init`, but configuration can be added):

```c
KTerm* term = KTerm_Create(config);
// KTerm_Net_Init(term) is called internally
```

### 2. Async Callbacks

To handle network events asynchronously:

```c
void my_connect(KTerm* term, KTermSession* session) {
    printf("Connected!\n");
}

KTermNetCallbacks callbacks = {
    .on_connect = my_connect,
    .on_disconnect = my_disconnect,
    .on_data = my_data,
    .on_error = my_error
};

KTerm_Net_SetCallbacks(term, session, callbacks);
```

### 3. The Event Loop

To handle network I/O, you must call `KTerm_Update` (which calls `KTerm_Net_Process`) periodically.

```c
while (running) {
    // KTerm_Update processes Network I/O, timers, and logic
    KTerm_Update(term);
}
```

### 4. Connecting (Client Mode)

**Programmatic:**
```c
KTermSession* session = &term->sessions[0];
KTerm_Net_Connect(term, session, "192.168.1.50", 23, "user", "password");
```

**Gateway Command:**
```
DCS GATE;KTERM;1;EXT;net;connect;192.168.1.50:23 ST
```

### 5. SSH Support

To enable SSH, define `KTERM_USE_LIBSSH` before including `kt_net.h` and link against `libssh`.

```c
#define KTERM_USE_LIBSSH
#define KTERM_NET_IMPLEMENTATION
#include "kt_net.h"
```

### 6. Security Hooks (TLS/SSL)

You can provide custom read/write/handshake functions to integrate TLS libraries like OpenSSL without modifying KTerm core.

```c
KTermNetSecurity sec = {
    .ctx = my_ssl_context,
    .handshake = my_ssl_handshake, // Returns KTERM_SEC_OK, AGAIN, or ERROR
    .read = my_ssl_read,
    .write = my_ssl_write,
    .close = my_ssl_close
};
KTerm_Net_SetSecurity(term, session, sec);
```

### 7. Protocol Framing

By default, `kt_net` uses `KTERM_NET_PROTO_RAW` (raw byte stream). You can switch to `KTERM_NET_PROTO_FRAMED` to support multiplexed events.

```c
KTerm_Net_SetProtocol(term, session, KTERM_NET_PROTO_FRAMED);
```

**Packet Format:** `[TYPE:1][LEN:4 (Big Endian)][PAYLOAD:LEN]`

| Type | Name | Payload |
|---|---|---|
| `0x01` | DATA | Raw terminal I/O bytes |
| `0x02` | RESIZE | `[Width:4][Height:4]` (BE) |
| `0x03` | GATEWAY | Gateway Command String |
| `0x04` | ATTACH | `[SessionID:1]` (Routes future traffic to session N) |

## Remote Session Control

You can control which local session processes network data using the Gateway:

```
DCS GATE;KTERM;1;ATTACH;SESSION=2 ST
```

This directs all subsequent data received on that connection to Session 2.
