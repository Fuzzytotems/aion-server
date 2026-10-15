#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //reload: reloads templates or handlers (static data).
 * <p>
 * C++: the static-data arms load the files with the strict binder and replace the published holder with HolderRef::republish (owner's
 * decision 2026-10-13; the old holder stays alive). npcskills and events replace their holder, where Java calls the in-place setters on it. The
 * quests arm stays AION_UNPORTED: it needs QuestEngine.reload (A-D1).
 *
 * @author MrPoke, Neon
 */
class Reload : public AdminCommand {
public:
	Reload();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
