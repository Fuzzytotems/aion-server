#include "aion/gameserver/handlers/ai/quests/QuestItemNpcAI.h"

#include <vector>

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/handler/CreatureEventHandler.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestActionType.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/drop/DropService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::handlers::ai::quests {

AION_AI(QuestItemNpcAI, "quest_use_item");

// Java QuestItemNpcAI.java:33-40
void QuestItemNpcAI::handleDialogStart(Player& player) {
	if (!(QuestEngine::getInstance().onCanAct(*QuestEnv::create(runtime::Ptr<VisibleObject>(getOwner()), player, 0),
			getObjectTemplate()->getTemplateId(), QuestActionType::ACTION_ITEM_USE))) {
		return;
	}
	ActionItemNpcAI::handleDialogStart(player);
}

// Java QuestItemNpcAI.java:42-71. Java's `registeredPlayers` is the list the group variants return (a new ArrayList) or a new one; an empty
// answer of a variant gets the user added, as in Java.
void QuestItemNpcAI::handleUseItemFinish(Player& player) {
	const runtime::Ref<QuestEnv> env = QuestEnv::create(runtime::Ptr<VisibleObject>(getOwner()), player, 0, model::DialogAction::USE_OBJECT);
	if (!QuestEngine::getInstance().onDialog(*env)) {
		if (getObjectTemplate()->isDialogNpc()) // show default dialog
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
		return;
	}

	if (QuestService::getQuestDrop(getNpcId()).empty())
		return;

	std::vector<runtime::Ptr<Player>> registeredPlayers;
	if (player.isInGroup()) {
		registeredPlayers = QuestService::getEachDropMembersGroup(*player.getPlayerGroup(), getNpcId(), env->getQuestId());
		if (registeredPlayers.empty()) {
			registeredPlayers.push_back(runtime::Ptr<Player>(player));
		}
	} else if (player.isInAlliance()) {
		registeredPlayers = QuestService::getEachDropMembersAlliance(*player.getPlayerAlliance(), getNpcId(), env->getQuestId());
		if (registeredPlayers.empty()) {
			registeredPlayers.push_back(runtime::Ptr<Player>(player));
		}
	} else {
		registeredPlayers.push_back(runtime::Ptr<Player>(player));
	}
	AIActions::registerDrop(*this, player, registeredPlayers);
	AIActions::die(*this, player);
	DropService::getInstance().requestDropList(runtime::Ptr<Player>(player), getObjectId());
}

// Java QuestItemNpcAI.java:73-76
void QuestItemNpcAI::handleCreatureSee(Creature& creature) {
	CreatureEventHandler::onCreatureSee(*this, creature);
}

} // namespace aion::gameserver::handlers::ai::quests
