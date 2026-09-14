#include "aion/gameserver/network/aion/serverpackets/SM_WAREHOUSE_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_WAREHOUSE_INFO::SM_WAREHOUSE_INFO(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, int32_t warehouseTypeValue,
	int32_t expandLvlValue, bool firstPacketValue, model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_WAREHOUSE_INFO>), warehouseType(warehouseTypeValue), itemList(items.begin(), items.end()),
	  firstPacket(firstPacketValue), expandLvl(expandLvlValue), player(playerValue) {
}

SM_WAREHOUSE_INFO::~SM_WAREHOUSE_INFO() = default;

void SM_WAREHOUSE_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

void SM_WAREHOUSE_INFO::writeItemInfo(model::gameobjects::Item& item) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
