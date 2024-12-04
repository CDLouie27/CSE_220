#include "two_level.h"
#include <stdlib.h>
#include <string.h>

// Two-Level Predictor Structure
typedef struct {
    unsigned int history;         // History register
    unsigned char* pattern_table; // Pattern table
    int num_entries;              // Number of entries in the pattern table
    int history_length;           // Dynamic history length
    unsigned int global_history;  // Global history for hybrid prediction
} Two_Level_Predictor;

// Static predictor (single instance for simplicity in this implementation)
static Two_Level_Predictor predictor;

// Initialize the two-level predictor
void bp_two_level_init(void) {
    predictor.history = 0;
    predictor.global_history = 0;
    predictor.num_entries = 1024; // Example size
    predictor.history_length = 10; // Start with 10-bit history
    predictor.pattern_table = (unsigned char*)calloc(predictor.num_entries, sizeof(unsigned char));
}

// Timestamp function (currently a stub for integration)
void bp_two_level_timestamp(Op* op) {
    (void)op; // Stub for integration
}

// Make a prediction
Flag bp_two_level_pred(Op* op) {
    unsigned int index = (predictor.history ^ predictor.global_history) % predictor.num_entries;
    unsigned char counter = predictor.pattern_table[index];

    // Combine two-level and global predictions
    unsigned char global_pred = (predictor.global_history % 2); // Simple global predictor
    unsigned char local_pred = (counter >= 2);

    return (local_pred + global_pred) > 1; // Majority vote
}

// Speculative update (placeholder for outcome retrieval)
void bp_two_level_spec_update(Op* op) {
    (void)op;
    // Stub: Outcome would need to be passed or retrieved here
}

// Update predictor after resolution
void bp_two_level_update(Op* op, Flag outcome) {
    unsigned int index = (predictor.history ^ predictor.global_history) % predictor.num_entries;

    // Update local prediction
    if (outcome) {
        if (predictor.pattern_table[index] < 3) {
            predictor.pattern_table[index]++;
        }
    } else {
        if (predictor.pattern_table[index] > 0) {
            predictor.pattern_table[index]--;
        }
    }

    // Update global history
    predictor.global_history = ((predictor.global_history << 1) | outcome) & ((1 << 10) - 1);

    // Update dynamic history length (adaptive mechanism)
    if (outcome && predictor.pattern_table[index] == 3) {
        if (predictor.history_length < 16) {
            predictor.history_length++;
        }
    } else if (!outcome && predictor.pattern_table[index] == 0) {
        if (predictor.history_length > 4) {
            predictor.history_length--;
        }
    }

    // Update local history
    predictor.history = ((predictor.history << 1) | outcome) & ((1 << predictor.history_length) - 1);
}

// Retire a branch (clean-up or final adjustments)
void bp_two_level_retire(Op* op) {
    (void)op; // Stub for integration
}

// Recover predictor state after a misprediction
void bp_two_level_recover(Recovery_Info* rec_info) {
    if (rec_info) {
        // Restore the global history to the state used during prediction
        predictor.global_history = rec_info->pred_global_hist & ((1 << 10) - 1);

        // Update the local history based on the correct branch direction
        predictor.history = ((predictor.history << 1) | rec_info->new_dir) & ((1 << predictor.history_length) - 1);

        // Optionally, adjust the pattern table entry corresponding to the branch
        unsigned int index = (predictor.history ^ predictor.global_history) % predictor.num_entries;
        if (rec_info->new_dir) {
            if (predictor.pattern_table[index] < 3) {
                predictor.pattern_table[index]++;
            }
        } else {
            if (predictor.pattern_table[index] > 0) {
                predictor.pattern_table[index]--;
            }
        }
    } else {
        // Default behavior: reset history to avoid cascading errors
        predictor.history = 0;
        predictor.global_history = 0;
    }
}

// Check if predictor is full
uns8 bp_two_level_full(uns id) {
    (void)id; // No capacity issues in this example
    return 0;
}
