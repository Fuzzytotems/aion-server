#pragma once

#include "aion/gameserver/handlers/playercommands/PlayerCommandsPrelude.h"

namespace aion::gameserver::handlers::playercommands {

/**
 * .id: shows item/quest/NPC IDs.
 */
class Id : public PlayerCommand {
public:
	Id();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	static std::string getQuestIcon(const QuestTemplate& template_);
};

} // namespace aion::gameserver::handlers::playercommands
