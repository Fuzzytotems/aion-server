#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_DECORATE.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/controllers/HouseController.h"
#include "aion/gameserver/model/gameobjects/HouseDecoration.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/PartTypeInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_EDIT.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;
using serverpackets::SM_HOUSE_EDIT;


CM_HOUSE_DECORATE::CM_HOUSE_DECORATE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_HOUSE_DECORATE.java:28-32
void CM_HOUSE_DECORATE::readImpl() {
	objectId = readD();
	readD(); // templateId (already known by objectId)
	lineNo = readUH();
}

// Java CM_HOUSE_DECORATE.java:35-56
void CM_HOUSE_DECORATE::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (!player)
		return;
	const runtime::Ptr<model::house::House> house = player->getActiveHouse();
	const std::optional<model::templates::housing::PartType> partType = model::templates::housing::getForLineNr(lineNo);
	if (!partType) // client may send lineNos which are not even implemented on client side (like 20-26)
		return;
	if (!house) // Java: house.getRegistry() of a player without a house
		throw runtime::NullPointerException("the player has no active house");
	int32_t roomNo = lineNo - getStartLineNr(*partType);
	if (objectId == 0) { // change appearance to default and delete any applied custom decor
		house->getRegistry()->discardDecor(*partType, roomNo);
	} else { // apply decor and remove it from registry
		const runtime::Ptr<model::gameobjects::HouseDecoration> decor = house->getRegistry()->getDecorByObjId(objectId);
		if (!decor) // Java: setUsed(null, roomNo) dereferences it
			throw runtime::NullPointerException("no house decoration " + std::to_string(objectId));
		house->getRegistry()->setUsed(*decor, roomNo);
		sendPacket(SM_HOUSE_EDIT(4, 2, objectId)); // yes, in retail it's sent twice!
	}
	sendPacket(SM_HOUSE_EDIT(4, 2, objectId));
	house->getController().updateAppearance();
	questEngine::QuestEngine::getInstance().onHouseItemUseEvent(*questEngine::model::QuestEnv::create(nullptr, *player, 0));
}

AION_CLIENT_PACKET(CM_HOUSE_DECORATE);

} // namespace aion::gameserver::network::aion::clientpackets
