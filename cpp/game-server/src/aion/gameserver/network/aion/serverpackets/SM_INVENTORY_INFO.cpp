#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_INVENTORY_INFO::SM_INVENTORY_INFO(bool isFirstPacketValue, const std::vector<runtime::Ptr<model::gameobjects::Item>>& itemsValue,
	model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_INVENTORY_INFO>) {
	AION_UNPORTED();
}

SM_INVENTORY_INFO::~SM_INVENTORY_INFO() = default;

void SM_INVENTORY_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

void SM_INVENTORY_INFO::writeItemInfo(model::gameobjects::Item& item) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
