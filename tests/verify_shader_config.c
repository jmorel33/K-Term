#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "kterm.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>

char last_response[1024];
void my_response_callback(KTerm* term, const char* response, int length) {
    int safe_len = length < 1023 ? length : 1023;
    strncpy(last_response, response, safe_len);
    last_response[safe_len] = '\0';
}

void test_shader_config() {
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);
    assert(term != NULL);

    // Initial state check
    assert(term->visual_effects.curvature == 0.0f);

    // Set values via Gateway - Integer test
    KTerm_GatewayProcess(term, &term->sessions[0], "KTERM", "TEST", "SET", "SHADER;CRT_CURVATURE=1");
    assert(fabs(term->visual_effects.curvature - 1.0f) < 0.001f);

    // Set values via Gateway - Float test
    KTerm_GatewayProcess(term, &term->sessions[0], "KTERM", "TEST", "SET", "SHADER;CRT_CURVATURE=0.5;SCANLINE_INTENSITY=0.8;GLOW_INTENSITY=0.3;NOISE_ENABLE=0");

    assert(fabs(term->visual_effects.curvature - 0.5f) < 0.001f);
    assert(fabs(term->visual_effects.scanline_intensity - 0.8f) < 0.001f);
    assert(fabs(term->visual_effects.glow_intensity - 0.3f) < 0.001f);
    // Noise enable=0 should clear the flag
    assert((term->visual_effects.flags & SHADER_FLAG_NOISE) == 0);

    // Check buffer update
    // KTerm_Update triggers Compositor Prepare which updates buffer
    KTerm_Update(term);

    assert(term->shader_config_buffer.id != 0);

    KTerm_Destroy(term);
}

void test_shader_get() {
    KTermConfig config = {0};
    config.response_callback = my_response_callback;
    KTerm* term = KTerm_Create(config);

    term->visual_effects.curvature = 0.123f;

    KTerm_GatewayProcess(term, &term->sessions[0], "KTERM", "TEST", "GET", "SHADER");
    KTerm_Update(term); // Flush response

    assert(strstr(last_response, "CRT_CURVATURE:0.123") != NULL);

    KTerm_Destroy(term);
}

int main() {
    test_shader_config();
    test_shader_get();
    printf("Shader Config Tests Passed\n");
    return 0;
}
