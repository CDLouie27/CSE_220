#include "two_level.h"
#include <stdlib.h>
#include <string.h>

// Two-Level Predictor Structure
typedef struct {
    unsigned int history;        // History register
    unsigned char* pattern_table; // Pattern table
    int num_entries;             // Number of entries in the pattern table
} Two_Level_Predictor;

// Static array of predictors (one per core or ID)
static Two_Level_Predictor predictor;

// Initialize the two-level predictor
void bp_two_level_init(void) {
    predictor.history = 0;
    predictor.num_entries = 1024; // Example size
    predictor.pattern_table = (unsigned char*)calloc(predictor.num_entries, sizeof(unsigned char));
}

// Timestamp function (currently a stub for integration)
void bp_two_level_timestamp(Op* op) {
    (void)op; // Stub for integration
}

// Make a prediction
Flag bp_two_level_pred(Op* op) {
    unsigned int index = predictor.history % predictor.num_entries;
    unsigned char counter = predictor.pattern_table[index];
    return counter >= 2; // Assume 2-bit saturating counters
}

// Speculative update (placeholder for outcome retrieval)
void bp_two_level_spec_update(Op* op) {
    (void)op;
    // Stub: Outcome would need to be passed or retrieved here
}

// Update predictor after resolution
void bp_two_level_update(Op* op, Flag outcome) {
    unsigned int index = predictor.history % predictor.num_entries;
    if (outcome) {
        if (predictor.pattern_table[index] < 3) {
            predictor.pattern_table[index]++;
        }
    } else {
        if (predictor.pattern_table[index] > 0) {
            predictor.pattern_table[index]--;
        }
    }
    predictor.history = ((predictor.history << 1) | outcome) & ((1 << 10) - 1); // 10-bit history
}

// Retire a branch (clean-up or final adjustments)
void bp_two_level_retire(Op* op) {
    (void)op; // Stub for integration
}

// Recover predictor state after a misprediction
void bp_two_level_recover(Recovery_Info* rec_info) {
    predictor.history = 0; // Reset history on recovery
    (void)rec_info; // Stub for recovery data usage
}

// Check if predictor is full
uns8 bp_two_level_full(uns id) {
    (void)id; // No capacity issues in this example
    return 0;
}
