#include "aion/gameserver/network/aion/clientpackets/CM_DELETE_QUEST.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/QuestService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;

CM_DELETE_QUEST::CM_DELETE_QUEST(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_DELETE_QUEST.java:22-25
void CM_DELETE_QUEST::readImpl() {
	questId = readD();
}

// Java CM_DELETE_QUEST.java:27-37
void CM_DELETE_QUEST::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	const model::templates::QuestTemplate* qt = dataholders::DataManager::QUEST_DATA->getQuestById(questId);

	if (qt != nullptr && qt->isTimer()) {
		player->getController().cancelTask(model::TaskId::QUEST_TIMER);
		sendPacket(serverpackets::SM_QUEST_ACTION(questId, 0));
	}
	services::QuestService::abandonQuest(*player, questId);
}

AION_CLIENT_PACKET(CM_DELETE_QUEST);

} // namespace aion::gameserver::network::aion::clientpackets
