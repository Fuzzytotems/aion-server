#include "aion/gameserver/network/aion/clientpackets/CM_PRIVATE_STORE.h"

#include <cstddef>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/trade/TradePSItem.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/services/PrivateStoreService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::trade::TradePSItem;

CM_PRIVATE_STORE::CM_PRIVATE_STORE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_PRIVATE_STORE.java:23-33
void CM_PRIVATE_STORE::readImpl() {
	int32_t itemCount = readUH();
	tradePSItems.clear();
	tradePSItems.reserve(static_cast<size_t>(itemCount));
	for (int32_t i = 0; i < itemCount; i++) {
		int32_t itemObjId = readD();
		int32_t itemId = readD();
		int32_t count = readUH();
		int64_t price = readQ();
		tradePSItems.push_back(TradePSItem::create(itemObjId, itemId, count, price));
	}
}

// Java CM_PRIVATE_STORE.java:36-42
void CM_PRIVATE_STORE::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (tradePSItems.size() <= 0) {
		services::PrivateStoreService::closePrivateStore(*player);
	} else {
		// the service borrows the items the packet owns (Java passes the array)
		std::vector<runtime::Ptr<TradePSItem>> items(tradePSItems.begin(), tradePSItems.end());
		services::PrivateStoreService::createStoreWithItems(*player, items);
	}
}

AION_CLIENT_PACKET(CM_PRIVATE_STORE);

} // namespace aion::gameserver::network::aion::clientpackets
