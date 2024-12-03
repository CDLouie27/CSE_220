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
static Two_Level_Predictor* predictors = NULL;

// Initialize the two-level predictor
void bp_two_level_init(int id, int num_entries) {
    if (!predictors) {
        predictors = (Two_Level_Predictor*)calloc(10, sizeof(Two_Level_Predictor)); // Assuming up to 10 predictors
    }
    predictors[id].history = 0;
    predictors[id].num_entries = num_entries;
    predictors[id].pattern_table = (unsigned char*)calloc(num_entries, sizeof(unsigned char));
}

// Timestamp function (currently a stub for integration)
void bp_two_level_timestamp(Op* op, int id) {
    // Could be used for debug or timing information
    (void)op;
    (void)id;
}

// Make a prediction
Flag bp_two_level_pred(Op* op, int id) {
    Two_Level_Predictor* pred = &predictors[id];
    unsigned int index = pred->history % pred->num_entries;
    unsigned char counter = pred->pattern_table[index];
    return counter >= 2; // Assume 2-bit saturating counters
}

// Speculative update (update predictor state speculatively)
void bp_two_level_spec_update(Op* op, int id, Flag outcome) {
    Two_Level_Predictor* pred = &predictors[id];
    unsigned int index = pred->history % pred->num_entries;
    if (outcome) {
        if (pred->pattern_table[index] < 3) {
            pred->pattern_table[index]++;
        }
    } else {
        if (pred->pattern_table[index] > 0) {
            pred->pattern_table[index]--;
        }
    }
    pred->history = ((pred->history << 1) | outcome) & ((1 << 10) - 1); // 10-bit history
}

// Update predictor after resolution
void bp_two_level_update(Op* op, int id, Flag outcome) {
    bp_two_level_spec_update(op, id, outcome);
}

// Retire a branch (clean-up or final adjustments)
void bp_two_level_retire(Op* op, int id) {
    // No additional action needed for this simple predictor
    (void)op;
    (void)id;
}

// Recover predictor state after a misprediction
void bp_two_level_recover(Op* op, int id) {
    Two_Level_Predictor* pred = &predictors[id];
    pred->history = 0; // Reset history on recovery
}

// Check if predictor is full (currently a stub for integration)
Flag bp_two_level_full(int id) {
    // Always return false, as no capacity check is needed
    return 0;
}
