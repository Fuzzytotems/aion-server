#include "aion/gameserver/network/aion/clientpackets/CM_QUESTIONNAIRE.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/HTMLService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_QUESTIONNAIRE::CM_QUESTIONNAIRE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_QUESTIONNAIRE.java:31-40
void CM_QUESTIONNAIRE::readImpl() {
	objectId = readD();
	itemSize = readUH();
	items.clear(); // Java: items = new ArrayList<>()
	for (int32_t i = 0; i < itemSize; i++) {
		itemId = readD();
		items.push_back(itemId);
	}
	stringItemsId = readS();
}

// Java CM_QUESTIONNAIRE.java:42-48
void CM_QUESTIONNAIRE::runImpl() {
	if (objectId > 0) {
		runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
		services::HTMLService::getReward(player, objectId, items);
	}
}

AION_CLIENT_PACKET(CM_QUESTIONNAIRE);

} // namespace aion::gameserver::network::aion::clientpackets
