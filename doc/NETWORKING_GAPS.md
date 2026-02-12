# Networking Gaps vs Competitors — Inspiration for Future Perks

This doc shines light on areas where K-Term's networking (custom/pluggable SSH/Telnet, Gateway diagnostics, non-blocking stack) lags behind standalone terminals with built-in remote features. K-Term excels at embeddability, custom crypto control, and deep GPU/Kitty integration — but as a library, it misses some out-of-box user conveniences.

Goal: Use this as roadmap fuel to close gaps (e.g., easier persistence, auto-passthrough, resume logic) without bloating the core.

## Competitors with Built-in Networking
- **Kitty** (`kitty +kitten ssh`)
- **WezTerm** (SSH domains in config)
- **Wave Terminal** (SSH manager with durable sessions)

## Key Disadvantages / Their Added Perks Over K-Term

| Area                  | Competitor Perk                                                                 | K-Term Current State                                      | Potential Improvement (Roadmap Idea)                          |
|-----------------------|---------------------------------------------------------------------------------|-----------------------------------------------------------|---------------------------------------------------------------|
| **Graphics Passthrough over SSH** | Kitty: Full Kitty protocol (images/animations/transparency/z-index) works seamlessly remotely — zero extra config, auto terminfo copy. | K-Term supports Kitty locally/Gateway, but passthrough over SSH requires manual integration/code. | Add auto Kitty protocol forwarding in `ssh_client.c` (detect remote, proxy graphics packets). Zero-config mode? |
| **Remote Environment Setup** | Kitty: Auto-copies terminfo/settings/fonts; works nested/over serial consoles. | Manual (user must ensure remote has K-Term terminfo or fallback). | Built-in terminfo push on connect (like Kitty kitten); fallback to xterm-256color with feature detection. |
| **Session Persistence & Resume** | WezTerm: Config-driven auto-reconnect/resume multiplexing on disconnect.<br>Wave: Durable sessions survive network changes/interruptions (auto-restore). | K-Term has keep-alive/timeouts, but no built-in resume logic (state lost on disconnect). | Add session save/restore (serialize grid/panes to file/Gateway); auto-reconnect callback hook. |
| **Config-Driven Remotes** | WezTerm: Simple YAML/ Lua config for SSH domains, auth, multiplexing. | Requires code/compilation for connections (great for embedding, but not end-user friendly). | Optional config file parser (JSON/TOML) for `ssh_client.c`/examples; predefined profiles. |
| **Extensibility/Scripting** | WezTerm: Lua scripts for custom SSH behaviors/auth flows. | Callbacks/hooks powerful, but C-only (no high-level scripting). | Embed Lua/TinyCC for user scripts in examples; or Gateway extensions for runtime scripting. |
| **Cross-Session Features** | Wave: Share data/commands/history across local/remote sessions; universal searchable history; AI assistance/inline previews over SSH. | Session-isolated (strong for security), but no built-in sharing. | Gateway enhancements for cross-session copy/paste/commands; optional history sync. |
| **Reconnect Resilience** | Wave: Auto-handle network flaps, WSL/remote switching. | Non-blocking/retries solid, but app-level resume needs custom code. | Built-in "durable mode" flag with heartbeat/reconnect loop in examples. |

## Implementation Roadmap (Phased Approach)

To bridge these gaps systematically without compromising K-Term's lightweight architecture, we propose a 3-phase plan.

### Phase 1: Resilience & Environment (Foundation)
**Goal:** Make connections robust and "just work" without manual setup.
1.  **Auto-Terminfo Injection:** Implement logic in `ssh_client.c` to automatically copy the local terminfo to the remote host (via `scp` or inline script) or fallback gracefully to `xterm-256color`.
2.  **Durable Mode:** Enhance the SSH client with a "durable" flag that attempts transparent reconnection on network drops, buffering input/output during the outage.
3.  **Session Serialization Hooks:** Create API hooks to serialize the current grid state (text + attributes) to a buffer. This is the prerequisite for full session resume.

### Phase 2: Visual Parity (Graphics)
**Goal:** Make remote applications look indistinguishable from local ones.
1.  **Kitty Graphics Forwarding:** Detect incoming Kitty Graphics Protocol sequences (`ESC _ G ... ST`) in the SSH stream and proxy them directly to the local GPU renderer, bypassing the text parser where necessary.
2.  **Sixel Passthrough:** Ensure Sixel data is transmitted cleanly over SSH channels without corruption or timeout issues.
3.  **Clipboard Sync:** Implement bidirectional clipboard sharing (OSC 52) between the local host and the remote session.

### Phase 3: User Convenience (The "Magic")
**Goal:** Provide a polished, "magical" end-user experience for standalone binaries.
1.  **Config-Driven Profiles:** Add a simple JSON/TOML parser to `ssh_client` to load connection profiles (host, port, key, user, durable mode setting) from a file.
2.  **Session Persistence:** Use the Phase 1 serialization hooks to save session state to disk on exit/disconnect and restore it on relaunch.
3.  **Gateway Scripting:** Introduce a lightweight scripting layer (or expand Gateway commands) to allow users to script complex login flows or automations.

## Why This Matters
- K-Term is **superior for embedding/custom apps** (own crypto, Telnet server, GPU tie-ins, thread-safe).
- But for standalone use (`telnet_client.c`/`ssh_client.c`), these perks make competitors feel more "magical" for end-users.
- Closing gaps keeps K-Term competitive without compromising lightweight ethos (optional features, examples-focused).
