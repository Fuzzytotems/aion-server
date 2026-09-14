#pragma once

// Prelude of the admin chat command handlers (data/handlers/admincommands; the three command packages form one library, aion_gs_handlers_commands).
// A prelude holds the core names of its category as using-declarations in the category namespace, e.g. `using gameserver::ai::NpcAI;`
// (handlers-and-porting-plan.md §1.2). It is identical for every file of the category, so unity batches may include it any number of times.
// Handler files include it first; it also provides the registration markers and AION_UNPORTED. S0b fills in the using-declarations of the
// handler-facing hub headers. The handler file rules apply (aion_gs_regscan): declarations only inside the namespace of this directory, no
// namespace-scope `static`, no other namespaces.
// Each command package has its own prelude: a using-declaration here and a command class of the same name in this package (Pet, Event,
// SysMail, WorldRaid) are a compile error in every build, instead of lookup that depends on the unity batch. Such a name is not re-exported;
// the commands spell the core type qualified (test_handler_preludes checks the names against the Java classes of the package).

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::handlers::admincommands {

} // namespace aion::gameserver::handlers::admincommands
