#include "aion/gameserver/network/aion/serverpackets/SM_UPDATE_PLAYER_APPEARANCE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_UPDATE_PLAYER_APPEARANCE::SM_UPDATE_PLAYER_APPEARANCE(int32_t playerIdValue, const std::vector<runtime::Ptr<model::gameobjects::Item>>& itemsValue)
	: AbstractPlayerInfoPacket(opcodeOf<SM_UPDATE_PLAYER_APPEARANCE>), playerId(playerIdValue), items(itemsValue.begin(), itemsValue.end()) {
}

SM_UPDATE_PLAYER_APPEARANCE::~SM_UPDATE_PLAYER_APPEARANCE() = default;

void SM_UPDATE_PLAYER_APPEARANCE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
