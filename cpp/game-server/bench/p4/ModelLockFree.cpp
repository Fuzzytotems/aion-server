// PROTOTYPE BENCHMARK CODE (design §19 P4): the prototype model with lock-free ConcurrentHashMap reads (the design default).

#define P4_NAMESPACE lockfree
#define P4_LOCKED_READS 0
#include "p4/ProtoModel.inl"
