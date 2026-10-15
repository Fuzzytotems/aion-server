#include "aion/gameserver/handlers/consolecommands/Endquest.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestState.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Endquest);

// Java Endquest.java:19-23
Endquest::Endquest() : ConsoleCommand("endquest", "Completes a quest.",
	"<quest> - Completes the specified quest of your target, without giving rewards (defaults to your character, if no player is targeted).\n") {
}

// Java Endquest.java:25-51
void Endquest::execute(Player& admin, std::span<const std::string> params) {
	using ActionType = network::aion::serverpackets::SM_QUEST_ACTION_ActionType; // parity: C++ using-declaration (Java imports SM_QUEST_ACTION.ActionType)
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget()); // parity= Player player = admin.getTarget() instanceof Player target ? target : admin;
	Player& player = target != nullptr ? *target : admin; // parity: (the same)
	int32_t questId = ChatUtil::getQuestId(params[0]);
	if (questId == 0) {
		sendInfo(admin, "Invalid quest link or ID.");
		return;
	}

	runtime::Ptr<QuestState> qs = player.getQuestStateList()->getQuestState(questId); // parity= QuestState qs = player.getQuestStateList().getQuestState(questId);
	if (qs == nullptr) {
		sendInfo(admin, "Quest must be started first.");
		return;
	}
	qs->setStatus(QuestStatus::COMPLETE);
	qs->setQuestVar(0);
	if (!DataManager::QUEST_DATA->getQuestById(qs->getQuestId())->getRewards().empty()) // parity= if (!DataManager.QUEST_DATA.getQuestById(qs.getQuestId()).getRewards().isEmpty())
		qs->setRewardGroup(0); // follow quests could require reward group > 0 to be unlocked (see quest_data.xml)
	PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(ActionType::UPDATE, *qs)); // parity= PacketSendUtility.sendPacket(player, new SM_QUEST_ACTION(ActionType.UPDATE, qs));
	QuestEngine::getInstance().onQuestCompleted(player, qs->getQuestId());
	player.getController().updateNearbyQuests();
}

} // namespace aion::gameserver::handlers::consolecommands
