# KTerm Networking Module (`kt_net.h`)

The `kt_net.h` module provides a lightweight, single-header networking abstraction for KTerm. It enables KTerm instances to connect to remote hosts via TCP (raw sockets) or SSH (via libssh), integrating seamlessly with the KTerm event loop and Gateway Protocol.

## Features

- **Client Mode:** Connect to remote TCP servers or SSH hosts.
- **Server Mode:** Accept incoming connections (`KTerm_Net_Listen`) with optional authentication.
- **Non-Blocking Architecture:** Connection attempts and I/O are non-blocking to prevent UI freezes.
- **Event-Driven:** Callbacks for connection status (`on_connect`, `on_disconnect`), data reception, and errors.
- **Security Hooks:** Interface for plugging in TLS/SSL (e.g., OpenSSL, mbedTLS) or custom encryption (SSH).
- **Protocol Support:**
    - `KTERM_NET_PROTO_RAW`: Raw byte stream.
    - `KTERM_NET_PROTO_FRAMED`: Binary framing for multiplexing resizing, gateway commands, and data.
    - `KTERM_NET_PROTO_TELNET`: Full Telnet protocol (RFC 854) support with state machine and negotiation callbacks.
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
    .on_error = my_error,
    // Optional: Handle Telnet Negotiation
    .on_telnet_command = my_telnet_negotiation,
    // Optional: Handle Telnet Subnegotiation (SB)
    .on_telnet_sb = my_telnet_sb_handler
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

### 5. Server Mode

Enable listening for incoming connections:

```c
// Callback to verify credentials
bool my_auth(KTerm* term, KTermSession* session, const char* user, const char* pass) {
    return (strcmp(user, "admin") == 0 && strcmp(pass, "secret") == 0);
}

KTermNetCallbacks cbs = { .on_connect = my_on_client, .on_auth = my_auth };
KTerm_Net_SetCallbacks(term, session, cbs);

// Start Server
KTerm_Net_Listen(term, session, 2323);
```

### 6. SSH Support (Custom Implementation)

You can implement a custom SSH transport using the `KTermNetSecurity` interface without linking `libssh`. This allows full control over the handshake and crypto.

```c
// See example/ssh_skeleton.c for full state machine implementation
KTermNetSecurity ssh_sec = {
    .ctx = my_ssh_context,
    .handshake = my_ssh_handshake, // Returns KTERM_SEC_OK, AGAIN, or ERROR
    .read = my_ssh_read,           // Decrypts and handles SSH packets
    .write = my_ssh_write,         // Encrypts and frames data
    .close = my_ssh_close
};
KTerm_Net_SetSecurity(term, session, ssh_sec);
```

### 7. Security Hooks (TLS/SSL)

Similar to SSH, you can wrap TLS libraries like OpenSSL:

```c
KTermNetSecurity sec = {
    .ctx = my_ssl_context,
    .handshake = my_ssl_handshake,
    .read = my_ssl_read,
    .write = my_ssl_write,
    .close = my_ssl_close
};
KTerm_Net_SetSecurity(term, session, sec);
```

### 8. Telnet Protocol Support

To enable Telnet support (RFC 854 processing), set the protocol to `KTERM_NET_PROTO_TELNET`. This automatically filters IAC sequences and triggers callbacks for negotiation.

```c
KTerm_Net_SetProtocol(term, session, KTERM_NET_PROTO_TELNET);
```

You can handle negotiation (WILL/WONT/DO/DONT) via the `on_telnet_command` callback. If the callback returns `false` (or is NULL), the library automatically rejects requests (sends DONT/WONT) to ensure compliance.

```c
bool my_telnet_handler(KTerm* term, KTermSession* session, unsigned char cmd, unsigned char opt) {
    if (cmd == KTERM_TELNET_WILL && opt == TELNET_OPT_ECHO) {
        // Accept remote echo
        KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_DO, opt);
        return true;
    }
    return false; // Reject everything else
}
```

### 9. Protocol Framing (Custom)

The `KTERM_NET_PROTO_FRAMED` mode supports multiplexed events.

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
