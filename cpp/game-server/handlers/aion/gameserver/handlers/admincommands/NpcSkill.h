#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //npcskill: lists the skills of the targeted NPC.
 */
class NpcSkill : public AdminCommand {
public:
	NpcSkill();

	void execute(Player& player, std::span<const std::string> params) override;

	void info(Player& player, std::optional<std::string_view> message) override;

private:
	void showAllLines(Player& admin, std::string_view str);
};

} // namespace aion::gameserver::handlers::admincommands
