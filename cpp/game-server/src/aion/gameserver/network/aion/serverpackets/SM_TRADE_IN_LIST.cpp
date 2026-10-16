#include "aion/gameserver/network/aion/serverpackets/SM_TRADE_IN_LIST.h"

#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.h"
#include "aion/gameserver/model/templates/tradelist/TradeNpcTypeInfo.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TRADE_IN_LIST::SM_TRADE_IN_LIST(model::gameobjects::Npc& npcValue, const model::templates::tradelist::TradeListTemplate* tlistValue,
	int32_t buyPriceModifierValue)
	: AionServerPacket(opcodeOf<SM_TRADE_IN_LIST>), npc(npcValue), tlist(tlistValue), buyPriceModifier(buyPriceModifierValue) {
}

SM_TRADE_IN_LIST::~SM_TRADE_IN_LIST() = default;

void SM_TRADE_IN_LIST::writeImpl(AionConnection* con) {
	if ((tlist != nullptr) && (tlist->getNpcId() != 0) && (tlist->getCount() != 0)) {
		writeD(npc->getObjectId());
		writeC(model::templates::tradelist::index(tlist->getTradeNpcType()));
		writeD(buyPriceModifier); // Vendor Buy Price Modifier
		writeD(100); // new aion 4.5
		writeH(tlist->getCount());
		for (const model::templates::tradelist::TradeListTemplate::TradeTab& tradeTabl : tlist->getTradeTablist())
			writeD(tradeTabl.getId());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
