#define KTERM_IMPLEMENTATION
// Define KTERM_TESTING to use mock_situation.h for headless compilation
#define KTERM_TESTING

#include "../kterm.h"
// kt_net.h is included and implemented by kterm.h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

// Simple Telnet Server Example

static bool server_running = true;

// Custom Telnet Command Handler
bool my_telnet_command(KTerm* term, KTermSession* session, unsigned char command, unsigned char option) {
    printf("[Server] Telnet Command: %d %d\n", command, option);

    // Accept Echo (client will echo locally or we echo back? Usually server echoes)
    if (command == KTERM_TELNET_DO && option == KTERM_TELNET_ECHO) {
        KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_WILL, KTERM_TELNET_ECHO);
        return true;
    }
    // Accept Suppress Go Ahead (SGA)
    if (command == KTERM_TELNET_DO && option == KTERM_TELNET_SGA) {
        KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_WILL, KTERM_TELNET_SGA);
        return true;
    }
    // Accept NAWS (Negotiate About Window Size)
    if (command == KTERM_TELNET_WILL && option == KTERM_TELNET_NAWS) {
        KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_DO, KTERM_TELNET_NAWS);
        return true;
    }

    return false; // Let default logic handle or reject
}

// Custom Auth Handler
bool my_auth(KTerm* term, KTermSession* session, const char* user, const char* pass) {
    printf("[Server] Auth Request: %s / %s\n", user, pass);
    if (strcmp(user, "admin") == 0 && strcmp(pass, "password") == 0) return true;
    return false;
}

// Shell State
typedef struct {
    char cmd_buf[256];
    int cmd_len;
    bool last_was_cr;
} ShellState;

ShellState shells[4];

void process_shell(KTerm* term, int session_idx, const char* data, size_t len) {
    ShellState* sh = &shells[session_idx];
    for(size_t i=0; i<len; i++) {
        char c = data[i];

        // Handle CR/LF
        if (c == '\r') {
            sh->last_was_cr = true;
        } else if (c == '\n') {
            if (sh->last_was_cr) {
                sh->last_was_cr = false;
                continue; // Ignore \n after \r
            }
            sh->last_was_cr = false;
        } else {
            sh->last_was_cr = false;
        }

        if (c == '\r' || c == '\n') {
            sh->cmd_buf[sh->cmd_len] = '\0';
            KTerm_WriteString(term, "\r\n"); // Echo newline

            if (sh->cmd_len > 0) {
                if (strcmp(sh->cmd_buf, "exit") == 0) {
                    KTerm_WriteString(term, "Goodbye.\r\n");
                    KTerm_Net_Disconnect(term, &term->sessions[session_idx]);
                } else if (strcmp(sh->cmd_buf, "help") == 0) {
                    KTerm_WriteString(term, "Commands: help, status, resize <w> <h>, clear, exit\r\n");
                } else if (strcmp(sh->cmd_buf, "status") == 0) {
                    KTerm_WriteString(term, "System OK. K-Term v2.5.11 Running.\r\n");
                } else if (strcmp(sh->cmd_buf, "clear") == 0) {
                    KTerm_WriteString(term, "\033[2J\033[H");
                } else if (strncmp(sh->cmd_buf, "resize ", 7) == 0) {
                    int w, h;
                    if (sscanf(sh->cmd_buf + 7, "%d %d", &w, &h) == 2) {
                        KTerm_Resize(term, w, h);
                        char msg[64]; snprintf(msg, sizeof(msg), "Resized to %dx%d\r\n", w, h);
                        KTerm_WriteString(term, msg);
                    } else {
                        KTerm_WriteString(term, "Usage: resize <w> <h>\r\n");
                    }
                } else {
                    KTerm_WriteString(term, "Unknown command.\r\n");
                }
            }

            KTerm_WriteString(term, "KTerm> ");
            sh->cmd_len = 0;
        } else if (c == 0x7F || c == 0x08) { // Backspace
            if (sh->cmd_len > 0) {
                sh->cmd_len--;
                KTerm_WriteString(term, "\x08 \x08");
            }
        } else if (c >= 32 && c <= 126) {
            if (sh->cmd_len < 255) {
                sh->cmd_buf[sh->cmd_len++] = c;
                char echo[2] = {c, 0};
                KTerm_WriteString(term, echo);
            }
        }
    }
}

void my_on_data(KTerm* term, KTermSession* session, const char* data, size_t len) {
    int idx = (int)(session - term->sessions);
    process_shell(term, idx, data, len);
}

void my_on_connect(KTerm* term, KTermSession* session) {
    int idx = (int)(session - term->sessions);
    printf("[Server] Client Connected on Session %d\n", idx);
    shells[idx].cmd_len = 0;
    shells[idx].last_was_cr = false;
    KTerm_WriteString(term, "\r\nWelcome to K-Term Telnet Server.\r\nType 'help' for commands.\r\nKTerm> ");
}

int main() {
    char ip[64] = "0.0.0.0";
    KTerm_Net_GetLocalIP(ip, sizeof(ip));
    printf("Starting K-Term Telnet Server on %s:8023...\n", ip);
    fflush(stdout);

    KTermConfig config;
    memset(&config, 0, sizeof(config));
    config.width = 80;
    config.height = 24;

    // Pass by value as KTerm_Create(KTermConfig config)
    KTerm* term = KTerm_Create(config);
    KTerm_Net_Init(term);

    // Setup Server on Session 0
    KTermNetCallbacks cb;
    memset(&cb, 0, sizeof(cb));
    cb.on_telnet_command = my_telnet_command;
    cb.on_auth = my_auth;
    cb.on_data = my_on_data;
    cb.on_connect = my_on_connect;

    KTerm_Net_SetCallbacks(term, &term->sessions[0], cb);
    KTerm_Net_SetProtocol(term, &term->sessions[0], KTERM_NET_PROTO_TELNET);
    KTerm_Net_Listen(term, &term->sessions[0], 8023);

    // Main Loop
    while(server_running) {
        KTerm_Net_Process(term);
        usleep(10000); // 10ms
    }

    KTerm_Destroy(term);
    return 0;
}
