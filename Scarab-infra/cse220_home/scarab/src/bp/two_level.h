#ifndef __TWO_LEVEL_H__
#define __TWO_LEVEL_H__

#include "op.h"

// Function prototypes for the two-level predictor

void bp_two_level_init(int id, int num_entries);
void bp_two_level_timestamp(Op* op, int id);
Flag bp_two_level_pred(Op* op, int id);
void bp_two_level_spec_update(Op* op, int id, Flag outcome);
void bp_two_level_update(Op* op, int id, Flag outcome);
void bp_two_level_retire(Op* op, int id);
void bp_two_level_recover(Op* op, int id);
Flag bp_two_level_full(int id);

#endif // __TWO_LEVEL_H__
