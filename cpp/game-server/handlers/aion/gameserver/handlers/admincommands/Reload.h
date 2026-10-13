#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //reload: reloads templates or handlers (static data).
 * <p>
 * C++: the config, commands and ai arms run. The eight static-data arms (quests, skills, npcskills, items, customdrops, events, arcade,
 * decomposables) stay AION_UNPORTED (D3): a reload replaces a published holder, which needs HolderRef to republish (it publishes once,
 * static-data.md §3.6) and a loader of one holder from one file; quests, npcskills and events also need the in-place reload setters (CP5) and
 * QuestEngine.reload (A-D1).
 *
 * @author MrPoke, Neon
 */
class Reload : public AdminCommand {
public:
	Reload();

	void execute(Player& admin, std::span<const std::string> params) override;
};

} // namespace aion::gameserver::handlers::admincommands
