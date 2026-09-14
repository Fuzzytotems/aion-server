#include "aion/gameserver/network/aion/serverpackets/SM_VIEW_PLAYER_DETAILS.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_VIEW_PLAYER_DETAILS::SM_VIEW_PLAYER_DETAILS(const std::vector<runtime::Ptr<model::gameobjects::Item>>& itemsValue,
	model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_VIEW_PLAYER_DETAILS>), items(itemsValue.begin(), itemsValue.end()), player(playerValue) {
	AION_UNPORTED();
}

SM_VIEW_PLAYER_DETAILS::~SM_VIEW_PLAYER_DETAILS() = default;

void SM_VIEW_PLAYER_DETAILS::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

void SM_VIEW_PLAYER_DETAILS::writeItemInfo(model::gameobjects::Item& item) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
