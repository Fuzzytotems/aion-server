// PROTOTYPE BENCHMARK CODE (design §19 P4): the prototype model with stripe-locked ConcurrentHashMap reads (AION_CHM_LOCKED_READS fallback,
// design §17 risk mitigation).

#define P4_NAMESPACE lockedreads
#define P4_LOCKED_READS 1
#include "p4/ProtoModel.inl"
