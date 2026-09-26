#include "aion/gameserver/network/aion/serverpackets/SM_TRADELIST.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GoodsListData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/limiteditems/LimitedItem.h"
#include "aion/gameserver/model/limiteditems/LimitedTradeNpc.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/templates/goods/GoodsList.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.h"
#include "aion/gameserver/model/templates/tradelist/TradeNpcTypeInfo.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TRADELIST::SM_TRADELIST(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::tradelist::TradeListTemplate* tlist, int32_t buyPriceModifierValue)
	: AionServerPacket(opcodeOf<SM_TRADELIST>) {
	runtime::Ptr<model::team::legion::Legion> legion = player.getLegion(); // Java reads getLegion() twice
	int32_t legionLevel = legion == nullptr ? 0 : legion->getLegionLevel();
	targetObjId = npc.getObjectId();
	playerObjId = player.getObjectId();
	tradeNpcType = tlist->getTradeNpcType();
	buyPriceModifier = buyPriceModifierValue;
	showBuyTab = npc.canSell();
	showSellTab = npc.canBuy();
	for (const model::templates::tradelist::TradeListTemplate::TradeTab& tab : tlist->getTradeTablist()) {
		const model::templates::goods::GoodsList* goodsList = dataholders::DataManager::GOODSLIST_DATA->getGoodsListById(tab.getId());
		if (goodsList == nullptr || goodsList->getLegionLevel() > legionLevel)
			continue;
		tradeTablist.push_back(&tab);
	}
	runtime::Ptr<model::limiteditems::LimitedTradeNpc> limitedTradeNpc = detail::getLimitedTradeNpc(tlist->getNpcId());
	if (limitedTradeNpc != nullptr) {
		for (const auto& limitedItem : limitedTradeNpc->getLimitedItems().snapshot())
			limitedItems.emplace_back(limitedItem);
	}
}

SM_TRADELIST::~SM_TRADELIST() = default;

void SM_TRADELIST::writeImpl(AionConnection* con) {
	writeD(targetObjId);
	writeC(model::templates::tradelist::index(tradeNpcType)); // reward, abyss or normal
	writeD(buyPriceModifier); // Vendor Buy Price Modifier
	writeD(100); // new aion 4.5
	writeC(showBuyTab ? 1 : 0);
	writeC(showSellTab ? 1 : 0);
	writeH(static_cast<int32_t>(tradeTablist.size()));
	for (const model::templates::tradelist::TradeListTemplate::TradeTab* tradeTabl : tradeTablist)
		writeD(tradeTabl->getId());
	writeH(static_cast<int32_t>(limitedItems.size()));
	for (const runtime::Ref<model::limiteditems::LimitedItem>& limitedItem : limitedItems) {
		writeD(limitedItem->getItemId());
		writeH(limitedItem->getBuyCount(playerObjId));
		writeH(limitedItem->getSellLimit());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
