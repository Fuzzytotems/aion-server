#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SEARCH.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PLAYER_SEARCH::SM_PLAYER_SEARCH(const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& playersValue)
	: AionServerPacket(opcodeOf<SM_PLAYER_SEARCH>), players(playersValue.begin(), playersValue.end()) {
}

SM_PLAYER_SEARCH::~SM_PLAYER_SEARCH() = default;

void SM_PLAYER_SEARCH::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
