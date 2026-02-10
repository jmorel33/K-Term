# K-Term Update Log

## v2.5.7 (Networking Hardening)
- **Server Mode:** Added `KTerm_Net_Listen` to allow K-Term to function as a TCP server, accepting incoming Telnet/Raw connections.
- **Authentication:** Implemented `on_auth` callback hook for server-side username/password verification.
- **Telnet Subnegotiation:** Added full support for parsing variable-length Telnet options (`IAC SB ... SE`) via `on_telnet_sb` callback.
- **Protocol Improvements:**
    - Corrected Telnet Echo negotiation logic (`IAC WILL ECHO`) to prevent double-echoing in server mode.
    - Restored `libssh` client implementation block.
    - Fixed memory initialization issues in listening sockets.
