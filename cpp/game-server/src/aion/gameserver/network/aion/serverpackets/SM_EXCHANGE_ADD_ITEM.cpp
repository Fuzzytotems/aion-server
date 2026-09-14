#include "aion/gameserver/network/aion/serverpackets/SM_EXCHANGE_ADD_ITEM.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_EXCHANGE_ADD_ITEM::SM_EXCHANGE_ADD_ITEM(int32_t actionValue, model::gameobjects::Item& itemValue, model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_EXCHANGE_ADD_ITEM>), player(playerValue), action(actionValue), item(itemValue) {
}

SM_EXCHANGE_ADD_ITEM::~SM_EXCHANGE_ADD_ITEM() = default;

void SM_EXCHANGE_ADD_ITEM::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
