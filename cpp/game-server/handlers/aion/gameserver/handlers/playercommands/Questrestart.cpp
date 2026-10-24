#include "aion/gameserver/handlers/playercommands/Questrestart.h"

#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::handlers::playercommands {

AION_PLAYER_COMMAND(Questrestart);

// Java Questrestart.java:17-21
Questrestart::Questrestart()
	: PlayerCommand("questrestart", "Restarts a quest.",
		  "<quest link|ID> - Restarts the specified quest.\n") {
}

// Java Questrestart.java:23-58
void Questrestart::execute(Player& player, std::span<const std::string> params) {
	if (params.empty()) { // parity= if (params == null || params.length == 0) {
		sendInfo(player);
		return;
	}

	int32_t id = ChatUtil::getQuestId(params[0]);
	if (id == 0) {
		sendInfo(player, "Invalid quest.");
		return;
	}

	runtime::Ptr<QuestState> qs = player.getQuestStateList()->getQuestState(id);
	if (qs == nullptr || (qs->getStatus() != QuestStatus::START && qs->getStatus() != QuestStatus::REWARD)) {
		sendInfo(player, "Only currently active quests can be restarted.");
		return;
	}

	if (id == 1006 || id == 2008 || (qs->getStatus() == QuestStatus::REWARD && qs->getQuestVars()->getQuestVars() == 1)) {
		sendInfo(player, "Quest " + ChatUtil::quest(id) + " can't be restarted.");
		return;
	}

	if (qs->getQuestVarById(0) == 0) {
		sendInfo(player, "Restarting quest " + ChatUtil::quest(id) + " would have no effect.");
		return;
	}

	qs->setStatus(QuestStatus::START);
	qs->setQuestVar(0);
	qs->setFlags(0);
	qs->setRewardGroup(std::nullopt);
	PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::UPDATE, *qs)); // parity= PacketSendUtility.sendPacket(player, new SM_QUEST_ACTION(ActionType.UPDATE, qs));
	sendInfo(player, "Quest " + ChatUtil::quest(id) + " was restarted.");
}

} // namespace aion::gameserver::handlers::playercommands
