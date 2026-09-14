#include "aion/gameserver/network/aion/serverpackets/SM_TRADE_IN_LIST.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TRADE_IN_LIST::SM_TRADE_IN_LIST(model::gameobjects::Npc& npcValue, const model::templates::tradelist::TradeListTemplate* tlistValue,
	int32_t buyPriceModifierValue)
	: AionServerPacket(opcodeOf<SM_TRADE_IN_LIST>), npc(npcValue), tlist(tlistValue), buyPriceModifier(buyPriceModifierValue) {
}

SM_TRADE_IN_LIST::~SM_TRADE_IN_LIST() = default;

void SM_TRADE_IN_LIST::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
