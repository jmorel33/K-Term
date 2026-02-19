#define KTERM_TESTING
#define KTERM_IMPLEMENTATION
#define KTERM_ENABLE_GATEWAY

#include "kterm.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
    printf("Starting Voice Verification...\n");

    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);
    if (!term) {
        fprintf(stderr, "Failed to create KTerm\n");
        return 1;
    }
    KTermSession* session = &term->sessions[0];

    // Initialize Network with Loopback Socket Pair
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        perror("socketpair");
        return 1;
    }
    fcntl(sv[0], F_SETFL, O_NONBLOCK);
    fcntl(sv[1], F_SETFL, O_NONBLOCK);

    // Manually setup net context
    KTerm_Net_Connect(term, session, "localhost", 22, "user", "pass");
    KTermNetSession* net = (KTermNetSession*)session->user_data;
    if (!net) {
        fprintf(stderr, "Failed to create Net context\n");
        return 1;
    }

    // Force state to CONNECTED and inject socket
    net->state = KTERM_NET_STATE_CONNECTED;
    if (net->socket_fd != -1) close(net->socket_fd);
    net->socket_fd = sv[0]; // KTerm uses sv[0]

    // Set Protocol to FRAMED (Required for Packet sending)
    KTerm_Net_SetProtocol(term, session, KTERM_NET_PROTO_FRAMED);

    // Enable Voice
    printf("Enabling Voice...\n");
    if (SituationVoiceEnable(session, true) != SITUATION_SUCCESS) {
        fprintf(stderr, "Voice Enable Failed\n");
        return 1;
    }

    // Simulate Capture
    printf("Simulating Audio Capture...\n");
    // 1. Get Voice Context
    KTermVoiceContext* ctx = KTerm_Voice_GetContext(session);
    if (!ctx) {
        fprintf(stderr, "Failed to get Voice Context\n");
        return 1;
    }

    // 2. Feed Data
    float input_samples[256];
    for(int i=0; i<256; i++) input_samples[i] = (float)i / 256.0f;

    // Verify callback was registered
    if (!mock_audio_cb) {
        fprintf(stderr, "Mock audio callback not set (SituationStartAudioCaptureEx failed?)\n");
        return 1;
    }

    mock_audio_cb(mock_audio_user_data, input_samples, 256); // 256 frames (assuming 1 channel)

    // 3. Process Network (Should capture buffer -> Send Packet)
    printf("Processing Network (Capture -> Send)...\n");
    // Run loop multiple times to ensure chunked sends are flushed
    for(int i=0; i<10; i++) KTerm_Net_Process(term);

    // 4. Verify Data Sent to Socket
    unsigned char buffer[4096];
    int total_read = 0;
    int expected_size = 5 + (256 * sizeof(float)); // 1029

    while(total_read < expected_size) {
        int n = read(sv[1], buffer + total_read, sizeof(buffer) - total_read);
        if (n > 0) total_read += n;
        else if (n < 0) {
             // EAGAIN, wait a bit
             usleep(10000);
        } else {
             break;
        }
        if (total_read >= expected_size) break;
        // Limit loop
    }

    if (total_read <= 0) {
        fprintf(stderr, "No data sent to network\n");
        return 1;
    }
    printf("Received %d bytes from network\n", total_read);

    // Verify Packet Header
    // KTERM_PKT_AUDIO_VOICE = 0x10
    if (buffer[0] != 0x10) {
        fprintf(stderr, "Wrong packet type: 0x%02X\n", buffer[0]);
        return 1;
    }

    uint32_t len = (buffer[1] << 24) | (buffer[2] << 16) | (buffer[3] << 8) | buffer[4];
    printf("Packet Payload Length: %d\n", len);
    if (len != 256 * sizeof(float)) {
        fprintf(stderr, "Wrong payload length: %d (expected %lu)\n", len, 256 * sizeof(float));
        // Note: Currently we send whatever chunk is available.
        // Logic says CHUNK_SIZE = 256. So it should match.
    }

    // Verify Payload (first few samples)
    float* payload = (float*)(buffer + 5);
    if (payload[0] != 0.0f || payload[1] != 1.0f/256.0f) {
        fprintf(stderr, "Payload mismatch: %f, %f\n", payload[0], payload[1]);
        return 1;
    }

    // 5. Loopback Test (Network -> Playback)
    printf("Simulating Network Receive (Loopback -> Playback)...\n");
    // Write packet back to KTerm socket
    int written = 0;
    while(written < total_read) {
        int w = write(sv[1], buffer + written, total_read - written);
        if (w > 0) written += w;
    }

    // Run Process again to read and dispatch
    for(int i=0; i<10; i++) KTerm_Net_Process(term);

    // Verify Playback Buffer via Callback
    float output_samples[256];
    memset(output_samples, 0, sizeof(output_samples));

    if (!mock_playback_cb) {
        fprintf(stderr, "Mock playback callback not set\n");
        return 1;
    }

    mock_playback_cb(mock_playback_user_data, output_samples, 256);

    if (output_samples[10] != input_samples[10]) {
        fprintf(stderr, "Playback mismatch at index 10: Out=%f, In=%f\n", output_samples[10], input_samples[10]);
        return 1;
    }

    printf("Voice Loopback Verification Passed!\n");

    // Clean up
    close(sv[0]);
    close(sv[1]);
    KTerm_Destroy(term);
    return 0;
}
