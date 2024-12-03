#ifndef __TWO_LEVEL_H__
#define __TWO_LEVEL_H__

#include "op.h"
#include "bp.h"

// Function prototypes for the two-level predictor
void bp_two_level_init(void);
void bp_two_level_timestamp(Op* op);
Flag bp_two_level_pred(Op* op);
void bp_two_level_spec_update(Op* op);
void bp_two_level_update(Op* op, Flag outcome);
void bp_two_level_retire(Op* op);
void bp_two_level_recover(Recovery_Info* rec_info);
uns8 bp_two_level_full(uns id);

#endif // __TWO_LEVEL_H__