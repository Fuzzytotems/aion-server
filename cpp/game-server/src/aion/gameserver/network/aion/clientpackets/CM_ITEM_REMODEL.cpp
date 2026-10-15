#include "aion/gameserver/network/aion/clientpackets/CM_ITEM_REMODEL.h"

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
#include "aion/gameserver/services/item/ItemRemodelService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_ITEM_REMODEL::CM_ITEM_REMODEL(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_ITEM_REMODEL.java:22-28
void CM_ITEM_REMODEL::readImpl() {
	readD(); // npcId
	keepItemId = readD();
	extractItemId = readD();
	readD(); // unk 0
}

// Java CM_ITEM_REMODEL.java:30-34
void CM_ITEM_REMODEL::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
	services::item::ItemRemodelService::remodelItem(*activePlayer, keepItemId, extractItemId);
}

AION_CLIENT_PACKET(CM_ITEM_REMODEL);

} // namespace aion::gameserver::network::aion::clientpackets
