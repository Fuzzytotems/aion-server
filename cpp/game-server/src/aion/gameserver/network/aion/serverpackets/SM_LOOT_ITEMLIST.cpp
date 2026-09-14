#include "aion/gameserver/network/aion/serverpackets/SM_LOOT_ITEMLIST.h"

#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LOOT_ITEMLIST::SM_LOOT_ITEMLIST(model::gameobjects::DropNpc& dropNpc, const std::unordered_set<runtime::Ptr<model::drop::DropItem>>& setItems,
	model::gameobjects::player::Player& player)
	: AionServerPacket(opcodeOf<SM_LOOT_ITEMLIST>) {
	AION_UNPORTED();
}

SM_LOOT_ITEMLIST::~SM_LOOT_ITEMLIST() = default;

void SM_LOOT_ITEMLIST::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
