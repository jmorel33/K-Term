# K-Term Multi-Venue IRC Integration Plan

This document outlines the feasibility analysis, architecture, and implementation roadmap for integrating a full multi-venue IRC ecosystem (client and server) directly into the K-Term emulation library.

## Introduction

That multi-venue vision is spot on for K-Term—it's already got the bones for it with its modular networking and session multiplexing. Based on the project's current architecture, here is a detailed feasibility analysis: overall feasibility, client-side IRC, server-side IRC, integration points, challenges/limitations, and a structured implementation plan. This assumes building on the existing C codebase with Situation for windowing/GPU, and keeping things thread-safe and non-blocking.

---

## 1. Overall Feasibility

**High feasibility overall**—K-Term's design screams extensibility for this. The networking module (`kt_net.h`) is async and protocol-agnostic at its core, built around TCP sockets with pluggable state machines (like how Telnet and SSH are handled). IRC is a simple text-based protocol (RFC 1459/2812), so "injecting" a handler means adding a new protocol layer without rewriting the wheel. It can work as:
- **Client (Remote)**: Connect to external IRC servers, parse/send messages, and display in a session (e.g., like a chat pane).
- **Server**: Listen for incoming clients, manage IRC daemon logic, and route to multiple sessions.

**Pros:**
- **Modularity**: You can add this via a new module (e.g., `kt_irc.h`) without touching core files much.
- **Thread-safety**: Op queues, atomics, and callbacks keep it safe for multi-threaded use.
- **Existing parallels**: Telnet/SSH already do similar client-server stuff, so pattern-match those.
- **Low overhead**: IRC is lightweight (no heavy crypto by default), fitting K-Term's embedded-friendly vibe.

**Cons:**
- **State Complexity**: IRC server mode is more stateful/complex than client (needs user auth, channel ops, etc.), so it'll add code bloat if not optional.
- **Security Hooks**: No native SSL/TLS in `kt_net` (SSH uses custom hooks), so for secure IRC (port 6697), you'd need to integrate something like OpenSSL or libsodium.
- **Main-thread restrictions**: Rendering/updates must stay on main, so async IRC processing needs careful queuing.

**Score**: 8/10 for client (straightforward), 6/10 for server (doable but beefier). Total lift: A weekend for a basic PoC, longer for polish.

---

## 2. Client-Side IRC Feasibility

**Super feasible**—basically an extension of the Telnet/SSH client examples (`telnet_client.c`, `ssh_client.c`).

- **How it Fits**: Use `KTerm_Net_Connect` to dial into an IRC server (e.g., `tcp://irc.libera.chat:6667`). Then, set a custom protocol via `KTerm_Net_SetProtocol`, pointing to an IRC state machine that handles parsing (e.g., split lines by `\r\n`, process commands like `JOIN`, `PRIVMSG`, `PING/PONG`).
- **Display/Interaction**: Route incoming messages to a `KTermSession` via `KTerm_Net_SetTargetSession` or Gateway commands (e.g., `EXT;net;attach;SESSION=irc-chat`). Output goes to the grid for rendering (like shell output), and user input (e.g., `/join #channel`) can be queued via `KTerm_QueueInputEvent` or a custom handler.
- **VoIP Tie-In**: If you want voice channels (e.g., IRC with audio), hook VoIP (RTP-based?) to IRC's DCC CHAT or a custom extension—trigger via callbacks like `on_data`.
- **Extensibility**: Add Gateway commands for IRC-specific stuff, e.g., `SET;IRC;NICK=yournick` or `EXT;irc;join;#channel`.
- **Edge Cases**: Handle reconnects with `kt_net`'s keep-alive/retries. For multi-server, use multiple net connections multiplexed across sessions.

This could be "injected" as a drop-in module, similar to how SSH plugs in its framer/auth.

---

## 3. Server-Side IRC Feasibility

**Also feasible, but more involved**—mirrors the `net_server.c` Telnet server example, but with IRC daemon logic (like a mini ircd).

- **How it Fits**: Use `KTerm_Net_Listen` to bind a port (e.g., 6667), accept connections, and spawn per-client state machines. Each client gets its own buffer/parser for IRC commands. Server state (users, channels, modes) can live in a new `KTermIrcServer` struct, managed globally or per-KTerm instance.
- **Handling Multiple Clients**: `kt_net`'s async callbacks (`on_connect`, `on_data`) make this non-blocking. Route client messages to broadcast handlers (e.g., `PRIVMSG` to channel members via session targeting). Use the multiplexer for "venues": One pane for logs, another for admin, etc.
- **Client-Server Duality**: A single handler can switch modes—e.g., based on a config flag, init as client (connect) or server (listen). Share the parser code between them.
- **VoIP Integration**: For server-side, proxy VoIP streams between clients (e.g., via IRC's CTCP for negotiation), using `kt_net`'s stream routing.
- **Security/Auth**: Basic IRC has none, but add hooks like SSH's `KTermNetSecurity` for optional SASL or SSL. Gateway could expose server commands (e.g., `EXT;irc;op;user`).
- **Scalability**: Fine for small-scale (hobby chats), but for 100+ users, you'd need to optimize the op queue to avoid bottlenecks.

This "injection" would extend `kt_net` with server-specific callbacks and a state tracker, keeping it optional via macros (e.g., `KTERM_ENABLE_IRC_SERVER`).

---

## 4. Integration Points in K-Term

- **Primary Hook: kt_net Extensions**
  - Add `kt_irc.h` with structs for IRC state (e.g., nick, channels).
  - Register parser in `KTerm_Net_SetProtocol(IRC_PROTOCOL, &irc_handler)`.
  - Use `on_data` callback to feed into `IRC_parse_message`, then queue outputs via `KTerm_WriteString`.
- **Gateway Dispatcher**
  - Add `EXT;irc` class for runtime control (e.g., join, part, query).
  - Target sessions with `SET;SESSION=n` for multi-venue.
- **Multiplexing/Layout**
  - Use `kt_layout.h` to split panes: One for IRC output, another for shell/VoIP.
  - Inject via `KTerm_SetMode` or custom events.
- **Input/Output Pipeline**
  - Local input: `KTerm_ProcessEvents` -> custom IRC command parser.
  - Remote: Net callbacks -> op queue -> grid updates.
- **Threading**: All net I/O async; use SPSC queues for cross-thread message passing.
- **Build**: Add `kt_irc.c` to Makefile/CMake, optional via `KTERM_ENABLE_IRC` macro. Depends on Situation (for any GUI demos).

---

## 5. Challenges and Limitations

- **Complexity Creep**: Server mode needs robust error handling (e.g., flood protection, ban lists)—start minimal to avoid bloat. Client is simpler.
- **Dependencies**: If adding SSL, pull in OpenSSL/libsodium (like SSH example). No internet in K-Term, so keep it offline-capable.
- **Performance**: GPU accel is great for rendering chat, but high-traffic IRC could stress the op queue—test with `speedtest_client.c` patterns.
- **Standards Compliance**: IRC has extensions (e.g., IRCv3 tags); implement core first, add via modules.
- **Testing**: Multi-client sims needed; use tools like ircII or netcat for PoCs.
- **Main-Thread Gotchas**: Net processing can be background, but updates/draw must be main—use condition vars to signal.
- **Repo State**: With only 1 star and heavy recent activity in polysonix/situation, ensure K-Term's net module is stable before extending.

---

## 6. Phased Implementation Plan

### Phase 1: Client-Side Proof of Concept (PoC)
- [ ] Fork `telnet_client.c` to create `irc_client.c`.
- [ ] Implement a basic TCP connection to a public IRC server using `KTerm_Net_Connect`.
- [ ] Create a barebones message parser to handle `PING`/`PONG` for keep-alive.
- [ ] Support joining a channel (`JOIN`) and reading messages (`PRIVMSG`).
- [ ] Test routing of parsed IRC output directly into a `KTermSession`.

### Phase 2: Core IRC Protocol Module
- [ ] Create `kt_irc.h` and `kt_irc.c` to house the IRC parser and state definitions.
- [ ] Define shared client/server IRC structures (e.g., user profiles, channel states).
- [ ] Register the new IRC protocol handler in the `kt_net` module.
- [ ] Integrate K-Term’s non-blocking I/O callbacks (`on_data`, `on_connect`) to feed into `IRC_parse_message`.

### Phase 3: Server-Side Foundation
- [ ] Extend `kt_irc.h` to support a listen mode (`KTerm_Net_Listen`).
- [ ] Create `KTermIrcServer` struct to track global state (connected users, active channels).
- [ ] Implement basic daemon logic: connection acceptance, basic user registration (`USER`/`NICK`), and channel joins.
- [ ] Build a broadcast handler to distribute messages (e.g., `PRIVMSG`) to all users in a channel.

### Phase 4: Gateway & Extensibility Integration
- [ ] Register a new Gateway extension class (`EXT;irc`).
- [ ] Add runtime commands for IRC configuration (e.g., `EXT;irc;join;#channel` or `SET;IRC;NICK=user`).
- [ ] Enable targeting specific sessions for IRC output using the `SESSION=n` parameter to support multi-venue layouts.

### Phase 5: Voice/VoIP Integration
- [ ] Extend IRC negotiation to support VoIP stream initialization (e.g., via a custom CTCP-like handshake or DCC CHAT).
- [ ] Hook the audio stream into the existing K-Term `kt_voice` architecture.
- [ ] Implement proxying of VoIP streams between users in server mode.

### Phase 6: Multi-Venue Layouts & Demos
- [ ] Build a comprehensive example showcasing a multi-venue environment (e.g., split panes with IRC in one, SSH in another).
- [ ] Implement focus management so users can easily switch context between the IRC pane and others.
- [ ] Add visual polish (e.g., specific colors for different users, IRC status bars).

### Phase 7: Hardening, Security, and Optimization
- [ ] Introduce timeouts, keep-alives, and robust error handling for both client and server modes.
- [ ] Integrate `KTermNetSecurity` hooks for optional SSL/TLS support (e.g., via OpenSSL or libsodium) for secure IRC connections (port 6697).
- [ ] Add compile-time macros (e.g., `KTERM_ENABLE_IRC_SERVER`) to easily omit the server logic for lightweight builds.
- [ ] Profile and optimize the operation queue to ensure high-traffic channels don't cause main-thread rendering lag.

### Phase 8: Finalization and Documentation
- [ ] Finalize the `Makefile`/`CMakeLists.txt` build scripts to include the new `kt_irc` module and example binaries.
- [ ] Update `README.md` to document the new IRC capabilities, use-cases, and extension commands.
- [ ] Clean up the codebase, review naming conventions, and finalize commits for repository inclusion.
