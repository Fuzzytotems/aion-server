#pragma once

// Prelude of the zone handlers (data/handlers/zone, also zone/pvpZones).
// A prelude holds the core names of its category as using-declarations in the category namespace, e.g. `using gameserver::ai::NpcAI;`
// (handlers-and-porting-plan.md §1.2). It is identical for every file of the category, so unity batches may include it any number of times,
// and it is the category's precompiled header. Handler files include it first; it also provides the registration markers and
// AION_UNPORTED. S0b fills in the using-declarations of the handler-facing hub headers. The handler file rules apply (aion_gs_regscan):
// declarations only inside the namespace of this directory, no namespace-scope `static`, no other namespaces.

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::handlers::zone {

} // namespace aion::gameserver::handlers::zone
