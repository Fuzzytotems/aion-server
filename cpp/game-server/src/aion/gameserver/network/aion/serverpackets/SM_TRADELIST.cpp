#include "aion/gameserver/network/aion/serverpackets/SM_TRADELIST.h"

#include "aion/gameserver/model/limiteditems/LimitedItem.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TRADELIST::SM_TRADELIST(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::tradelist::TradeListTemplate* tlist, int32_t buyPriceModifierValue)
	: AionServerPacket(opcodeOf<SM_TRADELIST>) {
	AION_UNPORTED();
}

SM_TRADELIST::~SM_TRADELIST() = default;

void SM_TRADELIST::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
