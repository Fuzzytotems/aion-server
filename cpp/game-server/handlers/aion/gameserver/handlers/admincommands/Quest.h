#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION_ActionType.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //quest: resets, starts, deletes, shows or sets the quest states of a player.
 *
 * @author MrPoke, Neon, Pad
 */
class Quest : public AdminCommand {
public:
	Quest();

	void execute(Player& admin, std::span<const std::string> params) override;

	void deleteQuest(Player& admin, Player& target, int32_t questId);

private:
	/** Java: import SM_QUEST_ACTION.ActionType */
	using ActionType = network::aion::serverpackets::SM_QUEST_ACTION_ActionType;

	void resetQuest(Player& admin, Player& target, int32_t questId);

	void startQuest(Player& admin, Player& target, int32_t questId);

	void startNpcFactionQuest(Player& admin, Player& target, int32_t questId, int32_t factionId);

	void showQuestStatus(Player& admin, Player& target, int32_t questId);

	void setQuestStatus(Player& admin, Player& target, int32_t questId, QuestStatus status, int32_t var, int32_t varNum);

	void setQuestFlags(Player& admin, Player& target, int32_t questId, int32_t flags);

	void sendQuestDialog(Player& admin, int32_t questId, int32_t dialogPageId);
};

} // namespace aion::gameserver::handlers::admincommands
