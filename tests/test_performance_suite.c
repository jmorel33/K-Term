#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "test_utilities.h"
#include <stdio.h>
#include <math.h>

#define EPSILON 1e-9

int test_set_pipeline_fps(KTerm* term, KTermSession* session) {
    // Default is 60 FPS -> 1/60 frame time, 0.5 budget (0.008333)
    // Wait, KTerm_InitSession sets it to 60 FPS and 0.5 budget.
    // KTerm_SetPipelineTargetFPS sets it to 0.3 budget.

    KTerm_SetPipelineTargetFPS(term, 30);
    double expected_frame_time = 1.0 / 30.0;
    double expected_budget = expected_frame_time * 0.3;

    if (fabs(session->VTperformance.target_frame_time - expected_frame_time) > EPSILON) {
        fprintf(stderr, "FAIL: target_frame_time mismatch. Got %f, expected %f\n",
                session->VTperformance.target_frame_time, expected_frame_time);
        return 0;
    }
    if (fabs(session->VTperformance.time_budget - expected_budget) > EPSILON) {
        fprintf(stderr, "FAIL: time_budget mismatch. Got %f, expected %f\n",
                session->VTperformance.time_budget, expected_budget);
        return 0;
    }

    // Test invalid FPS (should not change)
    KTerm_SetPipelineTargetFPS(term, 0);
    if (fabs(session->VTperformance.target_frame_time - expected_frame_time) > EPSILON) return 0;

    KTerm_SetPipelineTargetFPS(term, -10);
    if (fabs(session->VTperformance.target_frame_time - expected_frame_time) > EPSILON) return 0;

    return 1;
}

int test_set_pipeline_time_budget(KTerm* term, KTermSession* session) {
    KTerm_SetPipelineTargetFPS(term, 60);
    double frame_time = 1.0 / 60.0;

    // Test valid percentage
    KTerm_SetPipelineTimeBudget(term, 0.5);
    double expected_budget = frame_time * 0.5;
    if (fabs(session->VTperformance.time_budget - expected_budget) > EPSILON) {
        fprintf(stderr, "FAIL: time_budget mismatch for 0.5. Got %f, expected %f\n",
                session->VTperformance.time_budget, expected_budget);
        return 0;
    }

    // Test boundary: 1.0
    KTerm_SetPipelineTimeBudget(term, 1.0);
    expected_budget = frame_time * 1.0;
    if (fabs(session->VTperformance.time_budget - expected_budget) > EPSILON) {
        fprintf(stderr, "FAIL: time_budget mismatch for 1.0. Got %f, expected %f\n",
                session->VTperformance.time_budget, expected_budget);
        return 0;
    }

    // Test very small valid percentage
    KTerm_SetPipelineTimeBudget(term, 0.01);
    expected_budget = frame_time * 0.01;
    if (fabs(session->VTperformance.time_budget - expected_budget) > EPSILON) return 0;

    // Test invalid: 0.0 (should not change from 0.01)
    KTerm_SetPipelineTimeBudget(term, 0.0);
    if (fabs(session->VTperformance.time_budget - expected_budget) > EPSILON) {
        fprintf(stderr, "FAIL: time_budget changed for invalid input 0.0. Got %f, expected %f\n",
                session->VTperformance.time_budget, expected_budget);
        return 0;
    }

    // Test invalid: 1.1 (should not change)
    KTerm_SetPipelineTimeBudget(term, 1.1);
    if (fabs(session->VTperformance.time_budget - expected_budget) > EPSILON) return 0;

    // Test invalid: -0.1 (should not change)
    KTerm_SetPipelineTimeBudget(term, -0.1);
    if (fabs(session->VTperformance.time_budget - expected_budget) > EPSILON) return 0;

    return 1;
}

int main() {
    KTerm* term = create_test_term(80, 25);
    if (!term) return 1;
    KTermSession* session = GET_SESSION(term);

    TestResults results = {0};
    print_test_header("Performance Suite");

    run_test("KTerm_SetPipelineTargetFPS valid and invalid", test_set_pipeline_fps, term, session, &results);
    run_test("KTerm_SetPipelineTimeBudget valid, boundary, and invalid", test_set_pipeline_time_budget, term, session, &results);

    print_test_summary(results.total, results.passed, results.failed);

    destroy_test_term(term);
    return results.failed > 0 ? 1 : 0;
}
