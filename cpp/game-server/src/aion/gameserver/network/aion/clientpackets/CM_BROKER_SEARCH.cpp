#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_SEARCH.h"

#include <cstddef>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/services/BrokerService.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

namespace {

/** C++ only: the list object the broker keeps in the cache of the player (BrokerPlayerCache.itemList); Java hands over the ArrayList itself */
runtime::Ref<runtime::RcArrayList<int32_t>> searchList(const std::vector<int32_t>& itemList) {
	runtime::Ref<runtime::RcArrayList<int32_t>> list = runtime::RcArrayList<int32_t>::create(AION_LOCK_CLASS(BrokerPlayerCache::itemList));
	for (int32_t itemId : itemList)
		list->add(itemId);
	return list;
}

} // namespace

CM_BROKER_SEARCH::CM_BROKER_SEARCH(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_BROKER_SEARCH.java:30-39
void CM_BROKER_SEARCH::readImpl() {
	brokerObjId = readD();
	sortType = readC(); // 1 - name; 2 - level; 4 - totalPrice; 6 - price for piece
	page = readUH();
	mask = readUH();
	int32_t itemCount = readUH();
	itemList.clear(); // Java: new ArrayList<>(itemCount)
	itemList.reserve(static_cast<size_t>(itemCount));
	for (int32_t index = 0; index < itemCount; index++)
		itemList.push_back(readD());
}

// Java CM_BROKER_SEARCH.java:42-48
void CM_BROKER_SEARCH::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (player->isTargetingNpcWithFunction(brokerObjId, model::DialogAction::OPEN_VENDOR))
		services::BrokerService::getInstance().showRequestedItems(*player, mask, sortType, page, searchList(itemList));
	else
		utils::audit::AuditLogger::log(*player, "tried to search for items in broker without targeting a broker");
}

AION_CLIENT_PACKET(CM_BROKER_SEARCH);

} // namespace aion::gameserver::network::aion::clientpackets
