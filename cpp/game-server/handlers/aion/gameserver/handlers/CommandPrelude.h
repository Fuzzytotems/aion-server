#pragma once

// Precompiled header of the chat command library aion_gs_handlers_commands (chunks C1/C2: data/handlers/admincommands, playercommands,
// consolecommands). It only includes the three package preludes and declares nothing: the using-declarations of a prelude go into the
// namespace of its own package, never into the common parent aion::gameserver::handlers, where a command class of the same name in an
// earlier file of a unity batch would silently win over the core name (admincommands::Pet versus model::gameobjects::Pet). Handler files
// include the prelude of their own package, not this header.

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"
#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"
#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"
