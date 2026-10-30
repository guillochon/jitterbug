/* jb_policy.h - per-source credit/health assumptions.
 * EVERY NUMBER HERE IS A PLACEHOLDER until measured on a real B&W G3 with
 * NIST ea_non_iid (see docs/ENTROPY_POLICY.md). Keep them conservative. */
#ifndef JB_POLICY_H
#define JB_POLICY_H
#include "jb_engine.h"

/* Apply the default policy for the given source IDs. */
void jb_policy_apply_defaults(jb_engine *e);
/* Paranoid policy: mix everything, credit nothing (engine never seeds
 * from live samples alone). Useful for measurement builds. */
void jb_policy_apply_zero_credit(jb_engine *e);
const char *jb_source_name(jb_u8 src);
jb_u8 jb_source_symkind(jb_u8 src);

#endif
