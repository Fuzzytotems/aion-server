#include "aion/gameserver/ai/handler/TalkEventHandler.h"

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/TownService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::ai::handler {

using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using utils::PacketSendUtility;

void TalkEventHandler::onTalk(NpcAI& npcAI, Creature& creature) {
	onSimpleTalk(npcAI, creature);

	if (runtime::Ptr<Player> player = runtime::as<Player>(creature)) {
		runtime::Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(
			runtime::Ptr<model::gameobjects::VisibleObject>(npcAI.getOwner()), *player, 0, model::DialogAction::USE_OBJECT);
		if (questEngine::QuestEngine::getInstance().onDialog(*env))
			return;
		// only player villagers can use villager npcs in oriel/pernon
		switch (npcAI.getOwner().getObjectTemplate()->getTitleId()) {
			case 462877: {
				int32_t playerTownId = services::TownService::getInstance().getTownResidence(*player);
				int32_t currentTownId = services::TownService::getInstance().getTownIdByPosition(*player);
				if (playerTownId != currentTownId) {
					PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(npcAI.getOwner().getObjectId(), 44));
				} else {
					PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(npcAI.getOwner().getObjectId(), 10));
				}
				return;
			}
			default: {
				int32_t dialogPageId = model::getStartPageId(npcAI.getOwner(), *player);
				PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(npcAI.getOwner().getObjectId(), dialogPageId));
				break;
			}
		}
	}
}

void TalkEventHandler::onSimpleTalk(NpcAI& npcAI, Creature& creature) {
	if (npcAI.getOwner().getObjectTemplate()->isDialogNpc()) {
		npcAI.setSubStateIfNot(AISubState::TALK);
		npcAI.getOwner().setTarget(runtime::Ptr<Creature>(creature));
	}
}

void TalkEventHandler::onFinishTalk(NpcAI& npcAI, Creature& creature) {
	Npc& owner = npcAI.getOwner();
	if (owner.isTargeting(creature.getObjectId())) {
		if (npcAI.getState() == AIState::FOLLOWING) {
			npcAI.think();
		} else {
			owner.setTarget(nullptr);
		}
	}
}

} // namespace aion::gameserver::ai::handler
