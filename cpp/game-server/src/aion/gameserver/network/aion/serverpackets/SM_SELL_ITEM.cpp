#include "aion/gameserver/network/aion/serverpackets/SM_SELL_ITEM.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/TradeListData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.h"
#include "aion/gameserver/model/templates/tradelist/TradeNpcTypeInfo.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SELL_ITEM::SM_SELL_ITEM(model::gameobjects::Npc& npc)
	: AionServerPacket(opcodeOf<SM_SELL_ITEM>) {
	const model::templates::tradelist::TradeListTemplate* tradeList = dataholders::DataManager::TRADE_LIST_DATA->getPurchaseTemplate(npc.getNpcId());
	targetObjectId = npc.getObjectId();
	tradeNpcType = tradeList != nullptr ? tradeList->getTradeNpcType() : model::templates::tradelist::TradeNpcType::NORMAL;
	buyPriceRate = tradeList != nullptr ? tradeList->getBuyPriceRate() : detail::getVendorSellModifier();
	showBuyTab = npc.canSell();
	showSellTab = npc.canBuy() || npc.canPurchase();
	if (tradeList != nullptr) {
		for (const model::templates::tradelist::TradeListTemplate::TradeTab& tradeTab : tradeList->getTradeTablist())
			tradeTabs.push_back(&tradeTab);
	}
}

void SM_SELL_ITEM::writeImpl(AionConnection* con) {
	writeD(targetObjectId);
	writeC(model::templates::tradelist::index(tradeNpcType));
	writeD(buyPriceRate); // price * (buyPriceRate / 100) = display price
	writeC(showBuyTab ? 1 : 0); // npc sells
	writeC(showSellTab ? 1 : 0); // npc buys
	writeH(static_cast<int32_t>(tradeTabs.size()));
	for (const model::templates::tradelist::TradeListTemplate::TradeTab* tradeTab : tradeTabs)
		writeD(tradeTab->getId());
}

} // namespace aion::gameserver::network::aion::serverpackets
