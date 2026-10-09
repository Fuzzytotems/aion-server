#include "aion/gameserver/network/aion/clientpackets/CM_BUY_TRADE_IN_TRADE.h"

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
#include "aion/gameserver/services/TradeService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_BUY_TRADE_IN_TRADE::CM_BUY_TRADE_IN_TRADE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_BUY_TRADE_IN_TRADE.java:28-38
void CM_BUY_TRADE_IN_TRADE::readImpl() {
	tradeInItemObjIds.clear();
	sellerObjId = readD();
	mask = readC(); // NEW - TODO find out what this is!
	itemId = readD();
	count = readD();
	tradeInListCount = readUH();
	for (int32_t i = 0; i < tradeInListCount; i++)
		tradeInItemObjIds.push_back(readD());
}

// Java CM_BUY_TRADE_IN_TRADE.java:40-46
void CM_BUY_TRADE_IN_TRADE::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (count < 1)
		return;
	services::TradeService::performBuyFromTradeInTrade(*player, sellerObjId, itemId, count, tradeInItemObjIds);
}

AION_CLIENT_PACKET(CM_BUY_TRADE_IN_TRADE);

} // namespace aion::gameserver::network::aion::clientpackets
