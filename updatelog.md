# Update Log

## v2.4.6 (Conversational UI Hardening)
*   **Synchronized Output (DECSET 2026):** Implemented support for synchronized updates to prevent visual tearing during rapid text streaming (e.g., from LLMs).
*   **Focus Tracking:** Added support for `CSI ? 1004 h` (Focus In/Out events) and introduced `KTerm_SetFocus` API to report focus state to the terminal.
*   **Clipboard Hardening:** Increased `MAX_COMMAND_BUFFER` to 256KB to support large clipboard operations via OSC 52, ensuring robust handling of large pastes.
*   **Tests:** Added `tests/stress_interleaved_io.c` for I/O stress testing and `tests/mock_situation.h` for robust unit testing without external dependencies. Expanded `tests/verify_clipboard.c` to test large payloads.
*   **Fixes:** Resolved compilation issues in tests by introducing a mock layer for the Situation framework.
