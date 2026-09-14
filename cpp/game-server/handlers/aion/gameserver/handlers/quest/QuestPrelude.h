#pragma once

// Prelude of the quest handlers (data/handlers/quest, also quest/<region>/...).
// A prelude holds the core names of its category as using-declarations in the category namespace, e.g. `using gameserver::ai::NpcAI;`
// (handlers-and-porting-plan.md §1.2). It is identical for every file of the category, so unity batches may include it any number of times,
// and it is the category's precompiled header. Handler files include it first; it also provides the registration markers and
// AION_UNPORTED. S0b fills in the using-declarations of the handler-facing hub headers. The handler file rules apply (aion_gs_regscan):
// declarations only inside the namespace of this directory, no namespace-scope `static`, no other namespaces.
// The DialogAction using-directive is the only one allowed in handler code (CONVENTIONS, handler file rules).

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::handlers::quest {

using namespace aion::gameserver::model::DialogAction; // Java: import static DialogAction.*

} // namespace aion::gameserver::handlers::quest
